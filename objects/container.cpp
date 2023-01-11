#include <cstring>

#include "config.h"
#include "custom_bacnet_config.h"
#include "keys.h"

#include "address.h"
#include "apdu.h"
#include "datetime.h"
#include "proplist.h"
#include "rp.h"
#include "timer.h"
#include "wp.h"

#include "analog_input.hpp"
#include "analog_input_intrinsic.hpp"
#include "analog_value.hpp"
#include "bitstring_value.hpp"
#include "c_wrapper.h"
#include "characterstring_value.hpp"
#include "container.hpp"
#include "date_value.hpp"
#include "device.hpp"
#include "multi_state_input.hpp"
#include "multi_state_value.hpp"
#include "notification_class.hpp"
#include "time_value.hpp"

#include "Poco/Crypto/Cipher.h"
#include "Poco/Crypto/CipherFactory.h"
#include "Poco/Crypto/CipherKey.h"
#include "Poco/Crypto/CryptoStream.h"
#include "Poco/Crypto/RSAKey.h"
#include <Poco/File.h>
#include <Poco/FileStream.h>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Path.h>
#include <Poco/TemporaryFile.h>

#include <fstream>
#include <sstream>

using namespace bacnet;
using namespace Poco::Crypto;

BACNET_TIME Local_Time; /* rely on OS, if there is one */
BACNET_DATE Local_Date; /* rely on OS, if there is one */
// bool Daylight_Savings_Status = false;
const std::string pathToRecipientListFile = "/root/.bacnet_recipient_list.pkg";

Container::Container()
    : instance(0) {
}

uint16_t Container::getVendorIdentifier() {
    uint16_t vendor_identifier;
    device->read.vendor_identifier(&vendor_identifier);
    return vendor_identifier;
}

uint32_t Container::getInstanceNumber() {
    return device->read.object_identifier();
}

void Container::reset() {
    instance = 0;
    if ((bool)device) {
        device.reset();
    }
}

void Container::getObjectsPropertyList(BACNET_OBJECT_TYPE object_type, struct special_property_list_t* pPropertyList) {
    pPropertyList->Required.pList = nullptr;
    pPropertyList->Required.count = 0;
    pPropertyList->Optional.pList = nullptr;
    pPropertyList->Optional.count = 0;
    pPropertyList->Proprietary.pList = nullptr;
    pPropertyList->Proprietary.count = 0;
    for (auto& object : device->objects) {
        if (object->type == object_type) {
            if (object->handler.rpm_property_list) {
                object->handler.rpm_property_list(&pPropertyList->Required.pList,
                    &pPropertyList->Optional.pList,
                    &pPropertyList->Proprietary.pList);

                pPropertyList->Required.count =
                    pPropertyList->Required.pList == nullptr ? 0 : property_list_count(pPropertyList->Required.pList);

                pPropertyList->Optional.count =
                    pPropertyList->Optional.pList == nullptr ? 0 : property_list_count(pPropertyList->Optional.pList);

                pPropertyList->Proprietary.count = pPropertyList->Proprietary.pList == nullptr
                    ? 0
                    : property_list_count(pPropertyList->Proprietary.pList);
            }
            return;
        }
    }
}

bool Container::getValidObjectName(BACNET_CHARACTER_STRING* object_name1, int* object_type, uint32_t* object_instance) {
    bool found = false;
    uint32_t instance;
    BACNET_CHARACTER_STRING object_name2;
    char object_name[MAX_OBJECT_NAME_LENGTH] = "";

    for (const auto& object : device->objects) {
        instance = object->instance;
        object->read.object_name(instance, object_name);
        characterstring_init_ansi(&object_name2, object_name);
        if (characterstring_same(object_name1, &object_name2)) {
            found = true;
            *object_type = object->type;
            *object_instance = object->read.object_identifier();
        }
    }

    return found;
}

bool Container::getValidObjectId(int object_type, uint32_t object_instance) {
    bool found = false;

    for (const auto& object : device->objects) {
        if (object_type == object->type && object_instance == object->instance) {
            found = true;
        }
    }

    return found;
}

bool Container::copyObjectName(BACNET_OBJECT_TYPE object_type,
    uint32_t object_instance,
    BACNET_CHARACTER_STRING* object_name) {
    bool found = false;

    for (const auto& object : device->objects) {
        if (object->type == object_type && object->read.object_identifier() == object_instance) {
            static char _object_name[MAX_OBJECT_NAME_LENGTH] = "";
            object->read.object_name(object_instance, _object_name);
            characterstring_init_ansi(object_name, _object_name);
            found = true;
        }
    }

    return found;
}

// bool Container::deviceEncodeValueList(BACNET_OBJECT_TYPE object_type,
//     uint32_t object_instance,
//     BACNET_PROPERTY_VALUE* value_list) {
//     bool found = false; /* Ever the pessamist! */

//     for (const auto& object : device->objects) {
//         if (object_type == object->type && object_instance == object->instance) {
//             if (object->handler.value_list) {
//                 found = true;
//                 object->handler.value_list(*object, value_list);
//             }
//         }
//     }

//     return found;
// }

// bool Container::deviceCov(BACNET_OBJECT_TYPE object_type, uint32_t object_instance) {
//     bool found = false; /* Ever the pessamist! */

//     for (const auto& object : device->objects) {
//         if (object->type == object_type && object->instance == object_instance) {
//             if (object->handler.cov) {
//                 found = true;
//                 object->handler.cov(*object);
//             }
//         }
//     }

//     return found;
// }

// void Container::deviceCovClear(BACNET_OBJECT_TYPE object_type, uint32_t object_instance) {
//     for (const auto& object : device->objects) {
//         if (object->type == object_type && object->instance == object_instance) {
//             if (object->handler.cov_clear) {
//                 object->handler.cov_clear(*object);
//             }
//         }
//     }
// }

// bool Container::deviceValueListSupported(BACNET_OBJECT_TYPE object_type) {
//     bool found = false; /* Ever the pessamist! */

//     for (const auto& object : device->objects) {
//         if (object->type == object_type) {
//             if (object->handler.value_list) {
//                 found = true;
//             }
//         }
//     }

//     return found;
// }

