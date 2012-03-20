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

void dns_query(char *server, char *host, int query_type, char *buffer)
{
	int i, j, s, stop;
	char *qname;
	struct sockaddr_in dest;
	struct DNS_HEADER *dns = NULL;
	struct QUESTION *qinfo = NULL;

	// Open UDP network socket for DNS packets
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);	
	dest.sin_family = AF_INET;
	dest.sin_port = htons(53);
	dest.sin_addr.s_addr = inet_addr(server);

	// Fill the DNS header structure
	dns = (struct DNS_HEADER *)buffer;
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
	dns->q_count = htons(1);  //we have only 1 question
	dns->ans_count = 0;
	dns->auth_count = 0;
	dns->add_count = 0;

	// Point to the query portion
	qname = (char *)&buffer[sizeof(struct DNS_HEADER)];
	change_to_dns_name_format(qname, host);
	qinfo = (struct QUESTION *)&buffer[sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1)];	
	qinfo->qtype = htons(1);
	qinfo->qclass = htons(1);

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
}


void dns_get_soa(char *buffer, ...)
{

}

void dns_get_a(char *buffer, char **ip_addr)
{
	int i, j, s, stop;
	char *qname, *reader;
	struct sockaddr_in a, dest;
	struct DNS_HEADER *dns = NULL;
	struct QUESTION *qinfo = NULL;
	struct RES_RECORD answers[20];

	// Initialize pointers
	dns = (struct DNS_HEADER *)buffer;
	qname = (char *)&buffer[sizeof(struct DNS_HEADER)];
	reader = &buffer[sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1) + 
		sizeof(struct QUESTION)];
	
	stop = 0;
	
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
		
		// Process resource type CNAME (Canonical name)
		//if (ntohs(answers[i].resource->type) == 5) 
		//{
			//printf("has alias name : %s", answers[i].rdata);
		//}
	}
}


void dns_print_packet_info(char *buffer)
{
	int i, j, s, stop;
	//char buf[65536], *qname, *reader;
	char *qname, *reader;
	struct sockaddr_in a, dest;
	struct RES_RECORD answers[20], auth[20], addit[20];
	struct DNS_HEADER *dns = NULL;
	struct QUESTION *qinfo = NULL;

	//printf("Done");

	//move ahead of the dns header and the query field
	//reader = &buf[sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1) + 
	//sizeof(struct QUESTION)];

	dns = (struct DNS_HEADER *)buffer;
	qname = (char *)&buffer[sizeof(struct DNS_HEADER)];



	printf("\nThe response contains : ");
	printf("\n %d Questions.",ntohs(dns->q_count));
	printf("\n %d Answers.",ntohs(dns->ans_count));
	printf("\n %d Authoritative Servers.",ntohs(dns->auth_count));
	printf("\n %d Additional records.\n\n",ntohs(dns->add_count));
	
	// Try to read ANSWER resource records
	// Point to beginning of ANSWER records
	reader = &buffer[sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1) + 
		sizeof(struct QUESTION)];
	
	stop = 0;
	
	for (i = 0; i < ntohs(dns->ans_count); i++)
	{
		answers[i].name = read_name(reader, buffer, &stop);	
		reader = reader + stop;
		
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
		printf("Name: %s ", answers[i].name);
		if (ntohs(answers[i].resource->type) == 1) // IPv4 address
		{
			long *p;
			p = (long *)answers[i].rdata;
			a.sin_addr.s_addr = (*p);  // working without ntohl
			printf("has IPv4 address : %s", inet_ntoa(a.sin_addr));
		}
		if (ntohs(answers[i].resource->type) == 5) // Canonical name
		{
			printf("has alias name : %s", answers[i].rdata);
		}
		printf("\n");
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


