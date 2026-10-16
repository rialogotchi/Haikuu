include ../../mconfig
include ../../build/version.conf

all: generate

generate: haiku.8 haikuctl.8 haiku-check.8 haiku-monitor.8 haiku-service.5 shutdown.8

install: generate
	mkdir -p $(DESTDIR)$(MANDIR)/man8
	install -m644 haiku.8 haikuctl.8 haiku-check.8 haiku-monitor.8 $(DESTDIR)$(MANDIR)/man8
	mkdir -p $(DESTDIR)$(MANDIR)/man5
	install -m644 haiku-service.5 $(DESTDIR)$(MANDIR)/man5
ifeq ($(BUILD_SHUTDOWN),yes)
	install -m644 shutdown.8 $(DESTDIR)$(MANDIR)/man8/$(SHUTDOWN_PREFIX)shutdown.8
	ln -sf $(SHUTDOWN_PREFIX)shutdown.8 $(DESTDIR)$(MANDIR)/man8/$(SHUTDOWN_PREFIX)halt.8
	ln -sf $(SHUTDOWN_PREFIX)shutdown.8 $(DESTDIR)$(MANDIR)/man8/$(SHUTDOWN_PREFIX)reboot.8
	ln -sf $(SHUTDOWN_PREFIX)shutdown.8 $(DESTDIR)$(MANDIR)/man8/$(SHUTDOWN_PREFIX)poweroff.8
	ln -sf $(SHUTDOWN_PREFIX)shutdown.8 $(DESTDIR)$(MANDIR)/man8/$(SHUTDOWN_PREFIX)soft-reboot.8
endif

clean:
	rm -f haiku.8 haikuctl.8 haiku-check.8 haiku-monitor.8 haiku-service.5 shutdown.8

html: generate
	./generate-html.sh

haiku.8: haiku.8.m4
	m4 -DVERSION=$(VERSION) -DMONTH=$(MONTH) -DYEAR=$(YEAR) -DSHUTDOWN_PREFIX=$(SHUTDOWN_PREFIX)\
	   -DSUPPORT_CGROUPS=$(SUPPORT_CGROUPS)\
	   haiku.8.m4 > haiku.8

haikuctl.8: haikuctl.8.m4
	m4 -DVERSION=$(VERSION) -DMONTH=$(MONTH) -DYEAR=$(YEAR) -DSHUTDOWN_PREFIX=$(SHUTDOWN_PREFIX)\
	   haikuctl.8.m4 > haikuctl.8

haiku-check.8: haiku-check.8.m4
	m4 -DVERSION=$(VERSION) -DMONTH=$(MONTH) -DYEAR=$(YEAR) haiku-check.8.m4 > haiku-check.8

haiku-monitor.8: haiku-monitor.8.m4
	m4 -DVERSION=$(VERSION) -DMONTH=$(MONTH) -DYEAR=$(YEAR) haiku-monitor.8.m4 > haiku-monitor.8

haiku-service.5: haiku-service.5.m4
	m4 -DVERSION=$(VERSION) -DMONTH=$(MONTH) -DYEAR=$(YEAR) -DDEFAULT_AUTO_RESTART=$(DEFAULT_AUTO_RESTART)\
	   -DDEFAULT_START_TIMEOUT=$(DEFAULT_START_TIMEOUT)\
	   -DDEFAULT_STOP_TIMEOUT=$(DEFAULT_STOP_TIMEOUT)\
	   -DSUPPORT_CGROUPS=$(SUPPORT_CGROUPS) -DSUPPORT_CAPABILITIES=$(SUPPORT_CAPABILITIES)\
	   -DSUPPORT_IOPRIO=$(SUPPORT_IOPRIO) -DSUPPORT_OOM_ADJ=$(SUPPORT_OOM_ADJ)\
	   haiku-service.5.m4 > haiku-service.5

shutdown.8: shutdown.8.m4
	m4 -DVERSION=$(VERSION) -DMONTH=$(MONTH) -DYEAR=$(YEAR) -DSHUTDOWN_PREFIX=$(SHUTDOWN_PREFIX) shutdown.8.m4 > shutdown.8
