BOOTCC='sudo x86_64-w64-mingw32-gcc'
BOOTLD='lld-link'
CC='sudo x86_64-w64-mingw32-gcc'
LD='lld-link'
AS='nasm'
CFLAGS='-O0 -mabi=ms -ffreestanding -fno-stack-protector -fshort-wchar -Wno-multichar -Wno-address-of-packed-member -Ikernel/include -Iuefi-headers/Include -Iuefi-headers/Include/X64'
OS=$(uname -s)
bash clean.sh
echo compiling bootloader
$BOOTCC $CFLAGS -fpic -c boot/bootloader.c -o build/objects/bootloader.o
echo linking bootloader
sudo $BOOTLD -subsystem:efi_application -entry:UefiEntry build/objects/bootloader.o -out:build/build/bootloader.efi 
echo compiling kernel
sudo mkdir -p build/objects/mem
sudo mkdir -p build/objects/drivers
sudo mkdir -p build/objects/cpu
sudo mkdir -p build/objects/stdlib
sudo mkdir -p build/objects/subsystem
sudo mkdir -p build/objects/crypto
sudo mkdir -p build/objects/drivers/filesystem
sudo $CC $CFLAGS -fpic -c kernel/drivers/graphics.c -o build/objects/drivers/graphics.o
sudo $CC $CFLAGS -fpic -c kernel/kernel.c -o build/objects/kernel.o
sudo $CC $CFLAGS -fpic -c kernel/cpu/interrupt.c -o build/objects/cpu/interrupt.o
sudo $CC $CFLAGS -fpic -c kernel/cpu/gdt.c -o build/objects/cpu/gdt.o
sudo $CC $CFLAGS -fpic -c kernel/cpu/port.c -o build/objects/cpu/port.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/filesystem.c -o build/objects/drivers/filesystem.o
sudo $CC $CFLAGS -fpic -c kernel/stdlib/stdlib.c -o build/objects/stdlib/stdlib.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/apic.c -o build/objects/drivers/apic.o
sudo $CC $CFLAGS -fpic -c kernel/cpu/cpuid.c -o build/objects/cpu/cpuid.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/pit.c -o build/objects/drivers/pit.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/pic.c -o build/objects/drivers/pic.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/acpi.c -o build/objects/drivers/acpi.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/keyboard.c -o build/objects/drivers/keyboard.o
sudo $CC $CFLAGS -fpic -c kernel/mem/pmm.c -o build/objects/mem/pmm.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/serial.c -o build/objects/drivers/serial.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/smbios.c -o build/objects/drivers/smbios.o
sudo $CC $CFLAGS -fpic -c kernel/mem/vmm.c -o build/objects/mem/vmm.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/smp.c -o build/objects/drivers/smp.o
sudo $CC $CFLAGS -fpic -c kernel/mem/heap.c -o build/objects/mem/heap.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/ahci.c -o build/objects/drivers/ahci.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/pcie.c -o build/objects/drivers/pcie.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/nvme.c -o build/objects/drivers/nvme.o
sudo $CC $CFLAGS -fpic -c kernel/subsystem/subsystem.c -o build/objects/subsystem/subsystem.o
sudo $CC $CFLAGS -fpic -c kernel/subsystem/drive.c -o build/objects/subsystem/drive.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/gpt.c -o build/objects/drivers/gpt.o
sudo $CC $CFLAGS -fpic -c kernel/crypto/crc.c -o build/objects/crypto/crc.o
sudo $CC $CFLAGS -fpic -c kernel/drivers/filesystem/fat32.c -o build/objects/drivers/filesystem/fat32.o
sudo $CC $CFLAGS -fpic -c kernel/crypto/guid.c -o build/objects/crypto/guid.o
sudo $CC $CFLAGS -fpic -c kernel/crypto/random.c -o build/objects/crypto/random.o
sudo $AS -f win64 kernel/stub.asm -o build/objects/kernel_stub.o
sudo $AS -f win64 kernel/cpu/isrs.asm -o build/objects/cpu/isrs.o
sudo $AS -f win64 kernel/cpu/gdt.asm -o build/objects/cpu/gdt_asm.o
sudo $AS -f win64 kernel/cpu/idt.asm -o build/objects/cpu/idt_asm.o
sudo $AS -f win64 kernel/cpu/msr.asm -o build/objects/cpu/msr.o
sudo $AS -f win64 kernel/drivers/timer.asm -o build/objects/drivers/timer.o
sudo $AS -f win64 kernel/drivers/thermal.asm -o build/objects/drivers/thermal.o
sudo $AS -f win64 kernel/mem/vmm.asm -o build/objects/mem/vmm_asm.o
echo linking kernel
sudo $LD -subsystem:native build/objects/kernel.o build/objects/drivers/graphics.o build/objects/kernel_stub.o build/objects/cpu/interrupt.o build/objects/cpu/isrs.o build/objects/cpu/gdt_asm.o build/objects/cpu/gdt.o build/objects/cpu/idt_asm.o build/objects/cpu/port.o build/objects/drivers/filesystem.o build/objects/stdlib/stdlib.o build/objects/cpu/msr.o build/objects/drivers/apic.o build/objects/cpu/cpuid.o build/objects/drivers/pit.o build/objects/drivers/pic.o build/objects/drivers/timer.o build/objects/drivers/thermal.o build/objects/drivers/acpi.o build/objects/drivers/keyboard.o build/objects/mem/pmm.o build/objects/mem/vmm_asm.o build/objects/drivers/serial.o build/objects/drivers/smbios.o build/objects/mem/vmm.o build/objects/drivers/smp.o build/objects/mem/heap.o build/objects/drivers/ahci.o build/objects/drivers/pcie.o build/objects/drivers/nvme.o build/objects/subsystem/subsystem.o build/objects/subsystem/drive.o build/objects/drivers/gpt.o build/objects/crypto/crc.o build/objects/drivers/filesystem/fat32.o build/objects/crypto/guid.o build/objects/crypto/random.o -entry:kernel_stub -out:build/build/kernel.exe
echo done
case "$OS" in
"Linux")
sudo dd if=/dev/zero of=drive.img bs=1M count=128
sudo parted drive.img --script -- mklabel gpt
sudo parted drive.img --script -- mkpart ESP fat32 1MiB 64MiB
sudo parted drive.img --script -- mkpart primary 64MiB 100%
sudo parted drive.img set 1 boot on
sudo losetup -Pf drive.img
sudo mkfs.fat -F32 /dev/loop0p1
sudo mkdir efimnt
sudo mount /dev/loop0p1 efimnt
sudo mkdir -p efimnt/EFI/BOOT
sudo cp build/build/bootloader.efi efimnt/EFI/BOOT/BOOTX64.EFI
sudo mkdir efimnt/KERNEL
sudo cp build/build/kernel.exe efimnt/KERNEL/kernel.exe
sudo losetup -d /dev/loop0
sudo umount -r efimnt
sudo rm -rf efimnt
;;
"Darwin")
sudo rm drive.img
sudo rm -rf drivemnt
sudo dd if=/dev/urandom of=drive.img bs=1M count=512
sudo chmod 777 drive.img
sudo mkdir drivemnt
echo attaching disk
DEV=$(hdiutil attach -nomount drive.img | awk '{print $1}')
echo partitioning disk
sudo diskutil partitionDisk "$DEV" GPT FAT32 "ESP" 64MB
sudo diskutil unmount ${DEV}s1
sudo newfs_msdos -F 32 -c 8 -v EFI ${DEV}s1
sudo mount -t msdos ${DEV}s1 drivemnt
sudo mkdir -p drivemnt/EFI/BOOT
sudo mkdir -p drivemnt/KERNEL
sudo cp build/build/bootloader.efi drivemnt/EFI/BOOT/BOOTX64.EFI
sudo cp build/build/kernel.exe drivemnt/KERNEL/kernel.exe
sudo cp test.txt drivemnt/test.txt
sudo cp -r fonts drivemnt/FONTS
sudo umount drivemnt
sudo hdiutil detach "$DEV"
;;
*)
esac
