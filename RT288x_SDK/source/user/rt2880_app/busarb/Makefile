

CONF_H	= $(ROOTDIR)/$(LINUXDIR)/include/linux/autoconf.h
UCONF_H	= $(ROOTDIR)/config/autoconf.h

all:	busarb busarb.o reg.o
#
#	Primary link
#
busarb: busarb.o reg.o
	$(CC) -o $@ busarb.o reg.o $(CFLAGS) $(LDFLAGS) $(LDLIBS)

busarb.o: busarb.c
	$(CC) -o $@ -c busarb.c $(CFLAGS) $(LDFLAGS) $(LDLIBS)

reg.o: reg.c
	$(CC) -o $@ -c reg.c $(CFLAGS) $(LDFLAGS) $(LDLIBS)

romfs:
	$(STRIP) busarb
	$(ROMFSINST) /bin/busarb

clean:
	rm -f busarb busarb.o reg.o
