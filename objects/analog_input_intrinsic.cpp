/**************************************************************************
 *
 * Copyright (C) 2005 Steve Karg <skarg@users.sourceforge.net>
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

#include "analog_input_intrinsic.hpp"
#include "custom_bacnet_config.h"
#include "notification_class.hpp"
//#include "datetime.h"
#include "handlers.h"
#include "rp.h"
#include "wp.h"

#include "c_wrapper.h"
#include "container.hpp"

using namespace bacnet;

static bool is_init = false;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int AI_Properties_Required[] = {PROP_OBJECT_IDENTIFIER,
                                             PROP_OBJECT_NAME,
                                             PROP_OBJECT_TYPE,
                                             PROP_PRESENT_VALUE,
                                             PROP_STATUS_FLAGS,
                                             PROP_EVENT_STATE,
                                             PROP_OUT_OF_SERVICE,
                                             PROP_UNITS,
                                             -1};

static const int AI_Properties_Optional[] = {PROP_DESCRIPTION,
                                             PROP_DEVICE_TYPE,
                                             PROP_MIN_PRES_VALUE,
                                             PROP_MAX_PRES_VALUE,
                                             PROP_RESOLUTION,
// PROP_PROPERTY_LIST,
#if defined(INTRINSIC_REPORTING)
                                             PROP_EVENT_DETECTION_ENABLE,
                                             PROP_TIME_DELAY,
                                             PROP_NOTIFICATION_CLASS,
                                             PROP_HIGH_LIMIT,
                                             PROP_LOW_LIMIT,
                                             PROP_DEADBAND,
                                             PROP_LIMIT_ENABLE,
                                             PROP_EVENT_ENABLE,
                                             PROP_ACKED_TRANSITIONS,
                                             PROP_NOTIFY_TYPE,
                                             PROP_EVENT_TIME_STAMPS,
// PROP_EVENT_MESSAGE_TEXTS,
// PROP_EVENT_MESSAGE_TEXTS_CONFIG,
// PROP_EVENT_ALGORITHM_INHIBIT,
// PROP_EVENT_ALGORITHM_INHIBIT_REF,
// PROP_TIME_DELAY_NORMAL,
#endif
                                             -1};

static const int AI_Properties_Proprietary[] = {-1};

static unsigned analog_input_intrinsic_read_property(const BACnetObject &object, BACNET_READ_PROPERTY_DATA *rpdata) {
    int apdu_len = 0;
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    uint8_t *apdu = nullptr;
#if defined(INTRINSIC_REPORTING)
    unsigned i = 0;
    int len = 0;
#endif

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
#if defined(INTRINSIC_REPORTING)
            uint8_t event_state;
            object.read.event_state(rpdata->object_instance, &event_state);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, event_state ? true : false);
#else
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
#endif
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, out_of_service);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        }

        case PROP_EVENT_STATE: {
#if defined(INTRINSIC_REPORTING)
            uint8_t event_state;
            object.read.event_state(rpdata->object_instance, &event_state);
            apdu_len = encode_application_enumerated(&apdu[0], event_state);
#else
            // TOOD: read from properties
            apdu_len = encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
#endif
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

        case PROP_DEVICE_TYPE: {
            static char device_type[MAX_DEVICE_TYPE_LENGTH] = "";
            object.read.device_type(rpdata->object_instance, device_type);
            characterstring_init_ansi(&char_string, device_type);
            apdu_len = encode_application_character_string(&apdu[0], &char_string);
            break;
        }

#if defined(INTRINSIC_REPORTING)
        case PROP_EVENT_DETECTION_ENABLE: {
            bool event_detection_enable;
            object.read.event_detection_enable(rpdata->object_instance, &event_detection_enable);
            apdu_len = encode_application_boolean(&apdu[0], event_detection_enable);
            break;
        }
        case PROP_TIME_DELAY: {
            unsigned time_delay;
            object.read.time_delay(rpdata->object_instance, &time_delay);
            apdu_len = encode_application_unsigned(&apdu[0], time_delay);
            break;
        }
        case PROP_NOTIFICATION_CLASS: {
            unsigned notification_class;
            object.read.notification_class(rpdata->object_instance, &notification_class);
            apdu_len = encode_application_unsigned(&apdu[0], notification_class);
            break;
        }
        case PROP_HIGH_LIMIT: {
            float high_limit;
            object.read.high_limit(rpdata->object_instance, &high_limit);
            apdu_len = encode_application_real(&apdu[0], high_limit);
            break;
        }
        case PROP_LOW_LIMIT: {
            float low_limit;
            object.read.low_limit(rpdata->object_instance, &low_limit);
            apdu_len = encode_application_real(&apdu[0], low_limit);
            break;
        }
        case PROP_DEADBAND: {
            float deadband;
            object.read.deadband(rpdata->object_instance, &deadband);
            apdu_len = encode_application_real(&apdu[0], deadband);
            break;
        }
        case PROP_LIMIT_ENABLE: {
            uint8_t limit_enable;
            object.read.limit_enable(rpdata->object_instance, &limit_enable);

            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, 0, (limit_enable & EVENT_LOW_LIMIT_ENABLE) ? true : false);
            bitstring_set_bit(&bit_string, 1, (limit_enable & EVENT_HIGH_LIMIT_ENABLE) ? true : false);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        }
        case PROP_EVENT_ENABLE: {
            uint8_t event_enable;
            object.read.event_enable(rpdata->object_instance, &event_enable);

            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string,
                              TRANSITION_TO_OFFNORMAL,
                              (event_enable & EVENT_ENABLE_TO_OFFNORMAL) ? true : false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT, (event_enable & EVENT_ENABLE_TO_FAULT) ? true : false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                              (event_enable & EVENT_ENABLE_TO_NORMAL) ? true : false);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        }
        case PROP_ACKED_TRANSITIONS: {
            std::vector<ACKED_INFO> acked_transitions;
            acked_transitions.reserve(MAX_BACNET_EVENT_TRANSITION);
            object.read.acked_transitions(rpdata->object_instance, acked_transitions);

            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                              acked_transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT, acked_transitions[TRANSITION_TO_FAULT].bIsAcked);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL, acked_transitions[TRANSITION_TO_NORMAL].bIsAcked);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        }
        case PROP_NOTIFY_TYPE: {
            uint8_t notify_type;
            object.read.notify_type(rpdata->object_instance, &notify_type);

            apdu_len = encode_application_enumerated(&apdu[0], notify_type ? NOTIFY_EVENT : NOTIFY_ALARM);
            break;
        }
        case PROP_EVENT_TIME_STAMPS: {
            std::vector<BACNET_DATE_TIME> event_time_stamps;
            event_time_stamps.reserve(MAX_BACNET_EVENT_TRANSITION);
            object.read.event_time_stamps(rpdata->object_instance, event_time_stamps);

            /* Array element zero is the number of elements in the array */
            if (rpdata->array_index == 0)
                apdu_len = encode_application_unsigned(&apdu[0], MAX_BACNET_EVENT_TRANSITION);
                /* if no index was specified, then try to encode the entire list */
                /* into one packet. */
            else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                for (i = 0; i < MAX_BACNET_EVENT_TRANSITION; i++) {
                    len = encode_opening_tag(&apdu[apdu_len], TIME_STAMP_DATETIME);
                    len += encode_application_date(&apdu[apdu_len + len], &event_time_stamps[i].date);
                    len += encode_application_time(&apdu[apdu_len + len], &event_time_stamps[i].time);
                    len += encode_closing_tag(&apdu[apdu_len + len], TIME_STAMP_DATETIME);

                    /* add it if we have room */
                    if ((apdu_len + len) < MAX_APDU)
                        apdu_len += len;
                    else {
                        rpdata->error_code = ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
                        apdu_len = BACNET_STATUS_ABORT;
                        break;
                    }
                }
            } else if (rpdata->array_index <= MAX_BACNET_EVENT_TRANSITION) {
                apdu_len = encode_opening_tag(&apdu[apdu_len], TIME_STAMP_DATETIME);
                apdu_len += encode_application_date(&apdu[apdu_len], &event_time_stamps[rpdata->array_index].date);
                apdu_len += encode_application_time(&apdu[apdu_len], &event_time_stamps[rpdata->array_index].time);
                apdu_len += encode_closing_tag(&apdu[apdu_len], TIME_STAMP_DATETIME);
            } else {
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                apdu_len = BACNET_STATUS_ERROR;
            }
            break;
        }
