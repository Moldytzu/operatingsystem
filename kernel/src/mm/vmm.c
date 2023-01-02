#include <mm/vmm.h>
#include <mm/pmm.h>
#include <drv/serial.h>
#include <fw/bootloader.h>
#include <cpu/idt.h>
#include <cpu/gdt.h>

vmm_page_table_t *baseTable;

// initialize the virtual memory manager
void vmmInit()
{
    baseTable = vmmCreateTable(true, false); // create the base table with hhdm
    vmmSwap(baseTable);                      // swap the table

    printk("vmm: loaded a new page table\n");
}

// set flags of some entries given by the indices
void vmmSetFlags(vmm_page_table_t *table, vmm_index_t index, bool user, bool rw)
{
    vmm_page_table_t *pml4, *pdp, *pd, *pt;
    uint64_t currentEntry;
    pml4 = table;

    currentEntry = pml4->entries[index.PDP];         // index pdp
    vmmSetFlag(&currentEntry, VMM_ENTRY_RW, rw);     // read-write
    vmmSetFlag(&currentEntry, VMM_ENTRY_USER, user); // userspace
    pml4->entries[index.PDP] = currentEntry;         // write the entry in the table

    pdp = (vmm_page_table_t *)(vmmGetAddress(&currentEntry) << 12); // continue
    currentEntry = pdp->entries[index.PD];                          // index further
    vmmSetFlag(&currentEntry, VMM_ENTRY_RW, rw);                    // read-write
    vmmSetFlag(&currentEntry, VMM_ENTRY_USER, user);                // userspace
    pdp->entries[index.PD] = currentEntry;                          // write the entry in the table

    pd = (vmm_page_table_t *)(vmmGetAddress(&currentEntry) << 12); // continue
    currentEntry = pd->entries[index.PT];                          // index further
    vmmSetFlag(&currentEntry, VMM_ENTRY_RW, rw);                   // read-write
    vmmSetFlag(&currentEntry, VMM_ENTRY_USER, user);               // userspace
    pd->entries[index.PT] = currentEntry;                          // write the entry in the table

    pt = (vmm_page_table_t *)(vmmGetAddress(&currentEntry) << 12); // continue
    currentEntry = pt->entries[index.P];                           // index further
    vmmSetFlag(&currentEntry, VMM_ENTRY_RW, rw);                   // read-write
    vmmSetFlag(&currentEntry, VMM_ENTRY_USER, user);               // userspace
    pt->entries[index.P] = currentEntry;                           // write the entry in the table
}

// map a virtual address to a physical address in a page table
void vmmMap(vmm_page_table_t *table, void *virtualAddress, void *physicalAddress, bool user, bool rw)
{
    vmm_index_t index = vmmIndex((uint64_t)virtualAddress); // get the offsets in the page tables
    vmm_page_table_t *pml4, *pdp, *pd, *pt;
    uint64_t currentEntry;

    pml4 = table;

    currentEntry = pml4->entries[index.PDP];           // index pdp
    if (!vmmGetFlag(&currentEntry, VMM_ENTRY_PRESENT)) // if there isn't any page present there, we generate it
    {
        pdp = pmmPage();                                    // allocate table
        zero(pdp, VMM_PAGE);                                // clear it
        vmmSetAddress(&currentEntry, (uint64_t)pdp >> 12);  // set it's address
        vmmSetFlag(&currentEntry, VMM_ENTRY_PRESENT, true); // present
        pml4->entries[index.PDP] = currentEntry;            // write the entry in the table
    }
    else
        pdp = (vmm_page_table_t *)(vmmGetAddress(&currentEntry) << 12); // continue

    currentEntry = pdp->entries[index.PD];             // index pd
    if (!vmmGetFlag(&currentEntry, VMM_ENTRY_PRESENT)) // if there isn't any page present there, we generate it
    {
        pd = pmmPage();                                     // allocate table
        zero(pd, VMM_PAGE);                                 // clear it
        vmmSetAddress(&currentEntry, (uint64_t)pd >> 12);   // set it's address
        vmmSetFlag(&currentEntry, VMM_ENTRY_PRESENT, true); // present
        pdp->entries[index.PD] = currentEntry;              // write the entry in the table
    }
    else
        pd = (vmm_page_table_t *)(vmmGetAddress(&currentEntry) << 12); // continue

    currentEntry = pd->entries[index.PT];              // index pt
    if (!vmmGetFlag(&currentEntry, VMM_ENTRY_PRESENT)) // if there isn't any page present there, we generate it
    {
        pt = pmmPage();                                     // allocate table
        zero(pt, VMM_PAGE);                                 // clear it
        vmmSetAddress(&currentEntry, (uint64_t)pt >> 12);   // set it's address
        vmmSetFlag(&currentEntry, VMM_ENTRY_PRESENT, true); // present
        pd->entries[index.PT] = currentEntry;               // write the entry in the table
    }
    else
        pt = (vmm_page_table_t *)(vmmGetAddress(&currentEntry) << 12); // continue

    currentEntry = pt->entries[index.P];                           // index p
    vmmSetAddress(&currentEntry, (uint64_t)physicalAddress >> 12); // set the address to the physical one
    vmmSetFlag(&currentEntry, VMM_ENTRY_PRESENT, true);            // present
    pt->entries[index.P] = currentEntry;                           // write the entry in the table

    vmmSetFlags(table, index, user, rw); // set the flags
}

