SERIAL_PATH="/dev/null"
MACOS_FLAGS="-cpu max,+x2apic,+apic -device qemu-xhci,id=xhci -device usb-mouse,bus=xhci.0 -device usb-kbd,bus=xhci.0 -display cocoa,gl=es,zoom-to-fit=on,full-screen=on -serial stdio"
LINUX_FLAGS="-enable-kvm -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-bot,id=bot0 -device scsi-hd,bus=bot0.0,drive=usb_drive -drive file=drive.img,if=none,format=raw,id=usb_drive -serial stdio -device virtio-gpu -display gtk,zoom-to-fit=off -vga none"
OS=$(uname -s)
bash restore_firmware.sh
case "$OS" in
"Linux")
sudo qemu-system-x86_64 $LINUX_FLAGS -trace "usb_xhci_*" -m 4G -drive if=pflash,format=raw,file=uefi-firmware/OVMF_CODE.fd -drive file=nvme_drive.img,if=none,id=nvme_drive,format=raw -device nvme,drive=nvme_drive,serial=serial -serial $SERIAL_PATH -smp cores=4 -machine q35
;;
"Darwin")
sudo qemu-system-x86_64 $MACOS_FLAGS -m 4G -drive if=pflash,format=raw,file=uefi-firmware/OVMF_CODE.fd -drive file=drive.img,if=none,id=nvme_drive,format=raw -device nvme,id=nvme_device,drive=nvme_drive,serial=deadbeef -smp cores=4 -machine q35
;;
*)
esac
