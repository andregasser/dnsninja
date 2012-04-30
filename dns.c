/******************************************************************************
 *    Copyright 2012 André Gasser
 *
 *    This file is part of Dnsninja.
 *
 *    Dnsninja is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Dnsninja is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Dnsninja.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "dns.h"


/*
 * This will convert www.google.com to 3www6google3com
 * got it :)
 * */
void change_to_dns_name_format(unsigned char* dns, unsigned char* host)
{
	int lock = 0 , i;
	strcat((char*)host, ".");

	for(i = 0; i < strlen((char*)host); i++)
	{
		if(host[i] == '.')
		{
			*dns++ = i - lock;
			for(; lock<i; lock++)
			{
				*dns++ = host[lock];
			}
			lock++; //or lock=i+1;
		}
	}
	*dns++ = '\0';
}

//
// dns_query_a_record
//
// Queries a DNS server by sending a host name. Tries to retrieve 
// one or more ip addresses for the specified host name.
//
void dns_query_a_record(char *server, char *host, char *ip_addr[])
{
	char buffer[65536];
	int i, j, s, stop;
	char *qname, *reader;
	struct sockaddr_in a, dest;
	struct DNS_HEADER *dns = NULL;
	struct QUESTION *qinfo = NULL;
	struct RES_RECORD answers[20];

	// Initialize buffer
	memset(buffer, 0, 65536);

	// Open UDP network socket for DNS packets
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);	
	dest.sin_family = AF_INET;
	dest.sin_port = htons(53);
	dest.sin_addr.s_addr = inet_addr(server);

	// Fill the DNS header structure
	dns = (struct DNS_HEADER *)&buffer;
	dns->id = (unsigned short) htons(getpid());
	dns->qr = 0;              // This is a query
	dns->opcode = 0;          // This is a standard query
	dns->aa = 0;              // Not Authoritative
	dns->tc = 0;              // This message is not truncated
	dns->rd = 1;              // Recursion Desired
	dns->ra = 0;              // Recursion not available! hey we dont have it (lol)
	dns->z = 0;
	dns->ad = 0;
	dns->cd = 0;
	dns->rcode = 0;
	dns->q_count = htons(1);  // we have only 1 question
	dns->ans_count = 0;
	dns->auth_count = 0;
	dns->add_count = 0;

	// Point to the query portion
	qname = (char *)&buffer[sizeof(struct DNS_HEADER)];
	change_to_dns_name_format(qname, host);
	qinfo = (struct QUESTION *)&buffer[sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1)];	
	qinfo->qtype = htons(DNS_RES_REC_A);  // qtype = A
	qinfo->qclass = htons(1); // qclass = IN

	if (sendto(s, (char *)buffer, sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1) + 
	sizeof(struct QUESTION), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
	{
		perror("sendto failed");
	}
	
	// Receive the answer
	i = sizeof(dest);
	if(recvfrom(s, (char *)buffer, 65536, 0, (struct sockaddr *)&dest, (socklen_t *)&i) < 0)
	{
		perror("recvfrom failed");
	}

	reader = &buffer[sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1) + 
		sizeof(struct QUESTION)];

	// Process answers	
	for (i = 0; i < ntohs(dns->ans_count); i++)
	{
		answers[i].name = read_name(reader, buffer, &stop);	
		reader = reader + stop;  // ??? remove that line - useless
		
		answers[i].resource = (struct R_DATA *)(reader);
		reader = reader + sizeof(struct R_DATA);
			
		// If it is an IPv4 address
		if (ntohs(answers[i].resource->type) == 1)
		{
			answers[i].rdata = (unsigned char *)malloc(ntohs(answers[i].resource->data_len));
			for (j = 0; j < ntohs(answers[i].resource->data_len); j++)
			{
				answers[i].rdata[j] = reader[j];
			}
			
			answers[i].rdata[ntohs(answers[i].resource->data_len)] = '\0';
			reader = reader + ntohs(answers[i].resource->data_len);
		}
		else
		{
			answers[i].rdata = read_name(reader, buffer, &stop);
			reader = reader + stop;
		}
	}
	
	for (i = 0; i < htons(dns->ans_count); i++)
	{
		// Process resource type A (IPv4 address)
		if (ntohs(answers[i].resource->type) == 1) 
		{
			long *p;
			p = (long *)answers[i].rdata;
			a.sin_addr.s_addr = (*p);  // working without ntohl
			ip_addr[i] = malloc(answers[i].resource->data_len);
			strcpy(ip_addr[i], inet_ntoa(a.sin_addr));
		}
	}
}


