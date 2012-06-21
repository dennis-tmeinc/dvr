
# To compile on SunOS: add "-lsocket -lnsl" to LDFLAGS
# To compile on MinGW: add "-lws2_32" to LDFLAGS
# To compile with PKCS11: add "-lpkcs11-helper" to LDFLAGS

CFLAGS	+= -I../include -D_FILE_OFFSET_BITS=64 -Wall -W -Wdeclaration-after-statement
OFLAGS	= -O
LDFLAGS	+= -L../library -lpolarssl $(SYS_LDFLAGS)

APPS =	ssl/ssl_server	
	
.SILENT:


ssl/ssl_server: ssl/ssl_server.c ../library/libpolarssl.a
	echo   "  CC    ssl/ssl_server.c"
	$(CC) $(CFLAGS) $(OFLAGS) ssl/ssl_server.c   $(LDFLAGS) -o $@

clean:
	rm -f $(APPS)

