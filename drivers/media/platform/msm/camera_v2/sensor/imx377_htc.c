/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "msm_sensor.h"
#include <linux/async.h>

#include "msm_cci.h"

#if 0
#if 1
#define HTC_OIS_HW_WORKAROUND 1
#else
#undef HTC_OIS_HW_WORKAROUND
#endif
#endif

#define IMX377_HTC_SENSOR_NAME "imx377_htc"
#define imx377_htc_obj imx377_htc_##obj

DEFINE_MSM_MUTEX(imx377_htc_mut);

static struct msm_sensor_ctrl_t imx377_htc_s_ctrl;

static struct msm_sensor_power_setting imx377_htc_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VAF,
		.config_val = 1,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 1,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 1,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 1,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 1
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct v4l2_subdev_info imx377_htc_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static const struct i2c_device_id imx377_htc_i2c_id[] = {
	{IMX377_HTC_SENSOR_NAME, (kernel_ulong_t)&imx377_htc_s_ctrl},
	{ }
};

static int32_t msm_imx377_htc_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &imx377_htc_s_ctrl);
}

static struct i2c_driver imx377_htc_i2c_driver = {
	.id_table = imx377_htc_i2c_id,
	.probe  = msm_imx377_htc_i2c_probe,
	.driver = {
		.name = IMX377_HTC_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx377_htc_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id imx377_htc_dt_match[] = {
	{.compatible = "htc,imx377_htc", .data = &imx377_htc_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, imx377_htc_dt_match);

static struct platform_driver imx377_htc_platform_driver = {
	.driver = {
		.name = "htc,imx377_htc",
		.owner = THIS_MODULE,
		.of_match_table = imx377_htc_dt_match,
	},
};

static const char *imx377_htcVendor = "Sony";
static const char *imx377_htcNAME = "imx377_htc";
static const char *imx377_htcSize = "12M";

static ssize_t sensor_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "%s %s %s\n", imx377_htcVendor, imx377_htcNAME, imx377_htcSize);
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(sensor, 0444, sensor_vendor_show, NULL);

static struct kobject *android_imx377_htc;

static int imx377_htc_sysfs_init(void)
{
	int ret ;
	pr_info("imx377_htc:kobject creat and add\n");
	android_imx377_htc = kobject_create_and_add("android_camera", NULL);
	if (android_imx377_htc == NULL) {
		pr_info("imx377_htc_sysfs_init: subsystem_register " \
		"failed\n");
		ret = -ENOMEM;
		return ret ;
	}
	pr_info("imx377_htc:sysfs_create_file\n");
	ret = sysfs_create_file(android_imx377_htc, &dev_attr_sensor.attr);
	if (ret) {
		pr_info("imx377_htc_sysfs_init: sysfs_create_file " \
		"failed\n");
		kobject_del(android_imx377_htc);
	}

	return 0 ;
}


static uint8_t otp[20];
#define EEPROM_COMPONENT_I2C_ADDR_WRITE 0xA0

static int imx377_htc_read_fuseid(struct sensorb_cfg_data *cdata,
	struct msm_sensor_ctrl_t *s_ctrl)
{
    int32_t rc = 0;
    int32_t index = 0;
	uint16_t address = 0;
    uint16_t read_data = 0;
    static int first = true;
    uint16_t cci_client_sid_backup = s_ctrl->sensordata->slave_info->sensor_slave_addr;

    if (first)
    {
        first = false;
        s_ctrl->sensor_i2c_client->cci_client->sid = EEPROM_COMPONENT_I2C_ADDR_WRITE >> 1;
        s_ctrl->sensor_i2c_client->addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
        for(index = 0; index < 11 ; index++)   
        {
            read_data = 0;
            msleep(1);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, address, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
                pr_info("[OTP]%s: i2c_read 0x%x fail\n", __func__, address);
            otp[index] = read_data & 0xff;
            address++;
        }
        
        s_ctrl->sensor_i2c_client->cci_client->sid = 0x56;
        msleep(1);
        read_data = 0;
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x56, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
            pr_err("[OTP]%s: i2c_read 0x56 failed\n", __func__);
        otp[index] = read_data & 0xff;
        index++;

        msleep(1);
        read_data = 0;
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x57, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
            pr_err("[OTP]%s: i2c_read 0x57 failed\n", __func__);
        otp[index] = read_data & 0xff;
        index++;

        msleep(1);
        read_data = 0;
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x58, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
            pr_err("[OTP]%s: i2c_read 0x58 failed\n", __func__);
        otp[index] = read_data & 0xff;
        index++;

        msleep(1);
        read_data = 0;
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x59, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
            pr_err("[OTP]%s: i2c_read 0x59 failed\n", __func__);
        otp[index] = read_data & 0xff;
        index++;

        s_ctrl->sensor_i2c_client->addr_type = MSM_CAMERA_I2C_WORD_ADDR;
        s_ctrl->sensor_i2c_client->cci_client->sid = cci_client_sid_backup >> 1;
    }

    if(cdata != NULL)
    {
    cdata->cfg.fuse.fuse_id_word1 = otp[11];
    cdata->cfg.fuse.fuse_id_word2 = otp[12];
    cdata->cfg.fuse.fuse_id_word3 = otp[13];
    cdata->cfg.fuse.fuse_id_word4 = otp[14];

    cdata->af_value.VCM_VENDOR = otp[0];
    cdata->af_value.VCM_VENDOR_ID_VERSION = otp[4];
    cdata->af_value.AF_INF_MSB = otp[5];
    cdata->af_value.AF_INF_LSB = otp[6];
    cdata->af_value.AF_MACRO_MSB = otp[9];
    cdata->af_value.AF_MACRO_LSB = otp[10];

    pr_info("%s: OTP Module vendor = 0x%x\n",               __func__,  otp[0]);
    pr_info("%s: OTP LENS = 0x%x\n",                        __func__,  otp[1]);
    pr_info("%s: OTP Sensor Version = 0x%x\n",              __func__,  otp[2]);
    pr_info("%s: OTP Driver IC Vendor & Version = 0x%x\n",  __func__,  otp[3]);
    pr_info("%s: OTP Actuator vender ID & Version = 0x%x\n",__func__,  otp[4]);

    pr_info("%s: OTP fuse 0 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word1);
    pr_info("%s: OTP fuse 1 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word2);
    pr_info("%s: OTP fuse 2 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word3);
    pr_info("%s: OTP fuse 3 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word4);

    pr_info("%s: OTP Infinity position code (MSByte) = 0x%x\n", __func__,  cdata->af_value.AF_INF_MSB);
    pr_info("%s: OTP Infinity position code (LSByte) = 0x%x\n", __func__,  cdata->af_value.AF_INF_LSB);
    pr_info("%s: OTP Macro position code (MSByte) = 0x%x\n",    __func__,  cdata->af_value.AF_MACRO_MSB);
    pr_info("%s: OTP Macro position code (LSByte) = 0x%x\n",    __func__,  cdata->af_value.AF_MACRO_LSB);

    strlcpy(cdata->af_value.ACT_NAME, "lc898214_act", sizeof("lc898214_act"));
    pr_info("%s: OTP Actuator Name = %s\n",__func__, cdata->af_value.ACT_NAME);
	}
	else
	{
	    pr_info("%s: OTP Module vendor = 0x%x\n",               __func__,  otp[0]);
	    pr_info("%s: OTP LENS = 0x%x\n",                        __func__,  otp[1]);
	    pr_info("%s: OTP Sensor Version = 0x%x\n",              __func__,  otp[2]);
	    pr_info("%s: OTP Driver IC Vendor & Version = 0x%x\n",  __func__,  otp[3]);
	    pr_info("%s: OTP Actuator vender ID & Version = 0x%x\n",__func__,  otp[4]);
	}
    return rc;
}

#ifdef CONFIG_COMPAT
static int imx377_htc_read_fuseid32(struct sensorb_cfg_data32 *cdata,
	struct msm_sensor_ctrl_t *s_ctrl)
{
    int32_t rc = 0;
    int32_t index = 0;
	uint16_t address = 0;
    uint16_t read_data = 0;
    static int first = true;
    uint16_t cci_client_sid_backup = s_ctrl->sensordata->slave_info->sensor_slave_addr;

    if (first)
    {
        first = false;
        s_ctrl->sensor_i2c_client->cci_client->sid = EEPROM_COMPONENT_I2C_ADDR_WRITE >> 1;
        s_ctrl->sensor_i2c_client->addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
        for(index = 0; index < 11 ; index++)   
        {
            read_data = 0;
            msleep(1);
            rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, address, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
                pr_info("[OTP]%s: i2c_read 0x%x fail\n", __func__, address);
            otp[index] = read_data & 0xff;
            address++;
        }
        
        s_ctrl->sensor_i2c_client->cci_client->sid = 0x56;
        msleep(1);
        read_data = 0;
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x56, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
            pr_err("[OTP]%s: i2c_read 0x56 failed\n", __func__);
        otp[index] = read_data & 0xff;
        index++;

        msleep(1);
        read_data = 0;
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x57, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
            pr_err("[OTP]%s: i2c_read 0x57 failed\n", __func__);
        otp[index] = read_data & 0xff;
        index++;

        msleep(1);
        read_data = 0;
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x58, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
            pr_err("[OTP]%s: i2c_read 0x58 failed\n", __func__);
        otp[index] = read_data & 0xff;
        index++;

        msleep(1);
        read_data = 0;
        rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x59, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0)
            pr_err("[OTP]%s: i2c_read 0x59 failed\n", __func__);
        otp[index] = read_data & 0xff;
        index++;

        s_ctrl->sensor_i2c_client->addr_type = MSM_CAMERA_I2C_WORD_ADDR;
        s_ctrl->sensor_i2c_client->cci_client->sid = cci_client_sid_backup >> 1;
    }

    if(cdata != NULL)
    {
    cdata->cfg.fuse.fuse_id_word1 = otp[11];
    cdata->cfg.fuse.fuse_id_word2 = otp[12];
    cdata->cfg.fuse.fuse_id_word3 = otp[13];
    cdata->cfg.fuse.fuse_id_word4 = otp[14];

    cdata->af_value.VCM_VENDOR = otp[0];
    cdata->af_value.VCM_VENDOR_ID_VERSION = otp[4];
    cdata->af_value.AF_INF_MSB = otp[5];
    cdata->af_value.AF_INF_LSB = otp[6];
    cdata->af_value.AF_MACRO_MSB = otp[9];
    cdata->af_value.AF_MACRO_LSB = otp[10];

    pr_info("%s: OTP Module vendor = 0x%x\n",               __func__,  otp[0]);
    pr_info("%s: OTP LENS = 0x%x\n",                        __func__,  otp[1]);
    pr_info("%s: OTP Sensor Version = 0x%x\n",              __func__,  otp[2]);
    pr_info("%s: OTP Driver IC Vendor & Version = 0x%x\n",  __func__,  otp[3]);
    pr_info("%s: OTP Actuator vender ID & Version = 0x%x\n",__func__,  otp[4]);

    pr_info("%s: OTP fuse 0 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word1);
    pr_info("%s: OTP fuse 1 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word2);
    pr_info("%s: OTP fuse 2 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word3);
    pr_info("%s: OTP fuse 3 = 0x%x\n", __func__,  cdata->cfg.fuse.fuse_id_word4);

    pr_info("%s: OTP Infinity position code (MSByte) = 0x%x\n", __func__,  cdata->af_value.AF_INF_MSB);
    pr_info("%s: OTP Infinity position code (LSByte) = 0x%x\n", __func__,  cdata->af_value.AF_INF_LSB);
    pr_info("%s: OTP Macro position code (MSByte) = 0x%x\n",    __func__,  cdata->af_value.AF_MACRO_MSB);
    pr_info("%s: OTP Macro position code (LSByte) = 0x%x\n",    __func__,  cdata->af_value.AF_MACRO_LSB);

    strlcpy(cdata->af_value.ACT_NAME, "lc898214_act", sizeof("lc898214_act"));
    pr_info("%s: OTP Actuator Name = %s\n",__func__, cdata->af_value.ACT_NAME);
	}
	else
	{
	    pr_info("%s: OTP Module vendor = 0x%x\n",               __func__,  otp[0]);
	    pr_info("%s: OTP LENS = 0x%x\n",                        __func__,  otp[1]);
	    pr_info("%s: OTP Sensor Version = 0x%x\n",              __func__,  otp[2]);
	    pr_info("%s: OTP Driver IC Vendor & Version = 0x%x\n",  __func__,  otp[3]);
	    pr_info("%s: OTP Actuator vender ID & Version = 0x%x\n",__func__,  otp[4]);
	}
    return rc;
}
#endif

#if 0
#ifdef HTC_OIS_HW_WORKAROUND
#define OTP_NVR0_DATA_SIZE 64
#define OTP_NVR1_DATA_SIZE 162
static uint8_t otp_NVR0_data[OTP_NVR0_DATA_SIZE];
static uint8_t otp_NVR1_data[OTP_NVR1_DATA_SIZE];
static int valid_otp_ois = 0;

static int imx377_htc_read_otp_ois_data(struct sensorb_cfg_data *cdata,
	struct msm_sensor_ctrl_t *s_ctrl)
{
    int32_t rc = 0;
    int32_t i = 0, j = 0;
    uint16_t read_data = 0;
    static int first = true;
    int valid_layer = -1;
    int retry_count = 0;

    if (first)
    {
        first = false;

        for(j = 5; j >=3; j--)
        {
            do {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A02, j, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A02 fail\n", __func__);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A00, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A00 fail\n", __func__);

                msleep(10);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A01, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_err("[OTP]%s: i2c_read 0x0A01 failed\n", __func__);

                if (rc < 0 || read_data != 1) {
                    if (retry_count > 5) {
                        pr_info("[OTP]%s: retry fail\n", __func__);
                        goto read_otp_ois_error;
                    } else{
                        retry_count++;
                        msleep(10);
                        pr_info("[OTP]%s: do retry mechanism , retry_count=%d\n", __func__, retry_count);
                    }
                }
            } while ( (rc < 0 || read_data != 1) && (retry_count <= 5) );

            for(i = 0; i < 64; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A04 + i, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                {
                    pr_err("[OTP]%s: i2c_read 0x%x failed\n", __func__, (0x0A04 + i));
                    goto read_otp_ois_error;
                }
                else
                {
                    otp_NVR0_data[i] = read_data;
                    if(read_data)
                        valid_layer = j;
                    read_data = 0;
                }
            }

            if(valid_layer != -1)
            {
                pr_info("[OTP]%s: valid_layer:%d \n", __func__,valid_layer);
                break;
            }
        }


        
        valid_layer = -1;
        for(j = 8; j >=6; j--)
        {
            do {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A02, j, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A02 fail\n", __func__);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A00, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A00 fail\n", __func__);

                msleep(10);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A01, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_err("[OTP]%s: i2c_read 0x0A01 failed\n", __func__);

                if (rc < 0 || read_data != 1) {
                    if (retry_count > 5) {
                        pr_info("[OTP]%s: retry fail\n", __func__);
                        goto read_otp_ois_error;
                    } else{
                        retry_count++;
                        msleep(10);
                        pr_info("[OTP]%s: do retry mechanism , retry_count=%d\n", __func__, retry_count);
                    }
                }
            }while ( (rc < 0 || read_data != 1) && (retry_count <= 5) );

            for(i = 0; i < 64; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A04 + i, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                {
                    pr_err("[OTP]%s: i2c_read 0x%x failed\n", __func__, (0x0A04 + i));
                    goto read_otp_ois_error;
                }
                else
                {
                    otp_NVR1_data[i] = read_data;
                    if(read_data)
                        valid_layer = j;
                    read_data = 0;
                }
            }

            if(valid_layer != -1)
            {
                pr_info("[OTP]%s: valid_layer:%d \n", __func__,valid_layer);
                break;
            }
        }

        
        valid_layer = -1;
        for(j = 11; j >=9; j--)
        {
            do {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A02, j, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A02 fail\n", __func__);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A00, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A00 fail\n", __func__);

                msleep(10);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A01, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_err("[OTP]%s: i2c_read 0x0A01 failed\n", __func__);

                if (rc < 0 || read_data != 1) {
                    if (retry_count > 5) {
                        pr_info("[OTP]%s: retry fail\n", __func__);
                        goto read_otp_ois_error;
                    } else{
                        retry_count++;
                        msleep(10);
                        pr_info("[OTP]%s: do retry mechanism , retry_count=%d\n", __func__, retry_count);
                    }
                }
            } while ( (rc < 0 || read_data != 1) && (retry_count <= 5) );


            for(i = 0; i < 64; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A04 + i, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                {
                    pr_err("[OTP]%s: i2c_read 0x%x failed\n", __func__, (0x0A04 + i));
                    goto read_otp_ois_error;
                }
                else
                {
                    otp_NVR1_data[64+i] = read_data;
                    if(read_data)
                        valid_layer = j;
                    read_data = 0;
                }
            }

            if(valid_layer != -1)
            {
                pr_info("[OTP]%s: valid_layer:%d \n", __func__,valid_layer);
                break;
            }
        }

        
        valid_layer = -1;
        for(j = 14; j >=12; j--)
        {
            do {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A02, j, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A02 fail\n", __func__);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, 0x0A00, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_info("[OTP]%s: i2c_write w 0x0A00 fail\n", __func__);

                msleep(10);

                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A01, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                    pr_err("[OTP]%s: i2c_read 0x0A01 failed\n", __func__);

                if (rc < 0 || read_data != 1) {
                    if (retry_count > 5) {
                        pr_info("[OTP]%s: retry fail\n", __func__);
                        goto read_otp_ois_error;
                    } else{
                        retry_count++;
                        msleep(10);
                        pr_info("[OTP]%s: do retry mechanism , retry_count=%d\n", __func__, retry_count);
                    }
                }
            } while ( (rc < 0 || read_data != 1) && (retry_count <= 5) );


            for(i = 0; i < 2; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, 0x0A04 + i, &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                {
                    pr_err("[OTP]%s: i2c_read 0x%x failed\n", __func__, (0x0A04 + i));
                    goto read_otp_ois_error;
                }
                else
                {
                    otp_NVR1_data[128+i] = read_data;
                    if(read_data)
                        valid_layer = j;
                    read_data = 0;
                }
            }

            for(i = 0; i < 32; i++)
            {
                rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client, (0x0A14 + i), &read_data, MSM_CAMERA_I2C_BYTE_DATA);
                if (rc < 0)
                {
                    pr_err("[OTP]%s: i2c_read 0x%x failed\n", __func__, (0x0A14 + i));
                    goto read_otp_ois_error;
                }
                else
                {
                    otp_NVR1_data[130+i] = read_data;
                    if(read_data)
                        valid_layer = j;
                    read_data = 0;
                }
            }

            if(valid_layer != -1)
            {
                pr_info("[OTP]%s: valid_layer:%d \n", __func__,valid_layer);
                break;
            }
        }
    }


    
    valid_otp_ois = 0;
    for(i=0; i < OTP_NVR0_DATA_SIZE; i++) {
        if (otp_NVR0_data[i])
	    valid_otp_ois = 1;
        pr_info("[OTP]%s : otp_NVR0_data[%d] = 0x%x\n", __func__, i, otp_NVR0_data[i]);
    }

    for(i=0; i < OTP_NVR1_DATA_SIZE; i++) {
        if (otp_NVR1_data[i])
	    valid_otp_ois = 1;
        pr_info("[OTP]%s : otp_NVR1_data[%d] = 0x%x\n", __func__, i, otp_NVR1_data[i]);
    }

    return rc;

read_otp_ois_error:
    valid_otp_ois = 0;
    return rc;
}


#define OIS_COMPONENT_I2C_ADDR_WRITE 0x7C

int htc_check_ois_component(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint16_t cci_client_sid_backup;

	
	cci_client_sid_backup = s_ctrl->sensor_i2c_client->cci_client->sid;

	
	s_ctrl->sensor_i2c_client->cci_client->sid = OIS_COMPONENT_I2C_ADDR_WRITE >> 1;

	
	lc898123_check_ois_component(s_ctrl, valid_otp_ois, otp_NVR0_data, otp_NVR1_data);

	
	s_ctrl->sensor_i2c_client->cci_client->sid = cci_client_sid_backup;

	return rc;
}
#endif
#endif


int32_t imx377_htc_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	int32_t rc1 = 0;
	static int first = 0;

	rc = msm_sensor_match_id(s_ctrl);
	if(rc == 0)
	{
	    if(first == 0)
	    {
	        pr_info("%s : read_fuseid\n", __func__);
#ifdef CONFIG_COMPAT
		rc1 = imx377_htc_read_fuseid32(NULL, s_ctrl);
#else
		rc1 = imx377_htc_read_fuseid(NULL, s_ctrl);
#endif
	        first = 1;

		
#if 0
#ifdef HTC_OIS_HW_WORKAROUND
	        pr_info("%s : read otp ois data\n", __func__);
	        imx377_htc_read_otp_ois_data(NULL, s_ctrl);

	        pr_info("%s : check ois component\n", __func__);
	        htc_check_ois_component(s_ctrl);
#endif
#endif
		
	    }
	}
	return rc;
}



static int32_t imx377_htc_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(imx377_htc_dt_match, &pdev->dev);
	if (match)
	{
		rc = msm_sensor_platform_probe(pdev, match->data);
	}
	else {
		pr_err("%s:%d, failed to match sensor\n", __func__, __LINE__);
		rc = -EINVAL;
	}
	return rc;
}

static void __init imx377_htc_init_module_async(void *unused, async_cookie_t cookie)
{
	int32_t rc = 0;
	async_synchronize_cookie(cookie);
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&imx377_htc_platform_driver,
		imx377_htc_platform_probe);
	if (!rc) {
		imx377_htc_sysfs_init();
		return;
	}
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	i2c_add_driver(&imx377_htc_i2c_driver);
}

static int __init imx377_htc_init_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	async_schedule(imx377_htc_init_module_async, NULL);
	return 0;
}

