///////////////////////////////////////////////////////////////////////////////
//
// DNSMAP Version 0.1
// Written by sh0ka 2012
//
// Command line parameters:
// -r	                     Do a reverse DNS lookup
// -f                        Do a forward DNS lookup
// -s <Server IP/hostname>   DNS server to query
// -d <Domain name>          Domain name to map, e.g. foo.org
// -w <hostlist file>        Name of file with hosts/ip's to use
//
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include "dns.h"

/* Define some constants */
#define LOOKUP_TYPE_FWD 1  /* Forward DNS lookup */
#define LOOKUP_TYPE_REV 2  /* Reverse DNS lookup */

/* Function prototypes */
int parse_cmd_args(int *argc, char *argv[], int *ltype, char *server, char *domain, char *wlfile);
void do_dns_lookups(int *ltype, char *server, char *domain, char *wlfile);
void do_forward_dns_lookup(char *server, char *host);
void do_reverse_dns_lookup(char *server, char *host);
void chomp(char *s);

int main(int argc, char *argv[]) 
{
	/* Command line variables */
	char server[100], domain[100], wlfile[256];
	int ltype = LOOKUP_TYPE_FWD;
	int ret = 0;

	printf("dnsmap 0.1 by sh0ka\n");
	printf("-------------------\n");
	printf("\n");
		
	// Check commandline args
	ret = parse_cmd_args(&argc, argv, &ltype, server, domain, wlfile);
	if (ret < 0) 
	{
		printf("Error: Could not parse cmd args (Error code: %d).\n", ret);
		return ret; 	
	}

	// Print parameters found
	printf("Lookup type: ");
	if (ltype == LOOKUP_TYPE_FWD) 
	{
		printf("Forward\n");
	}
	else
	{
		printf("Reverse\n");
	}
	printf("Using server: %s\n", server);
	printf("Using domain: %s\n", domain);
	printf("Using wordlist: %s\n", wlfile);

	/* Invoke lookups */
	do_dns_lookups(&ltype, server, domain, wlfile);

	return 0;
}

int parse_cmd_args(int *argc, char *argv[], int *ltype, char *server, char *domain, char *wlfile)
{
	int i;

	printf("argc = %d\n", *argc);

	if (*argc == 8)
	{
		for (i = 0; i < *argc; i++)
		{
			printf("argv[%d] = %s\n", i, argv[i]);

			/* Check for lookup type */
			if ((strcmp("-r", argv[i]) == 0))
			{
				*ltype = LOOKUP_TYPE_REV;
			}
			
			if ((strcmp("-f", argv[i]) == 0))
			{
				*ltype = LOOKUP_TYPE_FWD;
			}

			/* Check for server name */
			if ((strcmp("-s", argv[i]) == 0))
			{
				if (argv[i + 1][0] == '-')
				{
					/* Error: no server specified */
					return -2;
				}
				else
				{
					strcpy(server, argv[i + 1]);
				} 
			}

			/* Check for domain name */
			if ((strcmp("-d", argv[i]) == 0))
			{
				if (argv[i + 1][0] == '-')
				{
					/* Error: no domain specified */
					return -3;
				}
				else
				{
					strcpy(domain, argv[i + 1]);
				} 
			}

			/* Check for wordlist file */
			if ((strcmp("-w", argv[i]) == 0) && (i < *argc))
			{
				if (argv[i + 1][0] == '-')
				{
					/* Error: no wordlist specified */
					return -4;
				}
				else
				{
					strcpy(wlfile, argv[i + 1]);
				} 
			}
		}
	}
	else 
	{
		/* Error: Wrong parameter count */
		return -1;
	}	
}

void do_dns_lookups(int *ltype, char *server, char *domain, char *wlfile)
{
	/* Open hostlist file */
	FILE *f;
	char line[255];
	char host[255];
	f = fopen(wlfile, "r");
	if (f != NULL)
	{
		while (fgets(line, 255, f) != NULL)
		{
			memset(host, 0, sizeof(host));
			//printf("Host: %s", line);
			if (*ltype == LOOKUP_TYPE_FWD) 
			{
				/* Lookup by hostname and try to get an IP address */
				chomp(line);
				strcat(host, line);
				strcat(host, ".");
				strcat(host, domain);
				do_forward_dns_lookup(server, host);
			}
			else 
			{
				/* Lookup by IP address and try to get a hostname */
				do_reverse_dns_lookup(server, host);
			}
		}
	}
	fclose(f);	
}

void do_forward_dns_lookup(char *server, char *host)
{
	printf("Looking up %s\n", host);
	dns_query(server, host, DNS_RES_REC_A);
 
}

void do_reverse_dns_lookup(char *server, char *host)
{

}

/* Removes newlines \n from the char array */
void chomp(char *s) {
    while(*s && *s != '\n' && *s != '\r') s++;
 
    *s = 0;
}
