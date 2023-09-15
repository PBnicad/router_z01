#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libubox/ulog.h>

#include "serial.h"

/**
 * Default value
 *   baudrate: 115200
 *   databits: 8
 *   stopbits: 1
 *   parity: None
 */
int set_parity(int fd, int baudrate, int databits, int stopbits, const char *parity)
{
	int i = 0;

	int baudrate_name[] = { 110, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 };
	int baudrate_value[] = { B110, B300, B600, B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200};

	struct termios options = {0};

	if (tcgetattr(fd, &options) != 0) {
		ULOG_ERR("UART device exception, fail to get termio attributes\n");
		return -1;
	}

	/* Flushes the input and/or output queue */
	tcflush (fd, TCIFLUSH);

	/* Default baudrate */
	cfsetispeed(&options, B115200);
	cfsetospeed(&options, B115200);

	/* Setting baudrate */
	for(i = 0; i < sizeof(baudrate_name)/sizeof(baudrate_name[0]); i++)
	{
		if(baudrate == baudrate_name[i]) {
			cfsetispeed(&options, baudrate_value[i]);
			cfsetospeed(&options, baudrate_value[i]);
		}
	}
	
	/*
    Raw mode
        cfmakeraw() sets the terminal to something like the "raw" mode of the old Version 7 terminal driver: input is available character by character, echoing is disabled, and all  special
        processing of terminal input and output characters is disabled.  The terminal attributes are set as follows:

            termios_p->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                            | INLCR | IGNCR | ICRNL | IXON);
            termios_p->c_oflag &= ~OPOST;
            termios_p->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
            termios_p->c_cflag &= ~(CSIZE | PARENB);
            termios_p->c_cflag |= CS8;
	*/

	options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	options.c_oflag &= ~OPOST;
	options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	options.c_cflag &= ~(CSIZE | PARENB);
	/* options.c_cflag |= CS8; */

	options.c_cflag &= ~CSIZE;

	switch (databits) {
		case 5:
			options.c_cflag |= CS5;
			break;
		case 6:
			options.c_cflag |= CS6;
			break;
		case 7:
			options.c_cflag |= CS7;
			break;
		default:
			options.c_cflag |= CS8;
			break;
	}

	/**
	 * PARENB: Enable/Disable parity bit
	 * INPCK:  Should enable input parity checking when you have enabled parity 
	 *         in the c_cflag member (PARENB)
	 * ISTRIP: Strip parity bits
	 * PARODD: =1: use odd parity, =0: use even parity
	   CMSPAR: (not in POSIX) Use "stick" (mark/space) parity (supported on
	           certain serial devices): if PARODD is set, the parity bit is
	           always 1; if PARODD is not set, then the parity bit is always
	           0.
	 */
	/* None, Odd, Even parity setting supported, Mark, Space not setting supported */
	if (!strcasecmp(parity, "odd")) {
		options.c_cflag |= PARENB;
		options.c_cflag |= PARODD;
		/* options.c_iflag |= (INPCK | ISTRIP); */
	} else if (!strcasecmp(parity, "even")) {
		options.c_cflag |= PARENB;
		options.c_cflag &= ~PARODD;
		/* options.c_iflag |= (INPCK | ISTRIP); */
	} else {
		options.c_cflag &= ~PARENB;
		/* options.c_iflag &= ~INPCK; */
	}

	switch (stopbits) {
		case 2:
			options.c_cflag |= CSTOPB;
			break;
		default:
			options.c_cflag &= ~CSTOPB;
			break;
	}

	/** 
	 * Timeouts are ignored in canonical input mode or when the NDELAY option is
	 * set on the file via open or fcntl.
	 * 
	 * VTIME is set to 0 by default, it means reads will block (wait) indefinitely
	 * unless the NDELAY option is set on the port with open or fcntl.
	 */
	/*
	   options.c_cc[VTIME] = 0;
	   options.c_cc[VMIN] = 0;
	*/

	if (tcsetattr(fd, TCSANOW, &options) != 0) {
		ULOG_ERR("UART device exception, fail to set termio attributes\n");
		return -1;
	}

	return 0;
}

int open_tty(const char *dev, int baudrate, int databits, int stopbits, const char *parity)
{
	int i = 0;
	int fd = 0;

	if (!dev) {
		ULOG_ERR("UART device not specified\n");
		return -1;
	}

	/* Open serial port */
	fd = open(dev, O_RDWR | O_NOCTTY);
	if (fd < 0 ) {
		ULOG_ERR("Fail to open %s\n", dev);
		return -1;
	}

	if (set_parity(fd, baudrate, databits, stopbits, parity) < 0) {
		return -1;
	}

	return fd;
}

void close_tty(int fd)
{
	close(fd);
}

