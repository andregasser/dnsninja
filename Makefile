.PHONY : log.o dns.o dnsmap.o dnsmap 

dnsmap : log.o dns.o dnsmap.o
	gcc -g -o dnsmap log.o dns.o dnsmap.o -lpthread

dnsmap.o : log.o
	gcc -g -c dnsmap.c -o dnsmap.o

dns.o : 
	gcc -g -c dns.c -o dns.o 

log.o :
	gcc -g -c log.c -o log.o

clean : 
	rm -f dnsmap
	rm -f *.o
