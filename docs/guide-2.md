# Sverige Linux

This installation builds a minimal x86_64 sverige Linux system directly from upstream source.

The examples use "/dev/sda" as the installation disk. Substitute the actual disk where necessary.
# 1. Build environment

Use the current x86_64 SystemRescue image.

Create the target:

```bash
mkdir -p /mnt/sverige
```


# 2. Install Saada

Get Saada from the Sverige repository before starting the installation. The installer script will be provided with Saada:

```bash
git clone https://github.com/ollipix/sverige
cd sverige
cd saada
chmod +x install.sh
./install.sh
```

After installation, use Saada to obtain the upstream source archives used throughout this guide.


## 3. Partition the target

Create a damn GPT partition table on /dev/sda
(if your disk isn't /dev/sda use lsblk to find it out, if you needed this, this OS probably isn't for you) now we continue.

The default layout is:

/dev/sda1    EFI System Partition    512 MiB
/dev/sda2    Linux filesystem        remaining space

Optional swap may be added separately. (you hear that? OPTIONAL!)

Format the filesystems:

```bash
mkfs.fat -F 32 /dev/sda1
mkfs.ext4 /dev/sda2
```

If swap was created:

```bash
mkswap /dev/sda3
swapon /dev/sda3
```

Mount the target:

```bash
mount /dev/sda2 /mnt/sverige
mkdir -p /mnt/sverige/boot
mount /dev/sda1 /mnt/sverige/boot
```

Create the source tree:

```bash
mkdir -p /mnt/sverige/sources
```

## 4. Source set

Use the current stable upstream releases available when the installation is made (listen, I'd give you links but you may be doing this in the future so the links might not work, so just do it yourself, hope you don't mind)

The source set consists of:

Linux API headers, GNU Binutils, GCC, Glibc, GMP, MPFR, MPC, GNU Make, GNU Patch, GNU M4, GNU Bison, GNU Flex, Bc, Perl, Python, Zlib, Bzip2, XZ, GNU Tar, GNU Gzip, GNU Bash, GNU Sed, GNU Gawk, GNU Grep, GNU Diffutils, GNU Findutils, File, GNU Coreutils, Util-linux, Readline, Ncurses, Procps-ng, Psmisc, Kmod, E2fsprogs, Attr, Acl, Libcap, Shadow, Linux-PAM, IPRoute2, IPUtils, D-Bus, systemd, Linux-firmware, Linux kernel, GRUB.

