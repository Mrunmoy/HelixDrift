#include "usbd_init.h"

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>

USBD_DEVICE_DEFINE(helix_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   CONFIG_HELIX_MOCAP_USB_VID, CONFIG_HELIX_MOCAP_USB_PID);
USBD_DESC_LANG_DEFINE(helix_lang);
USBD_DESC_MANUFACTURER_DEFINE(helix_mfr, "HelixDrift");
#if defined(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_CENTRAL)
USBD_DESC_PRODUCT_DEFINE(helix_product, "Helix Mocap Central");
#else
USBD_DESC_PRODUCT_DEFINE(helix_product, "Helix Mocap Node");
#endif
USBD_DESC_CONFIG_DEFINE(helix_fs_cfg_desc, "FS Configuration");

static const uint8_t helix_attributes = USB_SCD_SELF_POWERED;

USBD_CONFIGURATION_DEFINE(helix_fs_config, helix_attributes, 125, &helix_fs_cfg_desc);

static bool helix_usbd_ready;

static void helix_fix_code_triple(struct usbd_context *ctx)
{
	usbd_device_set_code_triple(ctx, USBD_SPEED_FS, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
}

static void helix_usbd_msg_cb(struct usbd_context *const ctx, const struct usbd_msg *msg)
{
	if (usbd_can_detect_vbus(ctx)) {
		if (msg->type == USBD_MSG_VBUS_READY) {
			(void)usbd_enable(ctx);
		} else if (msg->type == USBD_MSG_VBUS_REMOVED) {
			(void)usbd_disable(ctx);
		}
	}
}

static int helix_usbd_init_once(void)
{
	int err;

	if (helix_usbd_ready) {
		return 0;
	}

	err = usbd_add_descriptor(&helix_usbd, &helix_lang);
	if (err) {
		return err;
	}
	err = usbd_add_descriptor(&helix_usbd, &helix_mfr);
	if (err) {
		return err;
	}
	err = usbd_add_descriptor(&helix_usbd, &helix_product);
	if (err) {
		return err;
	}
	err = usbd_add_configuration(&helix_usbd, USBD_SPEED_FS, &helix_fs_config);
	if (err) {
		return err;
	}
	err = usbd_register_all_classes(&helix_usbd, USBD_SPEED_FS, 1, NULL);
	if (err) {
		return err;
	}
	helix_fix_code_triple(&helix_usbd);
	usbd_self_powered(&helix_usbd, true);
	err = usbd_msg_register_cb(&helix_usbd, helix_usbd_msg_cb);
	if (err) {
		return err;
	}
	err = usbd_init(&helix_usbd);
	if (err) {
		return err;
	}

	helix_usbd_ready = true;
	return 0;
}

int helix_usbd_enable(const struct device *console_dev, int wait_for_dtr_ms)
{
	int err;

	err = helix_usbd_init_once();
	if (err) {
		return err;
	}

	if (!usbd_can_detect_vbus(&helix_usbd)) {
		err = usbd_enable(&helix_usbd);
		if (err) {
			return err;
		}
	}

#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
	if (wait_for_dtr_ms > 0) {
		int64_t deadline = k_uptime_get() + wait_for_dtr_ms;

		while (k_uptime_get() < deadline) {
			uint32_t dtr = 0U;

			(void)uart_line_ctrl_get(console_dev, UART_LINE_CTRL_DTR, &dtr);
			if (dtr != 0U) {
				break;
			}
			k_sleep(K_MSEC(50));
		}
	}
#else
	ARG_UNUSED(console_dev);
	ARG_UNUSED(wait_for_dtr_ms);
#endif

	return 0;
}

#else

int helix_usbd_enable(const struct device *console_dev, int wait_for_dtr_ms)
{
	ARG_UNUSED(console_dev);
	ARG_UNUSED(wait_for_dtr_ms);
	return 0;
}

#endif
