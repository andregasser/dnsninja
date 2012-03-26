#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include "log.h"

/* static reduces this variables scope to file scope.
 * This variable is global, but only throughout this file.
 */
static int global_loglevel = LOG_INFO;

void logline(int loglevel, const char* format, ...) 
{
	va_list args;
		
	if (loglevel <= global_loglevel)
	{
		char timestr[20];
		char *loginfo;
		struct tm *ptr;
		time_t lt;

		lt = time(NULL);
		ptr = localtime(&lt);
		strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", ptr);

		switch (loglevel)
		{
			case LOG_ERROR: loginfo = "E"; break;
			case LOG_INFO: loginfo = "I"; break;
			case LOG_DEBUG: loginfo = "D"; break;
			default: loginfo = "I"; 
		}

		printf("[%s %s] ", timestr, loginfo);
		va_start(args, format);
		vfprintf(stdout, format, args);
		va_end(args);
		printf("\n");
	}
}

/*
 * This function sets the loglevel of the logger.
 */
void set_loglevel(int loglevel)
{
	if ((loglevel == LOG_ERROR) || (loglevel == LOG_INFO) || (loglevel == LOG_DEBUG))
	{
		global_loglevel = loglevel;
	}
}

