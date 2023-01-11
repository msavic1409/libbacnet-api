#ifndef CALLBACKS_HPP
#define CALLBACKS_HPP

#if defined(INTRINSIC_REPORTING)
#include "alarm_ack.h"
#include "get_alarm_sum.h"
#include "getevent.h"
#endif
#include <bacnet/bacenum.h>
#include <bacnet/bacstr.h>
#include <bacnet/datetime.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

/* max "length" of recipient_list */
#ifndef NC_MAX_RECIPIENTS
#define NC_MAX_RECIPIENTS 10
#endif // !NC_MAX_RECIPIENTS

// forward declarations
typedef struct BACnet_Write_Property_Data BACNET_WRITE_PROPERTY_DATA;
typedef struct BACnet_Read_Property_Data BACNET_READ_PROPERTY_DATA;
typedef struct Acked_info ACKED_INFO;
typedef struct Ack_Notification ACK_NOTIFICATION;
typedef struct BACnet_Destination BACNET_DESTINATION;

namespace bacnet {
// forward declare custom status code enum
enum CustomErrorStatusCode : unsigned;

typedef std::function<int(unsigned object_instance, char* object_name)> read_object_name_cb;
typedef std::function<int(unsigned object_instance, const char* object_name)> write_object_name_cb;

typedef std::function<unsigned()> read_object_identifier_cb;
typedef std::function<int(unsigned object_identifier)> write_object_identifier_cb;

typedef std::function<int(unsigned object_instance, float* present_value)> read_present_value_real_cb;
typedef std::function<int(unsigned object_instance, float present_value, CustomErrorStatusCode& status)>
    write_present_value_real_cb;

typedef std::function<int(unsigned object_instance, char* present_value)> read_present_value_characterstring_cb;
typedef std::function<int(unsigned object_instance, const char* const present_value, CustomErrorStatusCode& status)>
    write_present_value_characterstring_cb;

typedef std::function<int(unsigned object_instance, unsigned* present_value)> read_present_value_unsigned_cb;
typedef std::function<int(unsigned object_instance, unsigned present_value, CustomErrorStatusCode& status)>
    write_present_value_unsigned_cb;

typedef std::function<int(unsigned object_instance, BACNET_TIME* present_value)> read_present_value_time_cb;
typedef std::function<int(unsigned object_instance, const BACNET_TIME* present_value, CustomErrorStatusCode& status)>
    write_present_value_time_cb;

typedef std::function<int(unsigned object_instance, BACNET_DATE* present_value)> read_present_value_date_cb;
typedef std::function<int(unsigned object_instance, const BACNET_DATE* present_value, CustomErrorStatusCode& status)>
    write_present_value_date_cb;

typedef std::function<int(unsigned object_instance, BACNET_BIT_STRING* present_value)> read_present_value_bitstring_cb;
typedef std::function<
    int(unsigned object_instance, /*const */ BACNET_BIT_STRING* present_value, CustomErrorStatusCode& status)>
    write_present_value_bitstring_cb;

typedef std::function<int(unsigned object_instance, unsigned bit_text_index, char* bit_text)> read_bit_text_cb;
typedef std::function<int(unsigned object_instance, unsigned* number_of_states)> read_number_of_bits_cb;

typedef std::function<int(unsigned object_instance, float* max_pres_value)> read_max_pres_value_cb;
typedef std::function<int(unsigned object_instance, float* min_pres_value)> read_min_pres_value_cb;
typedef std::function<int(unsigned object_instance, float* resolution)> read_resolution_cb;

typedef std::function<int(unsigned object_instance, BACNET_ENGINEERING_UNITS* units)> read_units_cb;

typedef std::function<int(unsigned object_instance, unsigned state_index, char* state_text)> read_state_text_cb;
typedef std::function<int(unsigned object_instance, unsigned* number_of_states)> read_number_of_states_cb;

typedef std::function<int(unsigned object_instance, bool* out_of_service)> read_out_of_service_cb;
typedef std::function<int(unsigned object_instance, uint8_t* event_state)> read_event_state_cb;

typedef std::function<int(char* model_name)> read_model_name_cb;
typedef std::function<int(char* vendor_name)> read_vendor_name_cb;
typedef std::function<int(uint16_t* vendor_identifier)> read_vendor_identifier_cb;
typedef std::function<int(char* firmware_version)> read_firmware_version_cb;
typedef std::function<int(char* application_version)> read_application_version_cb;
typedef std::function<int(char* location)> read_location_cb;

typedef std::function<int(unsigned object_instance, char* description)> read_description_cb;
typedef std::function<int(unsigned object_instance, char* device_type)> read_device_type_cb;

typedef std::function<int(BACNET_TIME* local_time)> read_local_time_cb;
typedef std::function<int(BACNET_DATE* local_date)> read_local_date_cb;

typedef std::function<unsigned()> read_database_revision_cb;

// Notification class callbacks
typedef std::function<int(unsigned object_instance, std::vector<uint8_t>& priority)> read_priority_cb;
typedef std::function<int(unsigned object_instance, const std::vector<uint8_t>& priority)> write_priority_cb;

typedef std::function<int(unsigned object_instance, uint8_t* ack_required)> read_ack_required_cb;
typedef std::function<int(unsigned object_instance, const uint8_t ack_required)> write_ack_required_cb;

typedef std::function<int(unsigned object_instance, std::vector<BACNET_DESTINATION>& recipient_list)>
    read_recipient_list_cb;
typedef std::function<int(unsigned object_instance, const std::vector<BACNET_DESTINATION>& recipient_list)>
    write_recipient_list_cb;

// Analog input intrinsic callbacks
typedef std::function<int(unsigned object_instance, bool* event_detection_enable)> read_event_detection_enable_cb;
typedef std::function<int(unsigned object_instance, const bool event_detection_enable)> write_event_detection_enable_cb;

typedef std::function<int(unsigned object_instance, uint32_t* time_delay)> read_time_delay_cb;
typedef std::function<int(unsigned object_instance, const uint32_t time_delay)> write_time_delay_cb;

typedef std::function<int(unsigned object_instance, uint32_t* notification_class)> read_notification_class_cb;
typedef std::function<int(unsigned object_instance, const uint32_t notification_class)> write_notification_class_cb;

typedef std::function<int(unsigned object_instance, float* high_limit)> read_high_limit_cb;
typedef std::function<int(unsigned object_instance, const float high_limit)> write_high_limit_cb;

typedef std::function<int(unsigned object_instance, float* low_limit)> read_low_limit_cb;
typedef std::function<int(unsigned object_instance, const float low_limit)> write_low_limit_cb;

typedef std::function<int(unsigned object_instance, float* deadband)> read_deadband_cb;
typedef std::function<int(unsigned object_instance, const float deadband)> write_deadband_cb;

typedef std::function<int(unsigned object_instance, uint8_t* limit_enable)> read_limit_enable_cb;
typedef std::function<int(unsigned object_instance, const uint8_t limit_enable)> write_limit_enable_cb;

typedef std::function<int(unsigned object_instance, uint8_t* event_enable)> read_event_enable_cb;
typedef std::function<int(unsigned object_instance, const uint8_t event_enable)> write_event_enable_cb;

typedef std::function<int(unsigned object_instance, std::vector<ACKED_INFO>& acked_transitions)>
    read_acked_transitions_cb;
typedef std::function<int(unsigned object_instance, const std::vector<ACKED_INFO>& acked_transitions)>
    write_acked_transitions_cb;

typedef std::function<int(unsigned object_instance, uint8_t* notify_type)> read_notify_type_cb;
typedef std::function<int(unsigned object_instance, const uint8_t notify_type)> write_notify_type_cb;

typedef std::function<int(unsigned object_instance, std::vector<BACNET_DATE_TIME>& event_time_stamps)>
    read_event_time_stamps_cb;
typedef std::function<int(unsigned object_instance, const std::vector<BACNET_DATE_TIME>& event_time_stamps)>
    write_event_time_stamps_cb;

typedef std::function<int(unsigned object_instance, uint32_t* remaining_time_delay)> read_remaining_time_delay_cb;
typedef std::function<int(unsigned object_instance, uint32_t remaining_time_delay)> write_remaining_time_delay_cb;

typedef std::function<int(unsigned object_instance, uint8_t event_state)> write_event_state_cb;

typedef std::function<int(unsigned object_instance, uint8_t* last_offnormal_event_state)>
    read_last_offnormal_event_state_cb;
typedef std::function<int(unsigned object_instance, uint8_t last_offnormal_event_state)>
    write_last_offnormal_event_state_cb;

typedef std::function<int(unsigned object_instance, ACK_NOTIFICATION& ack_notify_data)> read_ack_notify_data_cb;
typedef std::function<int(unsigned object_instance, const ACK_NOTIFICATION& ack_notify_data)> write_ack_notify_data_cb;

struct ReadProperty {
    read_object_name_cb object_name;
    read_object_identifier_cb object_identifier;
    read_present_value_real_cb present_value_real;
    read_present_value_unsigned_cb present_value_unsigned;
    read_present_value_characterstring_cb present_value_characterstring;
    read_present_value_time_cb present_value_time;
    read_present_value_date_cb present_value_date;
    read_present_value_bitstring_cb present_value_bitstring;
    read_number_of_bits_cb number_of_bits;
    read_max_pres_value_cb max_pres_value;
    read_min_pres_value_cb min_pres_value;
    read_resolution_cb resolution;
    read_units_cb units;
    read_number_of_states_cb number_of_states;
    read_state_text_cb state_text;
    read_bit_text_cb bit_text;
    read_out_of_service_cb out_of_service;
    read_event_state_cb event_state;
    read_description_cb description;
    read_device_type_cb device_type;
    read_application_version_cb application_version;
    read_firmware_version_cb firmware_revision;
    read_location_cb location;
    read_local_time_cb local_time;
    read_local_date_cb local_date;
    read_vendor_name_cb vendor_name;
    read_model_name_cb model_name;
    read_vendor_identifier_cb vendor_identifier;
    read_database_revision_cb database_revision;

