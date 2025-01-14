#pragma once
#include <misc/utils.h>

ifunc void outb(uint16_t port, uint8_t val) // out byte
{
    iasm("outb %0, %1" ::"a"(val), "Nd"(port));
}

ifunc uint8_t inb(uint16_t port) // in byte
{
    uint8_t val;
    iasm("inb %%dx,%%al"
         : "=a"(val)
         : "d"(port));
    return val;
}

ifunc void outw(uint16_t port, uint16_t val) // out word
{
    iasm("outw %0, %1" ::"a"(val), "Nd"(port));
}

ifunc uint16_t inw(uint16_t port) // in word
{
    uint16_t val;
    iasm("inw %%dx,%%ax"
         : "=a"(val)
         : "d"(port));
    return val;
}

ifunc void outd(uint16_t port, uint32_t val) // out dword
{
    iasm("out %0, %1" ::"a"(val), "Nd"(port));
}

ifunc uint32_t ind(uint16_t port) // in dword
{
    uint32_t val;
    iasm("in %1, %0"
         : "=a"(val)
         : "Nd"(port));
    return val;
}