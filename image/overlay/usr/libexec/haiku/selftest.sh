#!/bin/sh
# Çok basit duman testi. Sonucu seri konsola "HAIKU-TEST: ..." olarak yazar.
say()  { echo "HAIKU-TEST: $*" > /dev/console; echo "HAIKU-TEST: $*"; }
fail() { say "FAIL: $1"; haikuctl list > /dev/console 2>&1; sleep 1; /sbin/poweroff; exit 1; }

say "begin"
sleep 2

[ "$(cat /proc/1/comm)" = "haiku" ] || fail "PID 1 haiku degil: $(cat /proc/1/comm)"

for s in early-fs network syslog heartbeat webdemo; do
    haikuctl is-started "$s" >/dev/null 2>&1 || fail "servis calismiyor: $s"
done

wget -qO- http://127.0.0.1:8080/ | grep -q "Haiku" || fail "httpd cevap vermedi"

for p in /sbin/openrc /sbin/openrc-run /sbin/rc-service /sbin/rc-update /etc/init.d /etc/runlevels /lib/rc; do
    [ ! -e "$p" ] || fail "OpenRC kalintisi var: $p"
done

haikuctl list > /dev/console 2>&1
say "PASS"
sleep 1
/sbin/poweroff
