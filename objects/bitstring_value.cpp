/**************************************************************************
 *
 * Copyright (C) 2006 Steve Karg <skarg@users.sourceforge.net>
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

#include "bitstring_value.hpp"
#include "custom_bacnet_config.h"
//#include "datetime.h"
#include "bacnet.hpp"
#include "handlers.h"
#include "rp.h"
#include "wp.h"

using namespace bacnet;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int BSV_Properties_Required[] =
    {PROP_OBJECT_IDENTIFIER, PROP_OBJECT_NAME, PROP_OBJECT_TYPE, PROP_PRESENT_VALUE, PROP_STATUS_FLAGS, -1};

static const int BSV_Properties_Optional[] = {PROP_DESCRIPTION,
    PROP_BIT_TEXT,
    PROP_EVENT_STATE,
    PROP_OUT_OF_SERVICE,
    // PROP_PROPERTY_LIST
    -1};

static const int BSV_Properties_Proprietary[] = {-1};

static unsigned bitstring_value_read_property(const BACnetObject& object, BACNET_READ_PROPERTY_DATA* rpdata) {
    int apdu_len = 0, len;
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    uint8_t* apdu = nullptr;

    if ((rpdata == nullptr) || (rpdata->application_data == nullptr) || (rpdata->application_data_len == 0)) {
        return 0;
    }

    apdu = rpdata->application_data;

    switch (rpdata->object_property) {
    case PROP_OBJECT_IDENTIFIER: {
        apdu_len = encode_application_object_id(&apdu[0], object.type, rpdata->object_instance);
        break;
    }

    case PROP_OBJECT_NAME: {
        static char object_name[MAX_OBJECT_NAME_LENGTH] = "";
        object.read.object_name(rpdata->object_instance, object_name);
        characterstring_init_ansi(&char_string, object_name);
        apdu_len = encode_application_character_string(&apdu[0], &char_string);
        break;
    }

    case PROP_OBJECT_TYPE:
        apdu_len = encode_application_enumerated(&apdu[0], object.type);
        break;

    case PROP_PRESENT_VALUE: {
        bitstring_init(&bit_string);
        object.read.present_value_bitstring(rpdata->object_instance, &bit_string);
        apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
        break;
    }

    case PROP_BIT_TEXT: {
        unsigned max_bits = 0;
        char bit_text[MAX_STATE_TEXT_LENGTH];
        object.read.number_of_bits(rpdata->object_instance, &max_bits);

        if (rpdata->array_index == 0) {
            /* Array element zero is the number of elements in the array */
            apdu_len = encode_application_unsigned(&apdu[0], max_bits);
        } else if (rpdata->array_index == BACNET_ARRAY_ALL) {
            /* if no index was specified, then try to encode the entire list */
            /* into one packet. */
            for (unsigned i = 1; i <= max_bits; i++) {

                object.read.bit_text(rpdata->object_instance, i, bit_text);
                characterstring_init_ansi(&char_string, bit_text);
                /* FIXME: this might go beyond MAX_APDU length! */
                len = encode_application_character_string(&apdu[apdu_len], &char_string);
                /* add it if we have room */
                if ((apdu_len + len) < MAX_APDU) {
                    apdu_len += len;
                } else {
                    rpdata->error_code = ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
                    apdu_len = BACNET_STATUS_ABORT;
                    break;
                }
            }
        } else {
            if (rpdata->array_index <= max_bits) {
                object.read.bit_text(rpdata->object_instance, rpdata->array_index, bit_text);
                characterstring_init_ansi(&char_string, bit_text);
                apdu_len = encode_application_character_string(&apdu[0], &char_string);
            } else {
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                apdu_len = BACNET_STATUS_ERROR;
            }
        }
        break;
    }

    case PROP_STATUS_FLAGS: {
        // TOOD: read from properties
        bool out_of_service = false;
        if (object.read.out_of_service)
            object.read.out_of_service(rpdata->object_instance, &out_of_service);

        bitstring_init(&bit_string);
        bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
        bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
        bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
        bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, out_of_service);

        apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
        break;
    }

    case PROP_EVENT_STATE: {
        // TOOD: read from properties
        apdu_len = encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
        break;
    }

    case PROP_OUT_OF_SERVICE: {
        bool out_of_service = false;
        if (object.read.out_of_service)
            object.read.out_of_service(rpdata->object_instance, &out_of_service);
        apdu_len = encode_application_boolean(&apdu[0], out_of_service);
        break;
    }

    case PROP_DESCRIPTION: {
        static char description[MAX_OBJECT_NAME_LENGTH] = "";
        object.read.description(rpdata->object_instance, description);
        characterstring_init_ansi(&char_string, description);
        apdu_len = encode_application_character_string(&apdu[0], &char_string);
        break;
    }

    default:
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
        apdu_len = BACNET_STATUS_ERROR;
        break;
    }

    // Only array properties can have array options
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_BIT_TEXT) && (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

static bool bitstring_value_write_property(BACnetObject& object, BACNET_WRITE_PROPERTY_DATA* wp_data) {
    bool status = false;
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;

    // Decode the some of the request
    len = bacapp_decode_application_data(wp_data->application_data, wp_data->application_data_len, &value);
    // FIXME: len < application_data_len: more data?
    if (len < 0) {
        // Error while decoding - a value larger than we can handle
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }
    // Only array properties can have array options
    if ((wp_data->object_property != PROP_BIT_TEXT) && (wp_data->array_index != BACNET_ARRAY_ALL)) {
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }

    switch (wp_data->object_property) {
    case PROP_PRESENT_VALUE: {
        status =
            WPValidateArgType(&value, BACNET_APPLICATION_TAG_BIT_STRING, &wp_data->error_class, &wp_data->error_code);

        if (status) {
            unsigned number_of_bits;
            object.read.number_of_bits(object.instance, &number_of_bits);
            if (value.type.Bit_String.bits_used != number_of_bits) {
                status = false;

                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;

                return status;
            }

            CustomErrorStatusCode customStatus = StatusWriteAccessDenied;
            if (!object.write.present_value_bitstring(wp_data->object_instance, &value.type.Bit_String, customStatus)) {
                status = false;

                wp_data->error_class = ERROR_CLASS_PROPERTY;

                if (customStatus == CustomErrorStatusCode::StatusWriteAccessDenied)
                    wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
                else if (customStatus == CustomErrorStatusCode::StatusNotInRange)
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;

                return status;
            }
        }

        break;
    }
    case PROP_OUT_OF_SERVICE:
    case PROP_OBJECT_IDENTIFIER:
    case PROP_OBJECT_NAME:
    case PROP_OBJECT_TYPE:
    case PROP_STATUS_FLAGS:
    case PROP_EVENT_STATE:
    case PROP_DESCRIPTION:
    case PROP_PROPERTY_LIST:
    case PROP_BIT_TEXT:
        // printf("WRITE_ACCESS_DENIED [0x%02d]\n", (int) wp_data->object_property);
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        break;
    default:
        // printf("Unrecognized property [0x%02d]\n", (int) wp_data->object_property);
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
        // wp_data->error_code = ERROR_CODE_REJECT_UNRECOGNIZED_SERVICE;
        break;
    }

    return status;
}

static void bitstring_value_rpm_property_list(const int** pRequired, const int** pOptional, const int** pProprietary) {
    if (pRequired)
        *pRequired = BSV_Properties_Required;

    if (pOptional)
        *pOptional = BSV_Properties_Optional;

    if (pProprietary)
        *pProprietary = BSV_Properties_Proprietary;
}

void bacnet::init_bitstring_value_object_handlers(ObjectTypeHandler& handler) {
    handler.read_property = bitstring_value_read_property;
    handler.write_property = bitstring_value_write_property;
    handler.rpm_property_list = bitstring_value_rpm_property_list;

    handler.intrinsic_reporting = [](const BACnetObject&) {

    };

    handler.init = [](BACnetObject&) {

    };

    handler.count = []() { return 0; };
}
