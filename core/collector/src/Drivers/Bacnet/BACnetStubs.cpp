#include <cstdarg>
#include <cstdio>
#include <iostream>

#include <bacnet/apdu.h>
#include <bacnet/bacaddr.h>
#include <bacnet/bacdef.h>
#include <bacnet/npdu.h>
#include <bacnet/rp.h>
#include <bacnet/wp.h>

extern "C" {

// 1. Debugging Stubs
void debug_print(const char *format, ...) {
  // Silent in production
}

void debug_printf(const char *format, ...) {
  // Silent in production
}

void debug_perror(const char *s) {
  // Silent
}

void debug_fprintf(FILE *stream, const char *format, ...) {
  // Silent
}

void debug_printf_disabled(const char *format, ...) {
  // Silent
}

// Used by h_wp.c
bool Device_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data) {
  return false;
}

bool Device_Objects_Property_List_Member(BACNET_OBJECT_TYPE object_type,
                                         uint32_t object_instance,
                                         BACNET_PROPERTY_ID property_id) {
  return false;
}

void Device_Objects_Property_List(BACNET_OBJECT_TYPE object_type,
                                  uint32_t object_instance,
                                  struct BACNET_PROPERTY_LIST *property_list) {
  // Mock
}

bool Routed_Device_Service_Approval(BACNET_CONFIRMED_SERVICE_DATA *service_data,
                                    BACNET_ADDRESS *src) {
  return true;
}

bool routed_get_my_address(BACNET_ADDRESS *my_address) { return false; }

} // extern "C"
