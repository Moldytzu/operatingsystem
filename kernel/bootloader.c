#include <bootloader.h>

struct stivale2_struct_tag_framebuffer *framebufTag;
struct stivale2_struct_tag_terminal *termTag;

void (*termWrite)(const char *string, size_t length);

// We need to tell the stivale bootloader where we want our stack to be.
static uint8_t stack[8*1024*1024]; // 8 mb stack

// terminal
static struct stivale2_header_tag_terminal terminal_hdr_tag = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_TERMINAL_ID, // id
        .next = 0 // end
    },
    .flags = 0 // unusded
};

// framebuffer
static struct stivale2_header_tag_framebuffer framebuffer_hdr_tag = {
    // Same as above.
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
        // Instead of 0, we now point to the previous header tag. The order in
        // which header tags are linked does not matter.
        .next = (uint64_t)&terminal_hdr_tag
    },
    // We set all the framebuffer specifics to 0 as we want the bootloader
    // to pick the best it can.
    .framebuffer_width  = 0,
    .framebuffer_height = 0,
    .framebuffer_bpp    = 0
};

// stivale header
__attribute__((section(".stivale2hdr"), used))
static struct stivale2_header stivale_hdr = {
    .entry_point = 0, // elf entry point
    .stack = (uintptr_t)stack + sizeof(stack), // stack
    .flags = 0b1111,
    .tags = (uintptr_t)&framebuffer_hdr_tag // root of the linked list
};

// get tag
void *bootloaderGetTag(struct stivale2_struct *stivale2_struct, uint64_t id) {
    struct stivale2_tag *current_tag = (void *)stivale2_struct->tags;
    while(true)
    {
        // check if the list is over
        if (current_tag == NULL) hang(); // hang if we don't find it

        // check whether the identifier matches.
        if (current_tag->identifier == id) return current_tag;

        // get the next tag
        current_tag = (void *)current_tag->next;
    }
}

// init stivale2 bootloader
void bootloaderInit(struct stivale2_struct *stivale2_struct)
{
    termTag = bootloaderGetTag(stivale2_struct, STIVALE2_STRUCT_TAG_TERMINAL_ID); // get terminal
    termWrite = (void*)termTag->term_write; // set write function

    framebufTag = bootloaderGetTag(stivale2_struct,STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID); // get frame buffer info
    memset((void*)framebufTag->framebuffer_addr,0xFF,framebufTag->framebuffer_pitch*framebufTag->framebuffer_height); // fill the screen with white
}

// write to stivale2 terminal
void bootloaderTermWrite(const char *str)
{
    termWrite(str,strlen(str));
}