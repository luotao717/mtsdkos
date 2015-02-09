all:
	$(MAKE) -C source3

romfs: all

	$(ROMFSINST) source3/bin/CP850.so /lib/CP850.so
	$(ROMFSINST) source3/bin/CP437.so /lib/CP437.so
	$(ROMFSINST) source3/bin/libwbclient.so.0 /lib/libwbclient.so.0
	$(ROMFSINST) source3/bin/libtalloc.so.2 /lib/libtalloc.so.2
	$(ROMFSINST) source3/bin/libtdb.so.1 /lib/libtdb.so.1
	$(ROMFSINST) source3/bin/nmbd /sbin/nmbd
	$(ROMFSINST) -e CONFIG_USER_SAMBA_ALL source3/bin/smbd /sbin/smbd
	$(ROMFSINST) -e CONFIG_USER_SAMBA_ALL source3/bin/smbpasswd /sbin/smbpasswd
	$(ROMFSINST) -e CONFIG_USER_SAMBA_ALL /bin/samba.sh
	$(ROMFSINST) -e CONFIG_USER_SAMBA_ALL /bin/samba_add_dir.sh

clean:
	$(MAKE) -C source3 clean
	rm -f source3/bin/*
