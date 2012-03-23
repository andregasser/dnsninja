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
// Related links:
// --------------
//
// Reverse DNS lookups
// http://www.xinotes.org/notes/note/1665/  
//
// RFC 1034 - Domain Names - Concepts and Facilities
// http://www.ietf.org/rfc/rfc1035.txt
//
// RFC 1035 - Domain Implementation and Specification
// http://www.ietf.org/rfc/rfc1035.txt
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
#define APP_NAME        "DNSMAP" /* Name of applicaton */
#define APP_VERSION     "0.1"    /* Version of application */

/* Define structs */
typedef struct 
{
	char *servers[5];
	char *domain;
	char *inputfile;
	int reverse;
	int help;
	int verbose;
} cmd_params;

/* Define struct for storing workitems as single linked list */
typedef struct workitem
{
	char *wi;
	struct workitem *next;
} workitem;

/* Function prototypes */
int parse_cmd_args(int *argc, char *argv[]);
int parse_server_cmd_arg(char *optarg, char *servers[]);
void do_dns_lookups(void);
void do_forward_dns_lookup(char *server, char *host);
void do_reverse_dns_lookup(char *server, char *ip);
void chomp(char *s);
void display_help_page(void);
void load_workitems(char *inputfile, workitem **wi_start, int reverse);
void dist_workitems(workitem **wi_list, workitem **wi_t1, workitem **wi_t2,
		workitem **wi_t3, workitem **wi_t4, workitem **wi_t5);

/* Global vars */
cmd_params *params;

int main(int argc, char *argv[]) 
{
	int ret = 0;

	/* Allocate structures on heap */
	params = malloc(sizeof(cmd_params));

	printf("\n\n");
	printf("%s %s by André Gasser (sh0ka)\n", APP_NAME, APP_VERSION);
	printf("----------------------------------\n");
	printf("\n");

	/* Parse commandline args */
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
	int i;
	int ret = 0;

	// Init struct
	params->reverse = 0;
	params->help = 0;

	for (i = 0; i < 5; i++)
		params->servers[i] = '\0';

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
				// Process server parameters
				ret = parse_server_cmd_arg(optarg, params->servers);
				if (ret != 0) { return ret; }
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
		if (params->servers == '\0') { return -1; }
		if (params->domain == '\0') { return -2; }
		if (params->inputfile == '\0') { return -3; }
	}
	else
	{
		// Reverse lookups require server and inputfile params
		if (params->servers == '\0') { return -4; }
		if (params->inputfile == '\0') { return -5; }
	}

	return 0;
}

int parse_server_cmd_arg(char *optarg, char *servers[])
{
	int i = 0;
	char *ptr;

	if (optarg != NULL) 
	{
		ptr = strtok(optarg, ",");
		while (ptr != NULL)
		{
			// Server speichern
			servers[i] = malloc(strlen(ptr));
			strcpy(servers[i], ptr);
			i++;

			// Get next token
			ptr = strtok(NULL, ",");
		}
	}
}

void do_dns_lookups(void)
{
	int i = 0;
	workitem *wi_start, *wi_t1, *wi_t2, *wi_t3, *wi_t4, *wi_t5;

	wi_start = NULL;
	wi_t1 = NULL;
	wi_t2 = NULL;
	wi_t3 = NULL;
	wi_t4 = NULL;
	wi_t5 = NULL;

	printf("Using DNS servers: ");
	while (params->servers[i] != '\0')
	{
		printf("%s ", params->servers[i]);
		i++;
	}
	printf("\n");

	/* Load workitems into linked list */
	load_workitems(params->inputfile, &wi_start, params->reverse);	

	/* Distribute workitems among threads */
	dist_workitems(&wi_start, &wi_t1, &wi_t2, &wi_t3, &wi_t4, &wi_t5);

	/* Do DNS lookups */
	/*
	   if (params->reverse == 0)
	   {
	   do_forward_dns_lookups(params->servers[0], host);
	   }
	   else
	   {
	   do_reverse_dns_lookups(params->servers[0], host);
	   }
	 */
}

/*
 * Load ip's/hosts in a linked list.
 */
void load_workitems(char *inputfile, workitem **wi_start, int reverse)
{
	FILE *f;
	workitem *curr, *head;
	char host[256];
	char line[256];
	int first = 1;

	head = NULL;

	f = fopen(params->inputfile, "r");
	if (f != NULL)
	{
		while (fgets(line, 255, f) != NULL)
		{
			memset(host, 0, sizeof(host));
			chomp(line);
			if (reverse)
			{
				strcpy(host, line);
			}
			else
			{
				strcat(host, line);
				strcat(host, ".");
				strcat(host, params->domain);
			}

			curr = (workitem *)malloc(sizeof(workitem));  /* free memory! */
			curr->wi = (char *)malloc(sizeof(host));
			strcpy(curr->wi, host);


			curr->next = head;
			head = curr;
		}
		fclose(f);	

		*wi_start = head;
	}
	else
	{
		printf("Error: File %s could not be opened\n", inputfile);
		exit -1;
	}
}


/*
 * Distribute workitems among threads.
 */
void dist_workitems(workitem **wi_list, workitem **wi_t1, workitem **wi_t2,
		workitem **wi_t3, workitem **wi_t4, workitem **wi_t5)
{
	workitem *curr;
	int max_trays = 5;
	int tray = 0;
	int count = 0;


	while (*wi_list)
	{
		/* Choose tray */
		tray = count % max_trays;

		printf("Placing workitem %s in tray %d\n", (*wi_list)->wi, tray);

		//curr = (workitem *)malloc(sizeof(workitem));

		/*
		   switch (tray)
		   {
0:
		 *wi_t1 = *wi_list;
		 break;
1:
2:
3:
4:
}
		 */

		*wi_list = (*wi_list)->next;

		count++;

		}
}


/*
 * Perform a forward DNS lookup
 */
void do_forward_dns_lookup(char *server, char *host)
{
	int found = 0;
	int first = 1;
	int i;
	char *ip_addr[20];

	/* Initialize array of ip adresses */
	for (i = 0; i < 20; i++) { ip_addr[i] = '\0'; }

	printf("%-30s  ->  ", host);

	dns_query_a_record(server, host, ip_addr);

	/* List ip addresses */
	for (i = 0; i < 20; i++)
	{
		if (ip_addr[i] != '\0')
		{
			if (first)
			{
				printf("%s\n", ip_addr[i]);
				first = 0;
			}
			else
			{
				printf("                                    %s\n", ip_addr[i]);
			}
			found = 1;
		}
	}

	if (!found) { printf("%s\n", "n/a"); }
}

// Perform a reverse dns lookup
void do_reverse_dns_lookup(char *server, char *ip)
{
	int found = 0;
	int first = 1;
	int i;
	char *domains[20];

	// Initialize array of domains
	for (i = 0; i < 20; i++) { domains[i] = '\0'; }

	printf("%-30s  ->  ", ip);

	dns_query_ptr_record(server, ip, domains);

	// List domains
	for (i = 0; i < 20; i++)
	{
		if (domains[i] != '\0')
		{
			if (first)
			{
				printf("%s\n", domains[i]);
				first = 0;
			}
			else
			{
				printf("                                    %s\n", domains[i]);
			}
			found = 1;
		}
	}

	if (!found) { printf("%s\n", "n/a"); }
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
