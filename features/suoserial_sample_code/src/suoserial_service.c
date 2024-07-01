/**
 ****************************************************************************************
 *
 * @file suoserial_service.c
 *
 * @brief SUOSERIAL service implementation API
 *
 * Copyright (C) 2016-2024 Renesas Electronics Corporation and/or its affiliates
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 ****************************************************************************************
 */
#if defined(SUOUART_SUPPORT) || defined(SUOUSB_SUPPORT)
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "osal.h"
#include "ad_nvms.h"
#include "hw_watchdog.h"
#include "hw_cpm.h"
#include "suota.h"
#include "sdk_crc16.h"
#include "suoserial_service.h"

#define SUOSERIAL_BUFFER_SIZE      (512)

/* Size of fixed-place data in product header (identifier ... length of flash config. section) */
#define PH_STATIC_DATA_SIZE            (22)
#define PH_UPDATE_FW_ADDRESS_OFFSET    (6)
#define PH_WRITE_RETRY_TRIES_NUM       (5)


typedef enum {
        SUOSERIAL_STATE_IDLE,
        SUOSERIAL_STATE_W4_HEADER,
        SUOSERIAL_STATE_W4_HEADER_EXT,
        SUOSERIAL_STATE_W4_IMAGE_DATA,
        SUOSERIAL_STATE_DONE,
        SUOSERIAL_STATE_ERROR,
} suoserial_state_t;

/**
 * SUOUART status
 *
 * As defined by Dialog SUOTA specification.
 *
 */
typedef enum {
        /* Value zero must not be used !! Notifications are sent when status changes. */
        SUOSERIAL_SRV_STARTED      = 0x01,     // Valid memory device has been configured by initiator. No sleep state while in this mode
        SUOSERIAL_CMP_OK           = 0x02,     // SUOSERIAL process completed successfully.
        SUOSERIAL_SRV_EXIT         = 0x03,     // Forced exit of SUOSERIAL service.
        SUOSERIAL_CRC_ERR          = 0x04,     // Overall Patch Data CRC failed
        SUOSERIAL_PATCH_LEN_ERR    = 0x05,     // Received patch Length not equal to PATCH_LEN characteristic value
        SUOSERIAL_EXT_MEM_WRITE_ERR= 0x06,     // External Mem Error (Writing to external device failed)
        SUOSERIAL_INT_MEM_ERR      = 0x07,     // Internal Mem Error (not enough space for Patch)
        SUOSERIAL_INVAL_MEM_TYPE   = 0x08,     // Invalid memory device
        SUOSERIAL_APP_ERROR        = 0x09,     // Application error

        /* SUOSERIAL application specific error codes */
        SUOSERIAL_IMG_STARTED      = 0x10,     // SUOSERIAL started for downloading image (SUOSERIAL application)
        SUOSERIAL_INVAL_IMG_BANK   = 0x11,     // Invalid image bank
        SUOSERIAL_INVAL_IMG_HDR    = 0x12,     // Invalid image header
        SUOSERIAL_INVAL_IMG_SIZE   = 0x13,     // Invalid image size
        SUOSERIAL_INVAL_PRODUCT_HDR= 0x14,     // Invalid product header
        SUOSERIAL_SAME_IMG_ERR     = 0x15,     // Same Image Error
        SUOSERIAL_EXT_MEM_READ_ERR = 0x16,     // Failed to read from external memory device
} suoserial_status_t;