#if defined(INTRINSIC_REPORTING)
void Container::deviceLocalReporting(void) {
    for (const auto& object : device->objects) {
        if (object->type == OBJECT_ANALOG_INPUT) {
            bool event_detection_enable;
#if !defined(CERTIFICATION_SOFTWARE)
            event_detection_enable = object->ai_irp.event_detection_enable;
#else
            object->read.event_detection_enable(object->instance, &event_detection_enable);
#endif
            if (event_detection_enable) {
                object->handler.intrinsic_reporting(*object);
            }
        }
    }
}
#endif

static void Update_Current_Time(void) {
    struct tm* tblock = NULL;
#if defined(_MSC_VER)
    time_t tTemp;
#else
    struct timeval tv;
#endif
/*
struct tm

int    tm_sec   Seconds [0,60].
int    tm_min   Minutes [0,59].
int    tm_hour  Hour [0,23].
int    tm_mday  Day of month [1,31].
int    tm_mon   Month of year [0,11].
int    tm_year  Years since 1900.
int    tm_wday  Day of week [0,6] (Sunday =0).
int    tm_yday  Day of year [0,365].
int    tm_isdst Daylight Savings flag.
*/
#if defined(_MSC_VER)
    time(&tTemp);
    tblock = localtime(&tTemp);
#else
    if (gettimeofday(&tv, NULL) == 0) {
        tblock = localtime(&tv.tv_sec);
    }
#endif

    if (tblock) {
        datetime_set_date(&Local_Date,
            (uint16_t)tblock->tm_year + 1900,
            (uint8_t)tblock->tm_mon + 1,
            (uint8_t)tblock->tm_mday);
#if !defined(_MSC_VER)
        datetime_set_time(&Local_Time,
            (uint8_t)tblock->tm_hour,
            (uint8_t)tblock->tm_min,
            (uint8_t)tblock->tm_sec,
            (uint8_t)(tv.tv_usec / 10000));
#else
        datetime_set_time(&Local_Time, (uint8_t)tblock->tm_hour, (uint8_t)tblock->tm_min, (uint8_t)tblock->tm_sec, 0);
#endif

        // if (tblock->tm_isdst) {
        //     Daylight_Savings_Status = true;
        // } else {
        //     Daylight_Savings_Status = false;
        // }
    } else {
        datetime_date_wildcard_set(&Local_Date);
        datetime_time_wildcard_set(&Local_Time);
        // Daylight_Savings_Status = false;
    }
}

void Container::getCurrentDateTime(BACNET_DATE_TIME* DateTime) {
    Update_Current_Time();

    DateTime->date = Local_Date;
    DateTime->time = Local_Time;
}

int Container::readProperty(BACNET_READ_PROPERTY_DATA* rp_data) {
    int apdu_len = BACNET_STATUS_ERROR;

#if (BACNET_PROTOCOL_REVISION >= 14)
    struct special_property_list_t property_list;
#endif
    /* initialize the default return values */
    rp_data->error_class = ERROR_CLASS_OBJECT;
    rp_data->error_code = ERROR_CODE_UNKNOWN_OBJECT;

    for (auto& object : device->objects) {
        if (object->type == rp_data->object_type && object->read.object_identifier() == rp_data->object_instance) {
            if (object->handler.read_property) {
#if (BACNET_PROTOCOL_REVISION >= 14)
                if ((int)rp_data->object_property == PROP_PROPERTY_LIST) {
                    getObjectsPropertyList(rp_data->object_type, &property_list);
                    apdu_len = property_list_encode(rp_data,
                        property_list.Required.pList,
                        property_list.Optional.pList,
                        property_list.Proprietary.pList);
                    return apdu_len;
                } else
#endif
                {
                    apdu_len = object->handler.read_property(*object.get(), rp_data);
                    return apdu_len;
                }
            } else {
                // warning, read_property not implemented
            }
        }
    }

    return apdu_len;
}

bool Container::writeProperty(BACNET_WRITE_PROPERTY_DATA* wp_data) {
    bool status = false; /* Ever the pessamist! */
    /* initialize the default return values */
    wp_data->error_class = ERROR_CLASS_OBJECT;
    wp_data->error_code = ERROR_CODE_UNKNOWN_OBJECT;

    auto obj_instance = wp_data->object_instance;
    auto obj_type = wp_data->object_type;

    for (auto& object : device->objects) {
        if (object->type == obj_type && object->read.object_identifier() == obj_instance) {
            if (object->handler.write_property) {
#if (BACNET_PROTOCOL_REVISION >= 14)
                if (wp_data->object_property == PROP_PROPERTY_LIST) {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
                    return (status);
                } else
#endif
                {
                    return object->handler.write_property(*object.get(), wp_data);
                }
            } else {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
                return (status);
            }
        }
    }

    wp_data->error_class = ERROR_CLASS_OBJECT;
    wp_data->error_code = ERROR_CODE_UNKNOWN_OBJECT;

    return (status);
}

bool Container::addDeviceObject(read_object_name_cb object_name_cb,
    write_object_name_cb write_object_name_cb,
    read_object_identifier_cb object_identifier_cb,
    write_object_identifier_cb write_object_identifier_cb,
    read_description_cb description_cb,
    read_location_cb location_cb,
    read_local_time_cb local_time_cb,
    read_local_date_cb local_date_cb,
    read_database_revision_cb database_revision_cb,
    const std::string& vendor_name,
    uint16_t vendor_identifier,
    const std::string& model_name,
    const std::string& firmware_revision,
    const std::string& application_software_revision) {

    if ((bool)device) {
#if PRINT_ENABLED
        printf("Device already created.\n");
#endif
        return false;
    }

    device = std::make_shared<BACnetObject>();

    device->type = OBJECT_DEVICE;
    device->read.object_name = object_name_cb;
    device->write.object_name = write_object_name_cb;
    device->read.object_identifier = object_identifier_cb;
    device->write.object_identifier = write_object_identifier_cb;
    device->read.description = description_cb;
    device->read.location = location_cb;
    device->read.local_time = local_time_cb;
    device->read.local_date = local_date_cb;
    device->read.database_revision = database_revision_cb;

    device->read.vendor_name = [vendor_name](char* _vendor_name) {
        strcpy(_vendor_name, vendor_name.c_str());
        return true;
    };

    device->read.vendor_identifier = [vendor_identifier](uint16_t* _vendor_identifier) {
        *_vendor_identifier = vendor_identifier;
        return true;
    };

    device->read.model_name = [model_name](char* _model_name) {
        strcpy(_model_name, model_name.c_str());
        return true;
    };

    device->read.firmware_revision = [firmware_revision](char* _firmware_revision) {
        strcpy(_firmware_revision, firmware_revision.c_str());
        return true;
    };

    device->read.application_version = [application_software_revision](char* _application_software_revision) {
        strcpy(_application_software_revision, application_software_revision.c_str());
        return true;
    };

    init_device_object_handlers(device->handler);

    device->objects.push_back(device);

    return true;
}