Download each required source archive by Saada. Saada obtains the archives from their upstream projects. (yes I'm giving you links for some of them, be thankful ok?)

The upstream locations used by Saada include:

https://www.kernel.org/
https://ftp.gnu.org/gnu/
https://sourceware.org/
https://systemd.io/
https://www.freedesktop.org/
https://www.kernel.org/pub/linux/utils/
https://www.kernel.org/pub/linux/utils/boot/

Keep the original archives in:

/mnt/sverige/sources

Verify upstream signatures or checksums before extraction (trust me you do not want anything corrupted here)

The installation does not depend on a sverige source mirror, bin repo or sverige stages

## 5. Bootstrap variables

Set:

```bash
export SVERIGE=/mnt/sverige
export PATH=$SVERIGE/tools/bin:$PATH
```

Create the temporary toolchain:

```bash
mkdir -p "$SVERIGE/tools"
```

The temporary build sequence is:

```bash
Binutils
-> GCC pass 1
-> Linux API headers
-> Glibc
-> GCC/libgcc
```

The compiler supplied by SystemRescue is only the initial bootstrap compiler.
## 6. Extracting sources

Source archives are extracted into "/mnt/sverige/sources".

For a normal tar archive:

```bash
tar -xf /sources/<package>.tar.xz
```

For other compression formats, use the appropriate tar option or decompression utility.

Do not compile every package immediately after extraction. Build them in dependency order. (trust me on this one)

After a package is installed, its extracted source directory can be removed if it is no longer required. (which it probably isn't unless)

The original archive SHOULD be kept until the installation has been done.

## 7. Temporary Binutils

Download Binutils by Saada.

Extract Binutils and create an out-of-tree build directory.

Configure it for the target architecture:

```bash
../configure \
    --prefix="$SVERIGE/tools" \
    --with-sysroot="$SVERIGE" \
    --target=x86_64-sverige-linux-gnu \
    --disable-nls \
    --disable-werror
```

Build and install:

```bash
make -j"$(nproc)"
make install
```

## 8. GCC pass 1

Download GCC by Saada.

Extract GCC.

Populate its bundled prerequisites if required by the selected GCC release.

Configure the bootstrap compiler:

```bash
../configure \
    --target=x86_64-sverige-linux-gnu \
    --prefix="$SVERIGE/tools" \
    --with-newlib \
    --without-headers \
    --enable-languages=c,c++ \
    --disable-nls \
    --disable-shared \
    --disable-multilib \
    --disable-threads \
    --disable-libatomic \
    --disable-libgomp \
    --disable-libquadmath \
    --disable-libssp \
    --disable-libvtv \
    --disable-libstdcxx-pch
```

Build the compiler and target libgcc:

```bash
make -j"$(nproc)" all-gcc
make -j"$(nproc)" all-target-libgcc
```

Install:

```bash
make install-gcc
make install-target-libgcc
```

This compiler is only the temporary compiler.
(This is probably a good time to get up and take a break before coming back in a bit)

## 9. Linux API headers

Download Linux by Saada.

Extract the Linux source.

Clean the source tree:

```bash
make mrproper
```

Install the exported userspace headers:

```bash
make headers
find usr/include -type f ! -name '*.install' -delete
```

Copy them into the target:

```bash
mkdir -p "$SVERIGE/usr/include"
cp -a usr/include/. "$SVERIGE/usr/include/"
```

These headers define the Linux userspace API used when building Glibc.

## 10. Glibc

Download Glibc by Saada.

Extract Glibc into a separate build directory.

Configure against the target headers and bootstrap compiler:

```bash
../configure \
    --prefix=/usr \
    --host=x86_64-sverige-linux-gnu \
    --build="$(../scripts/config.guess)" \
    --with-headers="$SVERIGE/usr/include" \
    --enable-kernel=<minimum-supported-kernel> \
    libc_cv_slibdir=/usr/lib
```

Build:

```bash
make -j"$(nproc)"
```

Install into the target filesystem:

```bash
make DESTDIR="$SVERIGE" install
```

"DESTDIR" is build-system staging. It is not a configuration the eventual user (you) needs to understand or maintain.

## 11. Target GCC

Build the target GCC against the newly installed Glibc.

Use the configuration required by the selected GCC release with:

target -> x86_64-sverige-linux-gnu
prefix -> /usr
languages -> c,c++
multilib -> disabled

Build:

```bash
make -j"$(nproc)"
```

Install:

```bash
make install
```

Install the target libgcc and libstdc++ components required by the selected GCC release.

The target now contains its own compiler and libc.

## 12. Enter the target

Create the target filesystem hierarchy:

```bash
mkdir -p \
    "$SVERIGE"/{boot,dev,etc,home,mnt,opt,proc,root,run,srv,sys,tmp,var} \
    "$SVERIGE"/usr/{bin,lib,sbin,src}
chmod 1777 "$SVERIGE/tmp"
```

Expose the required kernel interfaces:

```bash
mount --bind /dev "$SVERIGE/dev"
mount --bind /dev/pts "$SVERIGE/dev/pts"
mount -t proc proc "$SVERIGE/proc"
mount -t sysfs sysfs "$SVERIGE/sys"
mount -t tmpfs tmpfs "$SVERIGE/run"
```

Provide resolver configuration for the chroot.

Enter:

```bash
chroot "$SVERIGE" /usr/bin/env -i \
    HOME=/root \
    TERM="$TERM" \
    PATH=/usr/bin:/usr/sbin:/bin:/sbin \
    /bin/bash --login
```

Set:

```bash
export HOME=/root
export LC_ALL=C
export PATH=/usr/bin:/usr/sbin:/bin:/sbin
```

## 13. Base libraries

Download Zlib, Bzip2, XZ, File, Readline, and Ncurses by Saada.

Build and install:

Zlib,Bzip2,XZ,File,Readline,Ncurses

Use each project's upstream build system.

For conventional Autotools projects:

```bash
./configure --prefix=/usr
make -j"$(nproc)"
make install
```

Use the project's native build procedure when it does not use Autotools.

## 14. Build tools (THIS IS PRETTY IMPORTANT, DON'T MISS IT)

Download M4, Bc, Flex, Bison, Make, and Patch by Saada.

Build:

M4,Bc,Flex,Bison,Make,Patch

These provide the tools required by the remaining source builds.

## 15. GNU userspace

Download Coreutils, Diffutils, Findutils, Gawk, Grep, Sed, Tar, Gzip, Texinfo, and Bash by Saada.

Build:

Coreutils,Diffutils,Findutils,Gawk,Grep,Sed,Tar,Gzip,Texinfo,Bash

Install them into "/usr".

Ensure Bash is available as:

/bin/bash 

## 16. Compiler rebuild

Rebuild GCC against the completed target userspace.

Download GCC, libgcc, and libstdc++ by Saada.

Build and install:

GCC,libgcc,libstdc++

Then rebuild Binutils.

The compiler and linker must now use the target Glibc and libraries.

Verify:

```bash
gcc --version
g++ --version
ld --version
```

Compile and execute a trivial C program.

The resulting executable must run without depending on SystemRescue libraries.

## 17. System utilities

Download Util-linux, E2fsprogs, Kmod, Procps-ng, Psmisc, IPRoute2, IPUtils, Libcap, Attr, and Acl by Saada.

Build and install:

Util-linux,E2fsprogs,Kmod,Procps-ng,Psmisc,IPRoute2,IPUtils,Libcap,Attr,Acl

These provide filesystem management, process utilities, kernel module handling and basic networking. (so if you don't set them up correctly you're fucked, so focus and install them correctly)

## 18. Shadow

Download Shadow by Saada.

Build Shadow from upstream source.

Install:

useradd,groupadd,passwd,su,login

Set the root password by using passwd

## 19. D-Bus

Download D-Bus by Saada.

Build D-Bus from upstream source.

Install the daemon, libraries and system configuration required by systemd.

Create the D-Bus system user and group required by the selected release.

The system bus will be started by systemd.

## 20. Linux-PAM

Download Linux-PAM by Saada.

Build Linux-PAM from upstream source.

Install:

PAM libraries
PAM modules
PAM configuration

Create the PAM configuration required by Shadow and systemd.

Do not copy PAM configuration from the live environment. (this is ALSO a pretty good time to stand up and take a break)

## 21. systemd

Download systemd by Saada.

Build systemd from its upstream source. Use its native Meson build system. (if you don't want to use systemd, skip to step 22, but you'll have to do it yourself, Sverige has no guide for other init systems, so you'll have to do it yourself)

The base build should include:

```bash
systemd,udev,journald,logind,tmpfiles,sysusers,networkd,resolved
```

Configure:

```bash
meson setup build \
    --prefix=/usr \
    --buildtype=release \
    -Dtests=false \
    -Dman=false
```

Build:

```bash
ninja -C build
```

Install:

```bash
ninja -C build install
```

Ensure:

/sbin/init

resolves to the installed systemd init.

## 22. systemd configuration

Enable the services required for a functional base system:

systemd-udevd,systemd-journald,systemd-logind,systemd-tmpfiles,systemd-sysusers,systemd-networkd,systemd-resolved

Configure networking under:

/etc/systemd/network/

A basic DHCP configuration can be:

```bash
[Match]
Name=en*

[Network]
DHCP=yes
```

Wireless networking requires the appropriate wireless userspace and configuration

## 23. DNS

Configure systemd-resolved.

The installed "/etc/resolv.conf" should reference the resolver provided by the installed system rather than SystemRescue

## 24. Machine configuration

The installer chooses machine-specific configuration.

Set:

/etc/hostname

to the desired hostname.

Configure:

/etc/hosts

Configure the desired locale.

Configure the desired timezone.

For example:

```bash
ln -sf /usr/share/zoneinfo/Europe/Stockholm /etc/localtime
```

## 25. Normal user

Create the normal account:

```bash
useradd -m -s /bin/bash sverige
```

```bash
passwd sverige
```

Create the administrative group:

```bash
groupadd wheel
usermod -aG wheel sverige
```

Add hardware-related groups only where appropriate.

Set ownership:

```bash
chown -R sverige:sverige /home/sverige
```

Install "sudo" or "doas" if administrative access from the normal account is desired.

If sudo is used, grant the "wheel" group access through visudo

## 26. Naming
pretty obvious, just edit /etc/os-release
to whatever you like, I'd recommend making it Sverige for when the logo is added to fastfetch

## 27. Linux kernel (hey there, if you're here, I just want to say you're doing amazing right now, this is also a good time to get up for a bit before continuing, thanks for using Sverige) now, where were we.

Download Linux by Saada.

Extract the Linux source under "/usr/src":

```bash
cd /usr/src
tar -xf /sources/linux-<version>.tar.xz
ln -s linux-<version> linux
cd linux
```

Clean:

```bash
make mrproper
```

Configure:

```bash
make menuconfig
```

The configuration must support the target hardware and boot path, including:

UEFI,EFI framebuffer,GPT,PCI,USB,input devices,storage controller,root FS,network hardware,wireless hardware where required,module loading,firmware loading

Anything required before userspace starts must be built into the kernel or made available through the initramfs.

Build:

```bash
make -j"$(nproc)"
```

Install modules:

```bash
make modules_install
```

Install the kernel:

```bash
cp arch/x86/boot/bzImage /boot/vmlinuz-<version>
```

Install its configuration:

```bash
cp .config /boot/config-<version>
```

Install System.map:

```bash
cp System.map /boot/System.map-<version>
```

Generate an initramfs if the selected kernel configuration requires one.

## 28. Linux firmware

Download Linux-firmware by Saada.

Install Linux-firmware from the archive obtained by Saada.

Install the firmware under:

/lib/firmware

Keep the firmware required by the target hardware.

The kernel and firmware are both part of the installed system.

## 29. GRUB

Download GRUB by Saada.

Extract the current stable GRUB source.

Configure it for UEFI:

```bash
./configure \
    --prefix=/usr \
    --sysconfdir=/etc \
    --disable-werror \
    --with-platform=efi \
    --target=x86_64
```

Build:

```bash
make -j"$(nproc)"
```

Install:

```bash
make install
```

Install GRUB to the EFI System Partition mounted at "/boot".

Generate the configuration:

```bash
grub-mkconfig -o /boot/grub/grub.cfg
```

Verify that the generated configuration references the installed kernel and correct root filesystem.

## 30. fstab (you're almost there, KEEP GOING)

Obtain the filesystem UUIDs:

```bash
blkid
```

Configure:

/etc/fstab

with the installed UUIDs.

The resulting structure is:

```bash
UUID=<root-uuid>    /       ext4    defaults    0 1
UUID=<efi-uuid>     /boot   vfat    defaults    0 2
```

Add swap if one was created.

Use UUIDs rather than assuming "/dev/sda2" will always identify the root filesystem.

## 31. Final self-hosting pass

The target now contains:

GCC,Binutils,Glibc,GNU userspace,systemd,D-Bus,PAM,Shadow,Linux,Linux-firmware,GRUB

Download Glibc, Binutils, GCC, Coreutils, Bash, Make, Util-linux, and Kmod by Saada.

Rebuild the critical components against the completed target:

Glibc,Binutils,GCC,Coreutils,Bash,Make,Util-linux,Kmod

The compiler used for this pass must be the compiler installed inside sverige. (and I mean MUST)

Compile and execute a test program.

The final target must not require:

/mnt/sverige/tools

or libraries supplied by SystemRescue.

Once this has been verified, the temporary "/tools" tree can be removed.

## 32. Final verification

Verify:

```bash
which gcc
which ld
which bash
which systemd
```

Verify:

```bash
ls -l /sbin/init
```

Verify:

```bash
gcc --version
ld --version
```

Verify:

```bash
/etc/fstab
/etc/hostname
/etc/hosts
/etc/os-release
/etc/passwd
/etc/group
/etc/shadow
```

Verify:

```bash
/boot
/lib/modules
/lib/firmware
/home/sverige
/usr/bin
/usr/lib
/sbin/init
```

Compile and execute a C program.

Verify networking.

Verify that the normal user can log in.

Verify that root access works.

Verify that the bootloader references the installed kernel.

## 33. Leave the chroot

Exit:

```bash
exit
```

Unmount the virtual filesystems:

```bash
umount -R /mnt/sverige/dev
umount -R /mnt/sverige/proc
umount -R /mnt/sverige/sys
umount -R /mnt/sverige/run
```

Unmount "/boot":

```bash
umount /mnt/sverige/boot
```

Unmount the root filesystem:

```bash
umount /mnt/sverige
```

Disable temporary swap if necessary and reboot. (congrats, you have officially downloaded Sverige, you can configure more components like desktop environments yourself or wait for the second guide "Suomi" to release, hope you enjoyed installing Sverige as I enjoyed making it!)
