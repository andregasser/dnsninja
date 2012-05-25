#! /bin/sh

tar --create --file=dnsninja-0.1.1.tar dnsninja.c dns.c dns.h log.c log.h Makefile COPYING README iplist-example.txt hostlist-example.txt TODO
gzip dnsninja-0.1.1.tar