bool Container::addAnalogInputObject(read_object_name_cb object_name_cb,
    read_present_value_real_cb present_value_cb,
#if defined(CERTIFICATION_SOFTWARE)
    read_event_detection_enable_cb event_detection_enable_cb,
#endif
    BACNET_ENGINEERING_UNITS units,
    const std::string& description,
    const std::string& device_type,
    float max_value,
    float min_value,
    float resolution
#if !defined(CERTIFICATION_SOFTWARE)
    ,
    bool event_detection_enable,
    unsigned notification_class_instance
#endif
) {
    if (!(bool)device)
        return false;

    auto aii_obj = std::make_shared<BACnetObject>();

    aii_obj->type = OBJECT_ANALOG_INPUT;
    aii_obj->read.object_name = object_name_cb;
    aii_obj->instance = ++instance;

    // printf("Add: analog-input %d\n", aii_obj->instance);
    aii_obj->read.object_identifier = [aii_obj]() { return aii_obj->instance; };

    aii_obj->read.present_value_real = present_value_cb;

    aii_obj->read.units = [units](unsigned /*object_instance*/, BACNET_ENGINEERING_UNITS* _units) {
        *_units = units;
        return true;
    };

    aii_obj->read.out_of_service = [](unsigned /*object_instance*/, bool* out_of_service) -> int {
        *out_of_service = false;
        return true;
    };

    aii_obj->read.description = [description](unsigned /*object_instance*/, char* _description) -> int {
        strcpy(_description, description.c_str());
        return true;
    };

    aii_obj->read.device_type = [device_type](unsigned /*object_instance*/, char* _device_type) -> int {
        strcpy(_device_type, device_type.c_str());
        return true;
    };

    aii_obj->read.max_pres_value = [max_value](unsigned /*object_instance*/, float* max_pres_value) -> int {
        *max_pres_value = max_value;
        return true;
    };

    aii_obj->read.min_pres_value = [min_value](unsigned /*object_instance*/, float* min_pres_value) -> int {
        *min_pres_value = min_value;
        return true;
    };

    aii_obj->read.resolution = [resolution](unsigned /*object_instance*/, float* _resolution) -> int {
        *_resolution = resolution;
        return true;
    };

#if defined(INTRINSIC_REPORTING)

#if !defined(CERTIFICATION_SOFTWARE)
    // event detection enable
    aii_obj->ai_irp.event_detection_enable = event_detection_enable;

    aii_obj->read.event_detection_enable = [aii_obj](unsigned /*object_instance*/, bool* _event_detection_enable) {
        *_event_detection_enable = aii_obj->ai_irp.event_detection_enable;
        return true;
    };
#else
    aii_obj->read.event_detection_enable = event_detection_enable_cb;
#endif

    aii_obj->write.event_detection_enable = [aii_obj](unsigned /*object_instance*/,
                                                const bool _event_detection_enable) {
        aii_obj->ai_irp.event_detection_enable = _event_detection_enable;
        return true;
    };
    // event enable
    aii_obj->read.event_enable = [aii_obj](unsigned /*object_instance*/, uint8_t* _event_enable) {
        *_event_enable = aii_obj->ai_irp.event_enable;
        return true;
    };
    aii_obj->write.event_enable = [aii_obj](unsigned /*object_instance*/, const uint8_t _event_enable) {
        aii_obj->ai_irp.event_enable = _event_enable;
        return true;
    };
    // time delay
    aii_obj->read.time_delay = [aii_obj](unsigned /*object_instance*/, uint32_t* _time_delay) {
        *_time_delay = aii_obj->ai_irp.time_delay;
        return true;
    };
    aii_obj->write.time_delay = [aii_obj](unsigned /*object_instance*/, const uint32_t _time_delay) {
        aii_obj->ai_irp.time_delay = _time_delay;
        return true;
    };
    // notification class
    aii_obj->read.notification_class = [aii_obj](unsigned /*object_instance*/, uint32_t* _notification_class) {
        *_notification_class = aii_obj->ai_irp.notification_class;
        return true;
    };
    aii_obj->write.notification_class = [aii_obj](unsigned /*object_instance*/, const uint32_t _notification_class) {
        aii_obj->ai_irp.notification_class = _notification_class;
        return true;
    };
    // high limit
    aii_obj->read.high_limit = [aii_obj](unsigned /*object_instance*/, float* _high_limit) {
        *_high_limit = aii_obj->ai_irp.high_limit;
        return true;
    };
    aii_obj->write.high_limit = [aii_obj](unsigned /*object_instance*/, const float _high_limit) {
        aii_obj->ai_irp.high_limit = _high_limit;
        return true;
    };
    // low limit
    aii_obj->read.low_limit = [aii_obj](unsigned /*object_instance*/, float* _low_limit) {
        *_low_limit = aii_obj->ai_irp.low_limit;
        return true;
    };
    aii_obj->write.low_limit = [aii_obj](unsigned /*object_instance*/, const float _low_limit) {
        aii_obj->ai_irp.low_limit = _low_limit;
        return true;
    };
    // deadband
    aii_obj->read.deadband = [aii_obj](unsigned /*object_instance*/, float* _deadband) {
        *_deadband = aii_obj->ai_irp.deadband;
        return true;
    };
    aii_obj->write.deadband = [aii_obj](unsigned /*object_instance*/, const float _deadband) {
        aii_obj->ai_irp.deadband = _deadband;
        return true;
    };
    // limit enable
    aii_obj->read.limit_enable = [aii_obj](unsigned /*object_instance*/, uint8_t* _limit_enable) {
        *_limit_enable = aii_obj->ai_irp.limit_enable;
        return true;
    };
    aii_obj->write.limit_enable = [aii_obj](unsigned /*object_instance*/, const uint8_t _limit_enable) {
        aii_obj->ai_irp.limit_enable = _limit_enable;
        return true;
    };
    // acked transitions
    aii_obj->read.acked_transitions = [aii_obj](unsigned /*object_instance*/,
                                          std::vector<ACKED_INFO>& _acked_transitions) {
        _acked_transitions = aii_obj->ai_irp.acked_transitions;
        return true;
    };
    aii_obj->write.acked_transitions = [aii_obj](unsigned /*object_instance*/,
                                           const std::vector<ACKED_INFO>& _acked_transitions) {
        aii_obj->ai_irp.acked_transitions = _acked_transitions;
        return true;
    };
    // notify type
    aii_obj->read.notify_type = [aii_obj](unsigned /*object_instance*/, uint8_t* _notify_type) {
        *_notify_type = aii_obj->ai_irp.notify_type;
        return true;
    };
    aii_obj->write.notify_type = [aii_obj](unsigned /*object_instance*/, const uint8_t _notify_type) {
        aii_obj->ai_irp.notify_type = _notify_type;
        return true;
    };
    // event time stamps
    aii_obj->read.event_time_stamps = [aii_obj](unsigned /*object_instance*/,
                                          std::vector<BACNET_DATE_TIME>& _event_time_stamps) {
        _event_time_stamps = aii_obj->ai_irp.event_time_stamps;
        return true;
    };
    aii_obj->write.event_time_stamps = [aii_obj](unsigned /*object_instance*/,
                                           const std::vector<BACNET_DATE_TIME>& _event_time_stamps) {
        aii_obj->ai_irp.event_time_stamps = _event_time_stamps;
        return true;
    };
    // remaining time delay
    aii_obj->read.remaining_time_delay = [aii_obj](unsigned /*object_instance*/, uint32_t* _remaining_time_delay) {
        *_remaining_time_delay = aii_obj->ai_irp.remaining_time_delay;
        return true;
    };
    aii_obj->write.remaining_time_delay = [aii_obj](unsigned /*object_instance*/,
                                              const uint32_t _remaining_time_delay) {
        aii_obj->ai_irp.remaining_time_delay = _remaining_time_delay;
        return true;
    };
    // event state
    aii_obj->read.event_state = [aii_obj](unsigned /*object_instance*/, uint8_t* _event_state) {
        *_event_state = aii_obj->ai_irp.event_state;
        return true;
    };
    aii_obj->write.event_state = [aii_obj](unsigned /*object_instance*/, const uint8_t _event_state) {
        aii_obj->ai_irp.event_state = _event_state;
        return true;
    };
    // last offnormal event state
    aii_obj->read.last_offnormal_event_state = [aii_obj](unsigned /*object_instance*/,
                                                   uint8_t* _last_offnormal_event_state) {
        *_last_offnormal_event_state = aii_obj->ai_irp.last_offnormal_event_state;
        return true;
    };
    aii_obj->write.last_offnormal_event_state = [aii_obj](unsigned /*object_instance*/,
                                                    const uint8_t _last_offnormal_event_state) {
        aii_obj->ai_irp.last_offnormal_event_state = _last_offnormal_event_state;
        return true;
    };
    // ack notify data
    aii_obj->read.ack_notify_data = [aii_obj](unsigned /*object_instance*/, ACK_NOTIFICATION& _ack_notify_data) {
        _ack_notify_data = *aii_obj->ai_irp.ack_notify_data;
        return true;
    };
    aii_obj->write.ack_notify_data = [aii_obj](unsigned /*object_instance*/, const ACK_NOTIFICATION& _ack_notify_data) {
        *aii_obj->ai_irp.ack_notify_data = _ack_notify_data;
        return true;
    };
#endif

    init_analog_input_intrinsic_object_handlers(aii_obj->handler);

    aii_obj->handler.count = [this]() -> unsigned {
        unsigned count = 0;
        for (const auto& obj : device->objects) {
            if (obj->type == OBJECT_ANALOG_INPUT)
                count++;
        }
        return count;
    };

    aii_obj->handler.init(*aii_obj);

#if defined(INTRINSIC_REPORTING)
#if !defined(CERTIFICATION_SOFTWARE)
    aii_obj->ai_irp.notification_class = notification_class_instance;
#endif
#endif

    device->objects.push_back(aii_obj);

    return true;
}

