EXEC = switch  
CFLAGS += -I$(ROOTDIR)/$(LINUXDIR)
CFLAGS += -I$(ROOTDIR)/$(LINUXDIR)/drivers/net/raeth
CONF_H  = $(ROOTDIR)/$(LINUXDIR)/include/linux/autoconf.h

ifeq ($(CONFIG_RALINK_RT6855),y)
SRC=switch_gsw.c
else
ifeq ($(CONFIG_RALINK_MT7620),y)
SRC=switch_gsw.c
else
ifeq ($(CONFIG_RALINK_MT7621),y)
SRC=switch_gsw.c
else
ifeq ($(CONFIG_RALINK_RT6855A),y)
SRC=switch_gsw.c
else
SRC=switch.c
endif
endif
endif
endif

all: $(EXEC)
ifeq ($(CONFIG_P5_RGMII_TO_MT7530_MODE),y)
	make -C ../switch_7530 all
endif

switch: $(SRC) $(CONF_H)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC) $(LDLIBS)

romfs:
	$(ROMFSINST) /bin/switch
ifeq ($(CONFIG_P5_RGMII_TO_MT7530_MODE),y)
	make -C ../switch_7530 romfs
endif

clean:
	-rm -f $(EXEC) *.elf *.gdb *.o

