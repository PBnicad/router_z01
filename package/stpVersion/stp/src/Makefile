#CFLAGS += $(shell pkg-config --cflags json-c)
#LDFLAGS += $(shell pkg-config --libs json-c) -luci
# LDFLAGS +=-ljson-c -luci -lglutil
LDFLAGS += -luci -lubox -lcurl

PROC=stp
SRC=$(wildcard *.c)
OBJS=$(patsubst %.c,%.o,$(wildcard *.c))

all : $(PROC)
	@echo "compile done"

$(PROC): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ 

clean:
	rm -fr $(PROC) $(OBJS)
