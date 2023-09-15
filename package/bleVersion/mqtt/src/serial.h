#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>

int set_parity(int fd, int databits, int stopbits, int parity, bool nonblocking);
int open_tty(const char *dev, int speed, bool nonblocking);
void close_tty(int fd);

#endif
