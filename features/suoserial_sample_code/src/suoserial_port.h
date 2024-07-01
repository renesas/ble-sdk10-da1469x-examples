/**
 ****************************************************************************************
 *
 * @file suoserial_port.h
 *
 * @brief SUOSERIAL port
 *
 * Copyright (c) 2024 Renesas Electronics Corporation and/or its affiliates
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

#ifndef SUOSERIAL_PORT_H_
#define SUOSERIAL_PORT_H_

#if defined(SUOUART_SUPPORT)
# include "ad_uart.h"

# define _SUOSERIAL_READ_DATA(_dev, _data, _len, _timeout)     ad_uart_read(_dev, _data, _len, _timeout)

# define _SUOSERIAL_WRITE_DATA(_dev, _data, _len, _timeout)    ad_uart_write(_dev, _data, _len)
#elif defined(SUOUSB_SUPPORT)
# include "USB_CDC.h"

# define _SUOSERIAL_READ_DATA(_dev, _data, _len, _timeout)     USBD_CDC_Receive(_dev, _data, _len, _timeout)

# define _SUOSERIAL_WRITE_DATA(_dev, _data, _len, _timeout)    USBD_CDC_Write(_dev, _data, _len, _timeout)
#endif

/* Default values */
#ifndef _SUOSERIAL_READ_DATA
   #define _SUOSERIAL_READ_DATA(_dev, _data, _len, _timeout)
#endif

#ifndef _SUOSERIAL_WRITE_DATA
   #define _SUOSERIAL_WRITE_DATA(_dev, _data_len, _timeout)
#endif

/**
 * Application-defined routine to read data over the serial interface
 *
 * \param[in] _dev       Handle of a valid serial device instance (typically acquired via \sa SERIAL_PORT_OPEN())
 * \param[in] _data      Pointer to a buffer where the received data will be stored
 * \param[in] _len       Number of bytes to read
 * \param[in] _timeout   Timeout after which the initiated read operation should be terminated
 *
 * \return The number of bytes received within the given timeout
 *
 */
#define SUOSERIAL_READ_DATA(_dev, _data, _len, _timeout)   _SUOSERIAL_READ_DATA(_dev, _data, _len, _timeout)

/**
 * Application-defined routine to write data over the serial interface
 *
 * \param[in] _dev       Handle of a valid serial device instance (typically acquired via \sa SERIAL_PORT_OPEN())
 * \param[in] _data      Pointer to data that should be sent over the serial interface
 * \param[in] _len       Number of bytes to be transmitted
 * \param[in] _timeout   Timeout after which the initiated write operation should be terminated
 *
 * \return The actual number of bytes transmitted before the timeout expiration.
 *
 */
#define SUOSERIAL_WRITE_DATA(_dev, _data, _len, _timeout)  _SUOSERIAL_WRITE_DATA(_dev, _data, _len, _timeout)

#endif /* SUOSERIAL_PORT_H_ */