// unmap a virtual address
void vmmUnmap(vmm_page_table_t *table, void *virtualAddress)
{
    vmm_index_t index = vmmIndex((uint64_t)virtualAddress); // get the offsets in the page tables
    vmm_page_table_t *pdp, *pd, *pt;

    uint64_t currentEntry = table->entries[index.PDP]; // index pdp
    pdp = (vmm_page_table_t *)(vmmGetAddress(&currentEntry) << 12);

    currentEntry = pdp->entries[index.PD]; // index pd
    pd = (vmm_page_table_t *)(vmmGetAddress(&currentEntry) << 12);

    currentEntry = pd->entries[index.PT]; // index pt
    pt = (vmm_page_table_t *)(vmmGetAddress(&currentEntry) << 12);

    currentEntry = pt->entries[index.P];                 // index p
    vmmSetFlag(&currentEntry, VMM_ENTRY_PRESENT, false); // unvalidate page
    pt->entries[index.P] = currentEntry;                 // write the entry in the table
}

// get the base table aka kernel table
void *vmmGetBaseTable()
{
    return baseTable;
}

// get physical address of a virtual address
void *vmmGetPhys(vmm_page_table_t *table, void *virtualAddress)
{
    // get physical memory address form virtual memory address
    vmm_index_t index = vmmIndex((uint64_t)virtualAddress); // get the offsets in the page tables
    vmm_page_table_t *pdp, *pd, *pt;

    uint64_t currentEntry = table->entries[index.PDP];              // index pdp
    pdp = (vmm_page_table_t *)(vmmGetAddress(&currentEntry) << 12); // continue

    currentEntry = pdp->entries[index.PD];                         // index pd
    pd = (vmm_page_table_t *)(vmmGetAddress(&currentEntry) << 12); // continue

    currentEntry = pd->entries[index.PT];                          // index pt
    pt = (vmm_page_table_t *)(vmmGetAddress(&currentEntry) << 12); // continue

    currentEntry = pt->entries[index.P];                                                              // index p
    return (void *)(vmmGetAddress(&currentEntry) * VMM_PAGE + ((uint64_t)virtualAddress % VMM_PAGE)); // get the address
}

// create a new table
vmm_page_table_t *vmmCreateTable(bool full, bool driver)
{
#ifdef K_VMM_DEBUG
    uint64_t a = pmmTotal().available;
#endif

    // create a new table to use as a base for everything
    register void *newTable = pmmPage(); // allocate a page for the new table
    zero(newTable, VMM_PAGE);            // clear it

    struct limine_memmap_response *memMap = bootloaderGetMemoryMap();
    uint64_t hhdm = (uint64_t)bootloaderGetHHDM();
    struct limine_kernel_address_response *kaddr = bootloaderGetKernelAddress();

    // map the system tables as kernel rw
    vmmMap(newTable, newTable, newTable, driver, true);                 // page table
    vmmMap(newTable, (void *)tssGet(), (void *)tssGet(), driver, true); // tss
    vmmMap(newTable, (void *)gdtGet(), (void *)gdtGet(), driver, true); // gdt

#ifdef K_IDT_IST
    vmmMap(newTable, (void *)tssGet()->ist[0], (void *)tssGet()->ist[0], driver, true); // kernel ist
    vmmMap(newTable, (void *)tssGet()->ist[1], (void *)tssGet()->ist[1], driver, true); // user ist
#endif

    // map memory map entries as kernel rw
    for (size_t i = 0; i < memMap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memMap->entries[i];

        if (entry->type != LIMINE_MEMMAP_KERNEL_AND_MODULES && !full) // don't map any type of memory besides kernel
            continue;

        if (entry->type == LIMINE_MEMMAP_KERNEL_AND_MODULES)
        {
            for (size_t i = 0; i < entry->length; i += 4096)
                vmmMap(newTable, (void *)(kaddr->virtual_base + i), (void *)(kaddr->physical_base + i), driver, true);
        }
        for (size_t i = 0; i < entry->length; i += 4096)
        {
            vmmMap(newTable, (void *)(entry->base + i), (void *)(entry->base + i), driver, true);
            vmmMap(newTable, (void *)(entry->base + i + hhdm), (void *)(entry->base + i), driver, true);
        }
    }

#ifdef K_VMM_DEBUG
    printks("vmm: wasted %d KB on a new page table\n\r", toKB(a - pmmTotal().available));
#endif

    return newTable; // return the created table
}

// free a table
void vmmDestroy(vmm_page_table_t *table)
{
    // fixme: this function deallocates one page more than needed?

#ifdef K_VMM_DEBUG
    uint64_t a = pmmTotal().available;
#endif

    // follow the top 3 levels of the page table
    for (int i = 0; i < 512; i++)
    {
        uint64_t address = table->entries[i];

        if (!vmmGetFlag(&address, VMM_ENTRY_PRESENT)) // we look for the present entries only
            continue;

        // look deeper
        for (int j = 0; j < 512; j++)
        {
            uint64_t a = ((vmm_page_table_t *)(vmmGetAddress(&address) << 12))->entries[j];

            if (!a) // non-existent
                continue;

            if (!vmmGetFlag(&a, VMM_ENTRY_PRESENT)) // we look for the present entries only
                continue;

            for (int k = 0; k < 512; k++)
            {
                uint64_t b = ((vmm_page_table_t *)(vmmGetAddress(&a) << 12))->entries[k];

                if (!b) // non-existent
                    continue;

                if (!vmmGetFlag(&b, VMM_ENTRY_PRESENT)) // we look for the present entries only
                    continue;

                b = align(b, 4096) + 4096; // round it down

                pmmDeallocate((void *)b); // deallocate it
            }

            a = align(a, 4096) + 4096; // round it down

            pmmDeallocate((void *)a); // deallocate it
        }

        address = align(address, 4096) + 4096; // round it down

        pmmDeallocate((void *)address); // deallocate it
    }

    pmmDeallocate(table);

#ifdef K_VMM_DEBUG
    printks("vmm: destroyed page table at 0x%p and saved %d kb\n\r", table, toKB(pmmTotal().available - a));
#endif
}