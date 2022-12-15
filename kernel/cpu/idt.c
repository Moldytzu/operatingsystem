#include <cpu/idt.h>
#include <cpu/gdt.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <drv/serial.h>
#include <sched/scheduler.h>
#include <subsys/socket.h>
#include <main/panic.h>
#include <cpu/pic.h>

idt_descriptor_t idtr;
idt_gate_descriptor_t *gates;
extern void *int_table[];
void *redirectTable[256];
int redirectTableMeta[256];

// change gate information
void idtSetGate(void *handler, uint8_t entry, uint8_t attributes, bool user)
{
    idt_gate_descriptor_t *gate = &gates[entry]; // select the gate
    zero(gate, sizeof(idt_gate_descriptor_t));
    if (gate->segmentselector == 0)                 // detect if we didn't touch the gate
        idtr.size += sizeof(idt_gate_descriptor_t); // if we didn't we can safely increase the size

    gate->attributes = attributes;                                     // set the attributes
    gate->segmentselector = (8 * 1);                                   // set the kernel code selector from gdt
    gate->offset = (uint16_t)((uint64_t)handler & 0x000000000000ffff); // offset to the entry
    gate->offset2 = (uint16_t)(((uint64_t)handler & 0x00000000ffff0000) >> 16);
    gate->offset3 = (uint32_t)(((uint64_t)handler & 0xffffffff00000000) >> 32);

#ifdef K_IDT_IST
    // enable ists
    if (user)
        gate->ist = 2; // separate ists
    else
        gate->ist = 1;
#endif
}

// initialize the intrerupt descriptor table
void idtInit()
{
    cli(); // disable intrerrupts

#ifdef K_IDT_IST
    // setup ist
    tssGet()->ist[0] = (uint64_t)pmmPage() + VMM_PAGE;
    tssGet()->ist[1] = (uint64_t)pmmPage() + VMM_PAGE;

    zero((void *)tssGet()->ist[0] - VMM_PAGE, VMM_PAGE);
    zero((void *)tssGet()->ist[1] - VMM_PAGE, VMM_PAGE);
#endif

    // allocate the gates
    gates = pmmPage();
    zero(gates, VMM_PAGE);

    idtr.offset = (uint64_t)gates; // set the offset to the data
    idtr.size = 0;                 // reset the size
    for (int i = 0; i < 0xFF; i++) // set all exception irqs to the base handler
        idtSetGate((void *)int_table[i], i, IDT_InterruptGateU, true);

    idtr.size--;                 // decrement to comply with the spec
    iasm("lidt %0" ::"m"(idtr)); // load the idtr
    sti();                       // enable intrerrupts

    // clear the redirection table
    zero(redirectTable, sizeof(redirectTable));

    printk("idt: loaded size %d\n", idtr.size);
}

void idtRedirect(void *handler, uint8_t entry, uint32_t tid)
{
    redirectTable[entry] = handler;
    redirectTableMeta[entry] = tid;
}
extern void callWithPageTable(uint64_t rip, uint64_t pagetable);

const char *exceptions[] = {
    "Divide By Zero", "Debug", "NMI", "Breakpoint", "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available", "Double Fault", "_", "Invalid TSS", "Segment Not Present", "Stack Fault", "General Protection Fault", "Page Fault"};

void exceptionHandler(idt_intrerrupt_stack_t *stack, uint64_t int_num)
{
    vmmSwap(vmmGetBaseTable()); // swap to the base table

    if (redirectTable[int_num]) // there is a request to redirect intrerrupt to a driver
    {
        struct sched_task *task = schedulerGet(redirectTableMeta[int_num]);

        if (!task)
        {
            printks("the task doesn't exist anymore!\n");
            redirectTable[int_num] = NULL;
            goto cnt;
        }

        // printks("redirecting %x to %x (requested by task %d with stack %x)\n", int_num, redirectTable[int_num], redirectTableMeta[int_num], task->intrerruptStack.rsp);

        // this line gives the control to the driver
        callWithPageTable((uint64_t)redirectTable[int_num], (uint64_t)task->pageTable);

        goto cnt;
    }

cnt:
    if (int_num >= 0x20 && int_num <= 0x2D) // pic intrerrupt
    {
        picEOI();
        return;
    }

    if (stack->cs == 0x23) // userspace
    {
        struct sock_socket *initSocket = sockGet(1);

        const char *name = schedulerGetCurrent()->name;

        printks("%s has crashed with %s! Terminating it.\n\r", name, exceptions[int_num]);

        if (initSocket)
        {
            char *str = malloc(8 + strlen(name));
            zero(str, 6 + strlen(name));

            // construct a string based on the format "crash %s", name
            memcpy(str, "crash ", 6);
            memcpy(str + 6, name, strlen(name));

            sockAppend(initSocket, str, strlen(str)); // announce that the application has crashed

            free(str);
        }

        schedulerKill(schedulerGetCurrent()->id); // terminate the task
        schedulerSchedule(stack);                 // schedule next task
        return;
    }

    framebufferClear(0);

    const char *message = to_hstring(int_num);

    if (int_num < sizeof(exceptions) / 8)
        message = exceptions[int_num];

    if (int_num == 0xE) // when a page fault occurs the faulting address is set in cr2
        printk("CR2=0x%p ", controlReadCR2());

    printk("RIP=0x%p CS=0x%p RFLAGS=0x%p RSP=0x%p SS=0x%p ERR=0x%p", stack->rip, stack->cs, stack->rflags, stack->rsp, stack->ss, stack->error);
    panick(message);
}