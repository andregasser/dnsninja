/******************************************************************************
 *    Copyright 2012 André Gasser
 *
 *    This file is part of DNSNINJA.
 *
 *    DNSNINJA is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    DNSNINJA is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with DNSNINJA.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <regex.h>
#include "dns.h"
#include "log.h"

/* Define some constants */
#define APP_NAME        "DNSNINJA" /* Name of applicaton */
#define APP_VERSION     "0.1.1"    /* Version of application */

/* Used to store command-line args */
typedef struct 
{
	char *servers[5];
	char *domain;
	char *inputfile;
	char *outputfile;
	int reverse;
	int help;
	int loglevel;
	int version;
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
int check_input_file_host(void);
int check_input_file_ip(void);
int do_forward_dns_lookup(char *server, char *host, result **result_list);
int do_reverse_dns_lookup(char *server, char *ip, result **result_list);
void chomp(char *s);
void display_help_page(void);
void display_version_info(void);
void load_workitems(char *inputfile, workitem **wi_start, int reverse);
void dist_workitems(workitem **wi_list, workitem **wi_t1, workitem **wi_t2,
		workitem **wi_t3, workitem **wi_t4, workitem **wi_t5);
int count_workitems(workitem *wi_list);
void *proc_workitems(void *arg);
void write_results(result *results);
int get_servers_count(void);
char *get_random_server(void);
void show_gnu_banner(void);

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

	/* Parse commandline args */
	ret = parse_cmd_args(&argc, argv);
	if (params->help)
	{
		display_help_page();
		return 0;
	}
	if (params->version)
	{
		display_version_info();
		return 0;
	}
	if (ret < 0)
	{
		switch (ret)
		{
			case -1:
				logline(LOG_ERROR, "Error: No server specified (use option -s)"); 	
				break;
			case -2:
				logline(LOG_ERROR, "Error: Input file must be specified (use option -i)");
				break;
			case -3:
				logline(LOG_ERROR, "Error: Servers could not be interpreted (use option -s)"); 	
				break;
			case -4:
				logline(LOG_ERROR, "Error: Invalid log level option specified (use option -l).");
				break;
			case -5:
				logline(LOG_ERROR, "Error: No domain specified (use -d option).");
				break;
			default:
				logline(LOG_ERROR, "Error: An unknown error occurred during parsing of command line args.");
		}
					
		logline(LOG_ERROR, "Use the -h option if you need help.");
		return ret;
	}

	/* Set log level */
	switch (params->loglevel)
	{
		case 1: set_loglevel(LOG_ERROR); break;
		case 2: set_loglevel(LOG_INFO); break;
		case 3: set_loglevel(LOG_DEBUG); break;
		default: set_loglevel(LOG_ERROR);
	}

	show_gnu_banner();
	printf("Executing %s Version %s\n", APP_NAME, APP_VERSION);
	printf("\n");
	
	/* Display help page if desired. Otherwise, do DNS lookups */
	ret = do_dns_lookups();
	if (ret < 0)
	{
		logline(LOG_ERROR, "Error: Could not perform DNS lookups. Errorcode: %d", ret);
	}

	/* Free memory on heap */
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
	int param_server_err = 0;
	int param_loglevel_err = 0;

	/* Init struct */
	params->reverse = 0;
	params->help = 0;

	for (i = 0; i < 5; i++)
		params->servers[i] = NULL;

	params->domain = NULL;
	params->inputfile = NULL;
	params->outputfile = NULL;
	params->loglevel = LOG_INFO;
	params->version = 0;

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
			{ "version",	no_argument,       0, 'v' },
			{ "loglevel",	required_argument, 0, 'l' },
			{ 0, 0, 0, 0 }
		};

		/* getopt_long stores the option index here */
		int option_index = 0;
		int c;

		c = getopt_long(*argc, argv, "rs:d:i:o:hvl:", long_options, &option_index);

		/* Detect the end of the options */
		if (c == -1)
			break;

		switch (c)
		{
			case 'r':
				params->reverse = 1;						
				break;
			case 's':
				/* Process server parameters */
				ret = parse_server_cmd_arg(optarg, params->servers);
				if (ret != 0) { param_server_err = 1; }
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
				params->version = 1;
				break;
			case 'l':
				params->loglevel = atoi(optarg);
	            if ((params->loglevel < 1) || (params->loglevel > 3))
	            	param_loglevel_err = 1;
				break;
		}
	}

	/* Check param dependencies */
	if (get_servers_count() == 0) { return -1; }
	if (params->inputfile == NULL) { return -2; }
	if (param_server_err == 1) { return -3; }
	if (param_loglevel_err == 1) { return -4; }
	if (params->reverse == 0)
	{
		/* Additional parameter checks when doing forward lookup requests */
		if (params->domain == NULL) { return -5; }
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
			/* Store server ip */
			servers[i] = malloc(strlen(ptr));
			strcpy(servers[i], ptr);
			i++;

			/* Get next token */
			ptr = strtok(NULL, ",");
		}
	
		return 0;
	}
	else
	{
		return -1;
	}
}


