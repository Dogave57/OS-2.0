SERIAL_PATH="/dev/null"
MACOS_FLAGS="-cpu max,+x2apic,+apic -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-bot,id=bot0 -device scsi-hd,bus=bot0.0,drive=usb_drive -drive file=drive.img,if=none,format=raw,id=usb_drive -device virtio-gpu"
LINUX_FLAGS="-cpu max,+x2apic,+apic -device qemu-xhci,id=xhci,msix=on -device usb-kbd,bus=xhci.0 -device usb-bot,id=bot0 -device scsi-hd,bus=bot0.0,drive=usb_drive -drive file=usb_drive.img,if=none,format=raw,id=usb_drive -serial stdio"
OS=$(uname -s)
bash restore_firmware.sh
case "$OS" in
"Linux")
sudo qemu-system-x86_64 $LINUX_FLAGS -m 4G -drive if=pflash,format=raw,file=uefi-firmware/OVMF_CODE.fd -drive format=raw,file=drive.img,id=disk,if=none -serial $SERIAL_PATH -smp cores=4 -machine q35 -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0
;;
"Darwin")
sudo qemu-system-x86_64 $MACOS_FLAGS -trace "pci_nvme_*" -m 4G -drive if=pflash,format=raw,file=uefi-firmware/OVMF_CODE.fd -drive file=nvme_drive.img,if=none,id=nvme_drive,format=raw -device nvme,drive=nvme_drive,serial=deadbeef -serial $SERIAL_PATH -smp cores=4 -machine q35
;;
*)
esac
