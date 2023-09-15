#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>

int open_tty(const char *dev, int baudrate, int databits, int stopbits, const char *parity);
void close_tty(int fd);

#endif
