#CFLAGS += $(shell pkg-config --cflags json-c)
#LDFLAGS += $(shell pkg-config --libs json-c) -luci
# LDFLAGS +=-ljson-c -luci -lglutil
LDFLAGS += -luci -ljson-c -lmosquitto -lpthread -lubox -lcurl

PROC=acmanage
SRC=$(wildcard *.c)
SRC+=$(wildcard ./modules/*.c)
SRC+=$(wildcard ./ble_dfu_serial_uart/*.c)
OBJS=$(patsubst %.c,%.o,$(wildcard *.c))
OBJS+=$(patsubst %.c,%.o,$(wildcard ./modules/*.c))
OBJS+=$(patsubst %.c,%.o,$(wildcard ./ble_dfu_serial_uart/*.c))
CFLAGS+=-I./modules
CFLAGS+=-I./ble_dfu_serial_uart

all : $(PROC)
	@echo "compile done"

$(PROC): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS) 

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ 

clean:
	rm -fr $(PROC) $(OBJS)
