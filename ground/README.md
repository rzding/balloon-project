# Ground station — packet v1 decoder (F7.4)

Software support for ops RX (second RFM95W + Nucleo). F7.4 does **not** flash Nucleo firmware; it documents modem settings and ships a host CLI to decode captured payloads.

## Modem match sheet (must match flight `lora.h` defaults)

| Parameter | Value |
|---|---|
| Frequency | 915.0 MHz US ISM |
| Modulation | LoRa |
| Spreading factor | SF8 |
| Bandwidth | 125 kHz |
| Coding rate | 4/5 (CR4/5) |
| Header | Explicit |
| CRC | On (SX1276 hardware CRC) |
| Sync word | `0x12` (private; not LoRaWAN `0x34`) |
| Preamble | 8 symbols |
| TX power (flight) | PA_BOOST +17 dBm |

Payload on air is **28 bytes** per `App/Inc/packet.h` (packet v1). SX1276 adds its LoRa header/CRC around this; the decoder expects the **application payload** hex dump (56 hex chars).

## Build and decode

```bash
cd ground
make
./decode_packet 0102002a00bc614e1683fed0f8b4073805dc05c8092effff05086ecf
```

With RX radio metadata (from `RegPktRssiValue` / `RegPktSnrValue` on the Nucleo RX node):

```bash
./decode_packet 0102002a00bc614e1683fed0f8b4073805dc05c8092effff05086ecf --rssi -72 --snr 9.5
```

Exit code **0** when version and CRC-16/CCITT-FALSE validate; **1** otherwise. RSSI/SNR are always logged (`n/a` if omitted).

## Ops workflow (when RX hardware exists)

1. Configure Nucleo + RFM95W RX with the modem table above (RadioLib or HAL).
2. Capture application payload bytes (28) from RX FIFO or serial dump.
3. Pass hex + optional `--rssi` / `--snr` to `decode_packet`.
4. Confirm `crc_ok=true` and monotonic `seq` across packets.

Bench verification: roadmap **§12.4** hardware exit and **§21 F7**.
