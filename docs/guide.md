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
