CC = cc
CFLAGS = -Wall -Wextra -std=c11 -I../../App/Inc
LDFLAGS = -lm

TARGETS = test_imu_scale test_ms5611_crc test_ms5611_adc test_ms5611_comp test_max31865_cvd test_gps_rx test_gps_nmea test_lora_frf test_packet_v1

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

test_gps_nmea: test_gps_nmea.c
	$(CC) $(CFLAGS) -o $@ $<

test_lora_frf: test_lora_frf.c
	$(CC) $(CFLAGS) -o $@ $<

test_packet_v1: test_packet_v1.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)
