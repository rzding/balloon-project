/*
 * SSI Balloon — ESP32 ground station receiver
 *
 * Receives packet v1 telemetry from the flight computer's RFM95W and prints
 * decoded fields over USB serial at 115200 baud.
 *
 * Hardware: ESP32 dev board + Adafruit RFM95W LoRa breakout (product 3072)
 *           + 915 MHz antenna (see wiring table in ground/README.md).
 *
 * Library:  "LoRa" by Sandeep Mistry  (Library Manager -> search "LoRa")
 *           Do NOT use RadioHead: RH_RF95 prepends a 4-byte TO/FROM/ID/FLAGS
 *           header to every packet, which the flight firmware does not send.
 *           This library transmits and receives raw payloads.
 *
 * Every modem parameter below must match balloon-project-stm32mx/App/Inc/lora.h.
 * If the flight side changes, change it here too or nothing will decode.
 */

#include <SPI.h>
#include <LoRa.h>

/* ---- Wiring: change these to match how you wired the breakout ----
 *
 * PIN_DIO0 is set to -1 because this sketch polls with LoRa.parsePacket()
 * rather than using an interrupt, so the breakout's "G0" pin can be left
 * unconnected. Only set a real pin here if you switch to LoRa.onReceive().
 *
 * Avoid GPIO2 for DIO0: it is a boot-strapping pin, and DIO0 is driven by
 * the radio, so the radio could hold it at an arbitrary level while the
 * ESP32 is deciding how to boot.
 */
#define PIN_SCK    18
#define PIN_MISO   19
#define PIN_MOSI   15
#define PIN_CS      5
#define PIN_RST     4
#define PIN_DIO0   -1   /* breakout "G0" — not needed when polling */

/* ---- Modem settings: must match flight lora.h ---- */
#define LORA_FREQ_HZ      915000000L  /* LORA_FREQ_HZ */
#define LORA_SF                    8  /* RegModemConfig2 0x84 -> SF8 */
#define LORA_BW_HZ            125000  /* RegModemConfig1 0x72 -> 125 kHz */
#define LORA_CR_DENOM              5  /* RegModemConfig1 0x72 -> CR 4/5 */
#define LORA_SYNC_WORD          0x12  /* LORA_SYNC_WORD (private, not LoRaWAN 0x34) */
#define LORA_PREAMBLE_LEN          8  /* LORA_PREAMBLE_LEN */

/* ---- Packet v1 layout: must match App/Inc/packet.h ---- */
#define PACKET_V1_LEN       28
#define PACKET_V1_VERSION 0x01
#define PACKET_V1_CRC_LEN   26  /* CRC covers bytes 0..25 */
#define PACKET_V1_BATT_NA 0xFFFF

typedef struct {
  uint8_t  version;
  uint8_t  mission_state;
  uint16_t seq;
  uint32_t time_ms;
  int32_t  lat_e7;
  int32_t  lon_e7;
  uint16_t gps_alt_m;
  int16_t  baro_alt_m;
  int16_t  temp_c_x100;
  uint16_t batt;
  uint8_t  flags;
  uint8_t  sats;
} packet_v1_t;

/* Big-endian readers — the wire format is MSB first. */
static uint16_t be_u16(const uint8_t *b) {
  return ((uint16_t)b[0] << 8) | (uint16_t)b[1];
}
static uint32_t be_u32(const uint8_t *b) {
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
         ((uint32_t)b[2] << 8)  | (uint32_t)b[3];
}
static int16_t be_i16(const uint8_t *b) { return (int16_t)be_u16(b); }
static int32_t be_i32(const uint8_t *b) { return (int32_t)be_u32(b); }

/* CRC-16/CCITT-FALSE: init 0xFFFF, poly 0x1021, MSB first.
   Mirrors packet_crc16() in App/Inc/packet.h. */
static uint16_t packet_crc16(const uint8_t *data, uint16_t len) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
                           : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

