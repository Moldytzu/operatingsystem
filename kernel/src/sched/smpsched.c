#include <sched/smpsched.h>
#include <misc/logger.h>
#include <cpu/smp.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <drv/serial.h>

// todo: replace these with a linked list, we use it like this for the proof of concept
sched_task_t queue[K_MAX_CORES][128];
uint32_t queueIdx[K_MAX_CORES];
uint32_t queueMax = 0;

bool _enabled = false;

void taskCommon()
{
    while (1)
        ;
}

void taskA()
{
    while (1)
        serialWrite("A");
}

void taskB()
{
    while (1)
        serialWrite("B");
}

void taskC()
{
    while (1)
        serialWrite("C");
}

void schedEnable()
{
    _enabled = true;
    sti();
    while (1)
        ;
}

void schedSchedule(idt_intrerrupt_stack_t *stack)
{
    if (!_enabled)
        return;

    uint64_t id = smpID();

    // save old state
    memcpy(&queue[id][queueIdx[id]].registers, stack, sizeof(idt_intrerrupt_stack_t));

    // get next id
    queueIdx[id]++;

    if (queueIdx[id] >= queueMax)
        queueIdx[id] = 0;

    // copy new state
    memcpy(stack, &queue[id][queueIdx[id]].registers, sizeof(idt_intrerrupt_stack_t));
}

void schedInit()
{
    zero(queueIdx, sizeof(queueIdx));

    queueMax = 2;

    int q = 0;

    for (int i = 0; i < smpCores(); i++) // single core for the moment
    {
        sched_task_t *t = queue[i];
        t[0].registers.rflags = 0b1000000010; // interrupts
        t[0].registers.cs = 8;
        t[0].registers.ss = 16;
        t[0].registers.rsp = t[0].registers.rbp = (uint64_t)pmmPage() + PMM_PAGE;
        t[0].registers.rip = (uint64_t)taskCommon;
        t[0].registers.cr3 = (uint64_t)vmmGetBaseTable();

        t[1].registers.rflags = 0b1000000010; // interrupts
        t[1].registers.cs = 8;
        t[1].registers.ss = 16;
        t[1].registers.rsp = t[1].registers.rbp = (uint64_t)pmmPage() + PMM_PAGE;

        t[1].registers.cr3 = (uint64_t)vmmGetBaseTable();

        switch (q++)
        {
        case 0:
            t[1].registers.rip = (uint64_t)taskA;
            break;
        case 1:
            t[1].registers.rip = (uint64_t)taskB;
            break;
        case 2:
            t[1].registers.rip = (uint64_t)taskC;
            break;

        default:
            t[1].registers.rip = (uint64_t)taskA;
            q = 0;
            break;
        }
    }
}