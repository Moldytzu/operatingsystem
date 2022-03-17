#include <pit.h>
#include <framebuffer.h>

struct pit_packet packet;

extern void PITHandlerEntry();
extern void PITHandler()
{
    framebufferWrite("T");
    picEOI();
}

void pitSet(uint32_t hz)
{
    // send packet
    outb(PIT_CMD, unsafe_cast(packet,uint8_t));

    // send divisor
    int div = PIT_DIV / hz;
    outb(PIT_CH_0, div & 0xFF); // low
    outb(PIT_CH_0, (div & 0xFF00) >> 8); // high
}

void pitInit()
{
    asm volatile("cli"); // disable intrerrupts

    // fill the packet
    packet.channel = 0b00; // channel 0
    packet.accessmode = 0b11; // send both low byte and high byte
    packet.operatingmode = 0b010; // mode 2, rate generator
    packet.binarymode = 0b0; // binary mode 

    pitSet(128); // 128 hz is enough

    // set idt gate
    idtSetGate((void*)PITHandlerEntry,PIC_IRQ_0,IDT_InterruptGate);

    // unmask IRQ 0 on PIC
    outb(PIC_MASTER_DAT,inb(PIC_MASTER_DAT) & ~0b00000001);
    
    asm volatile("sti"); // enable intrerrupts
}