                               ==DNSMAP==

                        Version 0.1 / 21.03.2012

--=[ Disclaimer ]=-----------------------------------------------------//

This document describes DNSMAP, a tool to perform dictionary based
queries against DNS servers. The author of this document provides the
software AS IS and and cannot be made responsible for any damage that
may occur while using this tool.

This software may contain errors or may not work properly in certain
circumstances. Therefore, use this tool at your own risk. 

You can use this tool free of charge and you are allowed to freely
modify and distribute it.




--[ Contents ]----------------------------------------------------------

1 - Introduction

2 - The tool
  2.1 - Overview
  2.2 - Features
  2.3 - Usage
    2.3.1 - Doing forward DNS lookups
    2.3.2 - Doing reverse DNS lookups
    2.3.3 - Saving results to a file
  2.4 - How to get it
  2.5 - License
  
3 - Bibliography

4 - Contact




--[ 1 - Introduction ]--------------------------------------------------



--[ 2 - The tool ]------------------------------------------------------

----[ 2.1 - Overview ]--------------------------------------------------

DNSMAP is a tool to query DNS servers by using prepared wordlists.
People usually use a tool like this, if the are not the owner of the
DNS server and therefore do not have access to the zone data on the 
server. Nevertheless, they want to enumerate / discover hosts located in
a certain zone.

By using DNSMAP, they get the possibility to do forward DNS lookups by
providing a list of hostnames. But they can also do reverse DNS lookups
(aka inverse DNS lookups) to identify domain names based on ip
addresses.

A tool like this can generate heavy load on the target. It is therefore
adviced to take caution. System administrators at the target's site
could interpret such scans as some kind of attack and take counter-
measures.

You have been warned.


----[ 2.2 - Features ]--------------------------------------------------

the current version of DNSMAP offers the following features:

  + Do forward DNS lookups based on wordlist
  + Do reverse DNS lookups based on a list of ip addresses
  + Query up to five DNS servers in parallel to distribute the load
  + Save the results to a text file


----[ 2.3 - Usage ]-----------------------------------------------------

----[ 2.3.1 - Doing forward DNS lookups ]-------------------------------

Usage of this tool is quite simple. Do do a simple forward DNS lookup
search based on a wordlist, run the following command:

$ ./dnsmap -s 111.222.333.444 -d mydomain.com -i myhosts.txt

This connects to the DNS server 111.222.333.444 and sends queries for the
mydomain.com domain. It uses the file myhosts.txt which contains entries like
these:

$ cat myhosts.txt
www
backup
mail
webmail
gatekeeper
www1
www2
www3

The myhosts.txt file and the specified domain are used to form DNS queries
such as:

www.mydomain.com
backup.mydomain.com
...

If a host has been found, the ip address of this host is returned.


----[ 2.3.2 - Doing reverse DNS lookups ]-------------------------------

Doing reverse DNS lookups works similar. To do reverse DNS lookups based
on a list of ip addresses, run the following command:

$ ./dnsmap -r -s 111.222.333.444 -i myips.txt

Again, the DNS server 111.222.333.444 is contacted to DNS PTR queries will
be sent to this machine. As a result, you will receive the name of the 
domains associated with that ip. 


----[ 2.3.3 - Saving output to a file ]---------------------------------




----[ 2.4 - How to get it ]---------------------------------------------

You'll always find the latest version of this tool on GitHub.

blabla...




----[ 2.5 - License ]---------------------------------------------------




----[ 3 - Bibliography ]------------------------------------------------

[1] Reverse DNS lookups
    http://www.xinotes.org/notes/note/1665/  

[2] RFC 1034 - Domain Names - Concepts and Facilities
    http://www.ietf.org/rfc/rfc1035.txt

[3] RFC 1035 - Domain Implementation and Specification
    http://www.ietf.org/rfc/rfc1035.txt
    
----[ 4 - Contact ]-----------------------------------------------------

Mail:      andre.gasser@gmx.ch





    
    
    
    
