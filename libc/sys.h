#include <stdint.h>
#include <stddef.h>

#define SYS_INPUT_KEYBOARD 0

#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_INPUT 3

extern void _syscall(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx, uint64_t r8, uint64_t r9);
void sys_exit(uint64_t status);
void sys_write(void *buffer, uint64_t count, uint64_t fd);
void sys_read(void *buffer, uint64_t count, uint64_t fd);
void sys_input(uint8_t deviceType, char *returnPtr);