bool Container::addAnalogValueObject(read_object_name_cb object_name_cb,
    read_present_value_real_cb read_present_value_cb,
    write_present_value_real_cb write_present_value_cb,
    BACNET_ENGINEERING_UNITS units,
    const std::string& description,
    float max_value,
    float min_value,
    float resolution) {

    if (!(bool)device)
        return false;

    auto av_obj = std::make_shared<BACnetObject>();

    av_obj->type = OBJECT_ANALOG_VALUE;
    av_obj->read.object_name = object_name_cb;
    av_obj->instance = ++instance;
    // printf("Add: analog-input %d\n", av_obj->instance);
    av_obj->read.object_identifier = [av_obj]() { return av_obj->instance; };

    av_obj->read.present_value_real = read_present_value_cb;
    av_obj->write.present_value_real = write_present_value_cb;

    av_obj->read.units = [units](unsigned /*object_instance*/, BACNET_ENGINEERING_UNITS* _units) {
        *_units = units;
        return true;
    };

    av_obj->read.out_of_service = [](unsigned /*object_instance*/, bool* out_of_service) -> int {
        *out_of_service = false;
        return true;
    };

    av_obj->read.description = [description](unsigned /*object_instance*/, char* _description) -> int {
        strcpy(_description, description.c_str());
        return true;
    };

    av_obj->read.max_pres_value = [max_value](unsigned /*object_instance*/, float* max_pres_value) -> int {
        *max_pres_value = max_value;
        return true;
    };

    av_obj->read.min_pres_value = [min_value](unsigned /*object_instance*/, float* min_pres_value) -> int {
        *min_pres_value = min_value;
        return true;
    };

    av_obj->read.resolution = [resolution](unsigned /*object_instance*/, float* _resolution) -> int {
        *_resolution = resolution;
        return true;
    };

    init_analog_value_object_handlers(av_obj->handler);

    av_obj->handler.count = [this]() -> unsigned {
        unsigned count = 0;
        for (const auto& obj : device->objects) {
            if (obj->type == OBJECT_ANALOG_VALUE)
                count++;
        }
        return count;
    };

    device->objects.push_back(av_obj);

    return true;
}

