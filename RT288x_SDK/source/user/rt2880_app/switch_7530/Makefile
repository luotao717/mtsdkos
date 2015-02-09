EXEC = switch_7530  
CFLAGS += -I$(ROOTDIR)/$(LINUXDIR)
CFLAGS += -I$(ROOTDIR)/$(LINUXDIR)/drivers/net/raeth
CONF_H  = $(ROOTDIR)/$(LINUXDIR)/include/linux/autoconf.h

ifeq ($(CONFIG_RALINK_MT7620),y)
SRC=switch_7530.c
endif

all: $(EXEC)
switch: $(SRC) $(CONF_H)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC) $(LDLIBS)

romfs:
	$(ROMFSINST) /bin/switch_7530

clean:
	-rm -f $(EXEC) *.elf *.gdb *.o

