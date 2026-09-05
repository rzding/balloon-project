CC = cc
CFLAGS = -Wall -Wextra -std=c11 -I../../App/Inc
LDFLAGS = -lm

TARGETS = test_imu_scale test_ms5611_crc test_ms5611_adc test_ms5611_comp test_max31865_cvd test_gps_rx test_gps_nmea test_lora_frf test_packet_v1 test_sdlog_name test_mission_sm test_schedule test_packetizer test_aprs_at

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

test_sdlog_name: test_sdlog_name.c
	$(CC) $(CFLAGS) -o $@ $<

test_mission_sm: test_mission_sm.c ../../App/Src/mission.c
	$(CC) $(CFLAGS) -o $@ test_mission_sm.c ../../App/Src/mission.c $(LDFLAGS)

test_schedule: test_schedule.c ../../App/Src/schedule.c
	$(CC) $(CFLAGS) -o $@ test_schedule.c ../../App/Src/schedule.c

test_packetizer: test_packetizer.c ../../App/Src/packetizer.c
	$(CC) $(CFLAGS) -o $@ test_packetizer.c ../../App/Src/packetizer.c

test_aprs_at: test_aprs_at.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)
