#ifndef BACNET_HEADERS_SAFE_H
#define BACNET_HEADERS_SAFE_H

#include <string>
#include <vector>
#include <deque>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <limits>

// datalink 매크로 비활성화
#ifdef datalink_init
#undef datalink_init
#endif
#ifdef datalink_send_pdu
#undef datalink_send_pdu
#endif
#ifdef data
#undef data
#endif

// 더미 BACnet 타입들 (BACnet 스택 없이도 컴파일 가능)
typedef enum {
    OBJECT_ANALOG_INPUT = 0,
    OBJECT_DEVICE = 8
} BACNET_OBJECT_TYPE;

typedef enum {
    PROP_PRESENT_VALUE = 85
} BACNET_PROPERTY_ID;

typedef struct {
    uint8_t net;
    uint8_t len;
    uint8_t adr[6];
} BACNET_ADDRESS;

#define BACNET_MAX_INSTANCE 4194303U
#define BACNET_ARRAY_ALL 0xFFFFFFFF

#endif
