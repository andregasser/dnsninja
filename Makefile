.PHONY : dnsmap dnsmap.o dns.o

dnsmap : dns.o dnsmap.o
	gcc -g -o dnsmap dns.o dnsmap.o

dnsmap.o :
	gcc -g -c dnsmap.c -o dnsmap.o

dns.o : 
	gcc -g -c dns.c -o dns.o 

clean : 
	rm -f dnsmap
	rm -f *.o
