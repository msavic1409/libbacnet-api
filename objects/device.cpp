/**************************************************************************
 *
 * Copyright (C) 2005,2006,2009 Steve Karg <skarg@users.sourceforge.net>
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

#include <cstring>

#include "config.h"
#include "custom_bacnet_config.h"

#if (BACNET_PROTOCOL_REVISION >= 14)
#include "proplist.h"
#endif
#include "address.h"
#include "apdu.h"
#include "datetime.h"
#include "handlers.h"
#include "rp.h"
#include "wp.h"

#include "analog_input.hpp"
#include "c_wrapper.h"
#include "device.hpp"

using namespace bacnet;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Device_Properties_Required[] = {PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME,
    PROP_OBJECT_TYPE,
    PROP_SYSTEM_STATUS,
    PROP_VENDOR_NAME,
    PROP_VENDOR_IDENTIFIER,
    PROP_MODEL_NAME,
    PROP_FIRMWARE_REVISION,
    PROP_APPLICATION_SOFTWARE_VERSION,
    PROP_PROTOCOL_VERSION,
    PROP_PROTOCOL_REVISION,
    PROP_PROTOCOL_SERVICES_SUPPORTED,
    PROP_PROTOCOL_OBJECT_TYPES_SUPPORTED,
    PROP_OBJECT_LIST,
    PROP_MAX_APDU_LENGTH_ACCEPTED,
    PROP_SEGMENTATION_SUPPORTED,
    PROP_APDU_TIMEOUT,
    PROP_NUMBER_OF_APDU_RETRIES,
    PROP_DEVICE_ADDRESS_BINDING,
    PROP_DATABASE_REVISION,
    -1};

static const int Device_Properties_Optional[] = {PROP_DESCRIPTION,
    PROP_LOCAL_TIME,
    PROP_LOCAL_DATE,
    PROP_LOCATION,
    // PROP_UTC_OFFSET,
    // PROP_DAYLIGHT_SAVINGS_STATUS,
    -1};

static const int Device_Properties_Proprietary[] = {-1};

static unsigned read_property(const BACnetObject& device, BACNET_READ_PROPERTY_DATA* rpdata) {
    int apdu_len = 0; /* return value */
    int len = 0;      /* apdu len intermediate value */
    BACNET_BIT_STRING bit_string = {0};
    BACNET_CHARACTER_STRING char_string = {0};
    uint32_t i = 0;
    int object_type = 0;
    uint32_t instance = 0;
    uint32_t count = 0;
    uint8_t* apdu = nullptr;
    bool found = false;

    if ((rpdata == nullptr) || (rpdata->application_data == nullptr) || (rpdata->application_data_len == 0)) {
        return 0;
    }
    apdu = rpdata->application_data;

    if ((rpdata == nullptr) || (rpdata->application_data == nullptr) || (rpdata->application_data_len == 0)) {
        return 0;
    }
    apdu = rpdata->application_data;
    switch (rpdata->object_property) {
    case PROP_OBJECT_IDENTIFIER: {
        apdu_len = encode_application_object_id(&apdu[0], OBJECT_DEVICE, device.read.object_identifier());
        break;
    }
    case PROP_OBJECT_NAME: {
        BACNET_CHARACTER_STRING char_string;
        static char object_name[MAX_OBJECT_NAME_LENGTH] = "";
        device.read.object_name(rpdata->object_instance, object_name);
        characterstring_init_ansi(&char_string, object_name);
        apdu_len = encode_application_character_string(&apdu[0], &char_string);
        break;
    }
    case PROP_OBJECT_TYPE:
        apdu_len = encode_application_enumerated(&apdu[0], OBJECT_DEVICE);
        break;
    case PROP_DESCRIPTION: {
        BACNET_CHARACTER_STRING char_string;
        static char description[MAX_DESCRIPTION_LENGTH] = "";
        device.read.description(rpdata->object_instance, description);
        characterstring_init_ansi(&char_string, description);
        apdu_len = encode_application_character_string(&apdu[0], &char_string);
        break;
    }
    case PROP_SYSTEM_STATUS:
        apdu_len = encode_application_enumerated(&apdu[0], STATUS_OPERATIONAL);
        break;
    case PROP_VENDOR_NAME: {
        BACNET_CHARACTER_STRING char_string;
        static char vendor_name[MAX_VENDOR_NAME_LENGTH] = "";
        device.read.vendor_name(vendor_name);
        characterstring_init_ansi(&char_string, vendor_name);
        apdu_len = encode_application_character_string(&apdu[0], &char_string);
        break;
    }
    case PROP_VENDOR_IDENTIFIER: {
        uint16_t vendor_identifier = 0;
        device.read.vendor_identifier(&vendor_identifier);
        apdu_len = encode_application_unsigned(&apdu[0], vendor_identifier);
    } break;
    case PROP_MODEL_NAME: {
        BACNET_CHARACTER_STRING char_string;
        static char model_name[MAX_MODEL_NAME_LENGTH] = "";
        device.read.model_name(model_name);
        characterstring_init_ansi(&char_string, model_name);
        apdu_len = encode_application_character_string(&apdu[0], &char_string);
        break;
    }
    case PROP_FIRMWARE_REVISION: {
        BACNET_CHARACTER_STRING char_string;
        static char firmware_revision[MAX_VERSION_LENGTH] = "";
        device.read.firmware_revision(firmware_revision);
        characterstring_init_ansi(&char_string, firmware_revision);
        apdu_len = encode_application_character_string(&apdu[0], &char_string);
        break;
    }
    case PROP_APPLICATION_SOFTWARE_VERSION: {
        BACNET_CHARACTER_STRING char_string;
        static char application_version[MAX_VERSION_LENGTH] = "";
        device.read.application_version(application_version);
        characterstring_init_ansi(&char_string, application_version);
        apdu_len = encode_application_character_string(&apdu[0], &char_string);
        break;
    }
    case PROP_LOCATION: {
        BACNET_CHARACTER_STRING char_string;
        static char location[MAX_LOCATION_LENGTH] = "";
        device.read.location(location);
        characterstring_init_ansi(&char_string, location);
        apdu_len = encode_application_character_string(&apdu[0], &char_string);
        break;
    }
    case PROP_LOCAL_TIME: {
        BACNET_TIME local_time;
        device.read.local_time(&local_time);
        apdu_len = encode_application_time(&apdu[0], &local_time);
        break;
    }
    case PROP_LOCAL_DATE: {
        BACNET_DATE local_date;
        device.read.local_date(&local_date);
        apdu_len = encode_application_date(&apdu[0], &local_date);
        break;
    }
    case PROP_PROTOCOL_VERSION: {
        apdu_len = encode_application_unsigned(&apdu[0], BACNET_PROTOCOL_VERSION);
        break;
    }
    case PROP_PROTOCOL_REVISION: {
        apdu_len = encode_application_unsigned(&apdu[0], BACNET_PROTOCOL_REVISION);
        break;
    }
    case PROP_PROTOCOL_SERVICES_SUPPORTED:
        /* Note: list of services that are executed, not initiated. */
        bitstring_init(&bit_string);
        for (i = 0; i < MAX_BACNET_SERVICES_SUPPORTED; i++) {
            /* automatic lookup based on handlers set */
            bitstring_set_bit(&bit_string, (uint8_t)i, apdu_service_supported((BACNET_SERVICES_SUPPORTED)i));
        }
        apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
        break;
    case PROP_PROTOCOL_OBJECT_TYPES_SUPPORTED: {
        bitstring_init(&bit_string);
        for (i = 0; i < MAX_ASHRAE_OBJECT_TYPE; i++) {
            /* initialize all the object types to not-supported */
            bitstring_set_bit(&bit_string, (uint8_t)i, false);
        }
        /* set the object types with objects to supported */
        for (const auto& obj : device.objects) {
            // printf("Supported: %d\n", obj->type);
            bitstring_set_bit(&bit_string, obj->type, true);
        }
        apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
        break;
    }
    case PROP_OBJECT_LIST: {
        count = 0;
        found = false;
        for (const auto& obj : device.objects) {
            count++;
        }
        if (rpdata->array_index == 0)
            apdu_len = encode_application_unsigned(&apdu[0], count);

        else if (rpdata->array_index == BACNET_ARRAY_ALL) {
            for (const auto& obj : device.objects) {
                instance = obj->read.object_identifier();
                object_type = obj->type;

                len = encode_application_object_id(&apdu[apdu_len], object_type, instance);
                apdu_len += len;
                if ((i != count) && (apdu_len + len) >= MAX_APDU) {
                    /* Abort response */
                    rpdata->error_code = ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
                    apdu_len = BACNET_STATUS_ABORT;
                    break;
                }
            }
        } else {
            unsigned idx = 0;
            for (const auto& obj : device.objects) {
                if (++idx == rpdata->array_index) {
                    found = true;
                    instance = obj->read.object_identifier();
                    object_type = obj->type;
                    break;
                }
            }
            if (found) {
                apdu_len = encode_application_object_id(&apdu[0], object_type, instance);
            } else {
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                apdu_len = BACNET_STATUS_ERROR;
            }
        }

        break;
    }
    case PROP_MAX_APDU_LENGTH_ACCEPTED:
        apdu_len = encode_application_unsigned(&apdu[0], MAX_APDU);
        break;
    case PROP_SEGMENTATION_SUPPORTED:
        apdu_len = encode_application_enumerated(&apdu[0], BACNET_SEGMENTATION_SUPPORT);
        break;
    case PROP_APDU_TIMEOUT:
        apdu_len = encode_application_unsigned(&apdu[0], apdu_timeout());
        break;
    case PROP_NUMBER_OF_APDU_RETRIES:
        apdu_len = encode_application_unsigned(&apdu[0], apdu_retries());
        break;
    case PROP_DEVICE_ADDRESS_BINDING:
        /* FIXME: the real max apdu remaining should be passed into function */
        apdu_len = address_list_encode(&apdu[0], MAX_APDU);
        break;
    case PROP_DATABASE_REVISION:
        // TODO: use read property
        apdu_len = encode_application_unsigned(&apdu[0], device.read.database_revision());
        break;
        // #if (BACNET_PROTOCOL_REVISION < 14)
        //         case PROP_PROPERTY_LIST:
        //             rpdata->error_class = ERROR_CLASS_PROPERTY;
        //             rpdata->error_code = ERROR_CODE_READ_ACCESS_DENIED;
        //             apdu_len = BACNET_STATUS_ERROR;
        //             break;
        // #endif
    default:
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
        apdu_len = BACNET_STATUS_ERROR;
        break;
    }

    /*  only array properties can have array options */
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_OBJECT_LIST) && (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

static bool write_property(BACnetObject& object, BACNET_WRITE_PROPERTY_DATA* wp_data) {
    bool status = false;
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;
    int object_type = 0;
    uint32_t object_instance = 0;
    int temp;

    // Decode the some of the request
    len = bacapp_decode_application_data(wp_data->application_data, wp_data->application_data_len, &value);
    if (len < 0) {
        // Error while decoding - a value larger than we can handle
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }
    if ((wp_data->object_property != PROP_OBJECT_LIST) && (wp_data->array_index != BACNET_ARRAY_ALL)) {
        // Only array properties can have array options
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }
    // FIXME: len < application_data_len: more data?
    switch (wp_data->object_property) {
    case PROP_OBJECT_NAME: {
        // object.write.object_name()
        status = WPValidateString(&value, MAX_OBJECT_NAME_LENGTH, false, &wp_data->error_class, &wp_data->error_code);
        if (status) {
            /* All the object names in a device must be unique */
            auto new_object_name = characterstring_value(&value.type.Character_String);
            len = strlen(new_object_name);

            if (!object.write.object_name(wp_data->object_instance, new_object_name)) {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            }
        }
        break;
    }
    case PROP_OBJECT_IDENTIFIER: {
        status =
            WPValidateArgType(&value, BACNET_APPLICATION_TAG_OBJECT_ID, &wp_data->error_class, &wp_data->error_code);

        if (status) {
            if ((value.type.Object_Id.type == OBJECT_DEVICE) &&
                (object.write.object_identifier(value.type.Object_Id.instance))) {
                /* FIXME: we could send an I-Am broadcast to let the world know */

            } else {
                status = false;
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
            }
        }

        break;
    }
    case PROP_NUMBER_OF_APDU_RETRIES:
    case PROP_SYSTEM_STATUS:
    case PROP_LOCATION:
    case PROP_DESCRIPTION:
    case PROP_MODEL_NAME:
    case PROP_VENDOR_IDENTIFIER:
    case PROP_OBJECT_TYPE:
    case PROP_VENDOR_NAME:
    case PROP_FIRMWARE_REVISION:
    case PROP_APPLICATION_SOFTWARE_VERSION:
    case PROP_LOCAL_TIME:
    case PROP_LOCAL_DATE:
    case PROP_PROTOCOL_VERSION:
    case PROP_PROTOCOL_REVISION:
    case PROP_PROTOCOL_SERVICES_SUPPORTED:
    case PROP_PROTOCOL_OBJECT_TYPES_SUPPORTED:
    case PROP_OBJECT_LIST:
    case PROP_MAX_APDU_LENGTH_ACCEPTED:
    case PROP_APDU_TIMEOUT:
    case PROP_SEGMENTATION_SUPPORTED:
    case PROP_DEVICE_ADDRESS_BINDING:
    case PROP_DATABASE_REVISION:
#if (BACNET_PROTOCOL_REVISION < 14)
    case PROP_PROPERTY_LIST:
#endif
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        return false;
    default:
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
        break;
    }

    return status;
}

static void rpm_property_list(const int** pRequired, const int** pOptional, const int** pProprietary) {
    // printf("device.rpm_property_list\n");
    if (pRequired)
        *pRequired = Device_Properties_Required;

    if (pOptional)
        *pOptional = Device_Properties_Optional;

    if (pProprietary)
        *pProprietary = Device_Properties_Proprietary;
}

void bacnet::init_device_object_handlers(ObjectTypeHandler& handler) {
    handler.read_property = read_property;
    handler.write_property = write_property;
    handler.rpm_property_list = rpm_property_list;

    handler.intrinsic_reporting = [](const BACnetObject&) {

    };

    handler.init = [](BACnetObject&) {

    };

    handler.count = []() { return 1; };
}
