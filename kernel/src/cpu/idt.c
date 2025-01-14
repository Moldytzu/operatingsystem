#include <cpu/idt.h>
#include <cpu/gdt.h>
#include <cpu/pic.h>
#include <cpu/smp.h>
#include <cpu/xapic.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <drv/serial.h>
#include <sched/scheduler.h>
#include <ipc/socket.h>
#include <main/panic.h>
#include <misc/logger.h>
#include <sys/sys.h>

idt_descriptor_t idtr;
idt_gate_descriptor_t *gates;
extern void *intHandlers[];
void *redirectTable[256];
int redirectTableMeta[256];
bool allocatedVector[0xFF];
uint8_t lastVector = 0x21;

// change gate information
void idtSetGate(void *handler, uint8_t entry)
{
    idt_gate_descriptor_t *gate = &gates[entry];    // select the gate
    if (gate->segmentselector == 0)                 // detect if we didn't touch the gate
        idtr.size += sizeof(idt_gate_descriptor_t); // if we didn't we can safely increase the size

    uint64_t base = (uint64_t)handler; // address of handler

    gate->attributes = 0xEE;     // set the attributes (set gate type to 64-bit interrupt, dpl to 3 and present bit)
    gate->segmentselector = 0x8; // set the kernel code selector from gdt
    gate->offset = base;         // first 16 bits of address
    gate->offset2 = base >> 16;  // second 16 bits of address
    gate->offset3 = base >> 32;  // last 32 bits of address
    gate->ist = 1;               // enable IST
}

void *idtGet()
{
    return (void *)gates;
}

// initialize the intrerupt descriptor table
void idtInit(uint16_t procID)
{
    cli(); // disable intrerrupts

    gates = vmmAllocateInitialisationVirtualAddressPage(); // allocate the gates

    idtr.offset = (uint64_t)gates; // set the offset to the data
    idtr.size = 0;                 // reset the size
    for (int i = 0; i < 0xFF; i++) // set all exception irqs to the base handler
        idtSetGate((void *)intHandlers[i], i);

    idtr.size--; // decrement to comply with the spec

    idtInstall(procID); // load the idtr

    // clear the redirection table
    zero(redirectTable, sizeof(redirectTable));

    // initialise allocated vectors
    zero(allocatedVector, sizeof(allocatedVector));

    logInfo("idt: loaded size %d", idtr.size);
}

void idtInstall(uint8_t procID)
{
    // setup ist
    gdt_tss_t *tss = tssGet()[procID];
    tss->ist[0] = tss->rsp[0] = (uint64_t)vmmAllocateInitialisationVirtualAddressPage() + VMM_PAGE; // kernel stack and IST

    iasm("lidt %0" ::"m"(idtr)); // load the idtr and don't enable intrerrupts yet
}

void idtRedirect(void *handler, uint8_t entry, uint32_t tid)
{
    if (tid != redirectTableMeta[entry] && tid != 0 && redirectTableMeta[entry] != 0) // don't allow to overwrite another driver's redirect
        return;

    redirectTable[entry] = handler;
    redirectTableMeta[entry] = tid;
}

void idtClearRedirect(uint32_t tid)
{
    for (int i = 0; i < 256; i++)
        if (redirectTableMeta[i] == tid)
            redirectTable[i] = NULL;
}

extern void callWithPageTable(uint64_t rip, uint64_t pagetable);

const char *exceptions[] = {
    "Divide By Zero", "Debug", "NMI", "Breakpoint", "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available", "Double Fault", "Segment Overrun", "Invalid TSS", "Segment Not Present", "Stack Fault", "General Protection Fault", "Page Fault"};