#endif

        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }

    // Only array properties can have array options
    if ((apdu_len >= 0) &&
        ((rpdata->object_property != PROP_EVENT_TIME_STAMPS) && (rpdata->object_property != PROP_PROPERTY_LIST)) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

static bool analog_input_intrinsic_write_property(BACnetObject &object, BACNET_WRITE_PROPERTY_DATA *wp_data) {
    bool status = false;
    unsigned int object_index = 0;
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;

    // printf("Analog_Input_Write_Property [0x%02x]\n", (unsigned) wp_data->object_property);

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

    switch ((int) wp_data->object_property) {
#if defined(INTRINSIC_REPORTING)
#if defined(CERTIFICATION_SOFTWARE)
        case PROP_TIME_DELAY:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT, &wp_data->error_class, &wp_data->error_code);

            if (status) {
                object.write.time_delay(wp_data->object_instance, value.type.Unsigned_Int);

                unsigned time_delay;
                object.read.time_delay(wp_data->object_instance, &time_delay);

                object.write.remaining_time_delay(wp_data->object_instance, time_delay);
            }
            break;

        case PROP_NOTIFICATION_CLASS:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT, &wp_data->error_class, &wp_data->error_code);

            if (status) {
                object.write.notification_class(wp_data->object_instance, value.type.Unsigned_Int);
            }
            break;

        case PROP_HIGH_LIMIT:
            status = WPValidateArgType(&value, BACNET_APPLICATION_TAG_REAL, &wp_data->error_class, &wp_data->error_code);

            if (status) {
                object.write.high_limit(wp_data->object_instance, value.type.Real);
            }
            break;

        case PROP_LOW_LIMIT:
            status = WPValidateArgType(&value, BACNET_APPLICATION_TAG_REAL, &wp_data->error_class, &wp_data->error_code);

            if (status) {
                object.write.low_limit(wp_data->object_instance, value.type.Real);
            }
            break;

        case PROP_DEADBAND:
            status = WPValidateArgType(&value, BACNET_APPLICATION_TAG_REAL, &wp_data->error_class, &wp_data->error_code);

            if (status) {
                object.write.deadband(wp_data->object_instance, value.type.Real);
            }
            break;

        case PROP_LIMIT_ENABLE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_BIT_STRING, &wp_data->error_class, &wp_data->error_code);

            if (status) {
                if (value.type.Bit_String.bits_used == 2) {
                    object.write.limit_enable(wp_data->object_instance, value.type.Bit_String.value[0]);
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                    status = false;
                }
            }
            break;

        case PROP_EVENT_ENABLE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_BIT_STRING, &wp_data->error_class, &wp_data->error_code);

            if (status) {
                if (value.type.Bit_String.bits_used == 3) {
                    object.write.event_enable(wp_data->object_instance, value.type.Bit_String.value[0]);
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                    status = false;
                }
            }
            break;
