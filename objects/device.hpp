#ifndef BACNET_DEVICE_HPP
#define BACNET_DEVICE_HPP

#include "callbacks.hpp"

namespace bacnet {

    void init_device_object_handlers(ObjectTypeHandler &handler);

} // namespace bacnet

#endif /* BACNET_DEVICE_HPP */
