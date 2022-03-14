#include <framebuffer.h>

struct psf1_header *font;
struct stivale2_module fontMod;
struct stivale2_struct_tag_framebuffer *framebufTag;

void framebufferInit()
{
    framebufTag = bootloaderGetFramebuf();

    if (framebufTag->memory_model != 1)
    {
        bootloaderTermWrite("Unsupported framebuffer memory model.\n");
        hang();
    }

    framebufferLoadFont("font-8x16.psf"); // load default font

    framebufferClear(0x000000); // clear framebuffer
}

void framebufferClear(uint32_t colour)
{
    memset32((void *)framebufTag->framebuffer_addr, colour, framebufTag->framebuffer_pitch * framebufTag->framebuffer_height); // clear the screen
}

void framebufferLoadFont(const char *module)
{
    fontMod = bootloaderGetModule(module);
    font = (struct psf1_header *)((void *)fontMod.begin);

    if(strlen(fontMod.string) != strlen(module)) // if the modules' string len doesn't match, just fail
        goto error;

    if (font->magic[0] != PSF1_MAGIC0 && font->magic[0] != PSF1_MAGIC1) // if the psf1's magic isn't the one we expect, just fail
        goto error;

    return;

error:
    bootloaderTermWrite("Failed to load font \"");
    bootloaderTermWrite(module);
    bootloaderTermWrite("\".\n");
    hang();
}

void framebufferPlotp(uint32_t x, uint32_t y, uint32_t colour)
{
    *(uint32_t*)((uint32_t*)framebufTag->framebuffer_addr + x + y * framebufTag->framebuffer_width) = colour; // set the pixel to colour
}

void framebufferPlotc(char c, uint32_t x, uint32_t y)
{
    uint8_t *character = (uint8_t*)font + sizeof(struct psf1_header) + c * font->charsize; // get the offset by skipping the header and indexing the character
    for(size_t dy = 0; dy < font->charsize; dy++, character++) // loop thru each line of the character
    {
        for(size_t dx = 0; dx < 8; dx++) // 8 pixels wide
        {
            uint8_t mask = 0b10000000 >> dx; // create a bitmask that shifts based on which pixel from the line we're indexing
            if(*character & mask) // and the mask with the line
                framebufferPlotp(dx+x,dy+y,0xFFFFFF);
        }
    }
}