/*
 * Main function for doing DNS lookups.
 */
int do_dns_lookups(void)
{
	int i = 0;
	int ret = 0;
	int hostcount = 0;
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
		logline(LOG_INFO, "    Using domain      : %s", params->domain);
	if (params->inputfile)
		logline(LOG_INFO, "    Using input file  : %s", params->inputfile);
	if (params->outputfile)
		logline(LOG_INFO, "    Using output file : %s", params->outputfile);
	if (params->reverse)
		logline(LOG_INFO, "    DNS lookup mode   : Reverse");
	else
		logline(LOG_INFO, "    DNS lookup mode   : Forward");

	switch (params->loglevel)
	{
		case 1: logline(LOG_INFO, "    Log Level         : Error"); break;
		case 2: logline(LOG_INFO, "    Log Level         : Info"); break;
		case 3: logline(LOG_INFO, "    Log Level         : Debug"); break;
		default: logline(LOG_INFO, "    Log Level         : Error");
	}

	/* Check work items prior to processing */
	logline(LOG_INFO, "Checking input file...");
	if (params->reverse)
	{
		/* File must contain a bunch of ip addresses */
		ret = check_input_file_ip();
	}
	else
	{
		/* File must contain host names */
		ret = check_input_file_host();
	}
	
	switch (ret)
	{
		case 0:
			logline(LOG_INFO, "Input file check successful. Data seems to be in required format.");		
			break;
		case -1:
		case -2:
			logline(LOG_INFO, "There is a problem with the input file format. Aborting.");		
			break;
		case -3:
			logline(LOG_INFO, "The input file could not be opened. Are you sure the file exists?");		
			break;
		default:
			logline(LOG_INFO, "An unhandled error related to the input file occured.");		
	}
	
	if (ret < 0)
		return -1;
	
	/* Load work items into linked list */
	logline(LOG_INFO, "Processing starts now, stay tuned...");
	logline(LOG_DEBUG, "    Loading work items...");
	load_workitems(params->inputfile, &wi_start, params->reverse);	
	logline(LOG_DEBUG, "    %d work items loaded from file", count_workitems(wi_start));

	/* Distribute workitems among threads */
	logline(LOG_DEBUG, "    Distributing work items among threads...");
	dist_workitems(&wi_start, &wi_t1, &wi_t2, &wi_t3, &wi_t4, &wi_t5);
	logline(LOG_DEBUG, "    Work items distributed");
	logline(LOG_DEBUG, "    Thread 1: Work item count = %d", count_workitems(wi_t1));
	logline(LOG_DEBUG, "    Thread 2: Work item count = %d", count_workitems(wi_t2));
	logline(LOG_DEBUG, "    Thread 3: Work item count = %d", count_workitems(wi_t3));
	logline(LOG_DEBUG, "    Thread 4: Work item count = %d", count_workitems(wi_t4));
	logline(LOG_DEBUG, "    Thread 5: Work item count = %d", count_workitems(wi_t5));

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
		logline(LOG_ERROR, "    Thread 1: Could not be created");
		return -1;
	}
	else
	{
		logline(LOG_DEBUG, "    Thread 1: Created");
	}

	if (pthread_create(&t2,	NULL, proc_workitems, &t2_params))
	{
		logline(LOG_ERROR, "    Thread 2: Could not be created");
		return -1;
	}
	else
	{
		logline(LOG_DEBUG, "    Thread 2: Created");
	}

	if (pthread_create(&t3,	NULL, proc_workitems, &t3_params))
	{
		logline(LOG_ERROR, "    Thread 3: Could not be created");
		return -1;
	}
	else
	{
		logline(LOG_DEBUG, "    Thread 3: Created");
	}

	if (pthread_create(&t4,	NULL, proc_workitems, &t4_params))
	{
		logline(LOG_ERROR, "    Thread 4: Could not be created");
		return -1;
	}
	else
	{
		logline(LOG_DEBUG, "    Thread 4: Created");
	}

	if (pthread_create(&t5,	NULL, proc_workitems, &t5_params))
	{
		logline(LOG_ERROR, "    Thread 5: Could not be created");
		return -1;
	}
	else
	{
		logline(LOG_DEBUG, "    Thread 5: Created");
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
		logline(LOG_ERROR, "    Thread 1: An error occurred");
	}
	else
	{
		/* Extract results from thread 1 */
		logline(LOG_DEBUG, "    Thread 1: Finished successfully");
	}

	if ((int *)t2_status < 0)
	{
		logline(LOG_ERROR, "    Thread 2: An error occurred");
	}
	else
	{
		/* Extract results from thread 2 */
		logline(LOG_DEBUG, "    Thread 2: Finished successfully");
	}

	if ((int *)t3_status < 0)
	{
		logline(LOG_ERROR, "    Thread 3: An error occurred");
	}
	else
	{
		/* Extract results from thread 3 */
		logline(LOG_DEBUG, "    Thread 3: Finished successfully");
	}

	if ((int *)t4_status < 0)
	{
		logline(LOG_ERROR, "    Thread 4: An error occurred");
	}
	else
	{
		/* Extract results from thread 4 */
		logline(LOG_DEBUG, "    Thread 4: Finished successfully");
	}

	if ((int *)t5_status < 0)
	{
		logline(LOG_ERROR, "    Thread 5: An error occurred");
	}
	else
	{
		/* Extract results from thread 5 */
		logline(LOG_DEBUG, "    Thread 5: Finished successfully");
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

	logline(LOG_INFO, "Finished processing data.");
	logline(LOG_INFO, "The following hosts have been identified:");

	/* Print results on screen */
	list_iterator = result_all;
	while (list_iterator)
	{
		hostcount++;
		logline(LOG_INFO, "    Host: %s, IP: %s", list_iterator->host, list_iterator->ip);
		list_iterator = list_iterator->next;
	}
	if (hostcount == 0)
	{
		logline(LOG_INFO, "    Unfortunately, no hosts have been found. Try again using different settings.");
	} 
	else
	{	
		logline(LOG_INFO, "    %d hosts found.", hostcount);
	}
	
	
	/* Export results to text file */
	if ((params->outputfile != NULL) && (hostcount > 0))
	{
		logline(LOG_INFO, "Exporting data to %s...", params->outputfile);
		list_iterator = result_all;
		write_results(list_iterator);
		logline(LOG_INFO, "Export finished.");
	}

	logline(LOG_INFO, "Thank you for flying with us!");
}

