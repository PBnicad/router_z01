#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"

int exec_command(const char *command)
{
	FILE *fp;

	fp = popen(command, "r");
	if (fp == NULL ) {
		return -1;
	}
		
	pclose(fp);

	return 0;
}


int exec_logstring_command(const char *string)
{
	char cmd[1024];
	memset(cmd, 0, 0);
	sprintf(cmd, "logger %s", string);
	exec_command(cmd);
	return 0;
}

int command_output(const char *command, char *value)
{
	int rc = 0;
	char buf[1024] = {0};
	FILE *fp = NULL;

	if (command == NULL) {
		return -1;
	}

	fp = popen(command, "r");
	if (fp == NULL) {
		return -1;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strcat(value, buf);
	}

	pclose(fp);

	return 0;
}

char *command_output_dynamic(const char *command)
{
	FILE *fp = NULL;
	char *value = NULL;
	char *value_tmp = NULL;
	char buf[1024] = {0};
	size_t count = 0;
	size_t size = 0;
	int rc = 0;

	if (command == NULL) {
		return NULL;
	}

	fp = popen(command, "r");
	if (fp == NULL) {
		return NULL;
	}

	while ((count = fread(buf, 1, sizeof(buf), fp)) != 0 ) {
		/* Point to value's address */
		value_tmp = value;
		value = (char *)malloc(size+count);

		/* Copy the old text */
		memcpy(value, value_tmp, size);
		/* Append the read text */
		memcpy(value+size, buf, count);

		if (value_tmp) {
			/* Free the last one */
			free(value_tmp);
		}

		size += count;
	}

	pclose(fp);

	return value;
}
