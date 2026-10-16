#!/bin/sh
# Alpine 3.20.3 konteyneri içinde, root olarak çalışır.
# Çıktı: out/vmlinuz-virt, out/haiku-initramfs.cpio.gz, out/SHA256SUMS
set -eu

ALPINE_VERSION="${ALPINE_VERSION:-3.20.3}"
INCLUDE_MODULES="${INCLUDE_MODULES:-0}"
ARCH=x86_64
BRANCH="v${ALPINE_VERSION%.*}"
BASE="${MIRROR:-https://dl-cdn.alpinelinux.org/alpine}/$BRANCH/releases/$ARCH"
HERE="$(cd "$(dirname "$0")" && pwd)"
TOP="$(dirname "$HERE")"
WORK=/tmp/haiku-build
ROOTFS="$WORK/rootfs"
OUT="$TOP/out"
die() { echo "HATA: $*" >&2; exit 1; }

rm -rf "$WORK" "$OUT"; mkdir -p "$WORK" "$ROOTFS" "$OUT"

echo "==> Araçlar"
apk add --no-cache build-base linux-headers curl libarchive-tools squashfs-tools

echo "==> Resmi Alpine $ALPINE_VERSION dosyaları (minirootfs + virt ISO)"
cd "$WORK"
MINI="alpine-minirootfs-$ALPINE_VERSION-$ARCH.tar.gz"
ISO="alpine-virt-$ALPINE_VERSION-$ARCH.iso"
for f in "$MINI" "$ISO"; do
    curl -fsSL --retry 3 -o "$f"        "$BASE/$f"
    curl -fsSL --retry 3 -o "$f.sha256" "$BASE/$f.sha256"
done
sha256sum -c "$MINI.sha256" "$ISO.sha256"

echo "==> virt çekirdeği"
mkdir iso
bsdtar -xf "$ISO" -C iso boot/vmlinuz-virt boot/modloop-virt
cp iso/boot/vmlinuz-virt "$OUT/vmlinuz-virt"

echo "==> Haiku derleniyor (statik)"
cp -a "$TOP/haiku" "$WORK/haiku-src"
cd "$WORK/haiku-src"
./configure
make -C build all
make -C src -j"$(nproc)" LDFLAGS_EXTRA=-static

echo "==> rootfs"
tar -xzf "$WORK/$MINI" -C "$ROOTFS"
mkdir -p "$ROOTFS"/proc "$ROOTFS"/sys "$ROOTFS"/dev "$ROOTFS"/run "$ROOTFS"/tmp
chmod 1777 "$ROOTFS/tmp"

echo "==> OpenRC kökten kaldırılıyor"
if apk --root "$ROOTFS" info -e openrc >/dev/null 2>&1; then
    apk --root "$ROOTFS" del openrc || true
fi
rm -rf "$ROOTFS"/etc/init.d "$ROOTFS"/etc/conf.d "$ROOTFS"/etc/runlevels \
       "$ROOTFS"/etc/rc.conf "$ROOTFS"/etc/inittab "$ROOTFS"/lib/rc \
       "$ROOTFS"/sbin/openrc* "$ROOTFS"/sbin/rc "$ROOTFS"/sbin/rc-* \
       "$ROOTFS"/sbin/runscript "$ROOTFS"/sbin/supervise-daemon
sed -i '/^openrc/d' "$ROOTFS/etc/apk/world" 2>/dev/null || true
# Bir daha paket olarak da gelmesin (apk conflict kısıtı)
echo '!openrc' >> "$ROOTFS/etc/apk/world"

echo "==> busybox applet bağlantıları"
chroot "$ROOTFS" /bin/busybox --install -s || true
# busybox'ın init/kapatma bağlantılarını Haiku'ya bırak
rm -f "$ROOTFS"/sbin/init "$ROOTFS"/sbin/halt "$ROOTFS"/sbin/reboot \
      "$ROOTFS"/sbin/poweroff "$ROOTFS"/sbin/shutdown

echo "==> Haiku kuruluyor"
make -C "$WORK/haiku-src/src" install DESTDIR="$ROOTFS" STRIPOPTS=-s LDFLAGS_EXTRA=-static
ln -sf ../usr/bin/haiku "$ROOTFS/sbin/haiku"
ln -sf haiku "$ROOTFS/sbin/init"

echo "==> Servisler ve örnek uygulamalar"
cp -a "$HERE/overlay/." "$ROOTFS/"
echo "Alpine $ALPINE_VERSION (virt) + Haiku init, OpenRC yok" > "$ROOTFS/etc/haiku-release"

if [ "$INCLUDE_MODULES" = "1" ]; then
    echo "==> Çekirdek modülleri"
    unsquashfs -q -d "$WORK/modloop" "$WORK/iso/boot/modloop-virt"
    mkdir -p "$ROOTFS/lib/modules"
    cp -a "$WORK"/modloop/modules/. "$ROOTFS/lib/modules/"
fi

echo "==> OpenRC kalmadığı doğrulanıyor"
for p in sbin/openrc sbin/openrc-run sbin/rc-service sbin/rc-update etc/init.d etc/runlevels lib/rc; do
    [ ! -e "$ROOTFS/$p" ] || die "OpenRC kalıntısı: /$p"
done
[ "$(readlink "$ROOTFS/sbin/init")" = haiku ] || die "/sbin/init Haiku'ya gitmiyor"

echo "==> initramfs"
mknod -m 600 "$ROOTFS/dev/console" c 5 1
mknod -m 666 "$ROOTFS/dev/null"    c 1 3
( cd "$ROOTFS" && find . | cpio -o -H newc --quiet | gzip -9 > "$OUT/haiku-initramfs.cpio.gz" )

cp "$HERE/run-qemu.sh" "$HERE/test-qemu.sh" "$OUT/"
( cd "$OUT" && sha256sum vmlinuz-virt haiku-initramfs.cpio.gz > SHA256SUMS )
chmod -R a+rX "$OUT"
echo "==> Bitti:"; ls -lh "$OUT"