/*
 * Checks the input file if it contains valid hostnames.
 */
int check_input_file_host(void)
{
	int error = 0;
	FILE *f;
	int ret = 0;
	regex_t regex_host;
	char line[256];

	/* Compile regex pattern */
	if ((ret = regcomp(&regex_host, "^[0-9a-zA-Z-]{3,100}$", REG_EXTENDED)) != 0)
	{
		return -1;    
	}

	/* Check if every line in the input file is a valid host */
	f = fopen(params->inputfile, "r");
	if (f != NULL)
	{
		while ((fgets(line, 255, f) != NULL) && (error == 0))
		{
			chomp(line);
			ret = regexec(&regex_host, line, 0, NULL, 0);
			if (ret == 0)
			{
				logline(LOG_DEBUG, "    Work item %s is valid", line);
			}
			else if (ret == REG_NOMATCH)
			{
				logline(LOG_DEBUG, "    Work item %s has an invalid format", line);
				error = 1;
			}
			else
			{
				logline(LOG_DEBUG, "    Work item %s has an unknown problem. Return code was: %d", line, ret);
				error = 1;
			}	
		}
	
		regfree(&regex_host);
		fclose(f);
		
		if (error == 1)
		{
			return -2;
		}
	}
	else
	{	
		/* File could not be opened */
		return -3;
	}
	
	return 0;
}

/*
 * Checks the input file if it contains valid ip addresses.
 */
