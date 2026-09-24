# sverige Linux

![sverige Linux](assets/images/Banner.jpg)

sverige Linux is an independent x86_64 Linux system built from upstream source.

The project focuses on building the system directly from the software it uses rather than distributing a pre-built base system.

---

## What is sverige?

The base system is built from upstream projects including GNU, the Linux kernel, systemd, and other components of the Linux userspace.

The build process is documented so that the system can be built from a clean x86_64 environment without relying on a pre-built sverige base image or binary package repository.

### Source-based

Software is obtained from its upstream source archives and built during the installation process.

### x86_64

The current build is designed for x86_64 systems.

### systemd

The base system uses systemd for initialization and system management.

### Documented

The build process, from the initial toolchain through the kernel and bootloader, is documented in the Base Guide.

---

## Documentation

### sverige Base Guide

The main installation guide.

It covers the base system, including:

- Build environment
- Toolchain bootstrap
- Glibc
- GCC and Binutils
- GNU userspace
- systemd
- Linux
- Linux firmware
- GRUB
- Final system configuration

[Read the Base Guide →](guide.md)

---

## Suomi

**Suomi** is the planned extended userspace guide for software that is not part of the base installation.

It will cover areas such as:

- Wayland and X11
- Mesa and NVIDIA
- PipeWire
- Desktop environments
- Multimedia software

**Status: In development**

---

## Source

The complete project is available on GitHub.

[View the sverige Linux repository →](https://github.com/ollipix/sverige)