#endif
        case PROP_NOTIFY_TYPE:
            status =
                    WPValidateArgType(&value, BACNET_APPLICATION_TAG_ENUMERATED, &wp_data->error_class,
                                      &wp_data->error_code);

            if (status) {
                switch ((BACNET_NOTIFY_TYPE) value.type.Enumerated) {
                    case NOTIFY_EVENT:
                        object.write.notify_type(wp_data->object_instance, 1);
                        break;
                    case NOTIFY_ALARM:
                        object.write.notify_type(wp_data->object_instance, 0);
                        break;
                    default:
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                        status = false;
                        break;
                }
            }
            break;
#if !defined(CERTIFICATION_SOFTWARE)
        case PROP_HIGH_LIMIT:
        case PROP_LOW_LIMIT:
        case PROP_DEADBAND:
        case PROP_LIMIT_ENABLE:
        case PROP_NOTIFICATION_CLASS:
        case PROP_TIME_DELAY:
        case PROP_EVENT_ENABLE:
#endif
#endif
        case PROP_OUT_OF_SERVICE:
        case PROP_PRESENT_VALUE:
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
        case PROP_DEVICE_TYPE:
        case PROP_PROPERTY_LIST:
#if defined(INTRINSIC_REPORTING)
        case PROP_ACKED_TRANSITIONS:
        case PROP_EVENT_TIME_STAMPS:
        case PROP_EVENT_DETECTION_ENABLE:
#endif
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

