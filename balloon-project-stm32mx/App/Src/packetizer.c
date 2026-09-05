/**
 * @file packetizer.c
 * @brief Packet v1 field fill from samples (F8.3).
 */

#include "packetizer.h"

#include <stddef.h>

void packetizer_fill(const packetizer_sample_t *in, packet_v1_t *out)
{
  if ((in == NULL) || (out == NULL))
  {
    return;
  }

  out->version = PACKET_V1_VERSION;
  out->mission_state = in->mission_state;
  out->seq = in->seq;
  out->time_ms = in->time_ms;
  out->lat_e7 = 0;
  out->lon_e7 = 0;
  out->gps_alt_m = 0u;
  out->baro_alt_m = 0;
  out->temp_c_x100 = 0;
  out->batt = PACKET_V1_BATT_NA;
  out->sats = 0u;
  out->flags = (uint8_t)(~in->error_flags & 0xFFu);

  if (in->baro_valid)
  {
    out->baro_alt_m = (int16_t)in->baro_alt_m;
    out->temp_c_x100 = in->baro_temp_centi_c;
  }

  if (in->temp_valid)
  {
    out->temp_c_x100 = in->temp_centi_c;
  }

  if (in->gps_sample_valid)
  {
    out->sats = in->sats;

    if (in->lat_lon_valid)
    {
      out->lat_e7 = in->lat_e7;
      out->lon_e7 = in->lon_e7;
    }

    if (in->gps_alt_valid)
    {
      out->gps_alt_m = in->gps_alt_m;
    }
  }
}

void packetizer_pack(const packet_v1_t *fields, uint8_t wire[PACKET_V1_LEN])
{
  if ((fields == NULL) || (wire == NULL))
  {
    return;
  }

  packet_v1_pack(fields, wire);
}
