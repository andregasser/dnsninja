/******************************************************************************
 *    Copyright 2012 André Gasser
 *
 *    This file is part of Dnsmap.
 *
 *    Dnsmap is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Dnsmap is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Dnsmap.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include "dns.h"
#include "log.h"

/* Define some constants */
#define APP_NAME        "DNSMAP" /* Name of applicaton */
#define APP_VERSION     "0.1"    /* Version of application */

/* Used to store command-line args */
typedef struct 
{
	char *servers[5];
	char *domain;
	char *inputfile;
	char *outputfile;
	int reverse;
	int help;
	int verbosity;
} cmd_params;

/* Used for storing workitems inside single linked list */
typedef struct workitem
{
	char *wi;
	struct workitem *next;
} workitem;

/* Used for storing DNS lookup results inside single linked list */
typedef struct result
{
	char *ip;
	char *host;
	struct result *next;
} result;

/* Used to pass arguments to threads */
typedef struct
{
	int thread_id;
	struct workitem *wi_list;
	int reverse;
	struct result *result_list;
	char *server;
} thread_params;

/* Function prototypes */
int parse_cmd_args(int *argc, char *argv[]);
int parse_server_cmd_arg(char *optarg, char *servers[]);
int do_dns_lookups(void);
void do_forward_dns_lookup(char *server, char *host, result *result_list);
void do_reverse_dns_lookup(char *server, char *ip, result **result_list);
void chomp(char *s);
void display_help_page(void);
void load_workitems(char *inputfile, workitem **wi_start, int reverse);
void dist_workitems(workitem **wi_list, workitem **wi_t1, workitem **wi_t2,
		workitem **wi_t3, workitem **wi_t4, workitem **wi_t5);
int count_workitems(workitem *wi_list);
void *proc_workitems(void *arg);
void write_results(result *results);
char *get_random_server(void);

/* Global vars */
cmd_params *params;
pthread_t t1, t2, t3, t4, t5;

/*
 * App entry point. The place where everything starts...
 */
int main(int argc, char *argv[]) 
{
	int ret = 0;

	/* Initialize pseudo-random number generator */
	srand((unsigned)time(NULL));

	/* Allocate structures on heap */
	params = malloc(sizeof(cmd_params));

	printf("%s  Copyright (C) 2012  André Gasser\n", APP_NAME);
    printf("This program comes with ABSOLUTELY NO WARRANTY.\n");
    printf("This is free software, and you are welcome to redistribute it\n");
    printf("under certain conditions.\n");
	printf("\n");
	printf("Executing %s Version %s\n", APP_NAME, APP_VERSION);
	printf("\n");

	/* Parse commandline args */
	ret = parse_cmd_args(&argc, argv);
	if ((ret == -1) || (ret == -4)) 
	{
		logline(LOG_ERROR, "Error: Server must be specified (use option -s)"); 	
	}
	else if (ret == -2)
	{
		logline(LOG_ERROR, "Error: Domain must be specified (use option -d)");
	}
	else if ((ret == -3) || (ret == -5))
	{
		logline(LOG_ERROR, "Error: File must be specified (use option -l)");
	}

	/* Set verbosity level */
	switch (params->verbosity)
	{
		case 1: set_loglevel(LOG_ERROR); break;
		case 2: set_loglevel(LOG_INFO); break;
		case 3: set_loglevel(LOG_DEBUG); break;
		default: set_loglevel(LOG_ERROR);
	}

	// Display help page if desired. Otherwise, do DNS lookups
	if (params->help)
	{
		display_help_page();
	}
	else
	{
		ret = do_dns_lookups();
		if (ret < 0)
		{
			logline(LOG_ERROR, "Error: Could not perform DNS lookups. Errorcode: %s", ret);
		}
	}

	// Free memory on heap
	free(params);

	return ret;
}

