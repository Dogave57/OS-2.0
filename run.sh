bash restore_firmware.sh
sudo qemu-system-x86_64 -drive if=pflash,format=raw,file=uefi-firmware/OVMF_CODE.fd -drive format=raw,file=drive.img -m 4G -cpu qemu64,+x2apic,+apic
