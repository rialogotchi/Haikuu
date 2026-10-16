include ../mconfig
include ../build/version.conf

STRIPOPTS ?= -s
ALL_CXXFLAGS=$(CPPFLAGS) $(CXXFLAGS) $(CXXFLAGS_EXTRA)
ALL_LDFLAGS=$(LDFLAGS) $(LDFLAGS_EXTRA)

ifeq ($(BUILD_SHUTDOWN),yes)
  SHUTDOWN=$(SHUTDOWN_PREFIX)shutdown
endif

haiku_objects = haiku.o load-service.o service.o proc-service.o baseproc-service.o control.o haiku-log.o \
		haiku-main.o run-child-proc.o options-processing.o haiku-env.o settings.o haiku-iostream.o

objects = $(haiku_objects) haikuctl.o haiku-check.o shutdown.o haiku-monitor.o haiku-client.o

all: haiku haikuctl haiku-check haiku-monitor $(SHUTDOWN)

../mconfig:
	@echo "*** No mconfig, please run make from top level"
	@false

haiku: $(haiku_objects)
	$(CXX) -o haiku $(haiku_objects) $(ALL_LDFLAGS) $(LDFLAGS_LIBCAP)

haikuctl: haikuctl.o options-processing.o settings.o haiku-iostream.o haiku-client.o
	$(CXX) -o haikuctl haikuctl.o options-processing.o settings.o haiku-iostream.o haiku-client.o $(ALL_LDFLAGS)

haiku-check: haiku-check.o options-processing.o settings.o haiku-iostream.o haiku-client.o
	$(CXX) -o haiku-check haiku-check.o options-processing.o settings.o haiku-iostream.o haiku-client.o $(ALL_LDFLAGS) $(LDFLAGS_LIBCAP)

haiku-monitor: haiku-monitor.o
	$(CXX) -o haiku-monitor haiku-monitor.o $(ALL_LDFLAGS)

$(SHUTDOWN_PREFIX)shutdown: shutdown.o
	$(CXX) -o $(SHUTDOWN_PREFIX)shutdown shutdown.o $(ALL_LDFLAGS)

$(objects): ../build/includes/mconfig.h

$(objects): %.o: %.cc
	$(CXX) $(ALL_CXXFLAGS) $(CPPFLAGS_LIBCAP) -MMD -MP -Iincludes -I../dasynq/include -I../build/includes -c $< -o $@

check: ../build/includes/mconfig.h $(haiku_objects)
	$(MAKE) -C tests check

check-igr: haiku haikuctl haiku-check
	$(MAKE) -C igr-tests check-igr

run-cppcheck:
	cppcheck --std=c++11 -Iincludes -Idasynq --force --enable=all *.cc 2>../cppcheck-report.txt

install: all
	install -d $(DESTDIR)$(BINDIR)
	install $(STRIPOPTS) haiku haikuctl haiku-check haiku-monitor $(DESTDIR)$(BINDIR)
ifeq ($(BUILD_SHUTDOWN),yes)
	install -d $(DESTDIR)$(SBINDIR)
	install $(STRIPOPTS) $(SHUTDOWN) $(DESTDIR)$(SBINDIR)
	ln -sf $(SHUTDOWN) $(DESTDIR)$(SBINDIR)/$(SHUTDOWN_PREFIX)halt
	ln -sf $(SHUTDOWN) $(DESTDIR)$(SBINDIR)/$(SHUTDOWN_PREFIX)reboot
	ln -sf $(SHUTDOWN) $(DESTDIR)$(SBINDIR)/$(SHUTDOWN_PREFIX)soft-reboot
	ln -sf $(SHUTDOWN) $(DESTDIR)$(SBINDIR)/$(SHUTDOWN_PREFIX)poweroff
endif

clean:
	rm -f *.o *.d
	rm -f haiku haikuctl haiku-check $(SHUTDOWN_PREFIX)shutdown haiku-monitor
	$(MAKE) -C tests clean
	$(MAKE) -C igr-tests clean

-include $(objects:.o=.d)