int check_input_file_ip(void)
{
	int error = 0;
	FILE *f;
	int ret = 0;
	regex_t regex_ip;
	char line[256];

	/* Compile regex pattern */
	if ((ret = regcomp(&regex_ip, "^[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}$", REG_EXTENDED)) != 0)
	{
		return -1;    
	}

	/* Check if every line in the input file is a valid host */
	f = fopen(params->inputfile, "r");
	if (f != NULL)
	{
		while ((fgets(line, 255, f) != NULL) && (error == 0))
		{
			chomp(line);
			ret = regexec(&regex_ip, line, 0, NULL, 0);
			if (ret == 0)
			{
				logline(LOG_DEBUG, "    Work item %s is valid", line);
			}
			else if (ret == REG_NOMATCH)
			{
				logline(LOG_DEBUG, "    Work item %s has an invalid format", line);
				error = 1;
			}
			else
			{
				logline(LOG_DEBUG, "    Work item %s has an unknown problem. Return code was: %d", line, ret);
				error = 1;
			}	
		}
	
		regfree(&regex_ip);
		fclose(f);
		
		if (error == 1)
		{
			return -2;
		}
	}
	else
	{ 
		/* File could not be opened */
		return -3;
	}
	
	return 0;
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
			memset(curr, 0, sizeof(workitem));
			curr->wi = (char *)malloc(strlen(host) + 1);
			memset(curr->wi, 0, strlen(host) + 1);
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
 * Distribute work items among threads.
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

		logline(LOG_DEBUG, "    Assigning work item %s to thread %d", (*wi_list)->wi, tray);

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
	if (*wi_t1 != NULL) { (*wi_t1)->next = NULL; }
	if (*wi_t2 != NULL) { (*wi_t2)->next = NULL; }
	if (*wi_t3 != NULL) { (*wi_t3)->next = NULL; }
	if (*wi_t4 != NULL) { (*wi_t4)->next = NULL; }
	if (*wi_t5 != NULL) { (*wi_t5)->next = NULL; }

	/* Return pointers to start of lists */
	*wi_t1 = wi_t1_start;
	*wi_t2 = wi_t2_start;
	*wi_t3 = wi_t3_start;
	*wi_t4 = wi_t4_start;
	*wi_t5 = wi_t5_start;
}


/*
 * Counts work items in list.
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
 * Process a list of work items.
 */
void *proc_workitems(void *arg)
{
	int ret = 0;
	workitem *wi_list;

	/* Cast input param to thread_params struct */
	thread_params* t_params = (thread_params *)arg;

	wi_list = t_params->wi_list;

	logline(LOG_DEBUG, "    Thread %d: Input params: reverse = %d, server = %s", t_params->thread_id, t_params->reverse, t_params->server);   

	while (wi_list)
	{
		logline(LOG_DEBUG, "    Thread %d: Processing workitem %s", t_params->thread_id, wi_list->wi);
		if (t_params->reverse)
		{
			ret = do_reverse_dns_lookup(t_params->server, wi_list->wi, &(t_params->result_list));
		}
		else
		{
			ret = do_forward_dns_lookup(t_params->server, wi_list->wi, &(t_params->result_list));
		}
		
		if (ret < 0)
		{
			if (ret == -2)
			{
				logline(LOG_ERROR, "    Thread %d: DNS server temporarily not available. Skipping %s", t_params->thread_id, wi_list->wi);
			}
			else
			{
				logline(LOG_ERROR, "    Thread %d: Error querying DNS server. Error code: %d", t_params->thread_id, ret);
				pthread_exit(&ret);
			}
		}

		wi_list = wi_list->next;
	}
}


/*
 * Perform a forward DNS lookup
 */
