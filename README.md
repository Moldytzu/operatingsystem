# mOS

### Hobby operating system written in C that targets early x86_64 computers and low RAM requirements
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
    - Preemptive round-robin scheduler
    - Socket IPC
    - Unlimited virtual terminals
    - 15+ system calls accessible in userspace
    - Driver infrastructure
    - Symmetric multiprocessing support
    - FAT32 support
    - AHCI support
    - ATA/IDE support
    - PS/2 keyboard and mouse support
    - Primitive window management
    - CLI applications
    - VMware SVGA II support
    - Bochs/QEmu video support
    - Serial debug console
    - Userspace libraries

## Lines of code
mOS (kernel+userspace+libraries) consist of 10825 lines of code (as of 20 Aug. 2023).

## Building and running
See [BUILDING.md](./docs/BUILDING.md)

## Screenshots
See [screenshots folder here](./docs/gallery/)

## Font
The default terminal font is a modified version of [Hack](https://github.com/source-foundry/Hack), licensed under the MIT License.