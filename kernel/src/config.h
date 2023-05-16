#pragma once

// ======================================================================================
// mOS Kernel configuration
// Uncomment to enable, comment to disable
// Be sure to do `make clean` before rebuilding to be sure the new settings will apply
// ======================================================================================

// Debug
#define K_RELEASE // generate release build of the kernel (removes all debug statements);

#ifndef K_RELEASE
#define K_VT_DEBUG      // vt debug messages
#define K_VFS_DEBUG     // vfs debug messages
#define K_SYSCALL_DEBUG // syscall debug messages
#define K_ELF_DEBUG     // elf debug messages
#define K_ACPI_DEBUG    // acpi debug messages
#define K_SCHED_DEBUG   // scheduler debug messages
#define K_INPUT_DEBUG   // input subsystem debug messages
#define K_VMM_DEBUG     // virtual memory manager debug messages
#define K_PMM_DEBUG     // physical memory manager debug messages
#define K_HEAP_DEBUG    // heap debug messages
#define K_BLDR_DEBUG    // bootloader debug messages
#define K_SOCK_DEBUG    // socket debug messages
#endif

// SMP
#define K_SMP // enables symetric multiprocessing support (experimental)

#ifdef K_SMP
#define K_MAX_CORES 128 // maximum cores
#else
#define K_MAX_CORES 1 // this decreases kernel memory usage
#endif

// Serial
#define K_COM_ENABLE     // enable support for serial
#define K_COM_BAUD_DIV 1 // divisor of the baud rate (base is 115200)
#define K_COM_LOG        // kernel logger on serial

// Scheduler
#define K_SCHED_MIN_QUANTUM 1  // minimum quantum of each task
#define K_STACK_SIZE 64 * 1024 // userspace stack size

// LAPIC
#define K_LAPIC_FREQ 300 // target lapic timer frequency

// Framebuffer
#define K_FB_SCROLL        // enables framebuffer scrolling (by default after last line it clears the screen then starts again from the top which is faster)
//#define K_FB_DOUBLE_BUFFER // enables kernel double buffering (decreases flickering at expense of memory and latency)

// ACPI
// #define K_ACPI_LAI // enables lai support (enables experimental aml support)

// Panic
// #define K_PANIC_REBOOT // reboot on kernel panic