void exceptionHandler(idt_intrerrupt_error_stack_t *stack, uint64_t int_num)
{
    vmmSwap(vmmGetBaseTable()); // swap to the base table

#ifdef K_SMP
    if (int_num == XAPIC_NMI_VECTOR) // hang on non-maskable interrupts
        hang();
#endif

    if (redirectTable[int_num] && schedGet(redirectTableMeta[int_num])) // there is a request to redirect intrerrupt to a driver (todo: replace this with a struct)
    {
        callWithPageTable((uint64_t)redirectTable[int_num], (uint64_t)schedGet(redirectTableMeta[int_num])->pageTable); // give control to the driver
        xapicEOI();                                                                                                     // todo: send eoi only if the task asks us in the syscall
        return;                                                                                                         // don't execute rest of the handler
    }

    // check if interrupt comes from userspace (only possible if CS is 0x23)
    bool userspace = stack->cs == 0x23;

    // userspace exceptions
    if (userspace && int_num < 32)
    {
        sched_task_t *currentTask = schedGetCurrent(smpID());

        if (int_num == 0xE) // handle page faults caused by stack being overused
        {
            volatile uint64_t faultAddress = controlReadCR2(); // for some reason volatile is required here.....

            if (faultAddress >= currentTask->virtualBaseAddress || faultAddress <= TASK_BASE_SWITCH_TO_BUFFER - 1) // check if the fault is outside the stack
                goto panic;                                                                                        // if it is then panic

            // expand the stack by a page
            volatile uint64_t pageAddress = faultAddress & ~0xFFF;
            volatile void *page = pmmPage();
            vmmMap((void *)stack->cr3, (void *)pageAddress, (void *)page, VMM_ENTRY_USER | VMM_ENTRY_RW);

            sysPushUsedPage(currentTask, (void *)page);

            logInfo("idt: expanding stack of %s with one page", currentTask->name);

            return; // try again
        }

    panic:
        const char *name = currentTask->name;

#ifdef K_PANIC_ON_USERSPACE_CRASH
        framebufferZero();
        xapicNMI();
#endif

        logWarn("%s has crashed with %s at %x! Terminating it.", name, exceptions[int_num], stack->rip); // display message

#ifdef K_PANIC_ON_USERSPACE_CRASH
        hang();
#endif

        // tell the init system we crashed
        struct sock_socket *initSocket = sockGet(1);
        if (initSocket)
        {
            char *str = pmmPage();

            sprintf(str, "crash %s", name); // todo: also pass the address and other information

            sockAppend(initSocket, str, strlen(str)); // announce that the application has crashed

            pmmDeallocate(str);
        }

        schedKill(schedGetCurrent(smpID())->id); // terminate the task
        schedSwitchNext();                       // ask scheduler to switch to next task

        unreachable(); // schedSwitchNext never returns here!
    }

    // if we couldn't handle the interrupt, panic!
    xapicNMI();        // send nmi to every other core
    framebufferZero(); // clear the framebuffer

    // generate a message
    char message[96];

    if (int_num < sizeof(exceptions) / sizeof(uint64_t)) // generate a different message for exceptions
        sprintf(message, "Exception %s (0x%x)", exceptions[int_num], int_num);
    else
        sprintf(message, "Interrupt 0x%x", int_num);

    if (int_num == 0xE) // when a page fault occurs the faulting address is set in cr2
        logError("CR2=0x%p ", controlReadCR2());

    logError("CORE #%d: RIP=0x%p RSP=0x%p CS=0x%x SS=0x%x RFLAGS=0x%x ERROR=0x%p", smpID(), stack->rip, stack->rsp, stack->cs, stack->ss, stack->rflags, stack->error); // if it does print the error too

    panick(message);
}

uint8_t idtAllocateVector()
{
    for (int i = 0x22; i < 0xFF; i++)
    {
        if (allocatedVector[i])
            continue;

        allocatedVector[i] = true;

        logDbg(LOG_SERIAL_ONLY, "idt: allocating vector 0x%x", i);
        return i;
    }

    panick("IDT vector allocation overflow");
}

void idtFreeVector(uint8_t vector)
{
    if (vector < 0x22 || vector > 0xFF)
        return;

    // fixme: we don't check if the driver owns that vector
    // we would probably want to do that but it's not a big security issue

    allocatedVector[vector] = false;
}