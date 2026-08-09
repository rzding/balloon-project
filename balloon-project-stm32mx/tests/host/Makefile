CC = cc
CFLAGS = -Wall -Wextra -std=c11 -I../../App/Inc
LDFLAGS = -lm

TARGETS = test_imu_scale test_ms5611_crc

.PHONY: all clean

all: $(TARGETS)

test_imu_scale: test_imu_scale.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

test_ms5611_crc: test_ms5611_crc.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)
