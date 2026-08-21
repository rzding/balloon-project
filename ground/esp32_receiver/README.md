# ESP32 ground station receiver

Receives packet v1 telemetry from the flight computer's RFM95W and prints decoded
fields over USB serial. No WiFi or BLE — the ESP32 stays wired to the laptop.

Sketch: [`esp32_receiver.ino`](esp32_receiver.ino)

## Hardware

| Item | Notes |
|---|---|
| ESP32 dev board | any; pins below are remappable |
| Adafruit RFM95W LoRa breakout | product [3072](https://www.adafruit.com/product/3072), 915 MHz |
| 915 MHz antenna | 3.12" (~79 mm) solid-core wire on the ANT pad, or SMA |

The breakout has an onboard 3.3 V regulator and level shifting, so it is 3–6 V
tolerant on Vin and safe on the ESP32's 3.3 V logic.

## Wiring

| Breakout | ESP32 | Notes |
|---|---|---|
| Vin | 3V3 | regulated on-board |
| GND | GND | |
| EN | *not connected* | pulled high to Vin by default |
| SCK | D18 | |
| MISO | D19 | |
| MOSI | D15 | remapped via the GPIO matrix |
| CS | D5 | |
| RST | D4 | library pulses this low to reset the radio |
| G0 | *not connected* | see below |
| ANT | antenna | **required — do not transmit or receive without one** |
| DIO1–DIO5 | *not connected* | |

`G0` (DIO0) is unused because the sketch polls with `LoRa.parsePacket()` rather
than using an interrupt. If you switch to `LoRa.onReceive()`, wire G0 to a
general-purpose pin and set `PIN_DIO0`.

**Avoid GPIO2 for G0.** It is a boot-strapping pin, and G0 is driven by the
radio, so the radio could hold it at an arbitrary level while the ESP32 is
deciding how to boot.

## Software setup

1. **ESP32 board support** — Arduino IDE → Settings → *Additional Board Manager
   URLs*:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
   Then Tools → Board → Boards Manager → install **esp32 by Espressif Systems**.

2. **Library** — Tools → Manage Libraries → install **LoRa by Sandeep Mistry**.

   Do **not** use RadioHead, even though Adafruit's guide does. `RH_RF95`
   prepends a 4-byte TO/FROM/ID/FLAGS header to every packet; the flight
   firmware sends a raw 28-byte payload, so RadioHead would consume the first
   four bytes and corrupt every packet.

3. Open `esp32_receiver.ino`, select your board and port, upload.

4. Serial Monitor at **115200 baud**.

## Expected output

```
SSI Balloon ground station — packet v1 receiver
Radio up: 915.0 MHz  SF8  BW125k  CR4/5  sync 0x12  CRC on
Listening...

[1/1] seq=0  t=5001 ms  state=0
    gps      : 0.0000000, 0.0000000   alt=0 m   sats=0
    baro alt : 138 m
    temp     : 22.94 C
    batt     : n/a
    flags    : 0x2e
    link     : rssi=-42 dBm  snr=9.8 dB
```

The counter is `[valid/total]`, so `[7/9]` means nine packets arrived and two
failed version or CRC checks — useful for judging link margin at range.

Zeroed GPS fields are normal indoors; the flight firmware still beacons without
a fix so the RF link can be tested independently of GPS.

## Modem settings

Every parameter below must match
[`App/Inc/lora.h`](../../balloon-project-stm32mx/App/Inc/lora.h). If the flight
side changes, change the sketch too or nothing will decode.

| Parameter | Value |
|---|---|
| Frequency | 915.0 MHz |
| Spreading factor | SF8 |
| Bandwidth | 125 kHz |
| Coding rate | 4/5 |
| Header | explicit |
| CRC | on |
| Sync word | `0x12` (private; not LoRaWAN `0x34`) |
| Preamble | 8 symbols |

Payload is 28 bytes, packet v1, per
[`App/Inc/packet.h`](../../balloon-project-stm32mx/App/Inc/packet.h). All
multi-byte fields are big-endian; the trailing CRC16 (CCITT-FALSE, init
`0xFFFF`, poly `0x1021`) covers bytes 0–25.

## Troubleshooting

| Symptom | Meaning |
|---|---|
| `ERROR: radio not found` | ESP32 cannot reach the radio over SPI — check wiring, Vin, and that EN is not pulled low |
| `Listening...` then silence | Radio is fine but nothing is arriving. RF problem: antennas, distance, or the flight board is not transmitting |
| `BAD PACKET ... raw=` | Receiving RF but failing version/CRC. The raw hex is printed — decode it by hand with [`ground/decode_packet`](../decode_packet.c) |

Keep the two boards at least a metre apart. At +17 dBm, antennas touching can
overload the receiver front end and produce exactly the "nothing received"
symptom you would blame on a wiring fault.

## Checking the flight side independently

If nothing is received, confirm the flight board is actually transmitting before
suspecting the receiver. Attach the debugger and watch these globals in the
Expressions view (see `App/Src/app.c`):

| Expression | Meaning |
|---|---|
| `g_beacon_attempts` | beacons tried |
| `g_beacon_ok` | `lora_tx()` returned true |
| `g_beacon_fail` | `lora_tx()` returned false |
| `g_beacon_wire` | the 28 bytes on air — copy as hex into `ground/decode_packet` |

`g_beacon_ok` climbing means the radio reported TxDone, so the problem is on the
RX side or in the air, not the flight firmware.
