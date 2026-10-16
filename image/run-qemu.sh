#!/bin/sh
# Etkileşimli çalıştırma. Çıkış: kabukta `poweroff` veya Ctrl-A X
D="$(cd "$(dirname "$0")" && pwd)"
exec qemu-system-x86_64 -m 256 -nographic -no-reboot \
    -kernel "$D/vmlinuz-virt" -initrd "$D/haiku-initramfs.cpio.gz" \
    -append "console=ttyS0 rdinit=/sbin/haiku panic=-1"
