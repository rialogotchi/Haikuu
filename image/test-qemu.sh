#!/bin/sh
# Duman testi: boot-test hedefiyle aç, selftest "HAIKU-TEST: PASS" yazmalı.
set -eu
D="${1:-$(cd "$(dirname "$0")" && pwd)}"
LOG="$D/qemu-boot.log"
ACCEL=tcg; [ -w /dev/kvm ] && ACCEL=kvm:tcg
timeout 300 qemu-system-x86_64 -machine "q35,accel=$ACCEL" -cpu max -m 256 \
    -nographic -no-reboot \
    -kernel "$D/vmlinuz-virt" -initrd "$D/haiku-initramfs.cpio.gz" \
    -append "console=ttyS0 rdinit=/sbin/haiku panic=-1 boot-test" \
    > "$LOG" 2>&1 < /dev/null || true
cat "$LOG"
grep -q 'HAIKU-TEST: PASS' "$LOG" && echo "==> TEST GECTI" || { echo "==> TEST BASARISIZ"; exit 1; }
