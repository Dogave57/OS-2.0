BOOTCC='sudo x86_64-w64-mingw32-gcc'
BOOTLD='lld-link'
CC='x86_64-elf-gcc'
LD='x86_64-elf-ld'
CFLAGS='-O0 -ffreestanding -fno-stack-protector -fshort-wchar -Iuefi_headers/Include -Iuefi_headers/Include/X64'
OS=$(uname -s)
echo compiling bootloader
$BOOTCC $CFLAGS -fpic -c bootloader.c -o bootloader.o
echo linking bootloader
sudo $BOOTLD -subsystem:efi_application -entry:UefiEntry bootloader.o -out:bootloader.efi 
echo compiling kernel
sudo $CC $CFLAGS -fpic -c graphics.c -o graphics.o
sudo $CC $CFLAGS -fpic -c kernel.c -o kernel.o
echo linking kernel
sudo $LD -pie -nostdlib -entry kmain kernel.o graphics.o -o kernel.elf
echo done
case "$OS" in
"Linux")
sudo dd if=/dev/zero of=drive.img bs=1M count=64
sudo parted drive.img --script -- mklabel gpt
sudo parted drive.img --script -- mkpart ESP fat32 1MiB 48MiB
sudo parted drive.img --script -- mkpart primary 48MiB 100%
sudo parted drive.img set 1 boot on
sudo losetup -Pf drive.img
sudo mkfs.fat -F32 /dev/loop0p1
sudo mkdir efimnt
sudo mount /dev/loop0p1 efimnt
sudo mkdir -p efimnt/EFI/BOOT
sudo cp bootloader.efi efimnt/EFI/BOOT/BOOTX64.EFI
sudo mkdir efimnt/KERNEL
sudo cp kernel.exe efimnt/KERNEL/kernel.exe
sudo losetup -d /dev/loop0
sudo umount -r efimnt
sudo rm -rf efimnt
;;
"Darwin")
sudo rm drive.img
sudo rm -rf drivemnt
sudo dd if=/dev/zero of=drive.img bs=1M count=64
sudo chmod 777 drive.img
sudo mkdir drivemnt
echo attaching disk
DEV=$(hdiutil attach -nomount drive.img | awk '{print $1}')
echo partitioning disk
sudo diskutil partitionDisk "$DEV" GPT FAT32 EFI 48M
sudo diskutil unmount ${DEV}s1
sudo mount -t msdos ${DEV}s1 drivemnt
sudo mkdir -p drivemnt/EFI/BOOT
sudo mkdir -p drivemnt/KERNEL
sudo cp bootloader.efi drivemnt/EFI/BOOT/BOOTX64.EFI
sudo cp kernel.elf drivemnt/KERNEL/kernel.elf
sudo umount drivemnt
sudo hdiutil detach "$DEV"
;;
*)
esac
