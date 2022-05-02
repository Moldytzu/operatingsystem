#pragma once
#include <sys/sys.h>
#include <scheduler.h>

// display (rsi = call, rdx = arg1, r8 = arg2)
void display(uint64_t syscallNumber, uint64_t call, uint64_t arg1, uint64_t returnAddress, uint64_t arg2, uint64_t r9)
{
    switch (call)
    {
    case 0: // display set mode
        if(arg1 == VT_DISPLAY_FB || arg1 == VT_DISPLAY_TTY0)
            vtSetMode(arg1); // set vt mode
        break;
    default:
        break;
    }
}