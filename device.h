#ifndef DEVICE_H
#define DEVICE_H


// force enum to char type to reduce mem
enum DeviceType: char {
    Speaker,
    Socket,
    Light,
    Thermostat, 
    Camera,
    NotADevice
};

struct Device {
    char id[4];
    DeviceType type;
    char location[16];
    bool state;
    char power; // char not int to reduce memory
};

DeviceType char_to_device_type(char);
#endif