const char *suoserial_status_str[] = {
        "INVALID:0",
        "SUOSERIAL_SRV_STARTED",           //= 0x01,     // Valid memory device has been configured by initiator. No sleep state while in this mode
        "SUOSERIAL_CMP_OK",                //= 0x02,     // SUOUSB process completed successfully.
        "SUOSERIAL_SRV_EXIT",              //= 0x03,     // Forced exit of SUOUSB service.
        "SUOSERIAL_CRC_ERR",               //= 0x04,     // Overall Patch Data CRC failed
        "SUOSERIAL_PATCH_LEN_ERR",         //= 0x05,     // Received patch Length not equal to PATCH_LEN characteristic value
        "SUOSERIAL_EXT_MEM_WRITE_ERR",     //= 0x06,     // External Mem Error (Writing to external device failed)
        "SUOSERIAL_INT_MEM_ERR",           //= 0x07,     // Internal Mem Error (not enough space for Patch)
        "SUOSERIAL_INVAL_MEM_TYPE",        //= 0x08,     // Invalid memory device
        "SUOSERIAL_APP_ERROR",             //= 0x09,     // Application error

        "INVALID:A",
        "INVALID:B",
        "INVALID:C",
        "INVALID:D",
        "INVALID:E",
        "INVALID:F",

        /* SUOUSB application specific error codes */
        "SUOSERIAL_IMG_STARTED",           //= 0x10,     // SUOUSB started for downloading image (SUOUSB application)
        "SUOSERIAL_INVAL_IMG_BANK",        //= 0x11,     // Invalid image bank
        "SUOSERIAL_INVAL_IMG_HDR",         //= 0x12,     // Invalid image header
        "SUOSERIAL_INVAL_IMG_SIZE",        //= 0x13,     // Invalid image size
        "SUOSERIAL_INVAL_PRODUCT_HDR",     //= 0x14,     // Invalid product header
        "SUOSERIAL_SAME_IMG_ERR",          //= 0x15,     // Same Image Error
        "SUOSERIAL_EXT_MEM_READ_ERR",      //= 0x16,     // Failed to read from external memory device

        "INVALID:17",
};

/**
 * SUOSERIAL commands
 *
 * As defined by Dialog SUOTA specification.
 *
 */
typedef enum {
        SUOSERIAL_IMG_INT_SYSRAM = 0x10,
        SUOSERIAL_IMG_INT_RETRAM = 0x11,
        SUOSERIAL_IMG_I2C_EEPROM = 0x12,
        SUOSERIAL_IMG_SPI_FLASH  = 0x13,

        /* DO NOT move. Must be before commands */
        SUOSERIAL_MEM_INVAL_DEV  = 0x14,

        SUOSERIAL_REBOOT         = 0xFD,
        SUOSERIAL_IMG_END        = 0xFE,

        /*
         * When initiator selects 0xff, it wants to exit SUOSERIAL service.
         * This is used in case of unexplained failures. If SUOSERIAL process
         * finishes correctly it will exit automatically.
         */
        SUOSERIAL_MEM_SERVICE_EXIT   = 0xFF,
} suoserial_commands_t;

typedef struct suoserial_service suoserial_service_t;

/** SUOSERIAL status callback during image transfer */
typedef void (* suoserial_error_cb_t) (suoserial_service_t *suoserial, suoserial_status_t status);

/** SUOSERIAL callback after full chunk is received during image transfer */
typedef void (* suoserial_chunk_cb_t) (suoserial_service_t *suoserial);

typedef struct suoserial_service {
        suoserial_state_t state;            // image transfer state
        suoserial_chunk_cb_t chunk_cb;      // called on every 'patch_len' bytes of data received
        suoserial_error_cb_t error_cb;      // called in case of error during image transfer

        uint8_t *buffer;
        uint16_t buffer_len;

        suota_1_1_image_header_da1469x_t header;
        uint32_t product_header_address;

        uint16_t chunk_len;             // length of data received in current chunk (depends on 'patch_len')
        uint32_t image_crc;             // CRC of received image
        uint32_t recv_total_len;        // length of received data
        uint32_t recv_hdr_ext_len;      // length of received header extension
        uint32_t recv_image_len;        // length of received image
        uint32_t flash_write_addr;      // flash address where data will be written to
        uint32_t flash_erase_addr;      // flash address which is not yet erased (assume everything prior to this address is erased)

        uint16_t patch_len;
        uint16_t conn_idx;

        nvms_t  nvms;
} suoserial_service_t;

static suoserial_service_t SUOSERIAL_svc;
static suoserial_service_t *pSUOSERIAL_svc;

static suoserial_notify_cb_t ext_notify_cb;

static uint16_t suoserial_external_status;

static const uint32_t crc32_tab[] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static nvms_t suoserial_open_suota_fw_partition(uint32_t* product_header_address);

