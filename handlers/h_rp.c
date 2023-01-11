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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "txbuf.h"
#include "bacdef.h"
#include "bacdcode.h"
#include "bacerror.h"
#include "bacdevobjpropref.h"
#include "apdu.h"
#include "npdu.h"
#include "abort.h"
#include "reject.h"
#include "rp.h"

#include "c_wrapper.h"
#include "handlers.h"

/**
 * Check for error during property reading and send PDU.
 *
 * @param error [in] Error indicator.
 * @param err_type [in] Type of detected error.
 * @param apdu_len [in] APDU length.
 * @param npdu_len [in] NPDU length.
 * @param npdu_data [in] NPDU data.
 * @param service_data [in] The BACNET_CONFIRMED_SERVICE_DATA information decoded from the APDU header of this message.
 * @param rp_data [in] Read property data.
 * @param src [in] BACNET_ADDRESS of the source of the message.
 */
void readproperty_send_pdu(bool error, int err_type, int apdu_len, int npdu_len, BACNET_NPDU_DATA* npdu_data, BACNET_CONFIRMED_SERVICE_DATA* service_data, BACNET_READ_PROPERTY_DATA* rp_data, BACNET_ADDRESS* src)
{
    if (error) {
        if (BACNET_STATUS_ABORT == err_type) {
            apdu_len = abort_encode_apdu(&Handler_Transmit_Buffer[npdu_len], service_data->invoke_id, abort_convert_error_code(rp_data->error_code), true);
#if PRINT_ENABLED
            fprintf(stderr, "RP: Sending Abort!\n");
#endif
        } else if (BACNET_STATUS_ERROR == err_type) {
            apdu_len = bacerror_encode_apdu(&Handler_Transmit_Buffer[npdu_len], service_data->invoke_id, SERVICE_CONFIRMED_READ_PROPERTY, rp_data->error_class, rp_data->error_code);
#if PRINT_ENABLED
            fprintf(stderr, "RP: Sending Error!\n");
#endif
        } else if (BACNET_STATUS_REJECT == err_type) {
            apdu_len = reject_encode_apdu(&Handler_Transmit_Buffer[npdu_len], service_data->invoke_id, reject_convert_error_code(rp_data->error_code));
#if PRINT_ENABLED
            fprintf(stderr, "RP: Sending Reject!\n");
#endif
        }
    }

    int pdu_len = npdu_len + apdu_len;
    int bytes_sent = datalink_send_pdu(src, npdu_data, &Handler_Transmit_Buffer[0], pdu_len);
    if (bytes_sent <= 0) {
#if PRINT_ENABLED
        fprintf(stderr, "Failed to send PDU (%s)!\n", strerror(errno));
#endif
    }
}

/**
 * Handler for a ReadProperty Service request.
 *
 * This handler will be invoked by apdu_handler() if it has been enabled by a call to apdu_set_confirmed_handler().
 * This handler builds a response packet, which is
 * - an Abort if
 *   - the message is segmented
 *   - if decoding fails
 *   - if the response would be too large
 * - the result from Device_Read_Property(), if it succeeds
 * - an Error if Device_Read_Property() fails or there isn't enough room in the APDU to fit the data.
 *
 * @param service_request [in] The contents of the service request.
 * @param service_len [in] The length of the service_request.
 * @param src [in] BACNET_ADDRESS of the source of the message.
 * @param service_data [in] The BACNET_CONFIRMED_SERVICE_DATA information decoded from the APDU header of this message.
 */
void handler_read_property(uint8_t* service_request, uint16_t service_len, BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_DATA* service_data)
{
    BACNET_READ_PROPERTY_DATA rp_data;
    int len = 0;
    int apdu_len = -1;
    int npdu_len = -1;
    BACNET_NPDU_DATA npdu_data;
    bool error = true;
    BACNET_ADDRESS my_address;

    // Configure default error code as an abort since it is common
    rp_data.error_code = ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
    // Encode the NPDU portion of the packet
    datalink_get_my_address(&my_address);
    npdu_encode_npdu_data(&npdu_data, false, MESSAGE_PRIORITY_NORMAL);
    npdu_len = npdu_encode_pdu(&Handler_Transmit_Buffer[0], src, &my_address, &npdu_data);
    if (service_data->segmented_message) {
        // We don't support segmentation - send an abort
        len = BACNET_STATUS_ABORT;
#if PRINT_ENABLED
        fprintf(stderr, "RP: Segmented message.  Sending Abort!\n");
#endif
        readproperty_send_pdu(error, len, apdu_len, npdu_len, &npdu_data, service_data, &rp_data, src);
        return;
    }
    len = rp_decode_service_request(service_request, service_len, &rp_data);
#if PRINT_ENABLED
    if (len <= 0) {
        fprintf(stderr, "RP: Unable to decode Request!\n");
    }
#endif
    if (len < 0) {
        // Bad decoding - skip to error/reject/abort handling
        error = true;
#if PRINT_ENABLED
        fprintf(stderr, "RP: Bad Encoding.\n");
#endif
        readproperty_send_pdu(error, len, apdu_len, npdu_len, &npdu_data, service_data, &rp_data, src);
        return;
    }
    // Test for case of indefinite Device object instance
    if ((rp_data.object_type == OBJECT_DEVICE) && (rp_data.object_instance == BACNET_MAX_INSTANCE)) {
        rp_data.object_instance = Device_Object_Instance_Number();
    }

    apdu_len = rp_ack_encode_apdu_init(&Handler_Transmit_Buffer[npdu_len], service_data->invoke_id, &rp_data);
    // Configure our storage
    rp_data.application_data = &Handler_Transmit_Buffer[npdu_len + apdu_len];
    rp_data.application_data_len = sizeof(Handler_Transmit_Buffer) - (npdu_len + apdu_len);
    len = Device_Read_Property(&rp_data);
    if (len >= 0) {
        apdu_len += len;
        len = rp_ack_encode_apdu_object_property_end(&Handler_Transmit_Buffer[npdu_len + apdu_len]);
        apdu_len += len;
        if (apdu_len > service_data->max_resp) {
            // Too big for the sender - send an abort
            // Setting of error code needed here as read property processing may have overriden the default set at start
            rp_data.error_code = ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
            len = BACNET_STATUS_ABORT;
#if PRINT_ENABLED
            fprintf(stderr, "RP: Message too large.\n");
#endif
        } else {
#if PRINT_ENABLED
            fprintf(stderr, "RP: Sending Ack!\n");
#endif
            error = false;
        }
    } else {
#if PRINT_ENABLED
        fprintf(stderr, "RP: Device_Read_Property: ");
        if (len == BACNET_STATUS_ABORT) {
            fprintf(stderr, "Abort!\n");
        } else if (len == BACNET_STATUS_ERROR) {
            fprintf(stderr, "Error!\n");
        } else if (len == BACNET_STATUS_REJECT) {
            fprintf(stderr, "Reject!\n");
        } else {
            fprintf(stderr, "Unknown Len=%d\n", len);
        }
#endif
    }
    readproperty_send_pdu(error, len, apdu_len, npdu_len, &npdu_data, service_data, &rp_data, src);
}
