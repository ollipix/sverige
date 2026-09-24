# Sverige Linux

Welcome to the official documentation for **sverige Linux**—an independent, source-built x86_64 operating system assembled directly from upstream software releases.

---

## System Philosophy

sverige is designed to provide full transparency into modern Linux assembly. By discarding monolithic pre-built distributions, binary package managers, and opaque build scripts, sverige puts the builder in direct control of every header, library, and toolchain component.

* **Upstream Purity:** Built using original source archives directly from GNU, Kernel.org, and Freedesktop.
* **Modern Defaults:** Utilizes a native `systemd` init and management stack, modern GCC/Glibc toolchains, and strict UEFI boot requirements.
* **Deterministic Toolchain Isolation:** Isolates build artifacts using clean sysroots and dedicated pass-based compilation phases.

---

## Documentation Tracks

The documentation is split into 3:

| :--- | :--- | :--- |
| **[sverige Base Guide](guide.md)** | Complete 32-step build guide covering toolchain isolation, Glibc, GNU userspace, `systemd`, kernel, and GRUB bootloader. | **Complete** |
| **Suomi (Extended)** | Extended userspace components: Display servers (Wayland/X11), graphics drivers (Mesa/NVIDIA), pipewire audio, desktop environments, and multimedia stacks. | *In Development* |
