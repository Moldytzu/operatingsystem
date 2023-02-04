# mOS
![Lines of code](https://img.shields.io/tokei/lines/github/Moldytzu/mOS?style=for-the-badge) 
### Hobby operating system written in C that targets 1st gen x86_64 Nocona Xeon Intel CPUs and low RAM requirements
### Features
    - Monolithic kernel design
    - 32 bpp linear frame buffer support
    - Custom initrd support
    - Serial console support
    - Static ELF loading support
    - Basic VFS
    - PCI enumeration
    - Very basic ACPI support
    - Block allocator
    - Page allocator
    - Preemptive scheduler
    - Socket IPC
    - Unlimited virtual terminals
    - 15+ system calls accessible in userspace
    - Driver infrastructure

## Building and running
See [BUILDING.md](./docs/BUILDING.md)