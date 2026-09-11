/*
 * SSI Balloon — ESP32 ground station receiver
 *
 * Receives three packet types from the flight computer's RFM95W and prints
 * decoded fields over USB serial at 115200. Packets are told apart by the type
 * byte at offset 0:
 *   0x01  v1 telemetry  (28 B: GPS, baro, temp, flags, sats)  — packet.h
 *   0x02  IMU           (22 B: accel + gyro)                   — telem_imu.h
 *   0x10  image header  (16 B: id, total_len, chunks, w, h)    — telem_img.h
 *   0x11  image chunk   (5 + data + 2 B: JPEG bytes)           — telem_img.h
 *
 * A JPEG arrives as one 0x10 header then N 0x11 chunks. When every chunk is in,
 * the whole image is printed once as base64 between markers; feed that to
 * ground/save_image.py to write a .jpg. LoRa is slow, so a frame takes ~10-20 s.
 *
 * Hardware + wiring: ground/esp32_receiver/README.md and ground/README.md.
 * Library: "LoRa" by Sandeep Mistry. Do NOT use RadioHead (adds a 4-byte header).
 * Every modem parameter below must match balloon-project-stm32mx/App/Inc/lora.h.
 */

#include <SPI.h>
#include <LoRa.h>

/* ---- Wiring: match how you wired the breakout (see README) ---- */
#define PIN_SCK    18
#define PIN_MISO   19
#define PIN_MOSI   15
#define PIN_CS      5
#define PIN_RST     4
#define PIN_DIO0   -1   /* breakout "G0" — not needed when polling */

/* ---- Modem settings: must match flight lora.h ---- */
#define LORA_FREQ_HZ      915000000L
#define LORA_SF                    8
#define LORA_BW_HZ            125000
#define LORA_CR_DENOM              5
#define LORA_SYNC_WORD          0x12
#define LORA_PREAMBLE_LEN          8

/* ---- Packet types (flight side: packet.h, telem_imu.h, telem_img.h) ---- */
#define PACKET_V1_VERSION       0x01
#define PACKET_V1_LEN             28
#define PACKET_V1_CRC_LEN         26
#define PACKET_V1_BATT_NA     0xFFFF

#define TELEM_IMU_TYPE          0x02
#define TELEM_IMU_LEN             22

#define TELEM_IMG_HDR_TYPE      0x10
#define TELEM_IMG_HDR_LEN         16
#define TELEM_IMG_CHUNK_TYPE    0x11

#define IMU_ACCEL_LSB_PER_G   2048.0f
#define IMU_GYRO_LSB_PER_DPS    16.4f

/* Ground reassembly limits (>= flight APP_IMG_BUF_MAX and chunking). */
#define GND_IMG_MAX            24576
#define GND_MAX_CHUNKS           256

typedef struct {
  uint8_t  version, mission_state;
  uint16_t seq;
  uint32_t time_ms;
  int32_t  lat_e7, lon_e7;
  uint16_t gps_alt_m;
  int16_t  baro_alt_m, temp_c_x100;
  uint16_t batt;
  uint8_t  flags, sats;
} packet_v1_t;