/* Returns false on wrong length, wrong version, or CRC mismatch. */
static bool packet_v1_unpack(const uint8_t *in, uint16_t len, packet_v1_t *out) {
  if (len != PACKET_V1_LEN)            return false;
  if (in[0] != PACKET_V1_VERSION)      return false;

  uint16_t crc_stored = be_u16(&in[26]);
  uint16_t crc_calc   = packet_crc16(in, PACKET_V1_CRC_LEN);
  if (crc_stored != crc_calc)          return false;

  out->version       = in[0];
  out->mission_state = in[1];
  out->seq           = be_u16(&in[2]);
  out->time_ms       = be_u32(&in[4]);
  out->lat_e7        = be_i32(&in[8]);
  out->lon_e7        = be_i32(&in[12]);
  out->gps_alt_m     = be_u16(&in[16]);
  out->baro_alt_m    = be_i16(&in[18]);
  out->temp_c_x100   = be_i16(&in[20]);
  out->batt          = be_u16(&in[22]);
  out->flags         = in[24];
  out->sats          = in[25];
  return true;
}

static uint32_t g_rx_total = 0;   /* every packet the radio handed us */
static uint32_t g_rx_valid = 0;   /* those that passed version + CRC */

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { /* wait briefly for USB serial */ }

  Serial.println();
  Serial.println(F("SSI Balloon ground station — packet v1 receiver"));

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  LoRa.setPins(PIN_CS, PIN_RST, PIN_DIO0);

  if (!LoRa.begin(LORA_FREQ_HZ)) {
    Serial.println(F("ERROR: radio not found."));
    Serial.println(F("Check wiring (CS/RST/G0), 3.3-6V on Vin, and that EN is not pulled low."));
    while (1) { delay(1000); }
  }

  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW_HZ);
  LoRa.setCodingRate4(LORA_CR_DENOM);
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.setPreambleLength(LORA_PREAMBLE_LEN);
  LoRa.enableCrc();                 /* flight side sets RxPayloadCrcOn */

  Serial.println(F("Radio up: 915.0 MHz  SF8  BW125k  CR4/5  sync 0x12  CRC on"));
  Serial.println(F("Listening..."));
  Serial.println();
}

void loop() {
  int len = LoRa.parsePacket();
  if (len == 0) {
    return;                          /* nothing received this pass */
  }

  uint8_t buf[PACKET_V1_LEN];
  int n = 0;
  while (LoRa.available() && n < (int)sizeof(buf)) {
    buf[n++] = (uint8_t)LoRa.read();
  }
  while (LoRa.available()) { (void)LoRa.read(); }   /* drain any excess */

  int   rssi = LoRa.packetRssi();
  float snr  = LoRa.packetSnr();
  g_rx_total++;

  packet_v1_t p;
  if (!packet_v1_unpack(buf, (uint16_t)n, &p)) {
    Serial.printf("[%lu] BAD PACKET  len=%d  rssi=%d dBm  snr=%.1f dB  raw=",
                  (unsigned long)g_rx_total, n, rssi, snr);
    for (int i = 0; i < n; i++) Serial.printf("%02x", buf[i]);
    Serial.println();
    return;
  }

  g_rx_valid++;

  Serial.printf("[%lu/%lu] seq=%u  t=%lu ms  state=%u\n",
                (unsigned long)g_rx_valid, (unsigned long)g_rx_total,
                p.seq, (unsigned long)p.time_ms, p.mission_state);
  Serial.printf("    gps      : %.7f, %.7f   alt=%u m   sats=%u\n",
                p.lat_e7 / 1e7, p.lon_e7 / 1e7, p.gps_alt_m, p.sats);
  Serial.printf("    baro alt : %d m\n", p.baro_alt_m);
  Serial.printf("    temp     : %.2f C\n", p.temp_c_x100 / 100.0);
  if (p.batt == PACKET_V1_BATT_NA) {
    Serial.println(F("    batt     : n/a"));
  } else {
    Serial.printf("    batt     : %u mV\n", p.batt);
  }
  Serial.printf("    flags    : 0x%02x\n", p.flags);
  Serial.printf("    link     : rssi=%d dBm  snr=%.1f dB\n", rssi, snr);
  Serial.println();
}
