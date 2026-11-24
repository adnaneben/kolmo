# Included after the root Makefile by integration/*/Makefile (make -f Makefile -f integration/common/kolmo-objs.mk):
# builds the position-independent objects of libkolmo and prints what a plugin needs to link them in statically,
# so the plugins follow the root Makefile's source list and flags without copying them.
kolmo-pic-objs: $(ARCHPIC)
kolmo-print-cflags:
	@echo $(CFLAGS) $(DEFS)
kolmo-print-objs:
	@echo $(ARCHPIC)
kolmo-print-libs:
	@echo -lpthread -lm
.PHONY: kolmo-pic-objs kolmo-print-cflags kolmo-print-objs kolmo-print-libs
