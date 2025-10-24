BOOTCC='sudo x86_64-w64-mingw32-gcc'
BOOTLD='lld-link'
CC='sudo x86_64-w64-mingw32-gcc'
LD='lld-link'
AS='nasm'
CFLAGS='-O0 -mabi=ms -ffreestanding -fno-stack-protector -fshort-wchar -Ikernel/include -Iuefi-headers/Include -Iuefi-headers/Include/X64'
OS=$(uname -s)
echo compiling bootloader
$BOOTCC $CFLAGS -fpic -c boot/bootloader.c -o build/objects/bootloader.o
echo linking bootloader
sudo $BOOTLD -subsystem:efi_application -entry:UefiEntry build/objects/bootloader.o -out:build/build/bootloader.efi 
echo compiling kernel
sudo $CC $CFLAGS -fpic -c kernel/graphics.c -o build/objects/graphics.o
sudo $CC $CFLAGS -fpic -c kernel/kernel.c -o build/objects/kernel.o
sudo $CC $CFLAGS -fpic -c kernel/interrupt.c -o build/objects/interrupt.o
sudo $CC $CFLAGS -fpic -c kernel/gdt.c -o build/objects/gdt.o
sudo $CC $CFLAGS -fpic -c kernel/port.c -o build/objects/port.o
sudo $CC $CFLAGS -fpic -c kernel/filesystem.c -o build/objects/filesystem.o
sudo $CC $CFLAGS -fpic -c kernel/stdlib/stdlib.c -o build/objects/stdlib.o
sudo $CC $CFLAGS -fpic -c kernel/apic.c -o build/objects/apic.o
sudo $CC $CFLAGS -fpic -c kernel/cpuid.c -o build/objects/cpuid.o
sudo $CC $CFLAGS -fpic -c kernel/pit.c -o build/objects/pit.o
sudo $CC $CFLAGS -fpic -c kernel/pic.c -o build/objects/pic.o
sudo $AS -f win64 kernel/stub.asm -o build/objects/kernel_stub.o
sudo $AS -f win64 kernel/isrs.asm -o build/objects/isrs.o
sudo $AS -f win64 kernel/gdt.asm -o build/objects/gdt_asm.o
sudo $AS -f win64 kernel/idt.asm -o build/objects/idt_asm.o
sudo $AS -f win64 kernel/msr.asm -o build/objects/msr.o
sudo $AS -f win64 kernel/timer.asm -o build/objects/timer.o
echo linking kernel
sudo $LD -subsystem:native build/objects/kernel.o build/objects/graphics.o build/objects/kernel_stub.o build/objects/interrupt.o build/objects/isrs.o build/objects/gdt_asm.o build/objects/gdt.o build/objects/idt_asm.o build/objects/port.o build/objects/filesystem.o build/objects/stdlib.o build/objects/msr.o build/objects/apic.o build/objects/cpuid.o build/objects/pit.o build/objects/pic.o build/objects/timer.o -entry:kernel_stub -out:build/build/kernel.exe
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
sudo cp build/build/bootloader.efi efimnt/EFI/BOOT/BOOTX64.EFI
sudo mkdir efimnt/KERNEL
sudo cp build/build/kernel.exe efimnt/KERNEL/kernel.exe
sudo cp -r fonts efimnt/FONTS
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
sudo cp build/build/bootloader.efi drivemnt/EFI/BOOT/BOOTX64.EFI
sudo cp build/build/kernel.exe drivemnt/KERNEL/kernel.exe
sudo cp -r fonts drivemnt/FONTS
sudo umount drivemnt
sudo hdiutil detach "$DEV"
;;
*)
esac
