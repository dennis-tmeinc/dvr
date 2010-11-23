
CFLAGS	= -I../include -Wall
OFLAGS	= -O
LDFLAGS	= -L../library -lpolarssl

ssl/ssl_server: ssl/ssl_server.c ../library/libpolarssl.a
	echo   "  CC    ssl/ssl_server.c"
	$(CC) $(CFLAGS) $(OFLAGS) ssl/ssl_server.c   $(LDFLAGS) -o $@

clean:
	rm -f $(APPS)