/*
 * Parse command line arguments and store them in a global struct.
 */
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
	params->outputfile = "";
	params->verbosity = 2;

	while (1)
	{
		static struct option long_options[] = 
		{
			{ "reverse",	no_argument,       0, 'r' },
			{ "servers",	required_argument, 0, 's' },
			{ "domain",		required_argument, 0, 'd' },
			{ "inputfile",	required_argument, 0, 'i' },
			{ "outputfile", required_argument, 0, 'o' },
			{ "help",		no_argument,       0, 'h' },
			{ "verbosity",	required_argument, 0, 'v' },
			{ 0, 0, 0, 0 }
		};

		/* getopt_long stores the option index here */
		int option_index = 0;
		int c;

		c = getopt_long(*argc, argv, "rs:d:i:o:hv:", long_options, &option_index);

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
			case 'o':
				params->outputfile = optarg;
				break;
			case 'h':
				params->help = 1;
				break;
			case 'v':
				params->verbosity = atoi(optarg);
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

/* 
 * Parse server option (-s).
 */
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

/*
 * Main function for doing DNS lookups.
 */
int do_dns_lookups(void)
{
	int i = 0;
	thread_params t1_params, t2_params, t3_params, t4_params, t5_params;
	void *t1_status, *t2_status, *t3_status, *t4_status, *t5_status;
	result *list_iterator;
	result *result_all;
	workitem *wi_start, *wi_t1, *wi_t2, *wi_t3, *wi_t4, *wi_t5;

	result_all = NULL;
	list_iterator = NULL;
	wi_start = NULL;
	wi_t1 = NULL;
	wi_t2 = NULL;
	wi_t3 = NULL;
	wi_t4 = NULL;
	wi_t5 = NULL;

	/* Display some info */
	logline(LOG_INFO, "Run configuration:");
	logline(LOG_INFO, "    Using DNS servers:");
	while (params->servers[i] != '\0')
	{
		logline(LOG_INFO, "        %s", params->servers[i]);
		i++;
	}
	if (params->domain)
	{
		logline(LOG_INFO, "    Using domain: %s", params->domain);
	}
	if (params->inputfile)
	{
		logline(LOG_INFO, "    Using input file: %s", params->inputfile);
	}
	if (params->outputfile)
	{
		logline(LOG_INFO, "    Using output file: %s", params->outputfile);
	}
	if (params->reverse)
	{
		logline(LOG_INFO, "    DNS lookup mode: Reverse");
	}
	else
	{
		logline(LOG_INFO, "    DNS lookup mode: Forward");
	}
	switch (params->verbosity)
	{
		case 1: logline(LOG_INFO, "    Verbosity: Error"); break;
		case 2: logline(LOG_INFO, "    Verbosity: Info"); break;
		case 3: logline(LOG_INFO, "    Verbosity: Debug"); break;
		default: logline(LOG_INFO, "    Verbosity: Error");
	}

	/* Load workitems into linked list */
	logline(LOG_DEBUG, "Loading workitems...");
	load_workitems(params->inputfile, &wi_start, params->reverse);	
	logline(LOG_DEBUG, "%d workitems loaded from file", count_workitems(wi_start));

	/* Distribute workitems among threads */
	logline(LOG_DEBUG, "Distributing workitems among threads...");
	dist_workitems(&wi_start, &wi_t1, &wi_t2, &wi_t3, &wi_t4, &wi_t5);
	logline(LOG_DEBUG, "Workitems distributed");
	logline(LOG_DEBUG, "Thread 1 workitems: %d", count_workitems(wi_t1));
	logline(LOG_DEBUG, "Thread 2 workitems: %d", count_workitems(wi_t2));
	logline(LOG_DEBUG, "Thread 3 workitems: %d", count_workitems(wi_t3));
	logline(LOG_DEBUG, "Thread 4 workitems: %d", count_workitems(wi_t4));
	logline(LOG_DEBUG, "Thread 5 workitems: %d", count_workitems(wi_t5));

	/* Prepare data structures for threads */
	t1_params.thread_id = 1;
	t1_params.wi_list = wi_t1;
	t1_params.reverse = params->reverse;
	t1_params.result_list = NULL;
	t1_params.server = get_random_server();

	t2_params.thread_id = 2;
	t2_params.wi_list = wi_t2;
	t2_params.reverse = params->reverse;
	t2_params.result_list = NULL;
	t2_params.server = get_random_server();
	
	t3_params.thread_id = 3;
	t3_params.wi_list = wi_t3;
	t3_params.reverse = params->reverse;
	t3_params.result_list = NULL;
	t3_params.server = get_random_server();
	
	t4_params.thread_id = 4;
	t4_params.wi_list = wi_t4;
	t4_params.reverse = params->reverse;
	t4_params.result_list = NULL;
	t4_params.server = get_random_server();

	t5_params.thread_id = 5;
	t5_params.wi_list = wi_t5;
	t5_params.reverse = params->reverse;
	t5_params.result_list = NULL;
	t5_params.server = get_random_server();

	if (pthread_create(&t1,	NULL, proc_workitems, &t1_params))
	{
		logline(LOG_ERROR, "Could not create thread 1");
		return -1;
	}
	else
	{
		logline(LOG_DEBUG, "Thread 1 created");
	}

	if (pthread_create(&t2,	NULL, proc_workitems, &t2_params))
	{
		logline(LOG_ERROR, "Could not create thread 2");
		return -1;
	}
	else
	{
		logline(LOG_DEBUG, "Thread 2 created");
	}

	if (pthread_create(&t3,	NULL, proc_workitems, &t3_params))
	{
		logline(LOG_ERROR, "Could not create thread 3");
		return -1;
	}
	else
	{
		logline(LOG_DEBUG, "Thread 3 created");
	}

	if (pthread_create(&t4,	NULL, proc_workitems, &t4_params))
	{
		logline(LOG_ERROR, "Could not create thread 4");
		return -1;
	}
	else
	{
		logline(LOG_DEBUG, "Thread 4 created");
	}

	if (pthread_create(&t5,	NULL, proc_workitems, &t5_params))
	{
		logline(LOG_ERROR, "Could not create thread 5");
		return -1;
	}
	else
	{
		logline(LOG_DEBUG, "Thread 5 created");
	}

	/* Wait for threads to finish */
	pthread_join(t1, &t1_status);
	pthread_join(t2, &t2_status);
	pthread_join(t3, &t3_status);
	pthread_join(t4, &t4_status);
	pthread_join(t5, &t5_status);

	/* Extract results out of threads */
	if ((int *)t1_status < 0)
	{
		logline(LOG_ERROR, "Thread 1 had an error condition");
	}
	else
	{
		/* Extract results from thread 1 */
		logline(LOG_DEBUG, "Thread 1 finished successfully");
	}

	if ((int *)t2_status < 0)
	{
		logline(LOG_ERROR, "Thread 2 had an error condition");
	}
	else
	{
		/* Extract results from thread 2 */
		logline(LOG_DEBUG, "Thread 2 finished successfully");
	}

	if ((int *)t3_status < 0)
	{
		logline(LOG_ERROR, "Thread 3 had an error condition");
	}
	else
	{
		/* Extract results from thread 3 */
		logline(LOG_DEBUG, "Thread 3 finished successfully");
	}

	if ((int *)t4_status < 0)
	{
		logline(LOG_ERROR, "Thread 4 had an error condition");
	}
	else
	{
		/* Extract results from thread 4 */
		logline(LOG_DEBUG, "Thread 4 finished successfully");
	}

	if ((int *)t5_status < 0)
	{
		logline(LOG_ERROR, "Thread 5 had an error condition");
	}
	else
	{
		/* Extract results from thread 5 */
		logline(LOG_DEBUG, "Thread 5 finished successfully");
	}

	/* Consolidate results */
	if (result_all == NULL)
	{
		result_all = t1_params.result_list;
	}
	else
	{
		list_iterator = result_all;
		while (list_iterator->next)
		{
			list_iterator = list_iterator->next;
		}
		list_iterator->next = t1_params.result_list;
	}

	if (result_all == NULL)
	{
		result_all = t2_params.result_list;
	}
	else
	{
		list_iterator = result_all;
		while (list_iterator->next)
		{
			list_iterator = list_iterator->next;
		}
		list_iterator->next = t2_params.result_list;
	}

	if (result_all == NULL)
	{
		result_all = t3_params.result_list;
	}
	else
	{
		list_iterator = result_all;
		while (list_iterator->next)
		{
			list_iterator = list_iterator->next;
		}
		list_iterator->next = t3_params.result_list;
	}

	if (result_all == NULL)
	{
		result_all = t4_params.result_list;
	}
	else
	{
		list_iterator = result_all;
		while (list_iterator->next)
		{
			list_iterator = list_iterator->next;
		}
		list_iterator->next = t4_params.result_list;
	}

	if (result_all == NULL)
	{
		result_all = t5_params.result_list;
	}
	else
	{
		list_iterator = result_all;
		while (list_iterator->next)
		{
			list_iterator = list_iterator->next;
		}
		list_iterator->next = t5_params.result_list;
	}

	/* Print results on screen */
	list_iterator = result_all;
	while (list_iterator)
	{
		logline(LOG_INFO, "Host: %s, IP: %s", list_iterator->host, list_iterator->ip);
		list_iterator = list_iterator->next;
	}

	/* TODO: Export results to text file */
	list_iterator = result_all;
	write_results(list_iterator);
}


/*
 * Writes results to a file.
 */
void write_results(result *results)
{
	FILE *f;

	f = fopen(params->outputfile, "w");
	if (f != NULL)
	{
		fprintf(f, "Host,IP\n");

		while (results)
		{
			fprintf(f, "%s,%s\n", results->host, results->ip);
			results = results->next;
		}

		fclose(f);
	}
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
	workitem *wi_t1_start, *wi_t2_start, *wi_t3_start, *wi_t4_start, *wi_t5_start;
	int t1_first = 1, t2_first = 1, t3_first = 1, t4_first = 1, t5_first = 1;
	int max_trays = 5, tray = 0;
	int count = 0;

	wi_t1_start = NULL;
	wi_t2_start = NULL;
	wi_t3_start = NULL;
	wi_t4_start = NULL;
	wi_t5_start = NULL;

	while (*wi_list)
	{
		/* Choose tray */
		tray = count % max_trays;

		logline(LOG_DEBUG, "Assigning work item %s to thread %d", (*wi_list)->wi, tray);

		switch (tray)
		{
			case 0:
				if (t1_first)
				{
					*wi_t1 = *wi_list;
					wi_t1_start = *wi_t1;
					t1_first = 0;
				}
				else
				{
					(*wi_t1)->next = *wi_list;
					*wi_t1 = *wi_list;
				}
				break;
			case 1:
				if (t2_first)
				{
					*wi_t2 = *wi_list;
					wi_t2_start = *wi_t2;
					t2_first = 0;
				}
				else
				{
					(*wi_t2)->next = *wi_list;
					*wi_t2 = *wi_list;
				}
				break;

			case 2:
				if (t3_first)
				{
					*wi_t3 = *wi_list;
					wi_t3_start = *wi_t3;
					t3_first = 0;
				}
				else
				{
					(*wi_t3)->next = *wi_list;
					*wi_t3 = *wi_list;
				}
				break;

			case 3:
				if (t4_first)
				{
					*wi_t4 = *wi_list;
					wi_t4_start = *wi_t4;
					t4_first = 0;
				}
				else
				{
					(*wi_t4)->next = *wi_list;
					*wi_t4 = *wi_list;
				}
				break;

			case 4:
				if (t5_first)
				{
					*wi_t5 = *wi_list;
					wi_t5_start = *wi_t5;
					t5_first = 0;
				}
				else
				{
					(*wi_t5)->next = *wi_list;
					*wi_t5 = *wi_list;
				}
				break;
		}

		/* Load next item in list */
		*wi_list = (*wi_list)->next;
		count++;
	}

	/* Terminate linked lists */
	(*wi_t1)->next = NULL;
	(*wi_t2)->next = NULL;
	(*wi_t3)->next = NULL;
	(*wi_t4)->next = NULL;
	(*wi_t5)->next = NULL;

	/* Return pointers to start of lists */
	*wi_t1 = wi_t1_start;
	*wi_t2 = wi_t2_start;
	*wi_t3 = wi_t3_start;
	*wi_t4 = wi_t4_start;
	*wi_t5 = wi_t5_start;
}


/*
 * Counts workitems in list.
 */
int count_workitems(workitem *wi_list)
{
	int count = 0;

	while (wi_list)
	{
		count++;
		wi_list = wi_list->next;
	}

	return count;
}



/*
 * Process a list of workitems.
 */
void *proc_workitems(void *arg)
{
	workitem *wi_list;

	/* Cast input param to thread_params struct */
	thread_params* t_params = (thread_params *)arg;

	wi_list = t_params->wi_list;

	logline(LOG_DEBUG, "Thread %d: Input params: reverse = %d, server = %s", t_params->thread_id, t_params->reverse, t_params->server);   

	while (wi_list)
	{
		logline(LOG_DEBUG, "Thread %d: Processing workitem %s", t_params->thread_id, wi_list->wi);
		if (t_params->reverse)
		{
			do_reverse_dns_lookup(params->servers[0], wi_list->wi, &(t_params->result_list));
		}
		else
		{
			do_forward_dns_lookup(params->servers[0], wi_list->wi, t_params->result_list);
		}

		wi_list = wi_list->next;
	}

	/* Temporary print list output */
	//while (t_params->result_list)
	//{
	//	printf("Result: Host: %s IP: %s\n", t_params->result_list->host, t_params->result_list->ip);
	//	t_params->result_list = t_params->result_list->next;
	//}
}


/*
 * Perform a forward DNS lookup
 */
void do_forward_dns_lookup(char *server, char *host, result *result_list)
{
	result *result_list_entry = NULL;
	result *result_list_start = NULL;
	int found = 0;
	int first = 1;
	int i;
	char *ip_addr[20];

	/* Save start point of linked list */
	result_list_start = result_list;

	/* Initialize array of ip adresses */
	for (i = 0; i < 20; i++) { ip_addr[i] = '\0'; }

	dns_query_a_record(server, host, ip_addr);

	/* Go to last entry in result_list */
	while (result_list)
	{
		result_list = result_list->next;
	}

	/* List ip addresses */
	for (i = 0; i < 20; i++)
	{
		if (ip_addr[i] != '\0')
		{
			/* Add new entry to list */
			result_list_entry = (result *)malloc(sizeof(result));
			result_list_entry->host = host;
			result_list_entry->ip = ip_addr[i];
			result_list->next = result_list_entry;
		}
	}
	/* Set correct pointer to start of linked list */
	result_list = result_list_start;
}

// Perform a reverse dns lookup
void do_reverse_dns_lookup(char *server, char *ip, result **result_list)
{
	result *list_orig_startaddr = *result_list;
	result *list_head = NULL;
	result *list_entry = NULL;
	result *list_start = NULL;
	int i = 0;
	char *domains[20];

	/* Initialize array of domains */
	for (i = 0; i < 20; i++) { domains[i] = '\0'; }

	dns_query_ptr_record(server, ip, domains);

	/* Add found domains to a local linked list */
	for (i = 0; i < 20; i++)
	{
		/* exit loop as soon as an empty entry appears */
		if (domains[i] == NULL) { break; }

		/* Add new entry to list */	
		list_entry = (result *)malloc(sizeof(result));
		list_entry->host = (char *)malloc(strlen(domains[i]));
		list_entry->ip = (char *)malloc(strlen(ip));
		strcpy(list_entry->host, domains[i]);
		strcpy(list_entry->ip, ip);
		list_entry->next = NULL;
		if (list_head == NULL)
		{
			list_head = list_entry;
			list_start = list_head;
		}
		else
		{			
			list_head->next = list_entry;
			list_head = list_head->next;
		}
	}

	/* Attach local linked list to existing list of results */
	if (*result_list == NULL)
	{
		/* This is the first entry in the list, therefore it is ok
		 * that this remains the start address of the list
		 */
		*result_list = list_start;
	} 
	else
	{
		/* Iterate through end of list and attach */
		while ((*result_list)->next)
		{
			*result_list = (*result_list)->next;
		}

		/* Attach newly generated linked list to the end of 
		 * master linked list 
		 */
		(*result_list)->next = list_start;

		/* Restore beginning of list pointer */
		*result_list = list_orig_startaddr;
	}
}

/*
 * Choose a random server out of server array.
 */
char *get_random_server(void)
{
	int i = 0;
	int rnd = 0;

	/* Count servers available */
	while (params->servers[i] != NULL)
	{
		i++;
	}

	/* Choose a random number between 0 and i-1 */
	rnd = rand() % i;

	return params->servers[rnd];
}




/*
 * Removes newlines \n from the char array.
 */
void chomp(char *s) {
	while(*s && *s != '\n' && *s != '\r') s++;

	*s = 0;
}

/* Display a helpful page */
void display_help_page(void)
{
	printf("APP_NAME APP_VERSION\n");
	printf("Written by André Gasser\n");
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
