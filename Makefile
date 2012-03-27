.PHONY : log.o dns.o dnsmap.o dnsmap 

# Set compiler to use
CC=gcc
CFLAGS=
DEBUG=0

ifeq ($(DEBUG),1)
	CFLAGS+=-g
else
	CFLAGS+=-O2
endif

dnsmap : log.o dns.o dnsmap.o
	$(CC) $(CFLAGS) -o dnsmap log.o dns.o dnsmap.o -lpthread

dnsmap.o : log.o
	$(CC) $(CFLAGS) -c dnsmap.c -o dnsmap.o

dns.o : 
	$(CC) $(CFLAGS) -c dns.c -o dns.o 

log.o :
	$(CC) $(CFLAGS) -c log.c -o log.o

clean : 
	rm -f dnsmap
	rm -f *.o
	rm -f *~