bool Container::addMultiStateInputObject(read_object_name_cb object_name_cb,
    read_present_value_unsigned_cb present_value_cb,
    read_number_of_states_cb number_of_states_cb,
    read_state_text_cb state_text_cb,
    const std::string& description) {
    if (!(bool)device)
        return false;

    auto msi_obj = std::make_shared<BACnetObject>();
    msi_obj->type = OBJECT_MULTI_STATE_INPUT;
    msi_obj->read.object_name = object_name_cb;
    msi_obj->instance = ++instance;
    // printf("Add: multi-state-input %d\n", msi_obj->instance);
    msi_obj->read.object_identifier = [msi_obj]() { return msi_obj->instance; };
    msi_obj->read.present_value_unsigned = present_value_cb;
    msi_obj->read.number_of_states = number_of_states_cb;
    msi_obj->read.state_text = state_text_cb;
    msi_obj->read.out_of_service = [](unsigned /*object_instance*/, bool* out_of_service) -> int {
        *out_of_service = false;
        return true;
    };
    msi_obj->read.description = [description](unsigned /*object_instance*/, char* _description) -> int {
        strcpy(_description, description.c_str());
        return true;
    };

    init_multi_state_input_object_handlers(msi_obj->handler);

    msi_obj->handler.count = [this]() -> unsigned {
        unsigned count = 0;
        for (const auto& obj : device->objects) {
            if (obj->type == OBJECT_MULTI_STATE_INPUT)
                count++;
        }
        return count;
    };

    device->objects.push_back(msi_obj);

    return true;
}

bool Container::addMultiStateValueObject(read_object_name_cb object_name_cb,
    read_present_value_unsigned_cb read_present_value_cb,
    write_present_value_unsigned_cb write_present_value_cb,
    read_number_of_states_cb number_of_states_cb,
    read_state_text_cb state_text_cb,
    const std::string& description) {
    if (!(bool)device)
        return false;

    auto msv_obj = std::make_shared<BACnetObject>();
    msv_obj->type = OBJECT_MULTI_STATE_VALUE;
    msv_obj->read.object_name = object_name_cb;
    msv_obj->instance = ++instance;
    // printf("Add: multi-state-input %d\n", msv_obj->instance);
    msv_obj->read.object_identifier = [msv_obj]() { return msv_obj->instance; };
    msv_obj->read.present_value_unsigned = read_present_value_cb;
    msv_obj->write.present_value_unsigned = write_present_value_cb;
    msv_obj->read.number_of_states = number_of_states_cb;
    msv_obj->read.state_text = state_text_cb;
    msv_obj->read.out_of_service = [](unsigned /*object_instance*/, bool* out_of_service) -> int {
        *out_of_service = false;
        return true;
    };
    msv_obj->read.description = [description](unsigned /*object_instance*/, char* _description) -> int {
        strcpy(_description, description.c_str());
        return true;
    };

    init_multi_state_value_object_handlers(msv_obj->handler);

    msv_obj->handler.count = [this]() -> unsigned {
        unsigned count = 0;
        for (const auto& obj : device->objects) {
            if (obj->type == OBJECT_MULTI_STATE_VALUE)
                count++;
        }
        return count;
    };

    device->objects.push_back(msv_obj);

    return true;
}

bool Container::addCharacterStringValueObject(read_object_name_cb object_name_cb,
    read_present_value_characterstring_cb read_present_value_cb,
    write_present_value_characterstring_cb write_present_value_cb,
    const std::string& description) {
    if (!(bool)device)
        return false;

    auto csv_obj = std::make_shared<BACnetObject>();
    csv_obj->type = OBJECT_CHARACTERSTRING_VALUE;
    csv_obj->read.object_name = object_name_cb;
    csv_obj->instance = ++instance;
    csv_obj->read.object_identifier = [csv_obj]() { return csv_obj->instance; };
    csv_obj->read.present_value_characterstring = read_present_value_cb;
    csv_obj->write.present_value_characterstring = write_present_value_cb;

    csv_obj->read.description = [description](unsigned /*object_instance*/, char* _description) -> int {
        strcpy(_description, description.c_str());
        return true;
    };

    init_characterstring_value_object_handlers(csv_obj->handler);

    csv_obj->handler.count = [this]() -> unsigned {
        unsigned count = 0;
        for (const auto& obj : device->objects) {
            if (obj->type == OBJECT_CHARACTERSTRING_VALUE)
                count++;
        }
        return count;
    };

    device->objects.push_back(csv_obj);

    return true;
}

bool Container::addTimeValueObject(read_object_name_cb object_name_cb,
    read_present_value_time_cb read_present_value_cb,
    write_present_value_time_cb write_present_value_cb,
    const std::string& description) {
    if (!(bool)device)
        return false;

    auto tv_obj = std::make_shared<BACnetObject>();
    tv_obj->type = OBJECT_TIME_VALUE;
    tv_obj->read.object_name = object_name_cb;
    tv_obj->instance = ++instance;
    tv_obj->read.object_identifier = [tv_obj]() { return tv_obj->instance; };
    tv_obj->read.present_value_time = read_present_value_cb;
    tv_obj->write.present_value_time = write_present_value_cb;

    tv_obj->read.description = [description](unsigned /*object_instance*/, char* _description) -> int {
        strcpy(_description, description.c_str());
        return true;
    };

    init_time_value_object_handlers(tv_obj->handler);

    tv_obj->handler.count = [this]() -> unsigned {
        unsigned count = 0;
        for (const auto& obj : device->objects) {
            if (obj->type == OBJECT_TIME_VALUE)
                count++;
        }
        return count;
    };

    device->objects.push_back(tv_obj);

    return true;
}

bool Container::addDateValueObject(read_object_name_cb object_name_cb,
    read_present_value_date_cb read_present_value_cb,
    write_present_value_date_cb write_present_value_cb,
    const std::string& description) {
    if (!(bool)device)
        return false;

    auto dv_obj = std::make_shared<BACnetObject>();
    dv_obj->type = OBJECT_DATE_VALUE;
    dv_obj->read.object_name = object_name_cb;
    dv_obj->instance = ++instance;
    dv_obj->read.object_identifier = [dv_obj]() { return dv_obj->instance; };
    dv_obj->read.present_value_date = read_present_value_cb;
    dv_obj->write.present_value_date = write_present_value_cb;

    dv_obj->read.description = [description](unsigned /*object_instance*/, char* _description) -> int {
        strcpy(_description, description.c_str());
        return true;
    };

    init_date_value_object_handlers(dv_obj->handler);

    dv_obj->handler.count = [this]() -> unsigned {
        unsigned count = 0;
        for (const auto& obj : device->objects) {
            if (obj->type == OBJECT_DATE_VALUE)
                count++;
        }
        return count;
    };

    device->objects.push_back(dv_obj);

    return true;
}

