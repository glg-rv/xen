obj-y += lib/

obj-y   += domctl.o
obj-y   += domain.o
obj-y   += domain_build.o
obj-y   += delay.o
obj-y   += entry.o
obj-y   += guestcopy.o
obj-y   += irq.o
obj-y   += kernel.o
obj-y   += p2m.o
obj-y   += mm.o
obj-y   += percpu.o
obj-y   += sbi.o
obj-y   += setup.o
obj-y   += shutdown.o
obj-y   += smp.o
obj-y   += smpboot.o
obj-y   += sysctl.o
obj-y   += traps.o
obj-y   += time.o
obj-y   += vm_event.o
obj-y   += vplic.o

#obj-bin-y += ....o

ALL_OBJS := head.o $(ALL_OBJS)

DEPS += .head.o.d

$(TARGET): $(TARGET)-syms
	$(OBJCOPY) -O binary -S $< $@

#prelink.o: $(ALL_OBJS)
#	$(LD) $(LDFLAGS) -r -o $@ $^

ifeq ($(CONFIG_LTO),y)
# Gather all LTO objects together
prelink_lto.o: $(ALL_OBJS) $(ALL_LIBS)
	$(LD_LTO) -r -o $@ $(filter-out %.a,$^) --start-group $(filter %.a,$^) --end-group

# Link it with all the binary objects
prelink.o: $(patsubst %/built_in.o,%/built_in_bin.o,$(ALL_OBJS)) prelink_lto.o
	$(call if_changed,ld)
else
prelink.o: $(ALL_OBJS) $(ALL_LIBS) FORCE
	$(call if_changed,ld)
endif

$(TARGET)-syms: prelink.o xen.lds
	$(LD) $(LDFLAGS) -T xen.lds -N prelink.o \
	    $(BASEDIR)/common/symbols-dummy.o -o $(@D)/.$(@F).0
	$(NM) -pa --format=sysv $(@D)/.$(@F).0 \
		| $(BASEDIR)/tools/symbols $(all_symbols) --sysv --sort >$(@D)/.$(@F).0.S
	$(MAKE) -f $(BASEDIR)/Rules.mk $(@D)/.$(@F).0.o
	$(LD) $(LDFLAGS) -T xen.lds -N prelink.o \
	    $(@D)/.$(@F).0.o -o $(@D)/.$(@F).1
	$(NM) -pa --format=sysv $(@D)/.$(@F).1 \
		| $(BASEDIR)/tools/symbols $(all_symbols) --sysv --sort >$(@D)/.$(@F).1.S
	$(MAKE) -f $(BASEDIR)/Rules.mk $(@D)/.$(@F).1.o
	$(LD) $(LDFLAGS) -T xen.lds -N prelink.o $(build_id_linker) \
	    $(@D)/.$(@F).1.o -o $@
	$(NM) -pa --format=sysv $(@D)/$(@F) \
		| $(BASEDIR)/tools/symbols --xensyms --sysv --sort \
		>$(@D)/$(@F).map
	rm -f $(@D)/.$(@F).[0-9]*

asm-offsets.s: asm-offsets.c
	$(CC) $(filter-out -flto,$(CFLAGS)) -S -o $@ $<

xen.lds: xen.lds.S
	$(CC) -P -E $(a_flags) -D__LINKER__ -MQ $@ -o $@ $<
	#$(CC) -P -E -Ui386 $(a_flags) -o $@ $<
	#sed -e 's/xen\.lds\.o:/xen\.lds:/g' <.xen.lds.d >.xen.lds.d.new
	#mv -f .xen.lds.d.new .xen.lds.d

.PHONY: clean
clean::
	rm -f asm-offsets.s xen.lds
	rm -f $(BASEDIR)/.xen-syms.[0-9]*

.PHONY: include
include:
