SERIAL_PATH="/dev/null"
bash restore_firmware.sh
sudo qemu-system-x86_64 -drive if=pflash,format=raw,file=uefi-firmware/OVMF_CODE.fd -drive format=raw,file=drive.img,id=disk,if=none -m 256M -cpu max,+x2apic,+apic -serial $SERIAL_PATH -smp cores=4 -machine q35 -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0
