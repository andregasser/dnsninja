#! /bin/bash

gdb --silent --args dnsmap -r -v -s 160.85.104.60 -i iplist.txt -o results.txt