static void __exit imx377_htc_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (imx377_htc_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&imx377_htc_s_ctrl);
		platform_driver_unregister(&imx377_htc_platform_driver);
	} else
		i2c_del_driver(&imx377_htc_i2c_driver);
	return;
}

static struct msm_sensor_fn_t imx377_htc_sensor_func_tbl = {
	.sensor_config = msm_sensor_config,
#ifdef CONFIG_COMPAT
	.sensor_config32 = msm_sensor_config32,
#endif
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = imx377_htc_sensor_match_id,
	.sensor_i2c_read_fuseid = imx377_htc_read_fuseid,
#ifdef CONFIG_COMPAT
	.sensor_i2c_read_fuseid32 = imx377_htc_read_fuseid32,
#endif
};

static struct msm_sensor_ctrl_t imx377_htc_s_ctrl = {
	.sensor_i2c_client = &imx377_htc_sensor_i2c_client,
	.power_setting_array.power_setting = imx377_htc_power_setting,
	.power_setting_array.size = ARRAY_SIZE(imx377_htc_power_setting),
	.msm_sensor_mutex = &imx377_htc_mut,
	.sensor_v4l2_subdev_info = imx377_htc_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx377_htc_subdev_info),
	.func_tbl = &imx377_htc_sensor_func_tbl
};

module_init(imx377_htc_init_module);
module_exit(imx377_htc_exit_module);
MODULE_DESCRIPTION("imx377_htc");
MODULE_LICENSE("GPL v2");
