#ifndef BACNET_C_WRAPPER_H
#define BACNET_C_WRAPPER_H

#include <stdbool.h>
#include <stdint.h>

#include "bacdef.h"
#include "bacenum.h"
#include "rp.h"
#include "wp.h"

//#if (BACNET_PROTOCOL_REVISION >= 14) || (BACNET_PROPERTY_LISTS == 1)
#include "proplist.h"
//#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Get device vendor identifier
 */
uint16_t Device_Vendor_Identifier(void);

/**
 * Get device object instance number
 */
uint32_t Device_Object_Instance_Number(void);

/**
 * Find an object name if it exists. (Validate an object name existance)
 *
 * @param object_name Object name to look for.
 * @param object_type Object type
 * @param object_instance Object instance
 *
 * @return true on success, false otherwise
 */
bool Device_Valid_Object_Name(BACNET_CHARACTER_STRING* object_name, int* object_type, uint32_t* object_instance);

/**
 * Copy a child object's object_name value, given its ID.
 *
 * @param object_type [in] The BACNET_OBJECT_TYPE of the child Object.
 * @param object_instance [in] The object instance number of the child Object.
 * @param object_name [out] The Object Name found for this child Object.
 *
 * @return True on success or else False if not found.
 */
bool Device_Object_Name_Copy(BACNET_OBJECT_TYPE object_type,
    uint32_t object_instance,
    BACNET_CHARACTER_STRING* object_name);

// /** Looks up the requested Object, and fills the Property Value list.
//  * If the Object or Property can't be found, returns false.
//  * @ingroup ObjHelpers
//  * @param [in] The object type to be looked up.
//  * @param [in] The object instance number to be looked up.
//  * @param [out] The value list
//  * @return True if the object instance supports this feature
//  *         and was encoded correctly
//  */
// bool Device_Encode_Value_List(BACNET_OBJECT_TYPE object_type,
//     uint32_t object_instance,
//     BACNET_PROPERTY_VALUE* value_list);

// /** Looks up the requested Object to see if the functionality is supported.
//  * @ingroup ObjHelpers
//  * @param [in] The object type to be looked up.
//  * @return True if the object instance supports this feature.
//  */
// bool Device_Value_List_Supported(BACNET_OBJECT_TYPE object_type);

// /** Checks the COV flag in the requested Object
//  * @ingroup ObjHelpers
//  * @param [in] The object type to be looked up.
//  * @param [in] The object instance to be looked up.
//  * @return True if the COV flag is set
//  */
// bool Device_COV(BACNET_OBJECT_TYPE object_type, uint32_t object_instance);

// /** Clears the COV flag in the requested Object
//  * @ingroup ObjHelpers
//  * @param [in] The object type to be looked up.
//  * @param [in] The object instance to be looked up.
//  */
// void Device_COV_Clear(BACNET_OBJECT_TYPE object_type, uint32_t object_instance);

/**
 * @brief Check whether given object id is correct
 *
 * @param object_type
 * @param object_instance
 * @return true
 * @return false
 */
bool Device_Valid_Object_Id(int object_type, uint32_t object_instance);

/**
 * Get object property list.
 *
 * @param object_type Object type identifier
 * @param pPropertyList Property list
 */
void Device_Objects_Property_List(BACNET_OBJECT_TYPE object_type, struct special_property_list_t* pPropertyList);

/**
 * Execute device read property service.
 *
 * @param rp_data Read property service data
 * @return APDU octet length
 */
int Device_Read_Property(BACNET_READ_PROPERTY_DATA* rp_data);

/**
 * Execute device write property service.
 *
 * @param wp_data Write property service data
 * @return true on success, false otherwise
 */
bool Device_Write_Property(BACNET_WRITE_PROPERTY_DATA* wp_data);

/**
 * @brief Get the actual Date and Time
 *
 * @param DateTime
 */
void Device_getCurrentDateTime(BACNET_DATE_TIME* DateTime);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BACNET_C_WRAPPER_H */
