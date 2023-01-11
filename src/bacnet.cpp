#include <cstring>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "address.h"
#include "bip.h"
#include "bvlc.h"
#include "client.h"
#include "config.h"
#include "custom_bacnet_config.h"
#include "datalink.h"
#include "dlenv.h"
#include "handlers.h"
#include "iam.h"
// #include "ihave.h"
#include "dcc.h"
#include "getevent.h"
#include "notification_class.hpp"
#include "tsm.h"
#include "txbuf.h"

#include "bacnet.hpp"
#include "datetime.h"

#include "c_wrapper.h"

#include "container.hpp"

#define REGISTRATION_RENEWAL_SAFE_TIME_SECS 30
#define ADDRESS_BINDING_TIME_SECS 60

bacnet::Container container;

namespace bacnet {

    BACnet::BACnet(const std::string &_vendor_name,
                   uint16_t _vendor_identifier,
                   const std::string &_model_name,
                   const std::string &_firmware_revision,
                   const std::string &_application_software_revision,
                   unsigned _port)
            : vendor_name(_vendor_name), vendor_identifier(_vendor_identifier), model_name(_model_name),
              firmware_revision(_firmware_revision), application_software_revision(_application_software_revision),
              database_revision(1), port(_port), _bbmd_addr(0), _bbmd_port(0), _bbmd_ttl(0) {
    }

    BACnet::~BACnet() {
        container.reset();
    }

    void BACnet::initialize() {
#if PRINT_ENABLED
        printf("BACnet Server\n"
               "BACnet Device ID: %u\n"
               "Max APDU: %d\n",
            Device_Object_Instance_Number(),
            MAX_APDU);
#endif

        // Load any static address bindings to show up in our device bindings list
        address_init();

        // We need to handle who-is to support dynamic device binding
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_HAS, handler_who_has);
        /* handle i-am to support binding to other devices */
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am_add);

        /* set the handler for all the services we don't implement
           It is required to send the proper reject message... */
        apdu_set_unrecognized_service_handler_handler(handler_unrecognized_service);

        // Set the handlers for any confirmed services that we support.
        // We must implement read property - it's required!
        apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
        apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROP_MULTIPLE, handler_read_property_multiple);

        apdu_set_confirmed_handler(SERVICE_CONFIRMED_WRITE_PROPERTY, handler_write_property);
        // apdu_set_confirmed_handler(SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE, handler_write_property_multiple);

        // apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_RANGE, handler_read_range);
        // apdu_set_confirmed_handler(SERVICE_CONFIRMED_REINITIALIZE_DEVICE, handler_reinitialize_device);

        // apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_UTC_TIME_SYNCHRONIZATION, handler_timesync_utc);
        // apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_TIME_SYNCHRONIZATION, handler_timesync);

        // apdu_set_confirmed_handler(SERVICE_CONFIRMED_SUBSCRIBE_COV, handler_cov_subscribe);
        // apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_COV_NOTIFICATION, handler_ucov_notification);

        // Handle communication so we can shutup when asked
        // apdu_set_confirmed_handler(SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL, handler_device_communication_control);

        // Handle the data coming back from private requests
        // apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_PRIVATE_TRANSFER, handler_unconfirmed_private_transfer);

#if defined(INTRINSIC_REPORTING)
        apdu_set_confirmed_handler(SERVICE_CONFIRMED_ACKNOWLEDGE_ALARM, handler_alarm_ack);
        apdu_set_confirmed_handler(SERVICE_CONFIRMED_GET_EVENT_INFORMATION, handler_get_event_information);
        apdu_set_confirmed_handler(SERVICE_CONFIRMED_GET_ALARM_SUMMARY, handler_get_alarm_summary);