static void analog_input_intrinsic_reporting(const BACnetObject &object) {

#if defined(INTRINSIC_REPORTING)
    BACNET_EVENT_NOTIFICATION_DATA event_data;
    uint8_t FromState = 0, ToState = 0;
    BACNET_CHARACTER_STRING msgText;
    bool SendNotify = false;
    float ExceededLimit = 0.0f;

    float high_limit, low_limit, deadband, present_val = 0.0f;
    uint8_t /* event_enable,*/ limit_enable, notify_type, event_state;
    uint32_t time_delay, remaining_time_delay;

    object.read.limit_enable(object.instance, &limit_enable);

    ACK_NOTIFICATION ack_notify_data;
    object.read.ack_notify_data(object.instance, ack_notify_data);

    if (ack_notify_data.bSendAckNotify) {
        /* clean bSendAckNotify flag */
        ack_notify_data.bSendAckNotify = false;
        object.write.ack_notify_data(object.instance, ack_notify_data);
        /* copy toState */
        ToState = ack_notify_data.EventState;

        characterstring_init_ansi(&msgText, "AckNotification");

        /* Notify Type */
        event_data.notifyType = NOTIFY_ACK_NOTIFICATION;

        /* Send EventNotification. */
        SendNotify = true;
    } else {
        /* actual Present_Value */
        object.read.present_value_real(object.instance, &present_val);

        object.read.event_state(object.instance, &event_state);
        FromState = event_state;

        // OUT_OF_RANGE algorithm (13.3.6 in 135-2016)

        switch (event_state) {
            case EVENT_STATE_NORMAL:
                /* A TO-OFFNORMAL event is generated under these conditions:
                   (a) Current state is normal
                   (b) the HighLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the Present_Value must exceed the High_Limit for a minimum
                   period of time, specified in the Time_Delay property, and
                   (d) OBSOLETE?: the TO-OFFNORMAL flag must be set in the Event_Enable property. */

                object.read.high_limit(object.instance, &high_limit);
                // object.read.event_enable(object.instance, &event_enable);
                object.read.remaining_time_delay(object.instance, &remaining_time_delay);

                if ((present_val > high_limit) && ((limit_enable & EVENT_HIGH_LIMIT_ENABLE) == EVENT_HIGH_LIMIT_ENABLE)/* &&
                ((event_enable & EVENT_ENABLE_TO_OFFNORMAL) == EVENT_ENABLE_TO_OFFNORMAL)*/) {

                    if (!remaining_time_delay)
                        object.write.event_state(object.instance, EVENT_STATE_HIGH_LIMIT);
                    else {
                        remaining_time_delay--;
                        object.write.remaining_time_delay(object.instance, remaining_time_delay);
                    }
                    break;
                }

                /* A TO-OFFNORMAL event is generated under these conditions:
                    (a) Current state is normal
                   (b) the LowLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the Present_Value must exceed the Low_Limit plus the Deadband
                   for a minimum period of time, specified in the Time_Delay property, and
                   (d) OBSOLETE?: the TO-NORMAL flag must be set in the Event_Enable property. */
                object.read.low_limit(object.instance, &low_limit);

                if ((present_val < low_limit) && ((limit_enable & EVENT_LOW_LIMIT_ENABLE) == EVENT_LOW_LIMIT_ENABLE)/* &&
                ((limit_enable & EVENT_ENABLE_TO_OFFNORMAL) == EVENT_ENABLE_TO_OFFNORMAL)*/) {

                    if (!remaining_time_delay)
                        object.write.event_state(object.instance, EVENT_STATE_LOW_LIMIT);
                    else {
                        remaining_time_delay--;
                        object.write.remaining_time_delay(object.instance, remaining_time_delay);
                    }
                    break;
                }
                /* value of the object is still in the same event state */
                object.read.time_delay(object.instance, &time_delay);
                object.write.remaining_time_delay(object.instance, time_delay);
                break;

            case EVENT_STATE_HIGH_LIMIT:
                /* Once exceeded, the Present_Value must fall below the High_Limit minus
                   the Deadband before a TO-NORMAL event is generated under these conditions:
                   (a) Current state is off-normal
                   (b) the Present_Value must fall below the High_Limit minus the Deadband
                   for a minimum period of time, specified in the Time_Delay property, and
                   (c) the HighLimitEnable flag must be set in the Limit_Enable property, and
                   (d) OBSOLETE?: the TO-NORMAL flag must be set in the Event_Enable property. */

                object.read.deadband(object.instance, &deadband);
                object.read.high_limit(object.instance, &high_limit);
                object.read.low_limit(object.instance, &low_limit);
                // object.read.event_enable(object.instance, &event_enable);
                object.read.remaining_time_delay(object.instance, &remaining_time_delay);

                // If High limit enable is false
                if ((limit_enable & EVENT_HIGH_LIMIT_ENABLE) != EVENT_HIGH_LIMIT_ENABLE) {
                    object.write.event_state(object.instance, EVENT_STATE_NORMAL);
                    break;
                }

                // Optional: Checking whether it's Off Normal -> Off Normal transition
                if ((present_val < low_limit) && ((limit_enable & EVENT_LOW_LIMIT_ENABLE) == EVENT_LOW_LIMIT_ENABLE)/* &&
                ((event_enable & EVENT_ENABLE_TO_OFFNORMAL) == EVENT_ENABLE_TO_OFFNORMAL)*/) {
                    if (!remaining_time_delay)
                        object.write.event_state(object.instance, EVENT_STATE_LOW_LIMIT);
                    else {
                        remaining_time_delay--;
                        object.write.remaining_time_delay(object.instance, remaining_time_delay);
                    }
                    break;
                }

                // To Normal Transition
                if ((present_val < high_limit - deadband)/* &&
                ((limit_enable & EVENT_HIGH_LIMIT_ENABLE) == EVENT_HIGH_LIMIT_ENABLE) &&
                ((event_enable & EVENT_ENABLE_TO_NORMAL) == EVENT_ENABLE_TO_NORMAL)*/) {
                    if (!remaining_time_delay) {
                        object.write.event_state(object.instance, EVENT_STATE_NORMAL);
                    } else {
                        remaining_time_delay--;
                        object.write.remaining_time_delay(object.instance, remaining_time_delay);
                    }
                    break;
                }

                /* value of the object is still in the same event state */
                object.read.time_delay(object.instance, &time_delay);
                object.write.remaining_time_delay(object.instance, time_delay);
                object.write.last_offnormal_event_state(object.instance, event_state);
                break;

            case EVENT_STATE_LOW_LIMIT:
                /* Once the Present_Value has fallen below the Low_Limit,
                   the Present_Value must exceed the Low_Limit plus the Deadband
                   before a TO-NORMAL event is generated under these conditions:
                   (a) Current state is normal
                   (b) the Present_Value must exceed the Low_Limit plus the Deadband
                   for a minimum period of time, specified in the Time_Delay property, and
                   (c) the LowLimitEnable flag must be set in the Limit_Enable property, and
                   (d) OBSOLETE?: the TO-NORMAL flag must be set in the Event_Enable property. */
                object.read.deadband(object.instance, &deadband);
                object.read.high_limit(object.instance, &high_limit);
                object.read.low_limit(object.instance, &low_limit);
                // object.read.event_enable(object.instance, &event_enable);
                object.read.remaining_time_delay(object.instance, &remaining_time_delay);

                // If Low limit enable is false
                if ((limit_enable & EVENT_LOW_LIMIT_ENABLE) != EVENT_LOW_LIMIT_ENABLE) {
                    object.write.event_state(object.instance, EVENT_STATE_NORMAL);
                    break;
                }

                // Optional: Checking whether it's Off Normal -> Off Normal transition
                if ((present_val > high_limit) && ((limit_enable & EVENT_HIGH_LIMIT_ENABLE) == EVENT_HIGH_LIMIT_ENABLE)/* &&
                ((event_enable & EVENT_ENABLE_TO_OFFNORMAL) == EVENT_ENABLE_TO_OFFNORMAL)*/) {

                    if (!remaining_time_delay)
                        object.write.event_state(object.instance, EVENT_STATE_HIGH_LIMIT);
                    else {
                        remaining_time_delay--;
                        object.write.remaining_time_delay(object.instance, remaining_time_delay);
                    }
                    break;
                }

                // To Normal Transition
                if ((present_val > low_limit + deadband)/* &&
                ((limit_enable & EVENT_LOW_LIMIT_ENABLE) == EVENT_LOW_LIMIT_ENABLE) &&
                ((event_enable & EVENT_ENABLE_TO_NORMAL) == EVENT_ENABLE_TO_NORMAL)*/) {
                    if (!remaining_time_delay)
                        object.write.event_state(object.instance, EVENT_STATE_NORMAL);
                    else {
                        remaining_time_delay--;
                        object.write.remaining_time_delay(object.instance, remaining_time_delay);
                    }
                    break;
                }
                /* value of the object is still in the same event state */
                object.read.time_delay(object.instance, &time_delay);
                object.write.remaining_time_delay(object.instance, time_delay);
                object.write.last_offnormal_event_state(object.instance, event_state);
                break;

            default:
                return; /* shouldn't happen */
        }           /* switch (FromState) */

        // Check event enable whether to send out a notification
        object.read.event_state(object.instance, &event_state);
        ToState = event_state;

        uint8_t event_enable_flag = EVENT_ENABLE_TO_NORMAL;
        if (ToState == EVENT_STATE_HIGH_LIMIT || ToState == EVENT_STATE_LOW_LIMIT)
            event_enable_flag = EVENT_ENABLE_TO_OFFNORMAL;

        uint8_t event_enable;
        object.read.event_enable(object.instance, &event_enable);
        bool isEventEnabled = (event_enable & event_enable_flag) == event_enable_flag;

        if (FromState != ToState && isEventEnabled) {
            /* Event_State has changed.
               Need to fill only the basic parameters of this type of event.
               Other parameters will be filled in common function. */

            switch (ToState) {
                case EVENT_STATE_HIGH_LIMIT:
                    object.read.high_limit(object.instance, &high_limit);
                    ExceededLimit = high_limit;
#if defined(CERTIFICATION_SOFTWARE)
                    characterstring_init_ansi(&msgText, "Goes to high limit");
#else
                    characterstring_init_ansi(&msgText, "Trigger");
#endif
                    break;

                case EVENT_STATE_LOW_LIMIT:
                    object.read.low_limit(object.instance, &low_limit);
                    ExceededLimit = low_limit;
                    characterstring_init_ansi(&msgText, "Goes to low limit");
                    break;

                case EVENT_STATE_NORMAL:
                    if (FromState == EVENT_STATE_HIGH_LIMIT) {
                        object.read.high_limit(object.instance, &high_limit);
                        ExceededLimit = high_limit;
                        characterstring_init_ansi(&msgText, "Back to normal state from high limit");
                    } else {
                        object.read.low_limit(object.instance, &low_limit);
                        ExceededLimit = low_limit;
                        characterstring_init_ansi(&msgText, "Back to normal state from low limit");
                    }
                    break;

                default:
                    ExceededLimit = 0;
                    break;
            } /* switch (ToState) */

            /* Notify Type */
            object.read.notify_type(object.instance, &notify_type);
            event_data.notifyType = static_cast<BACNET_NOTIFY_TYPE>(notify_type);

            /* Send EventNotification. */
            SendNotify = true;
        }
            // To return the state to normal after an alarm has been triggered
#if !defined(CERTIFICATION_SOFTWARE)
        else {
            object.read.high_limit(object.instance, &high_limit);
            if (high_limit < present_val && event_state != EVENT_STATE_NORMAL) {
                object.write.high_limit(object.instance, present_val);
            }
        }
#endif
    }

    if (SendNotify) {
        /* Event Object Identifier */
        event_data.eventObjectIdentifier.type = OBJECT_ANALOG_INPUT;
        event_data.eventObjectIdentifier.instance = object.instance;

        /* Time Stamp */
        event_data.timeStamp.tag = TIME_STAMP_DATETIME;

        Device_getCurrentDateTime(&event_data.timeStamp.value.dateTime);

        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            std::vector<BACNET_DATE_TIME> event_time_stamps;
            event_time_stamps.reserve(MAX_BACNET_EVENT_TRANSITION);
            object.read.event_time_stamps(object.instance, event_time_stamps);

            /* fill Event_Time_Stamps */
            switch (ToState) {
                case EVENT_STATE_HIGH_LIMIT:
                case EVENT_STATE_LOW_LIMIT:
                    event_time_stamps[TRANSITION_TO_OFFNORMAL] = event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_FAULT:
                    event_time_stamps[TRANSITION_TO_FAULT] = event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_NORMAL:
                    event_time_stamps[TRANSITION_TO_NORMAL] = event_data.timeStamp.value.dateTime;
                    break;
            }
            object.write.event_time_stamps(object.instance, event_time_stamps);
        }

        /* Notification Class */
        unsigned notification_class;
        object.read.notification_class(object.instance, &notification_class);
        event_data.notificationClass = notification_class;

        /* Event Type */
        event_data.eventType = EVENT_OUT_OF_RANGE;

        /* Message Text */
        event_data.messageText = &msgText;

        /* Notify Type */
        /* filled before */

        /* From State */
        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION)
            event_data.fromState = static_cast<BACNET_EVENT_STATE>(FromState);

        /* To State */
        event_data.toState = static_cast<BACNET_EVENT_STATE>(ToState);

        bool out_of_service;
        object.read.out_of_service(object.instance, &out_of_service);

        /* Event Values */
        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            /* Value that exceeded a limit. */
            event_data.notificationParams.outOfRange.exceedingValue = present_val;
            /* Status_Flags of the referenced object. */
            bitstring_init(&event_data.notificationParams.outOfRange.statusFlags);
            bitstring_set_bit(&event_data.notificationParams.outOfRange.statusFlags,
                              STATUS_FLAG_IN_ALARM,
                              event_state ? true : false);
            bitstring_set_bit(&event_data.notificationParams.outOfRange.statusFlags, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&event_data.notificationParams.outOfRange.statusFlags, STATUS_FLAG_OVERRIDDEN, false);
            bitstring_set_bit(&event_data.notificationParams.outOfRange.statusFlags,
                              STATUS_FLAG_OUT_OF_SERVICE,
                              out_of_service);
            /* Deadband used for limit checking. */
            object.read.deadband(object.instance, &deadband);
            event_data.notificationParams.outOfRange.deadband = deadband;
            /* Limit that was exceeded. */
            event_data.notificationParams.outOfRange.exceededLimit = ExceededLimit;
        }

        /* add data from notification class */
        notificationClassCommonReportingFunction(&event_data);

        /* Ack required */
        if ((event_data.notifyType != NOTIFY_ACK_NOTIFICATION) && (event_data.ackRequired == true)) {
            std::vector<ACKED_INFO> acked_transitions;
            acked_transitions.reserve(MAX_BACNET_EVENT_TRANSITION);
            object.read.acked_transitions(object.instance, acked_transitions);

            switch (event_data.toState) {
                case EVENT_STATE_OFFNORMAL:
                case EVENT_STATE_HIGH_LIMIT:
                case EVENT_STATE_LOW_LIMIT:
                    acked_transitions[TRANSITION_TO_OFFNORMAL].bIsAcked = false;
                    acked_transitions[TRANSITION_TO_OFFNORMAL].Time_Stamp = event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_FAULT:
                    acked_transitions[TRANSITION_TO_FAULT].bIsAcked = false;
                    acked_transitions[TRANSITION_TO_FAULT].Time_Stamp = event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_NORMAL:
                    acked_transitions[TRANSITION_TO_NORMAL].bIsAcked = false;
                    acked_transitions[TRANSITION_TO_NORMAL].Time_Stamp = event_data.timeStamp.value.dateTime;
                    break;
            }

            object.write.acked_transitions(object.instance, acked_transitions);
        }
    }

