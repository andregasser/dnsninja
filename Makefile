.PHONY : log.o dns.o dnsninja.o dnsninja 

# Set compiler to use
CC=gcc
CFLAGS=
DEBUG=1

ifeq ($(DEBUG),1)
	CFLAGS+=-g
else
	CFLAGS+=-O2
endif

dnsninja : log.o dns.o dnsninja.o
	$(CC) $(CFLAGS) -o dnsninja log.o dns.o dnsninja.o -lpthread -lgcc_s

dnsninja.o : log.o
	$(CC) $(CFLAGS) -c dnsninja.c -o dnsninja.o

dns.o : 
	$(CC) $(CFLAGS) -c dns.c -o dns.o 

log.o :
	$(CC) $(CFLAGS) -c log.c -o log.o

clean : 
	rm -f dnsninja
	rm -f *.o
	rm -f *~