int do_forward_dns_lookup(char *server, char *host, result **result_list)
{
	result *list_orig_startaddr = *result_list;
	result *list_head = NULL;
	result *list_entry = NULL;
	result *list_start = NULL;
	int ret = 0;
	int i;
	char *ip_addr[20];

	/* Initialize array of ip adresses */
	for (i = 0; i < 20; i++) { ip_addr[i] = NULL; }

	ret = dns_query_a_record(server, host, ip_addr);
	if (ret != 0)
	{
		if (ret == -4)
		{
			return -2;
		}
		else
		{
			//logline(LOG_INFO, "ERRORCODE dns_query_ptr_record was: %d", ret);
			return -1;
		}
	}	
	//if (ret != 0)
	//{
	//	return -1;
	//}

	/* Add found ips to a local linked list */
	for (i = 0; i < 20; i++)
	{
		/* Exit loop as soon as an empty entry appears */
		if (ip_addr[i] == NULL) { break; }

		/* Add new entry to list */
		list_entry = (result *)malloc(sizeof(result));
		list_entry->host = (char *)malloc(strlen(host));
		list_entry->ip = (char *)malloc(strlen(ip_addr[i]));
		strcpy(list_entry->host, host);
		strcpy(list_entry->ip, ip_addr[i]);
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
	
	return 0;
}


/*
 * Perform a reverse dns lookup
 */
int do_reverse_dns_lookup(char *server, char *ip, result **result_list)
{
	result *list_orig_startaddr = *result_list;
	result *list_head = NULL;
	result *list_entry = NULL;
	result *list_start = NULL;
	int i = 0;
	int ret = 0;
	char *domains[20];

	/* Initialize array of domains */
	for (i = 0; i < 20; i++) { domains[i] = NULL; }

	ret = dns_query_ptr_record(server, ip, domains);
	if (ret != 0)
	{
		if (ret == -4)
		{
			return -2;
		}
		else
		{
			//logline(LOG_INFO, "ERRORCODE dns_query_ptr_record was: %d", ret);
			return -1;
		}
	}

	/* Add found domains to a local linked list */
	for (i = 0; i < 20; i++)
	{
		/* Exit loop as soon as an empty entry appears */
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
	
	return 0;
}


/*
 * Count servers provided by user
 */
int get_servers_count(void)
{
	int i = 0;
	
	/* Count servers available */
	while (params->servers[i] != NULL)
	{
		i++;
	}

	return i;
}


/*
 * Choose a random server out of server array.
 */
char *get_random_server(void)
{
	int i = 0;
	int rnd = 0;

	/* Choose a random number between 0 and i-1 */
	rnd = rand() % get_servers_count();

	return params->servers[rnd];
}


/*
 * Removes newlines \n from the char array.
 */
void chomp(char *s) {
	while(*s && *s != '\n' && *s != '\r') s++;

	*s = 0;
}


/* 
 * Display a helpful page.
 */
void display_help_page(void)
{
	show_gnu_banner();
	printf("\n");
	printf("Syntax:\n");
	printf("-------\n");
	printf("To perform forward DNS lookups the -i parameter must be specified. When reverse\n");
	printf("DNS lookups shall be performed, the -r parameter must be specified additionally.\n\n");
	printf("Forward DNS Lookups:     %s [OPTIONS] -i <file>\n", APP_NAME);
	printf("Reverse DNS lookups:     %s [OPTIONS] -r -i <file>\n", APP_NAME);
	printf("\n");
	printf("Parameters:\n");
	printf("-----------\n");
	printf("%s currently supports the following parameters:\n\n", APP_NAME);
	printf("--reverse, -r                              Do a reverse DNS lookup. If not\n");
	printf("                                           specified, a forward DNS lookup will\n");
	printf("                                           be performed\n");
	printf("--server=<ip1,ip2,...>, -s <ip1,ip2,...>   List of DNS servers, which shall be\n");
	printf("                                           used as targets for DNS queries.\n");
	printf("--domain=<mydomain>, -d <mydomain>         Specify the domain to be queried. Only\n");
	printf("                                           used when doing forward DNS lookups\n");
	printf("                                           (e.g. -d foo.org).\n");
	printf("--inputfile=<inputfile>, -i <inputfile>    The file containing either a list of\n");
	printf("                                           ip addresses or host names, depending\n");
	printf("                                           on the lookup mode (reverse, forward).\n");
	printf("--outputfile=<outputfile>, -o <outputfile> Allows you to write the query results\n");
	printf("                                           into a simple, comma-separated text\n");
	printf("                                           file. This allows you to further process\n");
	printf("                                           the results in other  tools.\n");
	printf("--loglevel=<level>, -l <level>             Specifies the desired log level. The\n");
	printf("                                           following levels are supported:\n");
	printf("                                             1 = ERROR (Log errors only)\n");
	printf("                                             2 = INFO (Log additional information)\n");
	printf("                                             3 = DEBUG (Log debug level information)\n");
	printf("--version, -v                              Displays version information.\n");
	printf("--help, -h                                 Displays this help page.\n");
	printf("\n");
    printf("Bug Reports and Other Comments:\n");
    printf("-------------------------------\n");
    printf("I'm glad to receive comments and bug reports on this tool. You can best reach me\n");
    printf("by email or IM using Jabber:\n\n");
    printf("+ E-Mail:     andre.gasser@gmx.ch\n");
    printf("+ Jabber:     sh0ka@jabber.ccc.de\n");
    printf("\n");
    printf("Source Code:\n");
    printf("------------\n");
    printf("Fetch the latest version from:\n");
    printf("+ GitHub:     https://github.com/shoka/dnsninja\n\n");
}


/* 
 * Display version information.
 */
void display_version_info(void)
{
	printf("%s %s\n", APP_NAME, APP_VERSION);
	printf("Copyright (C) 2012 André Gasser\n");
	printf("License GPLv3: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n");
	printf("This is free software: you are free to change and redistribute it.\n");
	printf("There is NO WARRANTY, to the extent permitted by law.\n");
}


/*
 * Display a short GNU banner.
 */
void show_gnu_banner(void)
{
	printf("%s %s  Copyright (C) 2012  André Gasser\n", APP_NAME, APP_VERSION);
    printf("This program comes with ABSOLUTELY NO WARRANTY.\n");
    printf("This is free software, and you are welcome to redistribute it\n");
    printf("under certain conditions.\n");
}
