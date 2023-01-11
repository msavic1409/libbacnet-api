//
// Created by mbajsic on 1.10.20..
//

#include "bacnet_sink.hpp"

#include <list>
#include <algorithm>

namespace bacnet::notifier {
    std::list<IBacnetSink *> sinks;

    void bacnet_register_sink(IBacnetSink *sink) {
        sinks.push_back(sink);
    }

    void bacnet_deregister_sink(IBacnetSink *sink) {
        std::remove(sinks.begin(), sinks.end(), sink);
    }

    void trigger_event(bacnet_event_t ev) {
        for (const auto &it : sinks) {
            it->HandleBacnetEvent(ev);
        }
    }
}


