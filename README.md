# Haiku + Alpine 3.20.3-virt mini imaj

- `haiku/`  : Dinit'ten türetilmiş init/servis yöneticisi (`haiku`, `haikuctl`, `haiku-check`, `haiku-monitor`).
  Servis dosyalarında ayarlar `@ayar = değer` biçiminde yazılabilir (`@include`/`@meta` hâlâ meta komut).
- `image/`  : imaj betikleri + servisler (`image/overlay/etc/haiku.d`).
- `.github/workflows/build-image.yml` : **Actions → Haiku Alpine mini image → Run workflow**.

Çıktı: `vmlinuz-virt` + `haiku-initramfs.cpio.gz` (resmi Alpine 3.20.3 minirootfs/virt ISO, sha256 doğrulamalı; OpenRC yok).
Yerelde: `sh out/run-qemu.sh`. Test hedefi: çekirdek satırına `boot-test` eklenir.
