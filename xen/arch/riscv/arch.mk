########################################
# RISCV-specific definitions

$(call cc-options-add,CFLAGS,CC,$(EMBEDDED_EXTRA_CFLAGS))
$(call cc-option-add,CFLAGS,CC,-Wnested-externs)
$(call cc-option-add,CFLAGS,CC,-mstrict-align)
$(call cc-option-add,CFLAGS,CC,-mtune=size)

CFLAGS-$(CONFIG_RISCV_64) += -mabi=lp64

riscv-march-$(CONFIG_RISCV_ISA_RV64IMA) := rv64ima
riscv-march-$(CONFIG_RISCV_ISA_C)       := $(riscv-march-y)c

# Note that -mcmodel=medany is used so that Xen can be mapped
# into the upper half _or_ the lower half of the address space.
# -mcmodel=medlow would force Xen into the lower half.

CFLAGS += -march=$(riscv-march-y) -mstrict-align -mcmodel=medany
