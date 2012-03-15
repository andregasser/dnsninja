///////////////////////////////////////////////////////////////////////////////
//
// DNSMAP Version 0.1
// Written by sh0ka 2012
//
// Command line parameters:
// --reverse, -r                   Do reverse DNS lookups
//                                 (if not specified, a forward DNS lookup
//                                 will be performed)
// --servers, -s <ip1, ip2, ...>   List of DNS servers to query
// --domain, -d <domain name>      Domain name to map, e.g. foo.org
//                                 (Forward mode only)
// --list, -l <filename>           File containing a host or ip, depending
//                                 on the mode choosen (forward, reverse).
// --help, -h                      Display help page
//
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "dns.h"

/* Define some constants */
#define APP_NAME        "DNSMAP" // Name of applicaton
#define APP_VERSION     "0.1"    // Version of application

// Define structs
typedef struct 
{
	char *server1;
	char *domain;
	char *list;
	int reverse;
	int help;
} cmd_params;

/* Function prototypes */
int parse_cmd_args(int *argc, char *argv[]);
void do_dns_lookups(void);
void do_forward_dns_lookup(char *server, char *host);
void do_reverse_dns_lookup(char *server, char *host);
void chomp(char *s);
void display_help_page(void);

// Global vars
cmd_params params;

int main(int argc, char *argv[]) 
{
	/* Command line variables */
	char server[100], domain[100], wlfile[256];
	int ret = 0;

	printf("\n\n");
	printf("%s %s by sh0ka\n", APP_NAME, APP_VERSION);
	printf("-------------------\n");
	printf("\n");
		
	// Check commandline args
	ret = parse_cmd_args(&argc, argv);
	if (ret < 0) 
	{
		printf("Error: Could not parse cmd args (Error code: %d).\n", ret);
		return ret; 	
	}

	// Decide what to do
	if (params.help)
	{
		display_help_page();
	}
	else
	{
		do_dns_lookups();
	}


	return 0;
}

int parse_cmd_args(int *argc, char *argv[])
{
	// Init struct
	params.reverse = 0;
	params.help = 0;
	params.server1 = "";
	params.domain = "";
	params.list = "";

	while (1)
	{
		static struct option long_options[] = 
		{
			{ "reverse", no_argument,       0, 'r' },
			{ "servers", required_argument, 0, 's' },
			{ "domain",  required_argument, 0, 'd' },
			{ "list",    required_argument, 0, 'l' },
			{ "help",    no_argument,       0, 'h' },
			{ 0, 0, 0, 0 }
		};
	
		/* getopt_long stores the option index here */
		int option_index = 0;
		int c;

		c = getopt_long(*argc, argv, "rs:d:l:h", long_options, &option_index);

		/* Detect the end of the options */
		if (c == -1)
			break;

		switch (c)
		{
			case 'r':
				params.reverse = 1;						
				break;
			case 's':
				params.server1 = optarg;
				break;
			case 'd':
				params.domain = optarg;
				break;
			case 'l':
				params.list = optarg;
				break;
			case 'h':
				params.help = 1;
				break;
		}
	}

	// Print args
	//printf("Server: %s\n", params.server1);
	//printf("Domain: %s\n", params.domain);
	//printf("List: %s\n", params.list);
	//printf("Reverse: %d\n", params.reverse);
	//printf("Help: %d\n", params.help);
	
	return 0;
}

void do_dns_lookups(void)
{
	/* Open hostlist file */
	FILE *f;
	char line[255];
	char host[255];
	f = fopen(params.list, "r");
	if (f != NULL)
	{
		while (fgets(line, 255, f) != NULL)
		{
			memset(host, 0, sizeof(host));
			//printf("Host: %s", line);
			if (params.reverse == 0) 
			{
				// Lookup by hostname and try to get an IP address
				chomp(line);
				strcat(host, line);
				strcat(host, ".");
				strcat(host, params.domain);
				do_forward_dns_lookup(params.server1, host);
			}
			else 
			{
				// Lookup by IP address and try to get a hostname
				do_reverse_dns_lookup(params.server1, host);
			}
		}
	}
	fclose(f);	
}

// Perform a forward dns lookup
void do_forward_dns_lookup(char *server, char *host)
{
	printf("Doing a forward DNS lookup of %s on server %s\n", host, server);
	dns_query(server, host, DNS_RES_REC_A);
 
}

// Perform a reverse dns lookup
void do_reverse_dns_lookup(char *server, char *host)
{
	printf("Doing a forward DNS lookup of %s\n", host);
	//dns_query();
}

// Removes newlines \n from the char array
void chomp(char *s) {
    while(*s && *s != '\n' && *s != '\r') s++;
 
    *s = 0;
}

// Display a helpful page
void display_help_page(void)
{
	printf("APP_NAME APP_VERSION\n");
	printf("Written by sh0ka\n");
	printf("This tool is GPL'ed software. Use as you like.\n");
	printf("\n");
	printf("Syntax:\n");
	printf("%s [options]\n", APP_NAME);
	printf("\n\n");
	printf("Parameter             Description\n");
	printf("---------             ---------------------------------------------\n");
	printf("--reverse, -r         Do a reverse DNS lookup. If not specified, a\n");
	printf("                      forward DNS lookup will be performed.\n");
	printf("--server, -s          IP of DNS server to query\n");
	printf("--domain, -d          Domain to query. Only used when doing forward\n");
	printf("                      DNS lookups\n");
	printf("--list, -l            File containint hostnames or IP addresses, depending\n");
	printf("                      on query mode (forward, reverse\n");
	printf("\n\n");
}
