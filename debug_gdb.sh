#! /bin/bash

#gdb --silent --args dnsmap -r -v 3 -s 160.85.104.60 -i iplist.txt -o results.txt
gdb --silent --args dnsninja -l 3 -s 193.192.238.98 -d ispin.ch -i iplist.txt