#endif /* defined(INTRINSIC_REPORTING) */

        last_seconds = time(NULL);
        address_binding_tmr = 0;
        recipient_scan_tmr = 0;
        foreign_device_renew_tmr = 0;

        // dlenv_init();
        // atexit(datalink_cleanup);

        // BIP_Port is statically initialized to 0xBAC0, so if it is different, then it was programmatically altered, and we
        // shouldn't just stomp on it here. Unless it is set below 1024, since: "The range for well-known ports managed by
        // the IANA is 0-1023."
        if (port < 1024)
            bip_set_port(htons(0xBAC0));
        else
            bip_set_port(htons(port));

        // apdu_timeout_set((uint16_t)0);
        // apdu_retries_set((uint8_t)0);

        // Initialize the Datalink Here
        if (!datalink_init(nullptr)) {
            // error handling
        }

        // reset bacnet settings
        bvlc_set_last_registration_status(NO_REGISTRATION);

        Send_I_Am(&Handler_Transmit_Buffer[0]);
    }

    void BACnet::deinitialize() {
        datalink_cleanup();
    }

    void BACnet::execute() {
        BACNET_ADDRESS src = {0}; // Address where message came from
        uint16_t pdu_len = 0;
        unsigned timeout = 1; // Milliseconds
        uint8_t Rx_Buf[MAX_MPDU] = {0};
        uint32_t elapsed_seconds = 0;
        uint32_t elapsed_milliseconds = 0;
        time_t current_seconds = 0;

        current_seconds = time(nullptr);

        pdu_len = datalink_receive(&src, &Rx_Buf[0], MAX_MPDU, timeout); // 0 bytes on timeout

        if (pdu_len) {
            npdu_handler(&src, &Rx_Buf[0], pdu_len);
        }

        elapsed_seconds = (uint32_t) (current_seconds - last_seconds);
        if (elapsed_seconds) {
            last_seconds = current_seconds;
            dcc_timer_seconds(elapsed_seconds);
            // dlenv_maintenance_timer(elapsed_seconds);
            elapsed_milliseconds = elapsed_seconds * 1000;
            tsm_timer_milliseconds(elapsed_milliseconds);

#if defined(INTRINSIC_REPORTING)
            container.deviceLocalReporting();
#endif
        }

        // Scan cache address
        address_binding_tmr += elapsed_seconds;
        if (address_binding_tmr >= ADDRESS_BINDING_TIME_SECS) {
            address_cache_timer(address_binding_tmr);
            address_binding_tmr = 0;
        }

#if defined(INTRINSIC_REPORTING)
        /* try to find addresses of recipients */
        recipient_scan_tmr += elapsed_seconds;
        if (recipient_scan_tmr >= NC_RESCAN_RECIPIENTS_SECS || hasRecipientListChanged) {
#if PRINT_ENABLED
            printf("Notification class recipient list update triggered.\n");
#endif
            hasRecipientListChanged = false;
            notificationClassFindRecipient();
            recipient_scan_tmr = 0;
        }
#endif

        switch (bvlc_get_last_registration_status()) {
            case NO_REGISTRATION:
                // no action
                break;
            case SUCCESS:
                foreign_device_renew_tmr += elapsed_seconds;
                // It is better not to rely on the grace period of 30s (...I think)
                if (foreign_device_renew_tmr >= (_bbmd_ttl - REGISTRATION_RENEWAL_SAFE_TIME_SECS)) {
#if PRINT_ENABLED
                    printf("Renewing registration with the BBMD.\n");
#endif

                    auto retval = bvlc_register_with_bbmd((uint32_t) _bbmd_addr, htons((uint16_t) _bbmd_port),
                                                          (uint16_t) _bbmd_ttl);
                    if (retval < 0) {
                        bvlc_reset_remote_bbmd_settings();
                        fprintf(stderr, "FAILED to Register with BBMD at %ld \n", _bbmd_addr);
                        bvlc_set_last_registration_status(FAIL);
                    }

                    foreign_device_renew_tmr = 0;
                }

                // TODO: QUIT after grace period?
                break;
            case FAIL:
                foreign_device_renew_tmr = 0;
                break;
            default: {
            }
        }
    }

    bool BACnet::registerForeign(const char *address, long port, long ttl) {
        long bbmd_address = bip_getaddrbyname(address);
        struct in_addr addr;
        addr.s_addr = bbmd_address;

#if PRINT_ENABLED
        printf("Registering with BBMD at %s:%ld for %ld seconds\n", inet_ntoa(addr), port, ttl);
#endif

        auto retval = bvlc_register_with_bbmd(bbmd_address, htons((uint16_t) port), (uint16_t) ttl);
        if (retval < 0) {
            bvlc_reset_remote_bbmd_settings();
            fprintf(stderr, "FAILED to Register with BBMD at %s \n", inet_ntoa(addr));
            return false;
        }

        _bbmd_addr = bbmd_address;
        _bbmd_port = port;
        _bbmd_ttl = ttl;
        return true;
    }

    bool BACnet::sendIAm() {
        Send_I_Am(&Handler_Transmit_Buffer[0]);
    }

    bool BACnet::addDeviceObject(read_object_name_cb object_name_cb,
                                 write_object_name_cb write_object_name_cb,
                                 read_object_identifier_cb object_identifier_cb,
                                 write_object_identifier_cb write_object_identifier_cb,
                                 read_description_cb description_cb,
                                 read_location_cb location_cb,
                                 read_local_time_cb local_time_cb,
                                 read_local_date_cb local_date_cb,
                                 read_database_revision_cb database_revision_cb) {
        return container.addDeviceObject(object_name_cb,
                                         write_object_name_cb,
                                         object_identifier_cb,
                                         write_object_identifier_cb,
                                         description_cb,
                                         location_cb,
                                         local_time_cb,
                                         local_date_cb,
                                         database_revision_cb,
                                         vendor_name,
                                         vendor_identifier,
                                         model_name,
                                         firmware_revision,
                                         application_software_revision);
    }

    bool BACnet::addAnalogInputObject(read_object_name_cb object_name_cb,
                                      read_present_value_real_cb present_value_cb,
#if defined(CERTIFICATION_SOFTWARE)
            read_event_detection_enable_cb event_detection_enable_cb,
#endif
                                      BACNET_ENGINEERING_UNITS units,
                                      const std::string &description,
                                      const std::string &device_type,
                                      float max_value,
                                      float min_value,
                                      float resolution
#if !defined(CERTIFICATION_SOFTWARE)
            ,
                                      bool event_detection_enable,
                                      unsigned notification_class_instance
#endif
    ) {
        return container.addAnalogInputObject(object_name_cb,
                                              present_value_cb,
#if defined(CERTIFICATION_SOFTWARE)
                event_detection_enable_cb,
#endif
                                              units,
                                              description,
                                              device_type,
                                              max_value,
                                              min_value,
                                              resolution
#if !defined(CERTIFICATION_SOFTWARE)
                ,
                                              event_detection_enable,
                                              notification_class_instance
#endif
        );
    }

    bool BACnet::addAnalogValueObject(read_object_name_cb object_name_cb,
                                      read_present_value_real_cb read_present_value_cb,
                                      write_present_value_real_cb write_present_value_cb,
                                      BACNET_ENGINEERING_UNITS units,
                                      const std::string &description,
                                      float max_value,
                                      float min_value,
                                      float resolution) {
        return container.addAnalogValueObject(object_name_cb,
                                              read_present_value_cb,
                                              write_present_value_cb,
                                              units,
                                              description,
                                              max_value,
                                              min_value,
                                              resolution);
    }

    bool BACnet::addMultiStateInputObject(read_object_name_cb object_name_cb,
                                          read_present_value_unsigned_cb present_value_cb,
                                          read_number_of_states_cb number_of_states_cb,
                                          read_state_text_cb state_text_cb,
                                          const std::string &description) {
        return container.addMultiStateInputObject(object_name_cb,
                                                  present_value_cb,
                                                  number_of_states_cb,
                                                  state_text_cb,
                                                  description);
    }

    bool BACnet::addMultiStateValueObject(read_object_name_cb object_name_cb,
                                          read_present_value_unsigned_cb read_present_value_cb,
                                          write_present_value_unsigned_cb write_present_value_cb,
                                          read_number_of_states_cb number_of_states_cb,
                                          read_state_text_cb state_text_cb,
                                          const std::string &description) {
        return container.addMultiStateValueObject(object_name_cb,
                                                  read_present_value_cb,
                                                  write_present_value_cb,
                                                  number_of_states_cb,
                                                  state_text_cb,
                                                  description);
    }

    bool BACnet::addCharacterStringValueObject(read_object_name_cb object_name_cb,
                                               read_present_value_characterstring_cb read_present_value_cb,
                                               write_present_value_characterstring_cb write_present_value_cb,
                                               const std::string &description) {
        return container.addCharacterStringValueObject(object_name_cb,
                                                       read_present_value_cb,
                                                       write_present_value_cb,
                                                       description);
    }

    bool BACnet::addTimeValueObject(read_object_name_cb object_name_cb,
                                    read_present_value_time_cb read_present_value_cb,
                                    write_present_value_time_cb write_present_value_cb,
                                    const std::string &description) {
        return container.addTimeValueObject(object_name_cb, read_present_value_cb, write_present_value_cb, description);
    }

    bool BACnet::addDateValueObject(read_object_name_cb object_name_cb,
                                    read_present_value_date_cb read_present_value_cb,
                                    write_present_value_date_cb write_present_value_cb,
                                    const std::string &description) {
        return container.addDateValueObject(object_name_cb, read_present_value_cb, write_present_value_cb, description);
    }

    bool BACnet::addBitStringValueObject(read_object_name_cb object_name_cb,
                                         read_present_value_bitstring_cb read_present_value_cb,
                                         write_present_value_bitstring_cb write_present_value_cb,
                                         read_number_of_bits_cb number_of_bits_cb,
                                         read_bit_text_cb bit_text_cb,
                                         const std::string &description) {
        return container.addBitStringValueObject(object_name_cb,
                                                 read_present_value_cb,
                                                 write_present_value_cb,
                                                 number_of_bits_cb,
                                                 bit_text_cb,
                                                 description);
    }

    bool BACnet::addNotificationClassObject(read_object_name_cb object_name_cb,
#if !defined(CERTIFICATION_SOFTWARE)
                                            unsigned &nc_instance,
#endif
                                            const std::string &description) {
        return container.addNotificationClassObject(object_name_cb,
#if !defined(CERTIFICATION_SOFTWARE)
                                                    nc_instance,
#endif
                                                    description);
    }

    std::shared_ptr<BACnetObject> BACnet::getDeviceObject() {
        return container.getDeviceObject();
    }

    unsigned BACnet::getDatabaseRevision() {
        return database_revision;
    }

    void BACnet::incDatabaseRevision() {
        database_revision++;
#if PRINT_ENABLED
        printf("database_revision = %d\n", database_revision);
#endif
    }

    bool isTimeWildcard(const BACNET_TIME *btime) {

        if (btime) {
            if (btime->hour == 0xFF || btime->min == 0xFF || btime->sec == 0xFF || btime->hundredths == 0xFF)
                return true;
        }

        return false;
    }

    bool isDateWildcard(const BACNET_DATE *bdate) {

        if (bdate) {
            if (bdate->year == (1900 + 0xFF) || bdate->month > 12 || bdate->day > 31 ||
                (bdate->wday < 1 || bdate->wday > 7))
                return true;
        }

        return false;
    }

} // namespace bacnet
