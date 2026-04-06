SERIAL_PATH="/dev/null"
MACOS_FLAGS="-cpu max,+x2apic,+apic -device qemu-xhci,id=xhci -device usb-mouse,bus=xhci.0 -device usb-kbd,bus=xhci.0 -display cocoa,gl=es,zoom-to-fit=on,full-screen=on -device virtio-gpu-gl -trace "virtio_*""
LINUX_FLAGS="-enable-kvm -device qemu-xhci,id=xhci -device usb-mouse,bus=xhci.0 -device usb-kbd,bus=xhci.0 -device virtio-gpu-gl -display sdl,gl=es -vga none -trace "virtio_*""
OS=$(uname -s)
bash restore_firmware.sh
case "$OS" in
"Linux")
sudo qemu-system-x86_64 $LINUX_FLAGS -m 4G -drive if=pflash,format=raw,file=uefi-firmware/OVMF_CODE.fd -drive file=drive.img,if=none,id=nvme_drive,format=raw -device nvme,drive=nvme_drive,serial=serial -serial $SERIAL_PATH -smp cores=4 -machine q35
;;
"Darwin")
sudo qemu-system-x86_64 $MACOS_FLAGS -m 4G -drive if=pflash,format=raw,file=uefi-firmware/OVMF_CODE.fd -drive file=drive.img,if=none,id=nvme_drive,format=raw -device nvme,id=nvme_device,drive=nvme_drive,serial=deadbeef -smp cores=4 -machine q35
;;
*)
esac
