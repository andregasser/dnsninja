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
// --inputfile, -i <filename>      File containing a host or ip, depending
//                                 on the mode choosen (forward, reverse).
// --verbose, -v                   Generate verbose output.
// --help, -h                      Display help page
//
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "dns.h"
#include "util.h"

/* Define some constants */
#define APP_NAME        "DNSMAP" // Name of applicaton
#define APP_VERSION     "0.1"    // Version of application

// Define structs
typedef struct 
{
	char *server1;
	char *domain;
	char *inputfile;
	int reverse;
	int help;
	int verbose;
} cmd_params;

/* Function prototypes */
int parse_cmd_args(int *argc, char *argv[]);
void do_dns_lookups(void);
void do_forward_dns_lookup(char *server, char *host);
void do_reverse_dns_lookup(char *server, char *host);
void chomp(char *s);
void display_help_page(void);

// Global vars
cmd_params *params;


int main(int argc, char *argv[]) 
{
	int ret = 0;
	
	// Allocate structures on heap
	params = malloc(sizeof(cmd_params));
		
	printf("\n\n");
	printf("%s %s by André Gasser (sh0ka)\n", APP_NAME, APP_VERSION);
	printf("----------------------------------\n");
	printf("\n");
		
	// Parse commandline args
	ret = parse_cmd_args(&argc, argv);
	if ((ret == -1) || (ret == -4)) 
	{
		printf("Error: Server must be specified (use option -s)\n"); 	
	}
	else if (ret == -2)
	{
		printf("Error: Domain must be specified (use option -d)\n");
	}
	else if ((ret == -3) || (ret == -5))
	{
		printf("Error: File must be specified (use option -l)\n");
	}

	// Display help page if desired. Otherwise, do DNS lookups
	params->help ? display_help_page() : do_dns_lookups();
	
	// Free memory on heap
	free(params);
	
	return ret;
}

int parse_cmd_args(int *argc, char *argv[])
{
	// Init struct
	params->reverse = 0;
	params->help = 0;
	params->server1 = "";
	params->domain = "";
	params->inputfile = "";
	params->verbose = 0;

	while (1)
	{
		static struct option long_options[] = 
		{
			{ "reverse",	no_argument,       0, 'r' },
			{ "servers",	required_argument, 0, 's' },
			{ "domain",		required_argument, 0, 'd' },
			{ "inputfile",	required_argument, 0, 'i' },
			{ "help",		no_argument,       0, 'h' },
			{ "verbose",	no_argument,       0, 'v' },
			{ 0, 0, 0, 0 }
		};
	
		/* getopt_long stores the option index here */
		int option_index = 0;
		int c;

		c = getopt_long(*argc, argv, "rs:d:i:hv", long_options, &option_index);

		/* Detect the end of the options */
		if (c == -1)
			break;

		switch (c)
		{
			case 'r':
				params->reverse = 1;						
				break;
			case 's':
				params->server1 = optarg;
				break;
			case 'd':
				params->domain = optarg;
				break;
			case 'i':
				params->inputfile = optarg;
				break;
			case 'h':
				params->help = 1;
				break;
			case 'v':
				params->verbose = 1;
				break;
		}
	}
	
	// Check param dependencies
	if (params->reverse == 0)
	{
		// Forward lookups require domain, server and inputfile params
		if (params->server1 == '\0') { return -1; }
		if (params->domain == '\0') { return -2; }
		if (params->inputfile == '\0') { return -3; }
	}
	else
	{
		// Reverse lookups require server and inputfile params
		if (params->server1 == '\0') { return -4; }
		if (params->inputfile == '\0') { return -5; }
	}
		
	return 0;
}

void do_dns_lookups(void)
{
	/* Open input file */
	FILE *f;
	char line[255];
	char host[255];
	
	/*
	if (params->reverse)
	{
		print_verb("Doing reverse DNS lookups against %s...\n", params->server1);
	} 
	else
	{
		print_verb("Doing forward DNS lookups against %s...\n", params->server1);
	}
	
	print_verb("Opening file %s...\n", params->inputfile);
	*/
	
	f = fopen(params->inputfile, "r");
	if (f != NULL)
	{
		while (fgets(line, 255, f) != NULL)
		{
			memset(host, 0, sizeof(host));
			if (params->reverse == 0) 
			{
				// Lookup by hostname and try to get an IP address
				chomp(line);
				strcat(host, line);
				strcat(host, ".");
				strcat(host, params->domain);
				do_forward_dns_lookup(params->server1, host);
			}
			else 
			{
				// Lookup by IP address and try to get a hostname
				do_reverse_dns_lookup(params->server1, host);
			}
		}
		//print_verb("Closing file %s...\n", params->inputfile);
		fclose(f);	
	}
	else
	{
		printf("Error: File %s could not be opened\n", params->reverse);
		exit -1;
	}
}

// Perform a forward dns lookup
void do_forward_dns_lookup(char *server, char *host)
{
	int i;
	char *buffer = malloc(65536);
	memset(buffer, 0, 65536);
	char *ip_addr[20];
	
	// Initialize array of ip adresses
	for (i = 0; i < 20; i++) { ip_addr[i] = '\0'; }
		
	printf("%s -->", host);


	//print_verb("Doing a forward DNS lookup of %s on server %s\n", host, server);
	dns_query(server, host, DNS_RES_REC_A, buffer);
 	
 	dns_get_ip_addr(buffer, ip_addr); 
	
	// List ip addresses
	for (i = 0; i < 20; i++)
	{
		if (ip_addr[i] != '\0')
		{
			printf("%s\n", ip_addr[i]);
		}
	}
	printf("\n");
	
 	free(buffer);
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
	printf("--inputfile, -i       File containint hostnames or IP addresses, depending\n");
	printf("                      on query mode (forward, reverse\n");
	printf("\n\n");
}
