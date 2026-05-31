/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_USB_UDC_DWC3_RK3588_H
#define ZEPHYR_DRIVERS_USB_UDC_DWC3_RK3588_H

#include <zephyr/drivers/syscon.h>

/* USBDPPHY GRF register offsets */
#define USBDPPHY_GRF_CON1    0x0004
#define USBDPPHY_GRF_CON2    0x0008
#define USBDPPHY_GRF_STATUS1 0x0084

/* USBDPPHY GRF bit definitions */
#define USBDPPHY_GRF_WRITE_ENABLE_SHIFT 16
#define USBDPPHY_GRF_RX_LFPS_EN         BIT(14)
#define USBDPPHY_GRF_USBDP_LOW_PWRN     BIT(13)
#define USBDPPHY_GRF_PHY_CLAMP          BIT(15)

/* USB2PHY GRF register offsets */
#define USB2PHY_GRF_CON1 0x0004
#define USB2PHY_GRF_CON2 0x0008
#define USB2PHY_GRF_CON3 0x000C

/* USB2PHY GRF bit definitions */
#define USB2PHY_GRF_WRITE_ENABLE_SHIFT  16
#define USB2PHY_GRF_SIDDQ               BIT(13)
#define USB2PHY_GRFPHY_SUS_ENABLE_SHIFT 11

struct udc_dwc3_rk3588_data {
	const struct device *usb2phy_grf;
	const struct device *usb3phy_grf;
};

static int rk3588_usb2phy_tuning(const struct device *usb2phy_grf)
{
	uint32_t val;

	/* Clear SIDDQ to power on analog block */
	val = (USB2PHY_GRF_SIDDQ << USB2PHY_GRF_WRITE_ENABLE_SHIFT);
	syscon_write_reg(usb2phy_grf, USB2PHY_GRF_CON2, val);
	LOG_DBG("USB2 PHY tuning: SIDDQ deasserted (GRF_CON2 = 0x%08x)", val);

	/* HS DC voltage adjustment (+5.89%) */
	val = (0xFu << (8 + USB2PHY_GRF_WRITE_ENABLE_SHIFT)) | (0x9u << 8);
	syscon_write_reg(usb2phy_grf, USB2PHY_GRF_CON1, val);
	LOG_DBG("USB2 PHY tuning: HS DC voltage adjusted (GRF_CON1 = 0x%08x)", val);

	/* Pre-emphasis set to 2x */
	val = (0x3u << (3 + USB2PHY_GRF_WRITE_ENABLE_SHIFT)) | (0x2u << 3);
	syscon_write_reg(usb2phy_grf, USB2PHY_GRF_CON2, val);
	LOG_DBG("USB2 PHY tuning: Pre-emphasis set to 2x (GRF_CON2 = 0x%08x)", val);

	return 0;
}

static int rk3588_usb2_phy_init(const struct device *usb2phy_grf)
{
	int ret;
	uint32_t val;

	ret = rk3588_usb2phy_tuning(usb2phy_grf);
	if (ret < 0) {
		return ret;
	}

	/* Exit PHY suspend mode */
	val = (1u << (USB2PHY_GRFPHY_SUS_ENABLE_SHIFT + USB2PHY_GRF_WRITE_ENABLE_SHIFT));
	syscon_write_reg(usb2phy_grf, USB2PHY_GRF_CON3, val);
	LOG_INF("USB2 PHY: exited suspend mode (GRF_CON3 = 0x%08x)", val);

	k_sleep(K_USEC(2000));

	return 0;
}