bool Container::addBitStringValueObject(read_object_name_cb object_name_cb,
    read_present_value_bitstring_cb read_present_value_cb,
    write_present_value_bitstring_cb write_present_value_cb,
    read_number_of_bits_cb number_of_bits_cb,
    read_bit_text_cb bit_text_cb,
    const std::string& description) {
    if (!(bool)device)
        return false;

    auto bsv_obj = std::make_shared<BACnetObject>();
    bsv_obj->type = OBJECT_BITSTRING_VALUE;
    bsv_obj->read.object_name = object_name_cb;
    bsv_obj->instance = ++instance;
    bsv_obj->read.object_identifier = [bsv_obj]() { return bsv_obj->instance; };
    bsv_obj->read.number_of_bits = number_of_bits_cb;
    bsv_obj->read.bit_text = bit_text_cb;
    bsv_obj->read.present_value_bitstring = read_present_value_cb;
    bsv_obj->write.present_value_bitstring = write_present_value_cb;

    bsv_obj->read.description = [description](unsigned /*object_instance*/, char* _description) -> int {
        strcpy(_description, description.c_str());
        return true;
    };

    init_bitstring_value_object_handlers(bsv_obj->handler);

    bsv_obj->handler.count = [this]() -> unsigned {
        unsigned count = 0;
        for (const auto& obj : device->objects) {
            if (obj->type == OBJECT_BITSTRING_VALUE)
                count++;
        }
        return count;
    };

    device->objects.push_back(bsv_obj);

    return true;
}

bool Container::addNotificationClassObject(read_object_name_cb object_name_cb,
#if !defined(CERTIFICATION_SOFTWARE)
    unsigned& nc_instance,
#endif
    const std::string& description) {
    if (!(bool)device)
        return false;

    auto nc_obj = std::make_shared<BACnetObject>();
    nc_obj->type = OBJECT_NOTIFICATION_CLASS;
    nc_obj->read.object_name = object_name_cb;
    nc_obj->instance = ++instance;
    nc_obj->read.object_identifier = [nc_obj]() { return nc_obj->instance; };

#if !defined(CERTIFICATION_SOFTWARE)
    nc_instance = nc_obj->instance; // For AI to know which instance number is for which data type monitoring
#endif

    nc_obj->read.description = [description](unsigned /*object_instance*/, char* _description) -> int {
        strcpy(_description, description.c_str());
        return true;
    };

    // priority
    nc_obj->read.priority = [nc_obj](unsigned /*object_instance*/, std::vector<uint8_t>& _priority) {
        _priority = nc_obj->nc_irp.priority;
        return true;
    };
    nc_obj->write.priority = [nc_obj](unsigned /*object_instance*/, const std::vector<uint8_t>& _priority) {
        nc_obj->nc_irp.priority = _priority;
        return true;
    };

    // ack required
    nc_obj->read.ack_required = [nc_obj](unsigned /*object_instance*/, uint8_t* _ack_required) {
        *_ack_required = nc_obj->nc_irp.ack_required;
        return true;
    };
    nc_obj->write.ack_required = [nc_obj](unsigned /*object_instance*/, const uint8_t _ack_required) {
        nc_obj->nc_irp.ack_required = _ack_required;
        return true;
    };

    // recipient list
    nc_obj->read.recipient_list = [nc_obj](unsigned /*object_instance*/,
                                      std::vector<BACNET_DESTINATION>& _recipient_list) {
        _recipient_list = nc_obj->nc_irp.recipient_list;
        return true;
    };
    nc_obj->write.recipient_list = [nc_obj](unsigned /*object_instance*/,
                                       const std::vector<BACNET_DESTINATION>& _recipient_list) {
        nc_obj->nc_irp.recipient_list = _recipient_list;
        return true;
    };

    // recipient list
    nc_obj->read.recipient_list = [nc_obj](unsigned /*object_instance*/,
                                      std::vector<BACNET_DESTINATION>& _recipient_list) {
        _recipient_list = nc_obj->nc_irp.recipient_list;
        return true;
    };
    nc_obj->write.recipient_list = [nc_obj](unsigned /*object_instance*/,
                                       const std::vector<BACNET_DESTINATION>& _recipient_list) {
        nc_obj->nc_irp.recipient_list = _recipient_list;
        return true;
    };

    init_notification_class_object_handlers(nc_obj->handler);

    nc_obj->handler.count = [this]() -> unsigned {
        unsigned count = 0;
        for (const auto& obj : device->objects) {
            if (obj->type == OBJECT_NOTIFICATION_CLASS)
                count++;
        }
        return count;
    };

    nc_obj->handler.init(*nc_obj);

    device->objects.push_back(nc_obj);

    container.ImportRecipientList();

    hasRecipientListChanged = true;

    return true;
}

std::shared_ptr<BACnetObject> Container::getDeviceObject() {
    return device;
}

void Container::ExportRecipientList() {

#if PRINT_ENABLED
    printf("Exporting recipient list to file.\n");
#endif

    try {
        // Delete previous file if exists
        Poco::File cryptedFile(pathToRecipientListFile);
        if (cryptedFile.exists())
            cryptedFile.remove();

        // Fill temporary file with json content
        Poco::TemporaryFile recipientListFile;
        Poco::FileOutputStream recipientListFileStream(recipientListFile.path());

        recipientListFileStream << getRecipientList();
        recipientListFileStream.close();

        // Load keys
        std::istringstream iss_priv(PRIVATE_KEY_DATA);
        std::istream& private_key(iss_priv);
        std::istringstream iss_pub(PUBLIC_KEY_DATA);
        std::istream& public_key(iss_pub);

        CipherFactory& factory = CipherFactory::defaultFactory();
        // Create cipher
        Cipher* pCipher = factory.createCipher(RSAKey(&public_key, &private_key, PRIVATE_KEY_PASSWORD));

        // Encrypt data to a new file
        Poco::FileOutputStream sink(Poco::Path(pathToRecipientListFile).toString());
        Poco::FileInputStream source(recipientListFile.path());
        CryptoOutputStream encryptor(sink, pCipher->createEncryptor());

        Poco::StreamCopier::copyStream(source, encryptor);
    } catch (Poco::Exception& e) {
#if PRINT_ENABLED
        fprintf(stderr, "Error %s; Message: %s", e.what(), e.message());
#endif
    }
}