    // notification class
    read_priority_cb priority;
    read_ack_required_cb ack_required;
    read_recipient_list_cb recipient_list;
    // analog input intrinsic
    read_event_detection_enable_cb event_detection_enable;
    read_time_delay_cb time_delay;
    read_notification_class_cb notification_class;
    read_high_limit_cb high_limit;
    read_low_limit_cb low_limit;
    read_deadband_cb deadband;
    read_limit_enable_cb limit_enable;
    read_event_enable_cb event_enable;
    read_acked_transitions_cb acked_transitions;
    read_notify_type_cb notify_type;
    read_event_time_stamps_cb event_time_stamps;
    read_remaining_time_delay_cb remaining_time_delay;
    read_ack_notify_data_cb ack_notify_data;
    read_last_offnormal_event_state_cb last_offnormal_event_state;
};

struct WriteProperty {
    write_object_name_cb object_name;
    write_object_identifier_cb object_identifier;
    write_present_value_real_cb present_value_real;
    write_present_value_unsigned_cb present_value_unsigned;
    write_present_value_characterstring_cb present_value_characterstring;
    write_present_value_time_cb present_value_time;
    write_present_value_date_cb present_value_date;
    write_present_value_bitstring_cb present_value_bitstring;

    // notification class
    write_priority_cb priority;
    write_ack_required_cb ack_required;
    write_recipient_list_cb recipient_list;
    // analog input intrinsic
    write_event_detection_enable_cb event_detection_enable;
    write_time_delay_cb time_delay;
    write_remaining_time_delay_cb remaining_time_delay;
    write_notification_class_cb notification_class;
    write_high_limit_cb high_limit;
    write_low_limit_cb low_limit;
    write_deadband_cb deadband;
    write_limit_enable_cb limit_enable;
    write_event_enable_cb event_enable;
    write_notify_type_cb notify_type;
    write_event_state_cb event_state;
    write_acked_transitions_cb acked_transitions;
    write_ack_notify_data_cb ack_notify_data;
    write_event_time_stamps_cb event_time_stamps;
    write_last_offnormal_event_state_cb last_offnormal_event_state;
};

struct BACnetObject;

typedef std::function<void(BACnetObject&)> object_init_cb;
typedef std::function<unsigned()> object_count_cb;
typedef std::function<unsigned(const BACnetObject&, BACNET_READ_PROPERTY_DATA*)> object_read_property_cb;
typedef std::function<bool(BACnetObject&, BACNET_WRITE_PROPERTY_DATA*)> object_write_property_cb;
typedef std::function<void(const int** pRequired, const int** pOptional, const int** pProprietary)>
    object_rpm_property_list_cb;
// typedef std::function<bool(const BACnetObject&, BACNET_PROPERTY_VALUE* value_list)> object_value_list_cb;
// typedef std::function<bool(const BACnetObject&)> object_cov_cb;
// typedef std::function<void(const BACnetObject&)> object_cov_clear_cb;
typedef std::function<void(const BACnetObject&)> object_intrinsic_reporting_cb;

struct ObjectTypeHandler {
    object_init_cb init;
    object_count_cb count;
    object_read_property_cb read_property;
    object_write_property_cb write_property;
    object_rpm_property_list_cb rpm_property_list;
    // object_cov_cb cov;
    // object_cov_clear_cb cov_clear;
    // object_value_list_cb value_list;
    object_intrinsic_reporting_cb intrinsic_reporting;
};

struct NcIntrinsicReportingParams {
    std::vector<uint8_t> priority;
    uint8_t ack_required;
    std::vector<BACNET_DESTINATION> recipient_list;
};

struct AiIntrinsicReportingParams {
    bool event_detection_enable;
    uint8_t event_enable;
    uint32_t time_delay;
    uint32_t notification_class;
    float high_limit;
    float low_limit;
    float deadband;
    uint8_t limit_enable;
    std::vector<ACKED_INFO> acked_transitions;
    uint8_t notify_type;
    std::vector<BACNET_DATE_TIME> event_time_stamps;
    uint32_t remaining_time_delay;
    uint8_t event_state;
    uint8_t last_offnormal_event_state;
    std::unique_ptr<ACK_NOTIFICATION> ack_notify_data;
};

struct BACnetObject {
    BACNET_OBJECT_TYPE type;
    uint32_t instance;

    ReadProperty read;
    WriteProperty write;
    ObjectTypeHandler handler;

    NcIntrinsicReportingParams nc_irp;
    AiIntrinsicReportingParams ai_irp;

    std::vector<std::shared_ptr<BACnetObject>> objects;
};

// typedef struct object_functions {
//     BACNET_OBJECT_TYPE Object_Type;
//     object_init_function Object_Init;
//     object_count_function Object_Count;
//     object_index_to_instance_function Object_Index_To_Instance;
//     object_valid_instance_function Object_Valid_Instance;
//     object_name_function Object_Name;
//     read_property_function Object_Read_Property;
//     write_property_function Object_Write_Property;
//     rpm_property_lists_function Object_RPM_List;
//     rr_info_function Object_RR_Info;
//     object_iterate_function Object_Iterator;
//     object_value_list_function Object_Value_List;
//     object_cov_function Object_COV;
//     object_cov_clear_function Object_COV_Clear;
//     object_intrinsic_reporting_function Object_Intrinsic_Reporting;
// } object_functions_t;

}; // namespace bacnet

#endif /* CALLBACKS_HPP */
