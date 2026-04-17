#pragma once

/* Hub OTA relay: receives UartOtaProtocol frames from PC over USB CDC,
 * forwards them to a Tag's BLE GATT OTA service, returns responses.
 *
 * Call ota_hub_relay_init() after host_stream_init().
 * Call ota_hub_relay_poll() in the main loop to process USB RX.
 * When an OTA session starts, ESB must be stopped and BLE enabled.
 * When the session ends, BLE is disabled and ESB is restarted. */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the USB CDC RX ring buffer and frame parser. */
int ota_hub_relay_init(const struct device *host_dev);

/* Poll for USB RX data and process OTA relay frames.
 * Returns true if an OTA session is currently active (ESB should be stopped).
 * Call from the main loop. */
bool ota_hub_relay_poll(void);

/* Check if we need to stop ESB for an OTA session. */
bool ota_hub_relay_needs_esb_stop(void);

/* Check if the OTA session ended and ESB can restart. */
bool ota_hub_relay_esb_can_restart(void);

/* Debug: get total RX byte count and poll count. */
uint32_t ota_hub_relay_rx_count(void);
uint32_t ota_hub_relay_poll_count(void);

#ifdef __cplusplus
}
#endif
