#ifndef BACNET_HPP
#define BACNET_HPP

#include "callbacks.hpp"
#include <functional>
#include <string>
#include <vector>

namespace bacnet {

    enum CustomErrorStatusCode : unsigned {
        StatusWriteAccessDenied, StatusNotInRange
    };

    bool isTimeWildcard(const BACNET_TIME *time);

    bool isDateWildcard(const BACNET_DATE *date);

    class BACnet {
    public:
        BACnet(const std::string &vendor_name,
               uint16_t vendor_identifier,
               const std::string &model_name,
               const std::string &firmware_revision,
               const std::string &application_software_revision,
               unsigned port = 0xBAC0);

        ~BACnet();

        bool registerForeign(const char *address, long port, long ttl);

        bool sendIAm();

        bool addDeviceObject(read_object_name_cb object_name_cb,
                             write_object_name_cb write_object_name_cb,
                             read_object_identifier_cb object_identifier_cb,
                             write_object_identifier_cb write_object_identifier_cb,

                             read_description_cb description_cb,
                             read_location_cb location_cb,
                             read_local_time_cb local_time_cb,
                             read_local_date_cb local_date_cb,

                             read_database_revision_cb database_revision_cb);

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
                                  const std::string &description = "Analog Input Intrinsic Object",
                                  const std::string &device_type = "Sensor",
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
                                  const std::string &description = "Analog Value Object",
                                  float max_value = 10000.0f,
                                  float min_value = -10000.0f,
                                  float resolution = 0.1f);

        bool addMultiStateInputObject(read_object_name_cb object_name_cb,
                                      read_present_value_unsigned_cb present_value_cb,
                                      read_number_of_states_cb number_of_states_cb,
                                      read_state_text_cb state_text_cb,
                                      const std::string &description = "Multi-State Input Object");

        bool addMultiStateValueObject(read_object_name_cb object_name_cb,
                                      read_present_value_unsigned_cb read_present_value_cb,
                                      write_present_value_unsigned_cb write_present_value_cb,
                                      read_number_of_states_cb number_of_states_cb,
                                      read_state_text_cb state_text_cb,
                                      const std::string &description = "Multi-State Value Object");

        bool addCharacterStringValueObject(read_object_name_cb object_name_cb,
                                           read_present_value_characterstring_cb read_present_value_cb,
                                           write_present_value_characterstring_cb write_present_value_cb,
                                           const std::string &description = "Character String Value Object");

        bool addTimeValueObject(read_object_name_cb object_name_cb,
                                read_present_value_time_cb read_present_value_cb,
                                write_present_value_time_cb write_present_value_cb,
                                const std::string &description = "Time Value Object");

        bool addDateValueObject(read_object_name_cb object_name_cb,
                                read_present_value_date_cb read_present_value_cb,
                                write_present_value_date_cb write_present_value_cb,
                                const std::string &description = "Date Value Object");

        bool addBitStringValueObject(read_object_name_cb object_name_cb,
                                     read_present_value_bitstring_cb read_present_value_cb,
                                     write_present_value_bitstring_cb write_present_value_cb,
                                     read_number_of_bits_cb number_of_bits_cb,
                                     read_bit_text_cb bit_text_cb,
                                     const std::string &description = "BitString Value Object");

        bool addNotificationClassObject(read_object_name_cb object_name_cb,
#if !defined(CERTIFICATION_SOFTWARE)
                                        unsigned &nc_instance,
#endif
                                        const std::string &description = "Notification Class Object");

        void initialize();

        void deinitialize();

        void execute();

        unsigned getDatabaseRevision();

        void incDatabaseRevision();

        std::shared_ptr<BACnetObject> getDeviceObject();

    private:
        std::string vendor_name;
        uint16_t vendor_identifier;
        std::string model_name;
        std::string firmware_revision;
        std::string application_software_revision;

        time_t last_seconds;
        uint32_t address_binding_tmr;
        uint32_t recipient_scan_tmr;
        uint32_t foreign_device_renew_tmr;

        unsigned database_revision;
        uint16_t port;

        long _bbmd_addr;
        long _bbmd_port;
        long _bbmd_ttl;
    };

} // namespace bacnet

#endif /* BACNET_HPP */