#endif
}

#if defined(INTRINSIC_REPORTING)

int analog_input_alarm_ack(BACNET_ALARM_ACK_DATA *alarmack_data, BACNET_ERROR_CODE *error_code) {

    bool found = false;

    for (auto &object : container.getDeviceObject()->objects) {
        if (object->type == OBJECT_ANALOG_INPUT && object->instance == alarmack_data->eventObjectIdentifier.instance) {

            bool event_detection_enable;
#if !defined(CERTIFICATION_SOFTWARE)
            event_detection_enable = object->ai_irp.event_detection_enable;
#else
            object->read.event_detection_enable(object->instance, &event_detection_enable);
#endif

            if (!event_detection_enable) {
                *error_code = ERROR_CODE_NO_ALARM_CONFIGURED;
                return -2;
            }

            std::vector<ACKED_INFO> acked_transitions;
            acked_transitions.reserve(MAX_BACNET_EVENT_TRANSITION);
            object->read.acked_transitions(object->instance, acked_transitions);

            uint8_t event_state;
            object->read.event_state(object->instance, &event_state);

            switch (alarmack_data->eventStateAcked) {
                case EVENT_STATE_OFFNORMAL:
                case EVENT_STATE_HIGH_LIMIT:
                case EVENT_STATE_LOW_LIMIT:

                    if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                        *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                        return -1;
                    }
                    if (datetime_compare(&acked_transitions[TRANSITION_TO_OFFNORMAL].Time_Stamp,
                                         &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                        *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                        return -1;
                    }

                    uint8_t last_offnormal_event_state;
                    object->read.last_offnormal_event_state(object->instance, &last_offnormal_event_state);

                    if (alarmack_data->eventStateAcked != EVENT_STATE_OFFNORMAL) {
                        if (alarmack_data->eventStateAcked !=
                            static_cast<BACNET_EVENT_STATE>(last_offnormal_event_state)) {
                            *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                            return -1;
                        }
                    }

                    /* FIXME: Send ack notification */
                    acked_transitions[TRANSITION_TO_OFFNORMAL].bIsAcked = true;
                    object->write.acked_transitions(object->instance, acked_transitions);
                    break;

                case EVENT_STATE_FAULT:
                    if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                        *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                        return -1;
                    }
                    if (datetime_compare(&acked_transitions[TRANSITION_TO_FAULT].Time_Stamp,
                                         &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                        *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                        return -1;
                    }

                    /* FIXME: Send ack notification */
                    acked_transitions[TRANSITION_TO_FAULT].bIsAcked = true;
                    object->write.acked_transitions(object->instance, acked_transitions);
                    break;

                case EVENT_STATE_NORMAL:
                    if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                        *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                        return -1;
                    }
                    if (datetime_compare(&acked_transitions[TRANSITION_TO_NORMAL].Time_Stamp,
                                         &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                        *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                        return -1;
                    }

                    /* FIXME: Send ack notification */
                    acked_transitions[TRANSITION_TO_NORMAL].bIsAcked = true;
                    object->write.acked_transitions(object->instance, acked_transitions);

                    break;

                default:
                    return -3;
            }

            ACK_NOTIFICATION ack_notify_data;
            object->read.ack_notify_data(object->instance, ack_notify_data);

            ack_notify_data.bSendAckNotify = true;
            ack_notify_data.EventState = alarmack_data->eventStateAcked;

            object->write.ack_notify_data(object->instance, ack_notify_data);

            found = true;

            return 1;
        }
    }

    if (!found) {
        *error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return -2;
    }
}