static uint32_t suoserial_update_crc(uint32_t crc, const uint8_t *data, size_t len)
{
        while (len--) {
                crc = crc32_tab[(crc ^ *data++) & 0xff] ^ (crc >> 8);
        }
        return crc;
}

__STATIC_INLINE uint16_t suoserial_get_u16(const uint8_t *buffer)
{
        return (buffer[0]) | (buffer[1] << 8);
}

__STATIC_INLINE uint32_t suoserial_get_u32(const uint8_t *buffer)
{
        return (buffer[0]) | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
}

static void suoserial_notify_client_status(suoserial_service_t *suoserial, uint8_t status)
{
        if (ext_notify_cb) {
                ext_notify_cb(suoserial_status_str[status]);
        }
}

static suoserial_error_t suoserial_do_bl_ccc_write(uint16_t offset, uint16_t length, const uint8_t *value)
{
        if (offset) {
                return SUOSERIAL_ERROR_ATTRIBUTE_NOT_LONG;
        }

        if (length != sizeof(suoserial_external_status)) {
                return SUOSERIAL_ERROR_APPLICATION_ERROR;
        }

        suoserial_external_status = suoserial_get_u16(value);

        return SUOSERIAL_ERROR_OK;
}

static bool suoserial_safe_product_header_write(nvms_t ph_part, size_t offset, const uint8_t *ph, uint16_t ph_size)
{
        int retry_cnt;
        uint8_t *ph_read = OS_MALLOC(ph_size);

        if (!ph_read) {
                return false;
        }

        for (retry_cnt = 0; retry_cnt < PH_WRITE_RETRY_TRIES_NUM; ++retry_cnt) {
                if (ad_nvms_write(ph_part, offset, ph, ph_size) !=  ph_size) {
                        continue;
                }

                if (ad_nvms_read(ph_part, offset, ph_read, ph_size) != ph_size) {
                        continue;
                }

                if (memcmp(ph_read, ph, ph_size)) {
                        continue;
                }

                /* Product header was written successfully */
                break;
        }

        OS_FREE(ph_read);

        return retry_cnt < PH_WRITE_RETRY_TRIES_NUM;
}

static bool suoserial_set_active_img_ptr(suoserial_service_t *suoserial)
{
        int written;

        nvms_t product_header_partition;
        const void* received_fw_partition_address;
        uint32_t received_fw_partition_offset;
        uint16_t fcs_size;
        uint16_t ph_size;
        uint16_t crc16;
        uint8_t *ph_buffer;

        /* Write FW header */
        written = ad_nvms_write(suoserial->nvms, 0, (uint8_t *)&suoserial->header, sizeof(suoserial->header));

        if (written != sizeof(suoserial->header)) {
                return false;
        }

        /* Open product header partition */
        product_header_partition = ad_nvms_open(NVMS_PRODUCT_HEADER_PART);
        if (!product_header_partition) {
                return false;
        }

        /* Get address of the newly received FW partition */
        if (!ad_nvms_get_pointer(suoserial->nvms, 0, 1, &received_fw_partition_address)) {
                return false;
        }

        /* Update 'Update FW address' field on product header */
        received_fw_partition_offset = (uint32_t) received_fw_partition_address - MEMORY_QSPIF_S_BASE;

        /* Read flash configuration section size */
        if (ad_nvms_read(product_header_partition, PH_STATIC_DATA_SIZE - 2, (uint8_t *)&fcs_size,
                                                        sizeof(fcs_size)) != sizeof(fcs_size)) {
                return false;
        }

        /* Buffer for product header: static-size data + dynamic-size data + CRC (2 bytes) */
        ph_size = PH_STATIC_DATA_SIZE + fcs_size + 2;
        ph_buffer = OS_MALLOC(ph_size);

        if (!ph_buffer) {
                return false;
        }

        /* Read whole product header */
        if (ad_nvms_read(product_header_partition, 0, ph_buffer, ph_size) != ph_size) {
                OS_FREE(ph_buffer);
                return false;
        }

        /* Change update image address in buffer */
        memcpy(&ph_buffer[PH_UPDATE_FW_ADDRESS_OFFSET], &received_fw_partition_offset,
                                                        sizeof(received_fw_partition_offset));

        /* Compute CRC16-CCITT */
        crc16 = crc16_calculate(ph_buffer, ph_size - 2);
        memcpy(&ph_buffer[ph_size - 2], &crc16, sizeof(crc16));

        /* Update backup product header */
        if (!suoserial_safe_product_header_write(product_header_partition, AD_FLASH_SECTOR_SIZE, ph_buffer, ph_size)) {
                OS_FREE(ph_buffer);
                return false;
        }

        /* Update primary product header */
        if (!suoserial_safe_product_header_write(product_header_partition, 0, ph_buffer, ph_size)) {
                OS_FREE(ph_buffer);
                return false;
        }

        OS_FREE(ph_buffer);
        return true;

}

