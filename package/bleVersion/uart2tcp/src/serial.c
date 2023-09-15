#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libubox/ulog.h>

#include "serial.h"

int set_parity(int fd, int databits, int stopbits, int parity, bool nonblocking)
{
	struct termios options;

	if (tcgetattr(fd, &options) != 0) {
		ULOG_ERR("Fail to get terminal attributes\n");
		return -1;
	}

	options.c_cflag &= ~CSIZE;
	options.c_iflag &= ~(ICRNL | IXON); /* Fix cannot receive 0x11 0x0d 0x13 bug */
	options.c_lflag  &= ~(ICANON | ECHO | ECHOE | ISIG); /* Input: Disable canonical mode */
	options.c_oflag  &= ~OPOST; /* Enable implementation-defined output processing */
	switch (databits) {
		case 7:
			options.c_cflag |= CS7;
			break;
		case 8:
			options.c_cflag |= CS8;
			break;
		default:
			ULOG_ERR("Specified databits %d unsupport\n", databits);
			return -1;
	}

	switch (parity) {
		case 'n':
		case 'N':
			options.c_cflag &= ~PARENB;   /* Clear parity enable */
			options.c_iflag &= ~INPCK;    /* Enable parity checking */
			break;
		case 'o':
		case 'O':
			options.c_cflag |= (PARODD | PARENB);
			options.c_iflag |= INPCK;     /* Disnable parity checking */
			break;
		case 'e':
		case 'E':
			options.c_cflag |= PARENB;	  /* Enable parity */
			options.c_cflag &= ~PARODD;
			options.c_iflag |= INPCK;     /* Disnable parity checking */
			break;
		case 'S':
		case 's':
			options.c_cflag &= ~PARENB;
			options.c_cflag &= ~CSTOPB;
			break;
		default:
			ULOG_ERR("Specified parity %c unsupport\n", parity);
			return -1;
	}

	switch (stopbits) {
		case 1:
			options.c_cflag &= ~CSTOPB;
			break;
		case 2:
			options.c_cflag |= CSTOPB;
			break;
		default:
			ULOG_ERR("Specified stopbits %d unsupport\n", stopbits);
			return -1;
	}

	/* Set input parity option */
	if (parity != 'n')
		options.c_iflag |= INPCK;

	tcflush (fd, TCIFLUSH);
	options.c_cc[VTIME] = 0;
	options.c_cc[VMIN] = nonblocking ? 0 : 1;     /* Update the options and do it NOW */

	if (tcsetattr(fd, TCSANOW, &options) != 0) {
		ULOG_ERR("Fail to set terminal attributes\n");
		return -1;
	}

	return 0;
}

int open_tty(const char *dev, int speed, bool nonblocking)
{
	int i = 0;
	int fd = 0;
	struct termios tty;

	int speed_arr[] ={ B115200,B38400, B19200, B9600, B4800, B2400, B1200, B300, B115200,B38400, B19200, B9600,B4800, B2400, B1200, B300 };
	int name_arr[] ={ 115200,38400, 19200, 9600, 4800, 2400, 1200, 300,115200, 38400, 19200, 9600, 4800, 2400,1200, 300 };

	if (!dev) {
		ULOG_ERR("Not tty device specified\n");
		return -1;
	}

	/* Open serial port */
	fd = open(dev, O_RDWR | O_NOCTTY);
	if (fd < 0 ) {
		ULOG_ERR("Fail to open %s\n", dev);
		return -1;
	}

	memset(&tty, 0, sizeof(tty));
	if (tcgetattr(fd, &tty) != 0) {
		ULOG_ERR("Fail to call tcgetattr\n");
		close(fd);
		return -1;
	}

	/* Set baudrate */
	for(i = 0; i < sizeof(speed_arr)/sizeof(int); i++)
	{
		if(speed == name_arr[i]) {
			tcflush(fd,TCIOFLUSH);	
			cfsetispeed(&tty, speed_arr[i]);
			cfsetospeed(&tty, speed_arr[i]);
			if (tcsetattr(fd, TCSANOW, &tty) != 0) {
				ULOG_ERR("Fail to set baudrate\n");
				close(fd);
				return -1;
			}
			tcflush(fd,TCIOFLUSH);
		}
	}

	if(set_parity(fd, 8, 1, 'N', false) < 0 ) {
		ULOG_ERR("Fail to set parity\n");
		close(fd);
		return -1;
	}

	return fd;
}

void close_tty(int fd)
{
	close(fd);
}

