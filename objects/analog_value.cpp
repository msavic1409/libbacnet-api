/**************************************************************************
 *
 * Copyright (C) 2006 Steve Karg <skarg@users.sourceforge.net>
 * Copyright (C) 2011 Krzysztof Malorny <malornykrzysztof@gmail.com>
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

#include "analog_value.hpp"
#include "custom_bacnet_config.h"
//#include "datetime.h"
#include "bacnet.hpp"
#include "handlers.h"
#include "rp.h"
#include "wp.h"

using namespace bacnet;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int AV_Properties_Required[] = {PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME,
    PROP_OBJECT_TYPE,
    PROP_PRESENT_VALUE,
    PROP_STATUS_FLAGS,
    PROP_EVENT_STATE,
    PROP_OUT_OF_SERVICE,
    PROP_UNITS,
    -1};

static const int AV_Properties_Optional[] = {PROP_DESCRIPTION,
    PROP_MIN_PRES_VALUE,
    PROP_MAX_PRES_VALUE,
    PROP_RESOLUTION,
    // PROP_PROPERTY_LIST
    -1};

static const int AV_Properties_Proprietary[] = {-1};

static unsigned analog_value_read_property(const BACnetObject& object, BACNET_READ_PROPERTY_DATA* rpdata) {

    int apdu_len = 0;
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
        float present_value = 0.0f;
        object.read.present_value_real(rpdata->object_instance, &present_value);
        apdu_len = encode_application_real(&apdu[0], present_value);
        break;
    }

    case PROP_MAX_PRES_VALUE: {
        float max_value = 0.0f;
        object.read.max_pres_value(rpdata->object_instance, &max_value);
        apdu_len = encode_application_real(&apdu[0], max_value);
        break;
    };

    case PROP_MIN_PRES_VALUE: {
        float min_value = 0.0f;
        object.read.min_pres_value(rpdata->object_instance, &min_value);
        apdu_len = encode_application_real(&apdu[0], min_value);
        break;
    };

    case PROP_RESOLUTION: {
        float resolution = 0.0f;
        object.read.resolution(rpdata->object_instance, &resolution);
        apdu_len = encode_application_real(&apdu[0], resolution);
        break;
    };

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

    case PROP_UNITS: {
        BACNET_ENGINEERING_UNITS units = UNITS_NO_UNITS;
        if (object.read.units)
            object.read.units(rpdata->object_instance, &units);
        apdu_len = encode_application_enumerated(&apdu[0], units);
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
    if ((apdu_len >= 0) &&
        ((rpdata->object_property != PROP_EVENT_TIME_STAMPS) || (rpdata->object_property != PROP_PROPERTY_LIST)) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

static bool analog_value_write_property(BACnetObject& object, BACNET_WRITE_PROPERTY_DATA* wp_data) {
    bool status = false;
    unsigned int object_index = 0;
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;

    // printf("analog_value_Write_Property [0x%02x]\n", (unsigned) wp_data->object_property);

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
    if ((wp_data->object_property != PROP_EVENT_TIME_STAMPS) && (wp_data->array_index != BACNET_ARRAY_ALL)) {
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }

    switch (wp_data->object_property) {
    case PROP_PRESENT_VALUE: {
        status = WPValidateArgType(&value, BACNET_APPLICATION_TAG_REAL, &wp_data->error_class, &wp_data->error_code);

        if (status) {
            float min_pres_value;
            float max_pres_value;
            object.read.min_pres_value(wp_data->object_instance, &min_pres_value);
            object.read.max_pres_value(wp_data->object_instance, &max_pres_value);

            if (value.type.Real < min_pres_value || value.type.Real > max_pres_value) {
                status = false;
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                return status;
            }

            CustomErrorStatusCode customStatus = StatusWriteAccessDenied;
            if (!object.write.present_value_real(wp_data->object_instance, value.type.Real, customStatus)) {
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
    case PROP_MIN_PRES_VALUE:
    case PROP_MAX_PRES_VALUE:
    case PROP_RESOLUTION:
    case PROP_UNITS:
    case PROP_OBJECT_IDENTIFIER:
    case PROP_OBJECT_NAME:
    case PROP_OBJECT_TYPE:
    case PROP_STATUS_FLAGS:
    case PROP_EVENT_STATE:
    case PROP_DESCRIPTION:
    case PROP_PROPERTY_LIST:
        // printf("WRITE_ACCESS_DENIED [0x%02d]\n", (int) wp_data->object_property);
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        break;
        //        case PROP_NOTIFY_TYPE:
        //            printf("Unrecognized property [0x%02d]\n", (int) wp_data->object_property);
        //            wp_data->error_class = ERROR_CLASS_PROPERTY;
        //            wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
        //            break;
    default:
        // printf("Unrecognized property [0x%02d]\n", (int) wp_data->object_property);
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
        // wp_data->error_code = ERROR_CODE_REJECT_UNRECOGNIZED_SERVICE;
        break;
    }

    return status;
}

static void analog_value_rpm_property_list(const int** pRequired, const int** pOptional, const int** pProprietary) {
    if (pRequired)
        *pRequired = AV_Properties_Required;

    if (pOptional)
        *pOptional = AV_Properties_Optional;

    if (pProprietary)
        *pProprietary = AV_Properties_Proprietary;
}

void bacnet::init_analog_value_object_handlers(ObjectTypeHandler& handler) {
    handler.read_property = analog_value_read_property;
    handler.write_property = analog_value_write_property;
    handler.rpm_property_list = analog_value_rpm_property_list;

    handler.intrinsic_reporting = [](const BACnetObject&) {

    };

    handler.init = [](BACnetObject&) {

    };

    handler.count = []() { return 0; };
}