static void suoserial_prepare_flash(suoserial_service_t *suoserial, size_t write_size)
{
        uint32_t* absolute_start_addr;
        uint32_t* absolute_end_addr;
        bool already_erased = true;
        uint32_t end_addr = suoserial->flash_write_addr + write_size - 1;
        size_t erase_size;

        /* If flash is already erased in required range, do nothing */
        if (end_addr < suoserial->flash_erase_addr) {
                return;
        }

        erase_size = end_addr - suoserial->flash_erase_addr + 1;

        /* Check if region [suoserial->flash_erase_addr, suoserial->flash_erase_addr + AD_FLASH_SECTOR_SIZE) is
         * already erased. If it is, skip erase. */
        ad_nvms_get_pointer(suoserial->nvms, suoserial->flash_erase_addr, 1, (const void **)&absolute_start_addr);
        absolute_end_addr = (uint32_t* )((uint32_t) absolute_start_addr + AD_FLASH_SECTOR_SIZE);
        while (absolute_start_addr < absolute_end_addr) {
                if (*absolute_start_addr++ != 0xFFFFFFFF) {
                        already_erased = false;
                        break;
                }
        }

        if (!already_erased) {
                ad_nvms_erase_region(suoserial->nvms, suoserial->flash_erase_addr, erase_size);
        }

        /*
         * Assume everything up to sector boundary is erased now, next erase address is at the
         * beginning of following sector
         */
        suoserial->flash_erase_addr += erase_size;
        suoserial->flash_erase_addr |= (AD_FLASH_SECTOR_SIZE - 1);
        suoserial->flash_erase_addr++;
}

__STATIC_INLINE void suoserial_error_cb(suoserial_service_t *suoserial, suoserial_status_t status)
{
        suoserial_notify_client_status(suoserial, status);
        suoserial->state = SUOSERIAL_STATE_ERROR;
}

__STATIC_INLINE void suoserial_chunk_cb(suoserial_service_t *suoserial)
{
        suoserial_notify_client_status(suoserial, SUOSERIAL_CMP_OK);
}

static size_t suoserial_pull_to_buffer(suoserial_service_t *suoserial, const uint8_t *data, size_t len,
        size_t expected_len)
{
        /* Caller guarantees that we'll pull data only up to buffer capacity */
        OS_ASSERT(expected_len <= SUOSERIAL_BUFFER_SIZE);
        /* We shall never have more than expected_len bytes of data in buffer */
        OS_ASSERT(suoserial->buffer_len <= expected_len);

        /* Required amount of data is already in buffer */
        if (suoserial->buffer_len >= expected_len) {
                return 0;
        }

        expected_len -= suoserial->buffer_len;
        if (expected_len > len) {
                expected_len = len;
        }

        memcpy(suoserial->buffer + suoserial->buffer_len, data, expected_len);
        suoserial->buffer_len += expected_len;

        return expected_len;
}

__STATIC_INLINE bool suoserial_validate_img_hdr(suota_1_1_image_header_da1469x_t *hdr)
{
        return (hdr->image_identifier[0] == SUOTA_1_1_IMAGE_DA1469x_HEADER_SIGNATURE_B1 &&
                hdr->image_identifier[1] == SUOTA_1_1_IMAGE_DA1469x_HEADER_SIGNATURE_B2);
}

