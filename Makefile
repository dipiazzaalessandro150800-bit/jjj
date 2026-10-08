.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Imposta DEVKITARM, ad esempio: export DEVKITARM=/opt/devkitpro/devkitARM")
endif

include $(DEVKITARM)/ds_rules

TARGET  := battle
LIBNDS  := $(DEVKITPRO)/libnds
ARM7    := $(LIBNDS)/default.elf

ARCH    := -marm -mthumb-interwork
CFLAGS  := -g -Wall -O2 $(ARCH) -march=armv5te -mtune=arm946e-s \
           -fomit-frame-pointer -ffast-math -I$(LIBNDS)/include -DARM9
LDFLAGS := -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(TARGET).map -L$(LIBNDS)/lib
LIBS    := -lnds9 -lm

all: $(TARGET).nds

$(TARGET).nds: $(TARGET).elf
	ndstool -c $@ -9 $< $(if $(wildcard $(ARM7)),-7 $(ARM7),)

$(TARGET).elf: source/main.c
	$(CC) $(CFLAGS) $(LDFLAGS) source/main.c $(LIBS) -o $@

clean:
	rm -f $(TARGET).elf $(TARGET).nds $(TARGET).map