int analog_input_alarm_summary(unsigned index, BACNET_GET_ALARM_SUMMARY_DATA *getalarm_data) {

    uint32_t last_analog_input_id = 0;
    for (auto &object : container.getDeviceObject()->objects) {
        if (object->type == OBJECT_ANALOG_INPUT) {
            if (last_analog_input_id < object->instance)
                last_analog_input_id = object->instance;
        }
    }

    if (index > last_analog_input_id)
        return -1;

    for (auto &object : container.getDeviceObject()->objects) {
        if (object->type == OBJECT_ANALOG_INPUT && object->instance == index) {

            uint8_t event_state, notify_type;
            object->read.event_state(object->instance, &event_state);
            object->read.notify_type(object->instance, &notify_type);

            std::vector<ACKED_INFO> acked_transitions;
            acked_transitions.reserve(MAX_BACNET_EVENT_TRANSITION);
            object->read.acked_transitions(object->instance, acked_transitions);

            /* Event_State is not equal to NORMAL  and
               Notify_Type property value is ALARM */
            if ((static_cast<BACNET_EVENT_STATE>(event_state) != EVENT_STATE_NORMAL) &&
                (static_cast<BACNET_NOTIFY_TYPE>(notify_type) == NOTIFY_ALARM)) {
                /* Object Identifier */
                getalarm_data->objectIdentifier.type = OBJECT_ANALOG_INPUT;
                getalarm_data->objectIdentifier.instance = object->instance;
                /* Alarm State */
                getalarm_data->alarmState = static_cast<BACNET_EVENT_STATE>(event_state);
                /* Acknowledged Transitions */
                bitstring_init(&getalarm_data->acknowledgedTransitions);
                bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                                  TRANSITION_TO_OFFNORMAL,
                                  acked_transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
                bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                                  TRANSITION_TO_FAULT,
                                  acked_transitions[TRANSITION_TO_FAULT].bIsAcked);
                bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                                  TRANSITION_TO_NORMAL,
                                  acked_transitions[TRANSITION_TO_NORMAL].bIsAcked);

                return 1; /* active alarm */
            } else
                return 0; /* no active alarm at this index */
        }
    }
}