void Container::ImportRecipientList() {

#if PRINT_ENABLED
    printf("Importing recipient list from file.\n");
#endif

    std::vector<std::pair<std::string, std::vector<BACNET_DESTINATION>>> object_name_recipient_pair_list;

    try {
        // Check whether a recipient list file exists
        Poco::Path recipientFilePath(pathToRecipientListFile);
        Poco::File recipientFile(recipientFilePath);

        if (!recipientFile.exists())
            return;

        CipherFactory& factory = CipherFactory::defaultFactory();

        // Load keys
        std::istringstream iss_priv(PRIVATE_KEY_DATA);
        std::istream& private_key(iss_priv);
        std::istringstream iss_pub(PUBLIC_KEY_DATA);
        std::istream& public_key(iss_pub);

        // Create cipher
        Cipher* pCipher = factory.createCipher(RSAKey(&public_key, &private_key, PRIVATE_KEY_PASSWORD));

        // Decrypt data to a temporary file
        Poco::FileInputStream source(pathToRecipientListFile);
        Poco::TemporaryFile jsonFile;
        Poco::FileStream jsonStream(jsonFile.path());
        pCipher->decrypt(source, jsonStream);

        jsonStream.seekg(0, std::ios_base::beg);

        // Parse data
        Poco::JSON::Parser parser;
        const auto json = parser.parse(jsonStream);
        const Poco::JSON::Object::Ptr root = json.extract<Poco::JSON::Object::Ptr>();

        // Notification classes
        Poco::JSON::Array::Ptr nc_objects = root->getArray("notification_classes");
        for (auto ncArrayIterator : *nc_objects) {

            // Object name
            auto json_object_name =
                ncArrayIterator.extract<Poco::JSON::Object::Ptr>()->get("nc_object_name").convert<std::string>();

            // Recipients
            const auto json_recipient_list =
                ncArrayIterator.extract<Poco::JSON::Object::Ptr>()->get("recipients").extract<Poco::JSON::Array::Ptr>();

            std::vector<BACNET_DESTINATION> recipients;
            for (auto json_recipient : *json_recipient_list) {

                BACNET_DESTINATION recipient;

                // Recipient -> Device identifier
                auto device_identifier =
                    json_recipient.extract<Poco::JSON::Object::Ptr>()->get("device_identifier").convert<uint32_t>();
                recipient.Recipient._.DeviceIdentifier = device_identifier;

                // Recipient -> Recipient type
                auto recipient_type =
                    json_recipient.extract<Poco::JSON::Object::Ptr>()->get("recipient_type").convert<uint8_t>();
                recipient.Recipient.RecipientType = recipient_type;

                auto address_obj = json_recipient.extract<Poco::JSON::Object::Ptr>()->get("address");

                // Recipient -> Address
                auto address = address_obj.extract<Poco::JSON::Object::Ptr>()->get("adr").convert<std::string>();

                std::vector<std::string> addr_str;

                std::stringstream adr_iss(address);
                std::string addr_token;
                while (std::getline(adr_iss, addr_token, ';'))
                    addr_str.push_back(addr_token);

                for (unsigned i = 0; i < MAX_MAC_LEN; i++)
                    recipient.Recipient._.Address.adr[i] = static_cast<uint8_t>(std::stoi(addr_str[i]));

                // Recipient -> Mac
                auto mac_address = address_obj.extract<Poco::JSON::Object::Ptr>()->get("mac").convert<std::string>();

                std::vector<std::string> mac_str;

                std::stringstream mac_iss(mac_address);
                std::string mac_token;
                while (std::getline(mac_iss, mac_token, ';'))
                    mac_str.push_back(mac_token);

                for (unsigned i = 0; i < MAX_MAC_LEN; i++)
                    recipient.Recipient._.Address.mac[i] = static_cast<uint8_t>(std::stoi(mac_str[i]));

                // Recipient -> len
                auto addr_len = address_obj.extract<Poco::JSON::Object::Ptr>()->get("len").convert<uint8_t>();
                recipient.Recipient._.Address.len = addr_len;

                // Recipient -> mac_len
                auto mac_len = address_obj.extract<Poco::JSON::Object::Ptr>()->get("mac_len").convert<uint8_t>();
                recipient.Recipient._.Address.mac_len = mac_len;

                // Recipient -> net
                auto net = address_obj.extract<Poco::JSON::Object::Ptr>()->get("net").convert<uint16_t>();
                recipient.Recipient._.Address.net = net;

                // Valid days
                auto valid_days =
                    json_recipient.extract<Poco::JSON::Object::Ptr>()->get("valid_days").convert<uint8_t>();
                recipient.ValidDays = valid_days;

                // From time
                auto from_time_obj = json_recipient.extract<Poco::JSON::Object::Ptr>()->get("from_time");

                // From time -> hour
                auto from_hour = from_time_obj.extract<Poco::JSON::Object::Ptr>()->get("hour").convert<uint8_t>();
                recipient.FromTime.hour = from_hour;

                // From time -> min
                auto from_min = from_time_obj.extract<Poco::JSON::Object::Ptr>()->get("min").convert<uint8_t>();
                recipient.FromTime.min = from_min;

                // From time -> sec
                auto from_sec = from_time_obj.extract<Poco::JSON::Object::Ptr>()->get("sec").convert<uint8_t>();
                recipient.FromTime.sec = from_sec;

                // From time -> hundredths
                auto from_hundredths =
                    from_time_obj.extract<Poco::JSON::Object::Ptr>()->get("hundredths").convert<uint8_t>();
                recipient.FromTime.hundredths = from_hundredths;

                // To time
                auto to_time_obj = json_recipient.extract<Poco::JSON::Object::Ptr>()->get("to_time");

                // To time -> hour
                auto to_hour = to_time_obj.extract<Poco::JSON::Object::Ptr>()->get("hour").convert<uint8_t>();
                recipient.ToTime.hour = to_hour;

                // To time -> min
                auto to_min = to_time_obj.extract<Poco::JSON::Object::Ptr>()->get("min").convert<uint8_t>();
                recipient.ToTime.min = to_min;

                // To time -> sec
                auto to_sec = to_time_obj.extract<Poco::JSON::Object::Ptr>()->get("sec").convert<uint8_t>();
                recipient.ToTime.sec = to_sec;

                // To time -> hundredths
                auto to_hundredths =
                    to_time_obj.extract<Poco::JSON::Object::Ptr>()->get("hundredths").convert<uint8_t>();
                recipient.ToTime.hundredths = to_hundredths;

                // Process identifier
                auto process_identifier =
                    json_recipient.extract<Poco::JSON::Object::Ptr>()->get("process_identifier").convert<uint32_t>();
                recipient.ProcessIdentifier = process_identifier;

                // Transitions
                auto transitions =
                    json_recipient.extract<Poco::JSON::Object::Ptr>()->get("transitions").convert<uint8_t>();
                recipient.Transitions = transitions;

                // Confirmed notify
                auto confirmed_notify =
                    json_recipient.extract<Poco::JSON::Object::Ptr>()->get("confirmed_notify").convert<bool>();
                recipient.ConfirmedNotify = confirmed_notify;

                recipients.push_back(recipient);
            }

            object_name_recipient_pair_list.push_back(std::make_pair(json_object_name, recipients));
        }

        // Configure notification classes
        configureNotificationClasses(object_name_recipient_pair_list);

    } catch (Poco::Exception& e) {
#if PRINT_ENABLED
        fprintf(stderr, "Error %s; Message: %s", e.what(), e.message());
#endif
    }
}

