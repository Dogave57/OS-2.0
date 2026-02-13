BOOTCC='sudo x86_64-w64-mingw32-gcc'
BOOTLD='sudo lld-link'
KERNELCC='sudo x86_64-elf-gcc'
KERNELLD='sudo x86_64-elf-ld'
PROGRAMCC='sudo x86_64-elf-gcc'
PROGRAMLD='sudo x86_64-elf-ld'
AS='sudo nasm'
BOOT_CFLAGS='-O0 -ffreestanding -fno-stack-protector -fshort-wchar -Wno-address-of-packed-member -Ikernel/include -Iuefi-headers/Include -Iuefi-headers/Include/X64'
KERNEL_CFLAGS='-O0 -mabi=ms -ffreestanding -fno-stack-protector -fno-plt -Wno-multichar -fshort-wchar -Wno-address-of-packed-member -Ikernel/include -Iuefi-headers/Include -Iuefi-headers/Include/X64 -fmax-errors=1 -fno-plt'
KERNEL_LINKFLAGS='build/objects/drivers/elf.o build/objects/kernel.o build/objects/drivers/gpu/framebuffer.o build/objects/kernel_stub.o build/objects/cpu/interrupt.o build/objects/cpu/isrs.o build/objects/cpu/gdt_asm.o build/objects/cpu/gdt.o build/objects/cpu/idt_asm.o build/objects/cpu/port.o build/objects/drivers/filesystem.o build/objects/stdlib/stdlib.o build/objects/cpu/msr.o build/objects/drivers/apic.o build/objects/cpu/cpuid.o build/objects/drivers/pit.o build/objects/drivers/pic.o build/objects/drivers/timer.o build/objects/drivers/thermal.o build/objects/drivers/acpi.o build/objects/drivers/keyboard.o build/objects/mem/pmm.o build/objects/mem/vmm_asm.o build/objects/drivers/serial.o build/objects/drivers/smbios.o build/objects/mem/vmm.o build/objects/drivers/smp.o build/objects/mem/heap.o build/objects/drivers/ahci.o build/objects/drivers/pcie.o build/objects/drivers/nvme.o build/objects/subsystem/subsystem.o build/objects/subsystem/drive.o build/objects/drivers/gpt.o build/objects/crypto/crc.o build/objects/drivers/filesystem/fat32.o build/objects/crypto/guid.o build/objects/crypto/random.o build/objects/panic.o build/objects/drivers/filesystem/exfat.o build/objects/subsystem/filesystem.o build/objects/drivers/filesystem/fluxfs.o build/objects/kexts/loader.o build/objects/cpu/thread.o build/objects/cpu/thread_asm.o build/objects/drivers/hpet.o build/objects/drivers/gpu/virtio.o build/objects/cpu/mutex.o build/objects/drivers/usb/xhci.o build/objects/drivers/usb/usb-kbd.o build/objects/subsystem/usb.o build/objects/drivers/usb/usb-bot.o build/objects/subsystem/pcie.o'
PROGRAM_CFLAGS='-O0 -mabi=ms -ffreestanding -fno-plt -Ikernel/include -Iuefi-headers/Include -Iuefi-headers/Include/X64 -Wno-builtin-declaration-mismatch'
OS=$(uname -s)
bash clean.sh
echo compiling bootloader
$BOOTCC $BOOT_CFLAGS -fpic -c boot/bootloader.c -o build/objects/bootloader.o
echo linking bootloader
$BOOTLD -subsystem:efi_application -entry:UefiEntry build/objects/bootloader.o -out:build/build/bootloader.efi 
echo compiling kernel
sudo mkdir build/build/kexts
sudo mkdir build/objects/mem
sudo mkdir build/objects/drivers
sudo mkdir build/objects/cpu
sudo mkdir build/objects/stdlib
sudo mkdir build/objects/subsystem
sudo mkdir build/objects/crypto
sudo mkdir build/objects/drivers/filesystem
sudo mkdir build/objects/kexts
sudo mkdir build/objects/drivers/gpu
sudo mkdir build/objects/drivers/usb
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/gpu/framebuffer.c -o build/objects/drivers/gpu/framebuffer.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/kernel.c -o build/objects/kernel.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/cpu/interrupt.c -o build/objects/cpu/interrupt.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/cpu/gdt.c -o build/objects/cpu/gdt.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/cpu/port.c -o build/objects/cpu/port.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/filesystem.c -o build/objects/drivers/filesystem.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/stdlib/stdlib.c -o build/objects/stdlib/stdlib.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/apic.c -o build/objects/drivers/apic.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/cpu/cpuid.c -o build/objects/cpu/cpuid.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/pit.c -o build/objects/drivers/pit.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/pic.c -o build/objects/drivers/pic.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/acpi.c -o build/objects/drivers/acpi.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/keyboard.c -o build/objects/drivers/keyboard.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/mem/pmm.c -o build/objects/mem/pmm.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/serial.c -o build/objects/drivers/serial.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/smbios.c -o build/objects/drivers/smbios.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/mem/vmm.c -o build/objects/mem/vmm.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/smp.c -o build/objects/drivers/smp.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/mem/heap.c -o build/objects/mem/heap.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/ahci.c -o build/objects/drivers/ahci.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/pcie.c -o build/objects/drivers/pcie.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/nvme.c -o build/objects/drivers/nvme.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/subsystem/subsystem.c -o build/objects/subsystem/subsystem.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/subsystem/drive.c -o build/objects/subsystem/drive.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/gpt.c -o build/objects/drivers/gpt.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/crypto/crc.c -o build/objects/crypto/crc.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/filesystem/fat32.c -o build/objects/drivers/filesystem/fat32.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/crypto/guid.c -o build/objects/crypto/guid.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/crypto/random.c -o build/objects/crypto/random.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/panic.c -o build/objects/panic.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/filesystem/exfat.c -o build/objects/drivers/filesystem/exfat.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/subsystem/filesystem.c -o build/objects/subsystem/filesystem.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/filesystem/fluxfs.c -o build/objects/drivers/filesystem/fluxfs.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/kexts/loader.c -o build/objects/kexts/loader.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/elf.c -o build/objects/drivers/elf.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/cpu/thread.c -o build/objects/cpu/thread.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/hpet.c -o build/objects/drivers/hpet.o
$KERNELCC $KERNEL_CFLAGS -fPIC -c kernel/drivers/gpu/virtio.c -o build/objects/drivers/gpu/virtio.o
$KERNELCC $KERNEL_CFLAGS -fpic -c kernel/cpu/mutex.c -o build/objects/cpu/mutex.o
$KERNELCC $KERNEL_CFLAGS -fpic -c kernel/drivers/usb/xhci.c -o build/objects/drivers/usb/xhci.o
$KERNELCC $KERNEL_CFLAGS -fpic -c kernel/drivers/usb/usb-kbd.c -o build/objects/drivers/usb/usb-kbd.o
$KERNELCC $KERNEL_CFLAGS -fpic -c kernel/subsystem/usb.c -o build/objects/subsystem/usb.o
$KERNELCC $KERNEL_CFLAGS -fpic -c kernel/drivers/usb/usb-bot.c -o build/objects/drivers/usb/usb-bot.o
$KERNELCC $KERNEL_CFLAGS -fpic -c kernel/subsystem/pcie.c -o build/objects/subsystem/pcie.o
echo compiling kernel extensions
$PROGRAMCC -fPIC $PROGRAM_CFLAGS -c kernel/kexts/test.c -o build/objects/kexts/test.o
$AS -f elf64 kernel/stub.asm -o build/objects/kernel_stub.o
$AS -f elf64 kernel/cpu/isrs.asm -o build/objects/cpu/isrs.o
$AS -f elf64 kernel/cpu/gdt.asm -o build/objects/cpu/gdt_asm.o
$AS -f elf64 kernel/cpu/idt.asm -o build/objects/cpu/idt_asm.o
$AS -f elf64 kernel/cpu/msr.asm -o build/objects/cpu/msr.o
$AS -f elf64 kernel/drivers/timer.asm -o build/objects/drivers/timer.o
$AS -f elf64 kernel/drivers/thermal.asm -o build/objects/drivers/thermal.o
$AS -f elf64 kernel/mem/vmm.asm -o build/objects/mem/vmm_asm.o
$AS -f elf64 kernel/cpu/thread.asm -o build/objects/cpu/thread_asm.o
echo linking kernel
echo linking base kernel
$KERNELLD -shared -fPIC -Bsymbolic -hash-style=both -fno-plt -z now $KERNEL_LINKFLAGS -e kernel_stub -o build/build/kernel.elf
$PROGRAMLD -pic -z now -Lbuild/build -l:kernel.elf -e kext_entry build/objects/kexts/test.o -o build/build/kexts/test.elf
echo done
sudo dd if=/dev/urandom of=nvme_drive.img bs=1M count=256
case "$OS" in
"Linux")
sudo rm drive.img
sudo rm -rf drivemnt
sudo dd if=/dev/zero of=drive.img bs=1M count=1024
sudo parted drive.img --script -- mklabel gpt
sudo parted drive.img --script -- mkpart ESP fat32 1MiB 512MiB
sudo parted drive.img --script -- mkpart primary 512MiB 100%
sudo parted drive.img set 1 boot on
sudo losetup -Pf drive.img
sudo mkfs.fat -F 32 -s 8 /dev/loop0p1
sudo mkdir efimnt
sudo mount /dev/loop0p1 efimnt
sudo mkdir -p efimnt/EFI/BOOT
sudo cp build/build/bootloader.efi efimnt/EFI/BOOT/BOOTX64.EFI
sudo mkdir efimnt/KERNEL
sudo mkdir efimnt/CONFIG
sudo mkdir efimnt/KEXTS
sudo cp build/build/kernel.elf efimnt/KERNEL/KERNEL.ELF
sudo cp build/build/kexts/test.elf efimnt/KEXTS/TEST.ELF
sudo ls -R efimnt
sudo losetup -d /dev/loop0
sudo umount -r efimnt
sudo rm -rf efimnt
;;
"Darwin")
sudo rm drive.img
sudo dd if=/dev/zero of=drive.img bs=1M count=2048
sudo chmod 777 drive.img
sudo mkdir esp_mnt
sudo mkdir rootfs_mnt
echo attaching disk
DEV=$(hdiutil attach -nomount drive.img | awk '{print $1}')
echo partitioning disk
sudo sgdisk --zap-all ${DEV}
sudo sgdisk -n 1:2048:+512M -t 1:EF00 ${DEV}
sudo newfs_msdos -F 32 -c 8 -v EFI ${DEV}s1
sudo mount -t msdos ${DEV}s1 esp_mnt
sudo mkdir -p esp_mnt/EFI/BOOT
sudo mkdir -p esp_mnt/KERNEL
sudo mkdir -p esp_mnt/KEXTS/
sudo cp build/build/bootloader.efi esp_mnt/EFI/BOOT/BOOTX64.EFI
sudo touch esp_mnt/EFI/BOOT/test.txt
sudo cp build/build/kernel.elf esp_mnt/KERNEL/KERNEL.ELF
sudo cp build/build/kexts/test.elf esp_mnt/KEXTS/TEST.ELF
sudo mkdir -p esp_mnt/CONFIG
mkdir esp_mnt/files
sudo cp -r fonts esp_mnt/FONTS
sudo ls -R esp_mnt
sudo umount esp_mnt
sudo hdiutil detach "$DEV"
;;
*)
esac
