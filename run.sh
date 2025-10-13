sudo qemu-system-x86_64 -drive if=pflash,format=raw,file=OVMF_CODE.fd -drive format=raw,file=drive.img -m 256M
