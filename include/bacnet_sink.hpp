//
// Created by mbajsic on 1.10.20..
//

#ifndef BACNET_SINK_HPP
#define BACNET_SINK_HPP

namespace bacnet::notifier {

    enum bacnet_event_t {
        FD_REGISTRATION_NO_REGISTRATION,
        FD_REGISTRATION_SUCCESS,
        FD_REGISTRATION_FAIL
    };

    class IBacnetSink {
    public:
        virtual ~IBacnetSink() = default;

        virtual void HandleBacnetEvent(bacnet_event_t ev) = 0;
    };

    void trigger_event(bacnet_event_t ev);

    void bacnet_deregister_sink(IBacnetSink *sink);

    void bacnet_register_sink(IBacnetSink *sink);

}

#endif //BACNET_SINK_HPP