static int rk3588_usb3_dp_phy_init(const struct device *usb3phy_grf)
{
	uint32_t val;

	LOG_INF("Initializing RK3588 USB3 DP PHY");

	syscon_read_reg(usb3phy_grf, USBDPPHY_GRF_CON1, &val);
	if (val == 0xffffffff) {
		LOG_ERR("USBDPPHY0_GRF not accessible");
		return -EIO;
	}

	val &= ~USBDPPHY_GRF_PHY_CLAMP;
	val |= USBDPPHY_GRF_RX_LFPS_EN;
	val |= USBDPPHY_GRF_USBDP_LOW_PWRN;
	val |= (0xFFFF << USBDPPHY_GRF_WRITE_ENABLE_SHIFT);
	syscon_write_reg(usb3phy_grf, USBDPPHY_GRF_CON1, val);

	syscon_read_reg(usb3phy_grf, USBDPPHY_GRF_CON2, &val);
	val |= (0xFFFF << USBDPPHY_GRF_WRITE_ENABLE_SHIFT);
	syscon_write_reg(usb3phy_grf, USBDPPHY_GRF_CON2, val);

	k_busy_wait(20000); /* 20ms */

	LOG_INF("USB3 DP PHY init completed (USB2.0 mode)");
	return 0;
}

static int udc_dwc3_rk3588_preinit(const struct device *const dev)
{
	const struct udc_dwc3_config *const cfg = dev->config;
	struct udc_dwc3_rk3588_data *const qdata = cfg->quirk_data;
	int ret;

	LOG_INF("Initializing RK3588 USB subsystem");

	ret = rk3588_usb2_phy_init(qdata->usb2phy_grf);
	if (ret < 0) {
		LOG_ERR("Failed to initialize USB2 PHY: %d", ret);
		return ret;
	}

	ret = rk3588_usb3_dp_phy_init(qdata->usb3phy_grf);
	if (ret < 0) {
		LOG_ERR("Failed to initialize USB3 DP PHY: %d", ret);
		return ret;
	}

	LOG_INF("RK3588 USB subsystem initialized");
	return 0;
}

static int udc_dwc3_rk3588_enable(const struct device *const dev)
{
	ARG_UNUSED(dev);
	return 0;
}

static int udc_dwc3_rk3588_init(const struct device *const dev)
{
	mm_reg_t base = DEVICE_MMIO_NAMED_GET(dev, base);
	uint32_t reg;

	/* Apply snps,del_phy_power_chg_quirk */
	reg = sys_read32(base + UDC_DWC3_GUSB3PIPECTL);
	reg |= UDC_DWC3_GUSB3PIPECTL_DELAYP0TOP1P2P3;
	sys_write32(reg, base + UDC_DWC3_GUSB3PIPECTL);
	LOG_DBG("GUSB3PIPECTL = 0x%08x (del_phy_power_chg_quirk)", reg);

	/* Apply USB2 PHY quirks: dis_enblslpm, dis-u2-freeclk-exists, utmi_wide */
	reg = sys_read32(base + UDC_DWC3_GUSB2PHYCFG);
	reg &= ~UDC_DWC3_GUSB2PHYCFG_SUSPHY;
	reg &= ~UDC_DWC3_GUSB2PHYCFG_ENBLSLPM;
	reg &= ~UDC_DWC3_GUSB2PHYCFG_U2_FREECLK_EXISTS;
	reg |= UDC_DWC3_GUSB2PHYCFG_PHYIF;
	sys_write32(reg, base + UDC_DWC3_GUSB2PHYCFG);
	LOG_DBG("GUSB2PHYCFG = 0x%08x (vendor quirks applied)", reg);

	return 0;
}

const struct udc_dwc3_vendor_quirks udc_dwc3_vendor_quirks = {
	.preinit = udc_dwc3_rk3588_preinit,
	.init = udc_dwc3_rk3588_init,
	.enable = udc_dwc3_rk3588_enable,
};

#define UDC_DWC3_QUIRK_DEFINE(n)                                                                   \
	static struct udc_dwc3_rk3588_data udc_dwc3_quirk_data_##n = {                             \
		.usb2phy_grf = DEVICE_DT_GET(DT_INST_PHANDLE(n, usb2phy_grf)),                     \
		.usb3phy_grf = DEVICE_DT_GET(DT_INST_PHANDLE(n, usb3phy_grf)),                     \
	};                                                                                         \
	static const void *udc_dwc3_quirk_config_##n;

#endif /* ZEPHYR_DRIVERS_USB_UDC_DWC3_RK3588_H */