__STATIC_INLINE uint32_t suoserial_get_code_size(suota_1_1_image_header_da1469x_t *hdr)
{
        return hdr->size;
}

__STATIC_INLINE uint32_t suoserial_get_exec_location(suota_1_1_image_header_da1469x_t *hdr)
{
        return hdr->pointer_to_ivt;
}

__STATIC_INLINE bool suoserial_validate_img_size(suoserial_service_t *suoserial)
{

        /* SUOTA 1.1 header + header extension + application code */
        return (suoserial_get_exec_location(&suoserial->header) + suoserial_get_code_size(&suoserial->header)
                <= ad_nvms_get_size(suoserial->nvms));
}

static bool suoserial_state_w4_header(suoserial_service_t *suoserial)
{
        memcpy(&suoserial->header, suoserial->buffer, sizeof(suoserial->header));

        if (!suoserial_validate_img_hdr(&suoserial->header)) {
                suoserial->error_cb(suoserial, SUOSERIAL_INVAL_IMG_HDR);
                return false;
        }

        if (suoserial->header.image_identifier[0] == SUOTA_1_1_IMAGE_DA1469x_HEADER_SIGNATURE_B1 &&
                suoserial->header.image_identifier[1] == SUOTA_1_1_IMAGE_DA1469x_HEADER_SIGNATURE_B2) {
                if (NULL == (suoserial->nvms = suoserial_open_suota_fw_partition(NULL))) {
                        suoserial->error_cb(suoserial, SUOSERIAL_INVAL_IMG_BANK);
                        return true;
                }
        }

        if (!suoserial_validate_img_size(suoserial)) {
                suoserial->error_cb(suoserial, SUOSERIAL_INVAL_IMG_SIZE);
                return false;
        }

        /* Erase flash for header, but don't write now - postpone until image is downloaded */
        suoserial_prepare_flash(suoserial, sizeof(suoserial->header));
        suoserial->flash_write_addr += sizeof(suoserial->header);

        suoserial->state = SUOSERIAL_STATE_W4_HEADER_EXT;

        return true;
}

static bool suoserial_state_w4_header_ext(suoserial_service_t *suoserial)
{
        int written;

        /* Write header extension before image's data */
        suoserial_prepare_flash(suoserial, suoserial->buffer_len);
        written = ad_nvms_write(suoserial->nvms, suoserial->flash_write_addr, suoserial->buffer, suoserial->buffer_len);
        if (written < 0) {
                return false;
        }

        suoserial->flash_write_addr += written;
        suoserial->recv_hdr_ext_len += written;

        if (suoserial->recv_hdr_ext_len == suoserial_get_exec_location(&suoserial->header) - sizeof(suoserial->header)) {
                suoserial->state = SUOSERIAL_STATE_W4_IMAGE_DATA;
        }

        return (written == suoserial->buffer_len);
}

static bool suoserial_state_w4_image_data(suoserial_service_t *suoserial)
{
        int write_count;
        int read_count;

        suoserial_prepare_flash(suoserial, suoserial->buffer_len);

        write_count = ad_nvms_write(suoserial->nvms, suoserial->flash_write_addr, suoserial->buffer, suoserial->buffer_len);
        if (write_count == suoserial->buffer_len) {
                /* Calculate CRC based on the contents of NVMS */
                read_count = ad_nvms_read(suoserial->nvms, suoserial->flash_write_addr, suoserial->buffer, suoserial->buffer_len);
                if (read_count == suoserial->buffer_len) {
                        suoserial->flash_write_addr += write_count;
                        suoserial->image_crc = suoserial_update_crc(suoserial->image_crc, suoserial->buffer, write_count);
                        suoserial->recv_image_len += write_count;
                        if (suoserial->recv_image_len == suoserial_get_code_size(&suoserial->header)) {
                                suoserial->state = SUOSERIAL_STATE_DONE;
                        }
                        return true;
                }
        }

        return false;
}