/* Big-endian readers — wire format is MSB first. */
static uint16_t be_u16(const uint8_t *b){ return ((uint16_t)b[0]<<8)|b[1]; }
static uint32_t be_u32(const uint8_t *b){ return ((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3]; }
static int16_t  be_i16(const uint8_t *b){ return (int16_t)be_u16(b); }
static int32_t  be_i32(const uint8_t *b){ return (int32_t)be_u32(b); }

/* CRC-16/CCITT-FALSE, mirrors packet_crc16() in packet.h. */
static uint16_t packet_crc16(const uint8_t *d, uint16_t len){
  uint16_t crc = 0xFFFF;
  for (uint16_t i=0;i<len;i++){
    crc ^= (uint16_t)d[i] << 8;
    for (uint8_t b=0;b<8;b++)
      crc = (crc & 0x8000) ? (uint16_t)((crc<<1)^0x1021) : (uint16_t)(crc<<1);
  }
  return crc;
}

/* Every packet's last two bytes are a CRC over all preceding bytes. */
static bool crc_ok(const uint8_t *b, int n){
  if (n < 3) return false;
  return be_u16(&b[n-2]) == packet_crc16(b, (uint16_t)(n-2));
}

static bool packet_v1_unpack(const uint8_t *in, int len, packet_v1_t *o){
  if (len != PACKET_V1_LEN || in[0] != PACKET_V1_VERSION) return false;
  if (be_u16(&in[26]) != packet_crc16(in, PACKET_V1_CRC_LEN)) return false;
  o->version=in[0]; o->mission_state=in[1]; o->seq=be_u16(&in[2]);
  o->time_ms=be_u32(&in[4]); o->lat_e7=be_i32(&in[8]); o->lon_e7=be_i32(&in[12]);
  o->gps_alt_m=be_u16(&in[16]); o->baro_alt_m=be_i16(&in[18]);
  o->temp_c_x100=be_i16(&in[20]); o->batt=be_u16(&in[22]);
  o->flags=in[24]; o->sats=in[25];
  return true;
}

static uint32_t g_rx_total = 0, g_rx_valid = 0;

/* ---- image reassembly state ---- */
static uint8_t  g_img[GND_IMG_MAX];
static bool     g_img_got[GND_MAX_CHUNKS];
static uint8_t  g_img_id       = 0;
static uint32_t g_img_len      = 0;
static uint16_t g_img_chunks   = 0;
static uint16_t g_img_have     = 0;
static uint16_t g_img_csize    = 0;
static uint16_t g_img_w = 0, g_img_h = 0;
static bool     g_img_active   = false;

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void dump_base64(const uint8_t *d, uint32_t len){
  uint32_t i = 0; int col = 0;
  while (i < len){
    uint32_t rem = len - i;
    uint8_t b0 = d[i], b1 = rem>1?d[i+1]:0, b2 = rem>2?d[i+2]:0;
    char o[4];
    o[0]=B64[b0>>2];
    o[1]=B64[((b0&0x03)<<4)|(b1>>4)];
    o[2]= rem>1 ? B64[((b1&0x0f)<<2)|(b2>>6)] : '=';
    o[3]= rem>2 ? B64[b2&0x3f] : '=';
    Serial.write((const uint8_t*)o, 4);
    i += 3; col += 4;
    if (col >= 76){ Serial.println(); col = 0; }
  }
  if (col) Serial.println();
}

static void handle_telemetry(const uint8_t *b, int n, int rssi, float snr){
  packet_v1_t p;
  if (!packet_v1_unpack(b, n, &p)){
    Serial.printf("[%lu] BAD v1  len=%d rssi=%d snr=%.1f raw=", (unsigned long)g_rx_total, n, rssi, snr);
    for (int i=0;i<n;i++) Serial.printf("%02x", b[i]);
    Serial.println();
    return;
  }
  g_rx_valid++;
  Serial.printf("[%lu/%lu] seq=%u  t=%lu ms  state=%u\n",
                (unsigned long)g_rx_valid,(unsigned long)g_rx_total, p.seq,(unsigned long)p.time_ms, p.mission_state);
  Serial.printf("    gps      : %.7f, %.7f   alt=%u m   sats=%u\n", p.lat_e7/1e7, p.lon_e7/1e7, p.gps_alt_m, p.sats);
  Serial.printf("    baro alt : %d m\n", p.baro_alt_m);
  Serial.printf("    temp     : %.2f C\n", p.temp_c_x100/100.0);
  Serial.printf("    flags    : 0x%02x  (IMU%c baro%c temp%c gps%c sd%c lora%c cam%c aprs%c)\n", p.flags,
     p.flags&0x01?'+':'-', p.flags&0x02?'+':'-', p.flags&0x04?'+':'-', p.flags&0x08?'+':'-',
     p.flags&0x10?'+':'-', p.flags&0x20?'+':'-', p.flags&0x40?'+':'-', p.flags&0x80?'+':'-');
  Serial.printf("    link     : rssi=%d dBm  snr=%.1f dB\n\n", rssi, snr);
}

static void handle_imu(const uint8_t *b, int n, int rssi, float snr){
  if (n != TELEM_IMU_LEN || !crc_ok(b, n)){
    Serial.printf("[%lu] BAD imu  len=%d rssi=%d snr=%.1f\n",(unsigned long)g_rx_total,n,rssi,snr);
    return;
  }
  g_rx_valid++;
  uint8_t state=b[1]; uint16_t seq=be_u16(&b[2]); uint32_t t=be_u32(&b[4]);
  int16_t ax=be_i16(&b[8]),ay=be_i16(&b[10]),az=be_i16(&b[12]);
  int16_t gx=be_i16(&b[14]),gy=be_i16(&b[16]),gz=be_i16(&b[18]);
  Serial.printf("[imu %u] t=%lu ms  state=%u\n", seq,(unsigned long)t,state);
  Serial.printf("    accel g  : % .3f  % .3f  % .3f\n", ax/IMU_ACCEL_LSB_PER_G, ay/IMU_ACCEL_LSB_PER_G, az/IMU_ACCEL_LSB_PER_G);
  Serial.printf("    gyro dps : % .1f  % .1f  % .1f\n", gx/IMU_GYRO_LSB_PER_DPS, gy/IMU_GYRO_LSB_PER_DPS, gz/IMU_GYRO_LSB_PER_DPS);
  Serial.printf("    link     : rssi=%d dBm  snr=%.1f dB\n\n", rssi, snr);
}

static void handle_img_header(const uint8_t *b, int n){
  if (n != TELEM_IMG_HDR_LEN || !crc_ok(b, n)){ Serial.println(F("BAD img header")); return; }
  g_img_id     = b[1];
  g_img_len    = be_u32(&b[2]);
  g_img_chunks = be_u16(&b[6]);
  g_img_w      = be_u16(&b[8]);
  g_img_h      = be_u16(&b[10]);
  g_img_csize  = be_u16(&b[12]);
  if (g_img_len > GND_IMG_MAX || g_img_chunks > GND_MAX_CHUNKS || g_img_csize == 0){
    Serial.printf("img %u too big (len=%lu chunks=%u) — ignored\n", g_img_id,(unsigned long)g_img_len,g_img_chunks);
    g_img_active = false; return;
  }
  memset(g_img_got, 0, sizeof(g_img_got));
  g_img_have = 0; g_img_active = true;
  Serial.printf(">> image %u incoming: %ux%u  %lu bytes  %u chunks\n",
                g_img_id, g_img_w, g_img_h,(unsigned long)g_img_len, g_img_chunks);
}

static void handle_img_chunk(const uint8_t *b, int n){
  if (n < 8 || !crc_ok(b, n)) { Serial.println(F("BAD img chunk (crc)")); return; }
  uint8_t id = b[1]; uint16_t idx = be_u16(&b[2]); uint8_t dlen = b[4];
  if (!g_img_active || id != g_img_id){ return; }         /* stale/other image */
  if (idx >= g_img_chunks || (5 + dlen + 2) != n) return;
  uint32_t off = (uint32_t)idx * g_img_csize;
  if (off + dlen > GND_IMG_MAX) return;
  memcpy(&g_img[off], &b[5], dlen);
  if (!g_img_got[idx]){ g_img_got[idx] = true; g_img_have++; }
  if ((idx % 10) == 0 || g_img_have == g_img_chunks)
    Serial.printf("   img %u: %u/%u chunks\n", id, g_img_have, g_img_chunks);

  if (g_img_have == g_img_chunks){
    Serial.printf("===IMG BEGIN id=%u w=%u h=%u len=%lu===\n", g_img_id, g_img_w, g_img_h,(unsigned long)g_img_len);
    dump_base64(g_img, g_img_len);
    Serial.println(F("===IMG END==="));
    g_img_active = false;
  }
}

void setup(){
  Serial.begin(115200);
  while (!Serial && millis() < 3000){}
  Serial.println();
  Serial.println(F("SSI Balloon ground station — telemetry + IMU + image receiver"));

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  LoRa.setPins(PIN_CS, PIN_RST, PIN_DIO0);
  if (!LoRa.begin(LORA_FREQ_HZ)){
    Serial.println(F("ERROR: radio not found. Check wiring (CS/RST), Vin, EN not pulled low."));
    while (1) delay(1000);
  }
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW_HZ);
  LoRa.setCodingRate4(LORA_CR_DENOM);
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.setPreambleLength(LORA_PREAMBLE_LEN);
  LoRa.enableCrc();
  Serial.println(F("Radio up: 915.0 MHz  SF8  BW125k  CR4/5  sync 0x12  CRC on"));
  Serial.println(F("Listening...\n"));
}

void loop(){
  int len = LoRa.parsePacket();
  if (len == 0) return;

  uint8_t buf[256];
  int n = 0;
  while (LoRa.available() && n < (int)sizeof(buf)) buf[n++] = (uint8_t)LoRa.read();
  while (LoRa.available()) (void)LoRa.read();

  int rssi = LoRa.packetRssi();
  float snr = LoRa.packetSnr();
  g_rx_total++;
  if (n < 1) return;

  switch (buf[0]){
    case PACKET_V1_VERSION:     handle_telemetry(buf, n, rssi, snr); break;
    case TELEM_IMU_TYPE:        handle_imu(buf, n, rssi, snr);       break;
    case TELEM_IMG_HDR_TYPE:    handle_img_header(buf, n);           break;
    case TELEM_IMG_CHUNK_TYPE:  handle_img_chunk(buf, n);            break;
    default:
      Serial.printf("[%lu] unknown type 0x%02x  len=%d  rssi=%d\n",(unsigned long)g_rx_total, buf[0], n, rssi);
      break;
  }
}
