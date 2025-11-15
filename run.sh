SERIAL_PATH="/dev/null"
bash restore_firmware.sh
sudo qemu-system-x86_64 -drive if=pflash,format=raw,file=uefi-firmware/OVMF_CODE.fd -drive format=raw,file=drive.img,id=disk,if=none -m 8G -cpu host,+x2apic,+apic -enable-kvm -serial $SERIAL_PATH -smp cores=4 -machine q35 -device nvme,drive=disk,serial=67
