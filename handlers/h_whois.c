/**************************************************************************
*
* Copyright (C) 2005 Steve Karg <skarg@users.sourceforge.net>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bacdcode.h"
#include "bacdef.h"
#include "config.h"
#include "iam.h"
#include "txbuf.h"
#include "whois.h"

#include "c_wrapper.h"
#include "client.h"
#include "handlers.h"

/**
 * Handler for Who-Is requests, with broadcast I-Am response.\
 *
 * @param service_request [in] The received message to be handled.
 * @param service_len [in] Length of the service_request message.
 * @param src [in] The BACNET_ADDRESS of the message's source (ignored).
 */
void handler_who_is(uint8_t* service_request, uint16_t service_len, BACNET_ADDRESS* src) {
    int len = 0;
    int32_t low_limit = 0;
    int32_t high_limit = 0;

    (void)src;
    len = whois_decode_service_request(service_request, service_len, &low_limit, &high_limit);

#if PRINT_ENABLED
    printf("WHO-IS: %d - %d\n", low_limit, high_limit);
#endif

    if (len == 0) {
        Send_I_Am(&Handler_Transmit_Buffer[0]);
    } else if (len != BACNET_STATUS_ERROR) {
        // Is my device id within the limits?
        if ((Device_Object_Instance_Number() >= (uint32_t)low_limit) &&
            (Device_Object_Instance_Number() <= (uint32_t)high_limit)) {
            Send_I_Am(&Handler_Transmit_Buffer[0]);
        }
    }
}

/**
 * Handler for Who-Is requests, with Unicast I-Am response (per Addendum 135-2004q).
 *
 * @param service_request [in] The received message to be handled.
 * @param service_len [in] Length of the service_request message.
 * @param src [in] The BACNET_ADDRESS of the message's source that the response will be sent back to.
 */
void handler_who_is_unicast(uint8_t* service_request, uint16_t service_len, BACNET_ADDRESS* src) {
    int len = 0;
    int32_t low_limit = 0;
    int32_t high_limit = 0;

    len = whois_decode_service_request(service_request, service_len, &low_limit, &high_limit);
    // If no limits, then always respond
    if (len == 0) {
        Send_I_Am_Unicast(&Handler_Transmit_Buffer[0], src);
    } else if (len != BACNET_STATUS_ERROR) {
        // Is my device id within the limits?
        if ((Device_Object_Instance_Number() >= (uint32_t)low_limit) &&
            (Device_Object_Instance_Number() <= (uint32_t)high_limit)) {
            Send_I_Am_Unicast(&Handler_Transmit_Buffer[0], src);
        }
    }
}
