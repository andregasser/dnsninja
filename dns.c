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

void dns_query(char *server, char *host, int query_type)
{
	int i, s;
	char buf[65536], *qname, *reader;
	struct sockaddr_in a, dest;
	struct RES_RECORD answers[20], auth[20], addit[20];
	struct DNS_HEADER *dns = NULL;
	struct QUESTION *qinfo = NULL;

	printf("Resolving %s", host);

	// Open UDP network socket for DNS packets
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);	
	dest.sin_family = AF_INET;
	dest.sin_port = htons(53);
	dest.sin_addr.s_addr = inet_addr(server);

	// Fill the DNS header structure
	dns = (struct DNS_HEADER *)&buf;
	dns->id = (unsigned short) htons(getpid());
	dns->qr = 0; //This is a query
	dns->opcode = 0; //This is a standard query
	dns->aa = 0; //Not Authoritative
	dns->tc = 0; //This message is not truncated
	dns->rd = 1; //Recursion Desired
	dns->ra = 0; //Recursion not available! hey we dont have it (lol)
	dns->z = 0;
	dns->ad = 0;
	dns->cd = 0;
	dns->rcode = 0;
	dns->q_count = htons(1); //we have only 1 question
	dns->ans_count = 0;
	dns->auth_count = 0;
	dns->add_count = 0;

	// Point to the query portion
	qname = (char *)&buf[sizeof(struct DNS_HEADER)];
	change_to_dns_name_format(qname, host);
	qinfo = (struct QUESTION *)&buf[sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1)];	
	qinfo->qclass = htons(1);

	printf("\nSending packet...");
	
	if(sendto(s, (char *)buf, sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1) + 
	sizeof(struct QUESTION), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
	{
		perror("sendto failed");
	}
	
	printf("Done");

	// Receive the answer
	i = sizeof(dest);
	printf("\nReceiving answer...");
	if(recvfrom(s, (char *)buf, 65536, 0, (struct sockaddr *)&dest, (socklen_t *)&i) < 0)
	{
		perror("recvfrom failed");
	}
	printf("Done");

	//move ahead of the dns header and the query field
	reader = &buf[sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1) + 
	sizeof(struct QUESTION)];

	printf("\nThe response contains : ");
	printf("\n %d Questions.",ntohs(dns->q_count));
	printf("\n %d Answers.",ntohs(dns->ans_count));
	printf("\n %d Authoritative Servers.",ntohs(dns->auth_count));
	printf("\n %d Additional records.\n\n",ntohs(dns->add_count));









}
