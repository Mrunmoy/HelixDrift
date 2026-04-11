#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

namespace {

const gpio_dt_spec g_led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

}

int main() {
    if (!gpio_is_ready_dt(&g_led)) {
        printk("propico bringup: led0 not ready\n");
        return 1;
    }

    if (gpio_pin_configure_dt(&g_led, GPIO_OUTPUT_INACTIVE) != 0) {
        printk("propico bringup: led0 config failed\n");
        return 2;
    }

    printk("propico bringup: booted, led0 heartbeat active\n");

    bool on = false;
    uint32_t tick = 0;
    while (true) {
        on = !on;
        gpio_pin_set_dt(&g_led, on ? 1 : 0);
        printk("propico bringup: tick %u led=%u\n", tick++, on ? 1u : 0u);
        k_msleep(500);
    }
}
