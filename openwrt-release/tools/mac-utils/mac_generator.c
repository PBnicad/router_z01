#include <stdio.h>

int main(int argc, char **argv)
{
	int rc = 0;
	int i = 0;
	unsigned long long int n = 0xf0000;

	/*
	 * 8C:D4:95:00:00:00
	 */
	/* unsigned short mac_h = 0, mac_m = 0, mac_l = 0; */
	unsigned short mac_h = 0x8cd4, mac_m = 0x9500, mac_l = 0;

	for (i = 0; i < n; i++) {
		if (mac_l++ == 0xffff) {
			mac_l = 0;

			if (mac_m++ == 0xffff) {
				return 0;
			}
		}

		printf("%02X:%02X:%02X:%02X:%02X:%02X\n", \
				mac_h >> 8 & 0xff, \
				mac_h & 0xff, \
				mac_m >> 8 & 0xff, \
				mac_m & 0xff, \
				mac_l >> 8 & 0xff, \
				mac_l & 0xff);
	}

	return rc;
}