std::string Container::getRecipientList() {
    Poco::JSON::Object::Ptr root = new Poco::JSON::Object();
    Poco::JSON::Object::Ptr rootObject = new Poco::JSON::Object();
    Poco::JSON::Array::Ptr notificationClassArray = new Poco::JSON::Array();

    unsigned notification_class_cnt = 0;
    for (const auto& object : getDeviceObject()->objects) {
        if (object->type == OBJECT_NOTIFICATION_CLASS) {
            Poco::JSON::Object::Ptr notificationClassObject = new Poco::JSON::Object();

            auto recipient_list = object->nc_irp.recipient_list;
            Poco::JSON::Array::Ptr recipientArray = new Poco::JSON::Array();

            // Notification class object namespace
            static char object_name[MAX_OBJECT_NAME_LENGTH] = "";
            object->read.object_name(object->instance, object_name);
            notificationClassObject->set("nc_object_name", std::string(object_name));

            unsigned recipient_cnt = 0;
            for (const auto& recipient : recipient_list) {

                // Recipient
                Poco::JSON::Object::Ptr recipientObject = new Poco::JSON::Object();

                // Recipient -> Device identifier
                recipientObject->set("device_identifier", recipient.Recipient._.DeviceIdentifier);

                // Recipient -> Recipient_type
                recipientObject->set("recipient_type", recipient.Recipient.RecipientType);

                // Recipient -> Address
                Poco::JSON::Object::Ptr recipientAddressObject = new Poco::JSON::Object();

                std::string address;
                for (int i = 0; i < MAX_MAC_LEN; i++) {
                    address.append(std::to_string(recipient.Recipient._.Address.adr[i]));
                    if (i != (MAX_MAC_LEN - 1))
                        address.append(";");
                }

                recipientAddressObject->set("adr", address);
                recipientAddressObject->set("len", recipient.Recipient._.Address.len);

                std::string mac;
                for (int i = 0; i < MAX_MAC_LEN; i++) {
                    mac.append(std::to_string(recipient.Recipient._.Address.mac[i]));
                    if (i != (MAX_MAC_LEN - 1))
                        mac.append(";");
                }

                recipientAddressObject->set("mac", mac);
                recipientAddressObject->set("mac_len", recipient.Recipient._.Address.mac_len);
                recipientAddressObject->set("net", recipient.Recipient._.Address.net);

                recipientObject->set("address", recipientAddressObject);

                // Valid days
                recipientObject->set("valid_days", recipient.ValidDays);

                // From time
                Poco::JSON::Object::Ptr fromTimeObject = new Poco::JSON::Object();
                fromTimeObject->set("hour", recipient.FromTime.hour);
                fromTimeObject->set("min", recipient.FromTime.min);
                fromTimeObject->set("sec", recipient.FromTime.sec);
                fromTimeObject->set("hundredths", recipient.FromTime.hundredths);
                recipientObject->set("from_time", fromTimeObject);

                // To time
                Poco::JSON::Object::Ptr toTimeObject = new Poco::JSON::Object();
                toTimeObject->set("hour", recipient.ToTime.hour);
                toTimeObject->set("min", recipient.ToTime.min);
                toTimeObject->set("sec", recipient.ToTime.sec);
                toTimeObject->set("hundredths", recipient.ToTime.hundredths);
                recipientObject->set("to_time", toTimeObject);

                // Process id
                recipientObject->set("process_identifier", recipient.ProcessIdentifier);

                // Transitions
                recipientObject->set("transitions", recipient.Transitions);

                // Confirmed notify
                recipientObject->set("confirmed_notify", recipient.ConfirmedNotify);

                recipientArray->set(recipient_cnt, recipientObject);
                recipient_cnt++;
            }

            notificationClassObject->set("recipients", recipientArray);

            notificationClassArray->set(notification_class_cnt, notificationClassObject);

            notification_class_cnt++;
        }
    }

    rootObject->set("notification_classes", notificationClassArray);

    root->set("root", rootObject);

    return JsonToString(rootObject);
}

void Container::configureNotificationClasses(
    std::vector<std::pair<std::string, std::vector<BACNET_DESTINATION>>>& recipient_list) {
    auto objects = getDeviceObject()->objects;
    for (const auto& nc : recipient_list) {

        auto nc_obj_name = nc.first;
        auto it = std::find_if(objects.begin(), objects.end(), [nc_obj_name](std::shared_ptr<BACnetObject> obj) {
            static char object_name[MAX_OBJECT_NAME_LENGTH] = "";
            obj->read.object_name(obj->instance, object_name);
            std::string name(object_name);

            return obj->type == OBJECT_NOTIFICATION_CLASS && name == nc_obj_name;
        });

        if (it == objects.end())
            continue;

#if PRINT_ENABLED
        printf("Found notification class with name %s. Setting appropriate values.\n", nc_obj_name.c_str());
#endif

        auto nc_obj = *it;

        nc_obj->write.recipient_list(nc_obj->instance, nc.second);
    }
}

std::string Container::JsonToString(Poco::JSON::Object::Ptr jsonObject) {
    std::string jsonString;

    try {
        Poco::Dynamic::Var dynJson = jsonObject;
        jsonString = dynJson.toString();
    } catch (...) {
    }

    return jsonString;
}