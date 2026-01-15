
#include "Drivers/Ble/BleBeaconDriver.h"
#include <iostream>

int main() {
    PulseOne::Drivers::Ble::BleBeaconDriver driver;
    std::cout << "Size: " << sizeof(driver) << std::endl;
    return 0;
}