static bool suoserial_process_patch_data(suoserial_service_t *suoserial, const uint8_t *data,
                size_t len, size_t *consumed)
{
        size_t expected_len;
        bool ret = false;

        /*
         * First make sure data buffer holds proper number of bytes required in current state.
         * We will only fetch exactly the number of bytes required, this makes processing simpler.
         */

        switch (suoserial->state) {
        case SUOSERIAL_STATE_W4_HEADER:
                expected_len = sizeof(suota_1_1_image_header_da1469x_t);
                break;
        case SUOSERIAL_STATE_W4_HEADER_EXT:
                expected_len = suoserial_get_exec_location(&suoserial->header) -
                        sizeof(suoserial->header) - suoserial->recv_hdr_ext_len;
                if (expected_len > SUOSERIAL_BUFFER_SIZE) {
                        expected_len = SUOSERIAL_BUFFER_SIZE;
                }
                break;
        case SUOSERIAL_STATE_W4_IMAGE_DATA:
                /* Fetch as much as possible, until expected end of image */
                expected_len = suoserial_get_code_size(&suoserial->header) - suoserial->recv_image_len;
                if (expected_len > SUOSERIAL_BUFFER_SIZE) {
                        expected_len = SUOSERIAL_BUFFER_SIZE;
                }
                break;
        case SUOSERIAL_STATE_DONE:
                /* Just ignore any trailing data */
                *consumed = len;
                return true;
        case SUOSERIAL_STATE_IDLE:
        case SUOSERIAL_STATE_ERROR:
        default:
                return false;
        }

        *consumed = suoserial_pull_to_buffer(suoserial, data, len, expected_len);
        if (suoserial->buffer_len < expected_len) {
                return true;
        }

        /*
         * Now we have data fetched into buffer. State handler is expected to consume all available
         * data from buffer so we don't need to check for this.
         */

        switch (suoserial->state) {
        case SUOSERIAL_STATE_W4_HEADER:
                ret = suoserial_state_w4_header(suoserial);
                break;
        case SUOSERIAL_STATE_W4_HEADER_EXT:
                if (suoserial->header.image_identifier[0] == SUOTA_1_1_IMAGE_DA1469x_HEADER_SIGNATURE_B1 &&
                        suoserial->header.image_identifier[1] == SUOTA_1_1_IMAGE_DA1469x_HEADER_SIGNATURE_B2) {
                        ret = suoserial_state_w4_header_ext(suoserial);
                } else {
                        suoserial->state = SUOSERIAL_STATE_W4_IMAGE_DATA;
                        ret = true;
                }
                break;
        case SUOSERIAL_STATE_W4_IMAGE_DATA:
                ret = suoserial_state_w4_image_data(suoserial);
                break;
        case SUOSERIAL_STATE_IDLE:
        case SUOSERIAL_STATE_DONE:
        case SUOSERIAL_STATE_ERROR:
                /* We should never reach this since these states should already return above */
                OS_ASSERT(0);
                break;
        }

        suoserial->buffer_len = 0;

        return ret;
}

static bool suoserial_handle_patch_data(suoserial_service_t *suoserial, const uint8_t *data, size_t recv_len)
{
        size_t len = 0;
        bool ret;

        if (!suoserial->buffer) {
                return false;
        }

        suoserial->recv_total_len += recv_len;

        do {
                size_t consumed = 0;

                ret = suoserial_process_patch_data(suoserial, data + len, recv_len - len, &consumed);
                len += consumed;
        } while (ret && len < recv_len);

        if (suoserial->chunk_cb) {
                suoserial->chunk_len += len;

                while (suoserial->chunk_len >= suoserial->patch_len) {
                        suoserial->chunk_cb(suoserial);
                        suoserial->chunk_len -= suoserial->patch_len;
                }
        }

        return ret;
}

