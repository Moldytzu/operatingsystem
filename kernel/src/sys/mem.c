#include <sys/sys.h>
#include <mm/pmm.h>

// mem (rsi = call, rdx = arg1, r8 = arg2)
void mem(uint64_t call, uint64_t arg1, uint64_t arg2, uint64_t r9, sched_task_t *task)
{
    switch (call)
    {
    case 0:                      // mem allocate
        if (!INBOUNDARIES(arg1)) // prevent crashing
            return;

        // pages to allocate
        size_t pages = arg2;
        if (!pages)
            pages = 1;

        // todo: make it so the application can allocate smaller chunks of memory (maybe implement an allocator for each task based on the one present in mm/blk.c?)
        void *page = pmmPages(pages);                                                                                                                           // allocate a page
        for (int i = 0; i < pages; i++)                                                                                                                         // map pages
            vmmMap((void *)task->pageTable, (void *)(task->lastVirtualAddress + i * 4096), (void *)((uint64_t)page + i * 4096), VMM_ENTRY_RW | VMM_ENTRY_USER); // map it

        *(uint64_t *)PHYSICAL(arg1) = task->lastVirtualAddress; // give the application the virtual address
        task->lastVirtualAddress += 4096 * pages;               // increment the last virtual address

        if (task->allocatedIndex == ((task->allocatedBufferPages * VMM_PAGE) / 8) - pages) // reallocation needed when we overflow
        {
            task->allocated = pmmReallocate(task->allocated, task->allocatedBufferPages, task->allocatedBufferPages + 1);
            task->allocatedBufferPages++;
        }

        for (int i = 0; i < pages; i++)
            task->allocated[task->allocatedIndex++] = (void *)((uint64_t)page + i * 4096); // keep evidence of the page
        break;
    case 1: // mem info
        if (!INBOUNDARIES(arg1) || !INBOUNDARIES(arg2))
            return;

        pmm_pool_t total = pmmTotal();
        *(uint64_t *)PHYSICAL(arg1) = total.used;
        *(uint64_t *)PHYSICAL(arg2) = total.available;
    default:
        break;
    }
}