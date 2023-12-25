#include "device.h"

DeviceType char_to_device_type(char c) {
    switch (c) {
        case 'S':
            return Speaker;
        case 'O':
            return Socket;
        case 'L':
            return Light;
        case 'T':
            return Thermostat;
        case 'C':
            return Camera;
        default:
            return NotADevice;
    };
    
}