static suoserial_error_t suoserial_do_mem_dev_write(suoserial_service_t *suoserial, uint16_t offset,
                uint16_t length, const uint8_t *value)
{
        uint8_t cmd;

        if (offset) {
                return SUOSERIAL_ERROR_ATTRIBUTE_NOT_LONG;
        }

        if (length != sizeof(uint32_t)) {
                return SUOSERIAL_ERROR_APPLICATION_ERROR;
        }

        cmd = suoserial_get_u32(value) >> 24;

        if (cmd < SUOSERIAL_MEM_INVAL_DEV) {
                suoserial->flash_write_addr = 0x0;
                suoserial->flash_erase_addr = suoserial->flash_write_addr;
        }

        switch (cmd) {
        case SUOSERIAL_IMG_SPI_FLASH:
                if (!suoserial->buffer) {
                        suoserial->buffer = OS_MALLOC((sizeof(uint8_t) * SUOSERIAL_BUFFER_SIZE) + 2);
                        if (!suoserial->buffer) {
                                suoserial_notify_client_status(suoserial, SUOSERIAL_SRV_EXIT);
                                return SUOSERIAL_ERROR_OK;
                        }
                }

                suoserial->recv_hdr_ext_len = 0;

                suoserial->buffer_len = 0;
                suoserial->state = SUOSERIAL_STATE_W4_HEADER;
                suoserial->chunk_len = 0;
                suoserial->recv_total_len = 0;
                suoserial->recv_image_len = 0;
                suoserial->image_crc = 0xFFFFFFFF;

                suoserial_notify_client_status(suoserial, SUOSERIAL_IMG_STARTED);
                break;
        case SUOSERIAL_REBOOT:
                hw_cpm_reboot_system();
                break;
        case SUOSERIAL_IMG_END:
                suoserial->image_crc ^= 0xFFFFFFFF;
                if (suoserial->image_crc != suoserial->header.crc) {
                        suoserial_notify_client_status(suoserial, SUOSERIAL_CRC_ERR);
                } else {
                        if (!suoserial_set_active_img_ptr(suoserial)) {
                                suoserial_notify_client_status(suoserial, SUOSERIAL_APP_ERROR);
                                return SUOSERIAL_ERROR_APPLICATION_ERROR;
                        } else {
                                suoserial_notify_client_status(suoserial, SUOSERIAL_CMP_OK);
                        }
                }
                break;
        case SUOSERIAL_MEM_SERVICE_EXIT:
                if (suoserial->buffer) {
                        OS_FREE(suoserial->buffer);
                        suoserial->buffer = NULL;
                }

                suoserial_notify_client_status(suoserial, SUOSERIAL_SRV_EXIT);
                break;
        }

        return SUOSERIAL_ERROR_OK;
}

static suoserial_error_t suoserial_do_patch_len_write(suoserial_service_t *suoserial, uint16_t offset,
                uint16_t length, const uint8_t *value)
{
        if (offset) {
                return SUOSERIAL_ERROR_ATTRIBUTE_NOT_LONG;
        }

        if (length != sizeof(suoserial->patch_len)) {
                return SUOSERIAL_ERROR_APPLICATION_ERROR;
        }

        suoserial->error_cb = suoserial_error_cb;
        suoserial->chunk_cb = suoserial_chunk_cb;

        suoserial->patch_len = suoserial_get_u16(value);

        return SUOSERIAL_ERROR_OK;
}

static suoserial_error_t suoserial_do_patch_data_write(suoserial_service_t *suoserial, uint16_t offset,
                uint16_t length, const uint8_t *value)
{
        bool ret;

        ret = suoserial_handle_patch_data(suoserial, value, length);

        return (ret ? SUOSERIAL_ERROR_OK : SUOSERIAL_ERROR_APPLICATION_ERROR);
}

suoserial_error_t suoserial_read_req(suoserial_write_request_t req, uint32_t *value)
{
        suoserial_error_t status = SUOSERIAL_ERROR_READ_NOT_PERMITTED;

        switch (req) {
        case SUOSERIAL_READ_STATUS:
                *value = suoserial_external_status;
                status = SUOSERIAL_ERROR_OK;
                break;
        case SUOSERIAL_READ_MEMINFO:
                *value = pSUOSERIAL_svc->recv_total_len;
                status = SUOSERIAL_ERROR_OK;
                break;
        default:
                break;
        }

        return status;
}

