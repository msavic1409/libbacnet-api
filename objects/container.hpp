#ifndef BACNET_CONTAINER_HPP
#define BACNET_CONTAINER_HPP

#include "callbacks.hpp"
#include "rp.h"
#include "wp.h"
#include <cstdint>
#include <memory>
#include <vector>

#include <Poco/JSON/Object.h>

namespace bacnet {

class Container {
  public:
    Container();

    uint16_t getVendorIdentifier();
    uint32_t getInstanceNumber();

    void getCurrentDateTime(BACNET_DATE_TIME* DateTime);

    bool getValidObjectName(BACNET_CHARACTER_STRING* object_name1, int* object_type, uint32_t* object_instance);
    bool getValidObjectId(int object_type, uint32_t object_instance);
    bool copyObjectName(BACNET_OBJECT_TYPE object_type, uint32_t object_instance, BACNET_CHARACTER_STRING* object_name);

    // bool
    // deviceEncodeValueList(BACNET_OBJECT_TYPE object_type, uint32_t object_instance, BACNET_PROPERTY_VALUE*
    // value_list); bool deviceCov(BACNET_OBJECT_TYPE object_type, uint32_t object_instance); void
    // deviceCovClear(BACNET_OBJECT_TYPE object_type, uint32_t object_instance); bool
    // deviceValueListSupported(BACNET_OBJECT_TYPE object_type);

    void getObjectsPropertyList(BACNET_OBJECT_TYPE object_type, struct special_property_list_t* pPropertyList);
    int readProperty(::BACNET_READ_PROPERTY_DATA* rp_data);
    bool writeProperty(::BACNET_WRITE_PROPERTY_DATA* wp_data);

#if defined(INTRINSIC_REPORTING)
    void deviceLocalReporting(void);
#endif

    /* Device */
    bool addDeviceObject(read_object_name_cb object_name_cb,
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
        const std::string& application_software_revision);

    /* Data objects */
    // bool addAnalogInputObject(read_object_name_cb object_name_cb,
    //     read_present_value_real_cb present_value_cb,
    //     BACNET_ENGINEERING_UNITS units = UNITS_NO_UNITS,
    //     const std::string& description = "Analog Input Object",
    //     const std::string& device_type = "Sensor",
    //     float max_value = 10000.0f,
    //     float min_value = -10000.0f,
    //     float resolution = 0.1f);

    bool addAnalogInputObject(read_object_name_cb object_name_cb,
        read_present_value_real_cb present_value_cb,
#if defined(CERTIFICATION_SOFTWARE)
        read_event_detection_enable_cb event_detection_enable_cb,
#endif
        BACNET_ENGINEERING_UNITS units = UNITS_NO_UNITS,
        const std::string& description = "Analog Input Intrinsic Object",
        const std::string& device_type = "Sensor",
        float max_value = 10000.0f,
        float min_value = -10000.0f,
        float resolution = 0.1f
#if !defined(CERTIFICATION_SOFTWARE)
        ,
        bool event_detection_enable = false,
        unsigned notification_class_instance = BACNET_MAX_INSTANCE
#endif
    );

    bool addAnalogValueObject(read_object_name_cb object_name_cb,
        read_present_value_real_cb read_present_value_cb,
        write_present_value_real_cb write_present_value_cb,
        BACNET_ENGINEERING_UNITS units = UNITS_NO_UNITS,
        const std::string& description = "Analog Value Object",
        float max_value = 10000.0f,
        float min_value = -10000.0f,
        float resolution = 0.1f);

    bool addMultiStateInputObject(read_object_name_cb object_name_cb,
        read_present_value_unsigned_cb present_value_cb,
        read_number_of_states_cb number_of_states_cb,
        read_state_text_cb state_text_cb,
        const std::string& description = "Multi-State Input Object");

    bool addMultiStateValueObject(read_object_name_cb object_name_cb,
        read_present_value_unsigned_cb read_present_value_cb,
        write_present_value_unsigned_cb write_present_value_cb,
        read_number_of_states_cb number_of_states_cb,
        read_state_text_cb state_text_cb,
        const std::string& description = "Multi-State Value Object");

    bool addCharacterStringValueObject(read_object_name_cb object_name_cb,
        read_present_value_characterstring_cb read_present_value_cb,
        write_present_value_characterstring_cb write_present_value_cb,
        const std::string& description = "Character String Value Object");

    bool addTimeValueObject(read_object_name_cb object_name_cb,
        read_present_value_time_cb read_present_value_cb,
        write_present_value_time_cb write_present_value_cb,
        const std::string& description = "Time Value Object");

    bool addDateValueObject(read_object_name_cb object_name_cb,
        read_present_value_date_cb read_present_value_cb,
        write_present_value_date_cb write_present_value_cb,
        const std::string& description = "Date Value Object");

    bool addBitStringValueObject(read_object_name_cb object_name_cb,
        read_present_value_bitstring_cb read_present_value_cb,
        write_present_value_bitstring_cb write_present_value_cb,
        read_number_of_bits_cb number_of_bits_cb,
        read_bit_text_cb bit_text_cb,
        const std::string& description = "BitString Value Object");

    bool addNotificationClassObject(read_object_name_cb object_name_cb,
#if !defined(CERTIFICATION_SOFTWARE)
        unsigned& nc_instance,
#endif
        const std::string& description = "Notification Class Object");

    std::shared_ptr<BACnetObject> getDeviceObject();

    void reset();

    void ExportRecipientList();
    void ImportRecipientList();

  private:
    std::string JsonToString(Poco::JSON::Object::Ptr jsonObject);
    std::string getRecipientList();
    void configureNotificationClasses(
        std::vector<std::pair<std::string, std::vector<BACNET_DESTINATION>>>& recipient_list);

    std::shared_ptr<BACnetObject> device;
    unsigned instance;
};

} // namespace bacnet

extern bacnet::Container container;

#endif /* BACNET_CONTAINER_HPP */
