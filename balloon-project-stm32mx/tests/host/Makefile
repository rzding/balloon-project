CC = cc
CFLAGS = -Wall -Wextra -std=c11 -I../../App/Inc
LDFLAGS = -lm

TARGETS = test_imu_scale test_ms5611_crc test_ms5611_adc test_ms5611_comp test_max31865_cvd test_gps_rx

.PHONY: all clean

all: $(TARGETS)

test_imu_scale: test_imu_scale.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

test_ms5611_crc: test_ms5611_crc.c
	$(CC) $(CFLAGS) -o $@ $<

test_ms5611_adc: test_ms5611_adc.c
	$(CC) $(CFLAGS) -o $@ $<

test_ms5611_comp: test_ms5611_comp.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

test_max31865_cvd: test_max31865_cvd.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

test_gps_rx: test_gps_rx.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)
