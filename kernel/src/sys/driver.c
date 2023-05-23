#include <sys/sys.h>
#include <mm/pmm.h>
#include <drv/framebuffer.h>
#include <drv/drv.h>
#include <drv/input.h>
#include <fw/acpi.h>
#include <elf/elf.h>
#include <cpu/ioapic.h>

#define SYS_DRIVER_TYPE_FRAMEBUFFER 1
#define SYS_DRIVER_TYPE_INPUT 2

// driver (rsi = call, rdx = arg1, r8 = arg2, r9 = arg3)
void driver(uint64_t call, uint64_t arg1, uint64_t arg2, uint64_t arg3, sched_task_t *task)
{
    if (!task->isDriver && task->id != 1) // unprivileged
        return;

    switch (call)
    {
    case 0: // driver start
        if (!INBOUNDARIES(arg1))
            return;

        uint64_t fd = openRelativePath(PHYSICAL(arg1), task);
        if(!fd)
            return;

        char path[512];
        zero(path, sizeof(path));
        vfsGetPath(fd, path);

        elfLoad(path, 0, 0, true); // start the driver
        break;

    case 1: // driver announce
        if (!INBOUNDARIES(arg2))
            return;

        uint64_t *ret = PHYSICAL(arg2);

        *ret = (uint64_t)drvRegister(task->id, arg1);

        vmmMap((vmm_page_table_t *)task->pageTable, (void *)*ret, (void *)*ret, VMM_ENTRY_RW | VMM_ENTRY_USER); // map address

        break;

    case 2:           // flush struct updates
        switch (arg1) // type
        {
        case SYS_DRIVER_TYPE_INPUT: // input
            inputFlush();
            break;
        case SYS_DRIVER_TYPE_FRAMEBUFFER: // framebuffer
            framebufferFlush();
            break;
        default:
            break;
        }

        break;

    case 3: // redirect idt gate
        if (!INBOUNDARIES(arg1))
            return;

        idtRedirect((void *)arg1, arg2, task->id); // redirect int arg2 to arg1

        break;
    case 4:                                // reset idt gate
        idtRedirect(NULL, arg1, task->id); // nullify
        break;

    case 5: // get pci header
        if (!INBOUNDARIES(arg1))
            return;

        acpi_pci_header_t **header = (acpi_pci_header_t **)PHYSICAL(arg1);

        if(!pciECAM()) // pcie not available
        {
            *header = NULL;
            return;
        }

        volatile acpi_pci_descriptor_t *functions = pciGetFunctions();
        volatile uint64_t num = pciGetFunctionsNum();

        // search for the pci device
        for (int i = 0; i < num; i++)
        {
            if (!functions[i].header)
                continue;

            // map it
            vmmMap(vmmGetBaseTable(), functions[i].header, functions[i].header, VMM_ENTRY_RW | VMM_ENTRY_USER | VMM_ENTRY_WRITE_THROUGH);
            vmmMap((void *)task->pageTable, functions[i].header, functions[i].header, VMM_ENTRY_RW | VMM_ENTRY_USER | VMM_ENTRY_WRITE_THROUGH);

            if (functions[i].header->vendor == arg2 && functions[i].header->device == arg3)
            {
                *header = functions[i].header; // pass the header
                return;
            }
        }

        break;

    case 6: // identity map
        vmmMap((void *)task->pageTable, (void *)arg1, (void *)arg1, VMM_ENTRY_RW | VMM_ENTRY_USER);
        break;

    case 7: // redirect irq to vector
        ioapicRedirectIRQ(arg1, arg2, smpID());
        break;

    default:
        break;
    }
}