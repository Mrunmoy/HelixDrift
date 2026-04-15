#pragma once

#include <zephyr/device.h>

int helix_usbd_enable(const struct device *console_dev, int wait_for_dtr_ms);