suoserial_error_t suoserial_write_req(suoserial_write_request_t req, uint16_t offset,
                uint16_t length, const uint8_t *value)
{
        suoserial_error_t status = SUOSERIAL_ERROR_ATTRIBUTE_NOT_FOUND;

        switch (req) {
        case SUOSERIAL_WRITE_STATUS:
                status = suoserial_do_bl_ccc_write(offset, length, value);
                break;
        case SUOSERIAL_WRITE_MEMDEV:
                status = suoserial_do_mem_dev_write(pSUOSERIAL_svc, offset, length, value);
                break;
        case SUOSERIAL_WRITE_PATCH_LEN:
                status = suoserial_do_patch_len_write(pSUOSERIAL_svc, offset, length, value);
                break;
        case SUOSERIAL_WRITE_PATCH_DATA:
                status = suoserial_do_patch_data_write(pSUOSERIAL_svc, offset, length, value);
                break;
        default:
                break;
        }

        return status;
}

static nvms_t suoserial_open_suota_fw_partition(uint32_t* product_header_address)
{
        nvms_t product_header_partition;
        nvms_t fw_partition1;
        nvms_t fw_partition2;

        const void* product_header_partition_addr;
        const void* fw_partition1_addr;
        const void* fw_partition2_addr;

        uint8_t signature[2];
        uint32_t active_fw_offset;

        /* Open product header partition */
        product_header_partition = ad_nvms_open(NVMS_PRODUCT_HEADER_PART);
        if (!product_header_partition) {
                return NULL;
        }

        /*
         * Get absolute address of product_header_partition and make sure that
         * it is possible to read the first 6 bytes (identifier & active FW addresses)
         */
        if (ad_nvms_get_pointer(product_header_partition, 0, 6, &product_header_partition_addr) != 6) {
                return NULL;
        }

        if (product_header_address) {
                *product_header_address = (uint32_t) product_header_partition_addr;
        }

        /* Check signature */
        if (ad_nvms_read(product_header_partition, 0, (uint8_t*) &signature, sizeof(signature)) != sizeof(signature)) {
                return NULL;
        }

        if (signature[0] != SUOTA_1_1_PRODUCT_DA1469x_HEADER_SIGNATURE_B1 ||
                signature[1] != SUOTA_1_1_PRODUCT_DA1469x_HEADER_SIGNATURE_B2) {
                return NULL;
        }

        /* Get active image offset */
        if (ad_nvms_read(product_header_partition, 2, (uint8_t*) &active_fw_offset, sizeof(active_fw_offset)) != sizeof(active_fw_offset)) {
                return NULL;
        }

        /* Open FW partition 1 */
        fw_partition1 = ad_nvms_open(NVMS_FW_EXEC_PART);
        if (!fw_partition1) {
                return NULL;
        }

        /* Get absolute address of fw_partition1 */
        if (!ad_nvms_get_pointer(fw_partition1, 0, 1, &fw_partition1_addr)) {
                return NULL;
        }

        /* Open FW partition 2 */
        fw_partition2 = ad_nvms_open(NVMS_FW_UPDATE_PART);
        if (!fw_partition2) {
                return NULL;
        }

        /* Get absolute address of fw_partition2 */
        if (!ad_nvms_get_pointer(fw_partition2, 0, 1, &fw_partition2_addr)) {
                return NULL;
        }

        if (MEMORY_QSPIF_S_BASE + active_fw_offset == (uint32_t) fw_partition1_addr) {
                return fw_partition2; /* fw_partition1 active, use fw_partition2 for update */
        } else if (MEMORY_QSPIF_S_BASE + active_fw_offset == (uint32_t) fw_partition2_addr) {
                return fw_partition1; /* fw_partition2 active, use fw_partition1 for update */
        } else {
                return NULL;
        }
}

int suoserial_init(suoserial_notify_cb_t cb)
{
        nvms_t nvms;

        uint32_t product_header_address;

        nvms = suoserial_open_suota_fw_partition(&product_header_address);

        if (!nvms) {
                OS_ASSERT(0);
        }

        pSUOSERIAL_svc = &SUOSERIAL_svc;

        ext_notify_cb = cb;
        pSUOSERIAL_svc->product_header_address = product_header_address;


        return 1;
}
#endif /* defined(SUOUART_SUPPORT) || defined(SUOUSB_SUPPORT) */
