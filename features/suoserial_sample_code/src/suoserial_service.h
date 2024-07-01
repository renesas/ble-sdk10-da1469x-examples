/**
 ****************************************************************************************
 *
 * @file suoserial_service.h
 *
 * @brief SUOSERIAL service implementation API
 *
 * Copyright (C) 2016-2022 Renesas Electronics Corporation and/or its affiliates
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

#ifndef SUOSERIAL_SERVICE_H_
#define SUOSERIAL_SERVICE_H_

#if defined(SUOUART_SUPPORT) || defined(SUOUSB_SUPPORT)

typedef void (* suoserial_notify_cb_t) (const char *status);

typedef enum {
        SUOSERIAL_ERROR_OK,
        SUOSERIAL_ERROR_READ_NOT_PERMITTED,
        SUOSERIAL_ERROR_REQUEST_NOT_SUPPORTED,
        SUOSERIAL_ERROR_ATTRIBUTE_NOT_LONG,
        SUOSERIAL_ERROR_ATTRIBUTE_NOT_FOUND,
        SUOSERIAL_ERROR_APPLICATION_ERROR,
} suoserial_error_t;

typedef enum {
        SUOSERIAL_READ_STATUS,
        SUOSERIAL_READ_MEMINFO,
        SUOSERIAL_WRITE_STATUS,
        SUOSERIAL_WRITE_MEMDEV,
        SUOSERIAL_WRITE_PATCH_LEN,
        SUOSERIAL_WRITE_PATCH_DATA
} suoserial_write_request_t;

/**
 * Register SUOUART Notify callback, and set size of buffer to use
 */
suoserial_error_t suoserial_write_req(suoserial_write_request_t req, uint16_t offset,
                uint16_t length, const uint8_t *value);
suoserial_error_t suoserial_read_req(suoserial_write_request_t req, uint32_t *value);

/**
 * Initialization of SUOUART Service instance
 *
 * \return error
 */
int suoserial_init(suoserial_notify_cb_t cb);

#endif /* defined(SUOUART_SUPPORT) || defined(SUOUSB_SUPPORT) */

#endif /* SUOSERIAL_SERVICE_H_ */
