#pragma once

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

int helix_usbd_enable(const struct device *console_dev, int wait_for_dtr_ms);

#ifdef __cplusplus
}
#endif
