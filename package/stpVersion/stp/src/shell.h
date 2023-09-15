#ifndef SHELL_H
#define SHELL_H

int exec_command(const char *command);
int command_output(const char *command, char *value);
char *command_output_dynamic(const char *command);

#endif
