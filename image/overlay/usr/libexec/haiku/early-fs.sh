#!/bin/sh
# Sanal dosya sistemlerini bağla. (/run'a dokunma: haikuctl soketi orada)
mount -t proc     proc     /proc
mount -t sysfs    sysfs    /sys
mount -t devtmpfs devtmpfs /dev || true
mkdir -p /dev/pts /dev/shm /tmp /var/log/haiku
mount -t devpts   devpts   /dev/pts || true
mount -t tmpfs    tmpfs    /tmp
hostname haiku