int analog_input_event_information(unsigned index, BACNET_GET_EVENT_INFORMATION_DATA *getevent_data) {

    uint32_t last_analog_input_id = 0;
    for (auto &object : container.getDeviceObject()->objects) {
        if (object->type == OBJECT_ANALOG_INPUT) {
            if (last_analog_input_id < object->instance)
                last_analog_input_id = object->instance;
        }
    }

    if (index > last_analog_input_id)
        return -1;

    for (auto &object : container.getDeviceObject()->objects) {
        if (object->type == OBJECT_ANALOG_INPUT && object->instance == index) {

            bool event_detection_enable;
#if !defined(CERTIFICATION_SOFTWARE)
            event_detection_enable = object->ai_irp.event_detection_enable;
#else
            object->read.event_detection_enable(object->instance, &event_detection_enable);
#endif

            if (event_detection_enable) {
                bool IsNotAckedTransitions;
                bool IsActiveEvent;
                int i;
                uint32_t notification_class;
                uint8_t event_state, notify_type, event_enable;
                object->read.event_state(object->instance, &event_state);
                object->read.notify_type(object->instance, &notify_type);
                object->read.event_enable(object->instance, &event_enable);
                object->read.notification_class(object->instance, &notification_class);

                std::vector<ACKED_INFO> acked_transitions;
                acked_transitions.reserve(MAX_BACNET_EVENT_TRANSITION);
                object->read.acked_transitions(object->instance, acked_transitions);

                std::vector<BACNET_DATE_TIME> event_time_stamps;
                event_time_stamps.reserve(MAX_BACNET_EVENT_TRANSITION);
                object->read.event_time_stamps(object->instance, event_time_stamps);

                /* Event_State not equal to NORMAL */
                IsActiveEvent = (static_cast<BACNET_EVENT_STATE>(event_state) != EVENT_STATE_NORMAL);

                /* Acked_Transitions property, which has at least one of the bits
                   (TO-OFFNORMAL, TO-FAULT, TONORMAL) set to FALSE. */
                IsNotAckedTransitions = (acked_transitions[TRANSITION_TO_OFFNORMAL].bIsAcked == false) |
                                        (acked_transitions[TRANSITION_TO_FAULT].bIsAcked == false) |
                                        (acked_transitions[TRANSITION_TO_NORMAL].bIsAcked == false);

                if ((IsActiveEvent) || (IsNotAckedTransitions)) {
                    /* Object Identifier */
                    getevent_data->objectIdentifier.type = OBJECT_ANALOG_INPUT;
                    getevent_data->objectIdentifier.instance = object->instance;
                    /* Event State */
                    getevent_data->eventState = static_cast<BACNET_EVENT_STATE>(event_state);
                    /* Acknowledged Transitions */
                    bitstring_init(&getevent_data->acknowledgedTransitions);
                    bitstring_set_bit(&getevent_data->acknowledgedTransitions,
                                      TRANSITION_TO_OFFNORMAL,
                                      acked_transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
                    bitstring_set_bit(&getevent_data->acknowledgedTransitions,
                                      TRANSITION_TO_FAULT,
                                      acked_transitions[TRANSITION_TO_FAULT].bIsAcked);
                    bitstring_set_bit(&getevent_data->acknowledgedTransitions,
                                      TRANSITION_TO_NORMAL,
                                      acked_transitions[TRANSITION_TO_NORMAL].bIsAcked);
                    /* Event Time Stamps */
                    for (i = 0; i < 3; i++) {
                        getevent_data->eventTimeStamps[i].tag = TIME_STAMP_DATETIME;
                        getevent_data->eventTimeStamps[i].value.dateTime = event_time_stamps[i];
                    }
                    /* Notify Type */
                    getevent_data->notifyType = static_cast<BACNET_NOTIFY_TYPE>(notify_type);
                    /* Event Enable */
                    bitstring_init(&getevent_data->eventEnable);
                    bitstring_set_bit(&getevent_data->eventEnable,
                                      TRANSITION_TO_OFFNORMAL,
                                      (event_enable & EVENT_ENABLE_TO_OFFNORMAL) ? true : false);
                    bitstring_set_bit(&getevent_data->eventEnable,
                                      TRANSITION_TO_FAULT,
                                      (event_enable & EVENT_ENABLE_TO_FAULT) ? true : false);
                    bitstring_set_bit(&getevent_data->eventEnable,
                                      TRANSITION_TO_NORMAL,
                                      (event_enable & EVENT_ENABLE_TO_NORMAL) ? true : false);
                    /* Event Priorities */
                    notificationClassGetPriorities(notification_class, getevent_data->eventPriorities);

                    return 1; /* active event */
                } else
                    return 0; /* no active event at this index */
            }
        }
    }
}

#endif /* defined(INTRINSIC_REPORTING) */

static void
analog_input_intrinsic_rpm_property_list(const int **pRequired, const int **pOptional, const int **pProprietary) {
    if (pRequired)
        *pRequired = AI_Properties_Required;

    if (pOptional)
        *pOptional = AI_Properties_Optional;

    if (pProprietary)
        *pProprietary = AI_Properties_Proprietary;
}

#if defined(INTRINSIC_REPORTING)

static void analog_input_intrinsic_init(BACnetObject &object) {

    for (int i = 0; i < MAX_BACNET_EVENT_TRANSITION; i++) {
        object.ai_irp.acked_transitions.push_back(Acked_info({.bIsAcked = true, BACNET_DATE_TIME()}));
        object.ai_irp.event_time_stamps.push_back(BACNET_DATE_TIME());
        datetime_wildcard_set(&object.ai_irp.event_time_stamps[i]);
    }

#if !defined(CERTIFICATION_SOFTWARE)
    if (object.ai_irp.event_detection_enable) {
        BACNET_BIT_STRING limit_enable_bit_string_bit_string;
        bitstring_init(&limit_enable_bit_string_bit_string);
        bitstring_set_bit(&limit_enable_bit_string_bit_string, 0, 0);
        bitstring_set_bit(&limit_enable_bit_string_bit_string, 1, 1);
        object.ai_irp.limit_enable = *limit_enable_bit_string_bit_string.value;

        object.ai_irp.event_enable |= EVENT_ENABLE_TO_OFFNORMAL;
//        object.ai_irp.event_enable |= EVENT_ENABLE_TO_NORMAL;

        object.ai_irp.deadband = -1.0f;
    } else {
        object.ai_irp.limit_enable = 0;
        object.ai_irp.event_enable = 0;
        object.ai_irp.deadband = 0.0f;
    }

    object.ai_irp.high_limit = 0.0f;
    object.ai_irp.low_limit = 0.0f;
    object.ai_irp.time_delay = 0;
    object.ai_irp.notification_class = BACNET_MAX_INSTANCE;
    object.ai_irp.notify_type = 0;
#else
    BACNET_BIT_STRING limit_enable_bit_string_bit_string;
    bitstring_init(&limit_enable_bit_string_bit_string);
    bitstring_set_bit(&limit_enable_bit_string_bit_string, 0, 1);
    bitstring_set_bit(&limit_enable_bit_string_bit_string, 1, 1);
    object.ai_irp.limit_enable = *limit_enable_bit_string_bit_string.value;

    object.ai_irp.event_enable |= EVENT_ENABLE_TO_OFFNORMAL;
    object.ai_irp.event_enable |= EVENT_ENABLE_TO_NORMAL;

    object.ai_irp.high_limit = 100.0f; // TODO: For testing purposes
    object.ai_irp.low_limit = 20.0f;   // TODO: For testing purposes
    object.ai_irp.time_delay = 0;
    object.ai_irp.notification_class = 14;
    object.ai_irp.deadband = 0.0f;
    object.ai_irp.notify_type = 0;
#endif

    object.ai_irp.remaining_time_delay = 0;
    object.ai_irp.event_state = static_cast<uint8_t>(EVENT_STATE_NORMAL);
    object.ai_irp.last_offnormal_event_state = static_cast<uint8_t>(EVENT_STATE_NORMAL);

    object.ai_irp.ack_notify_data =
            std::make_unique<Ack_Notification>(
                    Ack_Notification({.bSendAckNotify = false, static_cast<uint8_t>(EVENT_STATE_NORMAL)}));

    if (!is_init) {
        handler_get_event_information_set(OBJECT_ANALOG_INPUT, analog_input_event_information);
        handler_alarm_ack_set(OBJECT_ANALOG_INPUT, analog_input_alarm_ack);
        handler_get_alarm_summary_set(OBJECT_ANALOG_INPUT, analog_input_alarm_summary);
        is_init = true;
    }
}

#endif

void bacnet::init_analog_input_intrinsic_object_handlers(ObjectTypeHandler &handler) {
#if defined(INTRINSIC_REPORTING)
    handler.init = analog_input_intrinsic_init;
#else
    handler.init = [](BACnetObject&) {

    };
#endif

    handler.read_property = analog_input_intrinsic_read_property;
    handler.write_property = analog_input_intrinsic_write_property;
    handler.rpm_property_list = analog_input_intrinsic_rpm_property_list;
#if defined(INTRINSIC_REPORTING)
    handler.intrinsic_reporting = analog_input_intrinsic_reporting;
#else
    handler.intrinsic_reporting = [](const BACnetObject&) {};
#endif

    handler.count = []() { return 0; };

    // handler.cov = [](const BACnetObject&) { return true; };
    // handler.cov_clear = [](const BACnetObject&) { return; };
    // handler.value_list = [](const BACnetObject&, BACNET_PROPERTY_VALUE*) { return true; };
}
