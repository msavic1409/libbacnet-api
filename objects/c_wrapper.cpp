#include "c_wrapper.h"
#include "container.hpp"
#include <stdio.h>
//#include "datetime.h"
#include "rp.h"
#include "wp.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void Device_Objects_Property_List(BACNET_OBJECT_TYPE object_type, struct special_property_list_t* pPropertyList) {
    container.getObjectsPropertyList(object_type, pPropertyList);
}

uint16_t Device_Vendor_Identifier(void) {
    return container.getVendorIdentifier();
}

uint32_t Device_Object_Instance_Number(void) {
    return container.getInstanceNumber();
}

// bool Device_Encode_Value_List(BACNET_OBJECT_TYPE object_type,
//     uint32_t object_instance,
//     BACNET_PROPERTY_VALUE* value_list) {
//     return container.deviceEncodeValueList(object_type, object_instance, value_list);
// }

// bool Device_Value_List_Supported(BACNET_OBJECT_TYPE object_type) {
//     return container.deviceValueListSupported(object_type);
// }

// bool Device_COV(BACNET_OBJECT_TYPE object_type, uint32_t object_instance) {
//     return container.deviceCov(object_type, object_instance);
// }

// void Device_COV_Clear(BACNET_OBJECT_TYPE object_type, uint32_t object_instance) {
//     return container.deviceCovClear(object_type, object_instance);
// }

int Device_Read_Property(BACNET_READ_PROPERTY_DATA* rp_data) {
    // printf("Device_Read_Property type: %d of id: %d\n", rp_data->object_type, rp_data->object_instance);
    return container.readProperty(rp_data);
}

bool Device_Write_Property(BACNET_WRITE_PROPERTY_DATA* wp_data) {
    return container.writeProperty(wp_data);
}

bool Device_Valid_Object_Name(BACNET_CHARACTER_STRING* object_name1, int* object_type, uint32_t* object_instance) {
    return container.getValidObjectName(object_name1, object_type, object_instance);
}

bool Device_Valid_Object_Id(int object_type, uint32_t object_instance) {
    return container.getValidObjectId(object_type, object_instance);
}

bool Device_Object_Name_Copy(BACNET_OBJECT_TYPE object_type,
    uint32_t object_instance,
    BACNET_CHARACTER_STRING* object_name) {
    return container.copyObjectName(object_type, object_instance, object_name);
}

void Device_getCurrentDateTime(BACNET_DATE_TIME* DateTime) {
    return container.getCurrentDateTime(DateTime);
}
#ifdef __cplusplus
}
#endif /* __cplusplus */
