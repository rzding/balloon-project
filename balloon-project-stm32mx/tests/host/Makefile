CC = cc
CFLAGS = -Wall -Wextra -std=c11 -I../../App/Inc
LDFLAGS = -lm

TARGET = test_imu_scale

.PHONY: all clean

all: $(TARGET)

$(TARGET): test_imu_scale.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)
