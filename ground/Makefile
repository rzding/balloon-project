CC = cc
CFLAGS = -Wall -Wextra -std=c11 -I../balloon-project-stm32mx/App/Inc

.PHONY: all clean

all: decode_packet

decode_packet: decode_packet.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f decode_packet
