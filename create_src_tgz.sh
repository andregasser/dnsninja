#! /bin/sh

tar --create --file=dnsmap.tar dnsmap.c dns.c dns.h log.c log.h Makefile COPYING README.TXT iplist.txt hostlist.txt TODO
gzip dnsmap.tar
