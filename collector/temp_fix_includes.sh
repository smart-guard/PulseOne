#!/bin/bash
sed -i 's|#include "bacnet/rp.h"|#include "bacnet/basic/client/bac-rw.h"|g' include/Drivers/Bacnet/BACnetDriver.h
sed -i 's|#include "bacnet/wp.h"|#include "bacnet/basic/client/bac-data.h"|g' include/Drivers/Bacnet/BACnetDriver.h
sed -i 's|#include "bacnet/tsm.h"|#include "bacnet/basic/tsm/tsm.h"|g' include/Drivers/Bacnet/BACnetDriver.h
sed -i 's|#include "bacnet/h_npdu.h"|#include "bacnet/basic/npdu/h_npdu.h"|g' include/Drivers/Bacnet/BACnetDriver.h
sed -i '/extern "C" {/a #include "bacnet/basic/client/bac-discover.h"\n#include "bacnet/basic/sys/sbuf.h"\n#include "bacnet/basic/services.h"' include/Drivers/Bacnet/BACnetDriver.h
