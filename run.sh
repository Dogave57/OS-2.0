SERIAL_PATH="/dev/null"
MACOS_FLAGS="-cpu max,+x2apic,+apic -serial stdio -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-mouse,bus=xhci.0"
LINUX_FLAGS="-cpu max,+x2apic,+apic -enable-kvm -serial stdio -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-mouse,bus=xhci.0"
OS=$(uname -s)
bash restore_firmware.sh
case "$OS" in
"Linux")
sudo qemu-system-x86_64 $LINUX_FLAGS -m 4G -drive if=pflash,format=raw,file=uefi-firmware/OVMF_CODE.fd -drive format=raw,file=drive.img,id=disk,if=none,cache=unsafe -m 4G -serial $SERIAL_PATH -smp cores=4 -machine q35 -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0
;;
"Darwin")
sudo qemu-system-x86_64 $MACOS_FLAGS -m 4G -drive if=pflash,format=raw,file=uefi-firmware/OVMF_CODE.fd -drive format=raw,file=drive.img,id=disk,if=none -m 4G -serial $SERIAL_PATH -smp cores=4 -machine q35 -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0
;;
*)
esac
