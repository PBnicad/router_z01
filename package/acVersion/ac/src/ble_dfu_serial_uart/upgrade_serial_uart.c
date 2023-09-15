#include "upgrade_serial_uart.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>  
#include <string.h>
#include <pthread.h>

#include "slip.h"
#include "serial.h"
#include "upgrade_serial.h"

#define SERIAL_LOG_INFO(...) //printf(__VA_ARGS__)

#define BUF_SIZE 1000

static uint8_t slip_buf[500] = {0};
static slip_t m_slip;

int tty_fd = 0;

static bool on_rx_byte_complete(uint8_t * p_data, uint8_t len)
{
    bool ret_code;
	bool error = false;

    ret_code = slip_decode_add_byte(&m_slip, p_data[0]);

    if (ret_code == true)
    {
		error = upgradeSerialUartRecv(m_slip.p_buffer, m_slip.current_index);

        // reset the slip decoding
        m_slip.p_buffer      = slip_buf;
        m_slip.current_index = 0;
        m_slip.state         = SLIP_STATE_DECODING;
    }
	return error;
}


void *serial_read_thread(void *p_context)
{
	(void)p_context;
	SERIAL_LOG_INFO("%s\n", __func__);

	uint8_t buf[BUF_SIZE] = {0};
	bool ret_code = false;
		
	while(1)
	{
		ssize_t r = read(tty_fd, buf, BUF_SIZE);
		if (r < 0) {
			SERIAL_LOG_INFO("Fail to read message from serial COM\n");
			pthread_exit((void *)1);
		} else {
/*			SERIAL_LOG_INFO("\n");
			for(int i=0; i<r; i++) {
				SERIAL_LOG_INFO("%02X ", buf[i]);
			}
			SERIAL_LOG_INFO("\n");*/
		}
		for(int i=0; i<r; i++) {
			ret_code = on_rx_byte_complete(&buf[i], 1);
			if(ret_code) {
				SERIAL_LOG_INFO("uart thread exit.\n");
				pthread_exit((void *)0);
			}
		}
	}

	pthread_exit((void *)0);
}

static void responed_send(uint8_t * p_data, uint32_t length, bool encode)
{
	uint8_t rsp_buf[1024] = {0};
    uint32_t slip_len;
	if(encode) {
		(void) slip_encode(rsp_buf, (uint8_t *)p_data, length, &slip_len);
		//SERIAL_LOG_INFO("encode size %d\n", slip_len);
	} else {
		memcpy(rsp_buf, p_data, length);
		slip_len = length;
	}
    

	int ret = write(tty_fd, rsp_buf, slip_len);
	if(ret<0) {
		SERIAL_LOG_INFO("write cmd error\n");
	}
}

void serial_buffer_reset(void)
{
    m_slip.p_buffer      = slip_buf;
    m_slip.current_index = 0;
    m_slip.buffer_len    = 100;
    m_slip.state         = SLIP_STATE_DECODING;
}


void upgradeSerialUartInit(char *serial)
{
	
	tty_fd = open_tty(serial, 115200, false);
	if (tty_fd < 0) {
		SERIAL_LOG_INFO("open tty fail\n");
		exit(EXIT_FAILURE);
	}
	
	responedFuncRegsiter(responed_send);
	upgradeSerialResetFuncRegsiter(serial_buffer_reset);
	
    m_slip.p_buffer      = slip_buf;
    m_slip.current_index = 0;
    m_slip.buffer_len    = 100;
    m_slip.state         = SLIP_STATE_DECODING;
	
	pthread_t ptd_id;
	uint8_t err = pthread_create(&ptd_id, NULL, serial_read_thread, NULL);
	SERIAL_LOG_INFO("thread uart create\n");
	if (0 != err) {
		SERIAL_LOG_INFO("can't create thread: %s\n", strerror(err));
	}
	pthread_detach(ptd_id);
	
	
}


void upgradeSerialUartUnInit(void)
{
	close_tty(tty_fd);
}