//
// dns_query_ptr_record
//
// Queries a DNS server by sending an ip address. Tries to retrieve 
// one or more domain names for the specified ip address.
//
void dns_query_ptr_record(char *server, char *ip, char *domains[])
{
	char buffer[65536];
	char ip_inaddr_arpa[256];
	int i, j, s, stop;
	char *qname, *reader;
	struct sockaddr_in a, dest;
	struct DNS_HEADER *dns = NULL;
	struct QUESTION *qinfo = NULL;
	struct RES_RECORD answers[20];


	// Initialize buffer
	memset(buffer, 0, 65536);

	// Open UDP network socket for DNS packets
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);	
	dest.sin_family = AF_INET;
	dest.sin_port = htons(53);
	dest.sin_addr.s_addr = inet_addr(server);

	// Fill the DNS header structure
	dns = (struct DNS_HEADER *)&buffer;
	dns->id = (unsigned short) htons(getpid());
	dns->qr = 0;              // This is a query
	dns->opcode = 0;          // This is a standard query
	dns->aa = 0;              // Not Authoritative
	dns->tc = 0;              // This message is not truncated
	dns->rd = 1;              // Recursion Desired
	dns->ra = 0;              // Recursion not available! hey we dont have it (lol)
	dns->z = 0;
	dns->ad = 0;
	dns->cd = 0;
	dns->rcode = 0;
	dns->q_count = htons(1);  // we have only 1 question
	dns->ans_count = 0;
	dns->auth_count = 0;
	dns->add_count = 0;

	// Point to the query portion
	qname = (char *)&buffer[sizeof(struct DNS_HEADER)];
	
	// Prepare ip address in in-addr.arpa format
	prep_inaddr_arpa(ip_inaddr_arpa, ip);
	
	change_to_dns_name_format(qname, ip_inaddr_arpa);
	
	qinfo = (struct QUESTION *)&buffer[sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1)];	
	qinfo->qtype = htons(DNS_RES_REC_PTR);  // qtype = PTR
	qinfo->qclass = htons(1); // qclass = IN

	if (sendto(s, (char *)buffer, sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1) + 
	sizeof(struct QUESTION), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
	{
		perror("sendto failed");
	}
	
	// Receive the answer
	i = sizeof(dest);
	if(recvfrom(s, (char *)buffer, 65536, 0, (struct sockaddr *)&dest, (socklen_t *)&i) < 0)
	{
		perror("recvfrom failed");
	}

	reader = &buffer[sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1) + 
		sizeof(struct QUESTION)];

	// Process answers	
	for (i = 0; i < ntohs(dns->ans_count); i++)
	{
		answers[i].name = read_name(reader, buffer, &stop);	
		reader = reader + stop;  // ??? remove that line - useless
		
		answers[i].resource = (struct R_DATA *)(reader);
		reader = reader + sizeof(struct R_DATA);
			
		// If it is an IPv4 address
		if (ntohs(answers[i].resource->type) == 1)
		{
			answers[i].rdata = (unsigned char *)malloc(ntohs(answers[i].resource->data_len));
			for (j = 0; j < ntohs(answers[i].resource->data_len); j++)
			{
				answers[i].rdata[j] = reader[j];
			}
			
			answers[i].rdata[ntohs(answers[i].resource->data_len)] = '\0';
			reader = reader + ntohs(answers[i].resource->data_len);
		}
		else
		{
			answers[i].rdata = read_name(reader, buffer, &stop);
			reader = reader + stop;
		}
	}
	
	for (i = 0; i < htons(dns->ans_count); i++)
	{
		// Process resource type A (IPv4 address)
		if (ntohs(answers[i].resource->type) == DNS_RES_REC_PTR) 
		{
			//long *p;
			//p = (long *)answers[i].rdata;
			//a.sin_addr.s_addr = (*p);  // working without ntohl
			domains[i] = malloc(answers[i].resource->data_len);
			strcpy(domains[i], answers[i].rdata);
		}
	}
}

void prep_inaddr_arpa(char *dest, char *src)
{
	char *copystr;
	char *tmp;
	char arpa_addr[256];
	char *token[4];
	int i = 0;
	char *ptr;
	
	memset(arpa_addr, 0, 256);
	
	if (src != NULL) 
	{
		/* strtok manipulats the source string. We therefore
		 * have to make a copy first
		 */
		copystr = (char *)malloc(strlen(src));
		strcpy(copystr, src);
		ptr = strtok(copystr, ".");
		while (ptr != NULL)
		{
			// Save token
			token[i] = malloc(strlen(ptr));
			strcpy(token[i], ptr);
			i++;
			
			// Get next token
			ptr = strtok(NULL, ".");
		}
		
		// Swap tokens
		tmp = token[0];
		token[0] = token[3];
		token[3] = tmp;
		tmp = token[1];
		token[1] = token[2];
		token[2] = tmp;

		// Prepare whole "in-addr.arpa" string
		strcat(arpa_addr, token[0]);
		strcat(arpa_addr, ".");
		strcat(arpa_addr, token[1]);
		strcat(arpa_addr, ".");
		strcat(arpa_addr, token[2]);
		strcat(arpa_addr, ".");
		strcat(arpa_addr, token[3]);
		strcat(arpa_addr, ".");
		strcat(arpa_addr, "in-addr.arpa");

		strcpy(dest, arpa_addr);

		/* Free allocated heap memory */
		free(copystr);
	}
}


unsigned char* read_name(unsigned char *reader, unsigned char *buffer, int *count)
{
	unsigned char *name;
	unsigned int p = 0, jumped = 0, offset;
	int i, j;
	
	*count = 1;
	name = (unsigned char *)malloc(256);
	
	name[0] = '\0';
	
	// Read the name in 3www6google3com format
	while (*reader != 0)
	{
		if (*reader >= 192)
		{
			// 49152 = 11000000 00000000 ;)
			offset = (*reader) * 256 + *(reader + 1) - 49152;
			reader = buffer + offset - 1;
			jumped = 1; // we have jumped to another location, so counting wont' go up!
		}
		else 
		{
			name[p++] = *reader;
		}
		
		reader = reader + 1;
		
		if (jumped == 0)
		{
			*count = *count + 1; // if we haven't jumped to another location then we can count up
		}
	}
		
	name[p] = '\0';  // string complete
		
	if (jumped == 1)
	{
		*count = *count + 1; // number of steps we actually moved forward in the packet
	}
	
	// Now convert 3www6google3com0 to www.google.com
	for (i = 0; i < (int)strlen((const char *)name); i++)
	{
		p = name[i];
		for (j = 0; j < (int)p; j++)
		{
			name[i] = name[i + 1];
			i = i + 1;
		}
		name[i] = '.';
	}

	name[i - 1] = '\0'; // Remove the last dot
		
	return name;
}


