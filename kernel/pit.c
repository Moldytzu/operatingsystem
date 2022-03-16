#include <pit.h>
#include <framebuffer.h>

extern void PITHandlerEntry();
extern void PITHandler()
{
    framebufferWrite("PIT!\n");
    picEOI();
}

void pitInit()
{
    // unmask IRQ 0 on PIC
    outb(PIC_MASTER_DAT,inb(PIC_MASTER_DAT) & 0b11111110);

    // set idt gate
    idtSetGate((void*)PITHandlerEntry,0x20,IDT_InterruptGate);
}