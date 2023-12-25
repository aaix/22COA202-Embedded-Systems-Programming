#ifndef UTIL_H
#define UTIL_H

#include "device.h"
#include "errors.h"
#include <Arduino.h>

#define MIN_COMMAND_LEN 5

#define NUM_BUTTONS 5
#define MAX_CAPACITY 40


// char literals take octal byte sequence
//[0, 77, device_len_in_bytes, DEVICE...., 79] is how eeprom is formatted
#define EEPROM_SIG_BYTE_1 '\0'
#define EEPROM_SIG_BYTE_2 '\115'
#define EEPROM_TERMINATING_BYTE '\117'


// here we define number as a char and use it
// in place of an int to save us 3bytes per
// number instead of using an int
// there are very few cases where we need to
// go outside -127 to 127 and for those cases
// we can use an int at the cost of double the memory
#define NUMBER char


#define NO_MODIFICATIONS 0b0000u
#define AT_TOP 0b0001u
#define AT_BOTTOM 0b0010u
#define DISPLAY_POWER 0b0100u
#define NO_DEVICES 0b1000u
#define DisplayFlags unsigned NUMBER



enum DisplayMode {
    ALL_DEVICES,
    ON_DEVICES,
    OFF_DEVICES,
    STUDENT_ID,
};

class SmartHomeState {
    private:
        //Device Storage
        NUMBER current_device_index;
        NUMBER num_devices;
        Device devices[MAX_CAPACITY];
        bool devices_slots_free[MAX_CAPACITY];
        NUMBER get_device_index_by_id(char[4]);
        NUMBER insert(char[4]);
        bool shuffle_up(NUMBER);
        bool shuffle_down(NUMBER);
        NUMBER next_free_index();
        bool device_meets_state_criteria(NUMBER);

        // Button State
        unsigned long buttons_down_since[NUM_BUTTONS];

    public:
        //Constructor
        SmartHomeState();

        // Button state
        bool button_down_for(int, unsigned long);
        void update_pressed_buttons(int);

        // Display State
        bool is_current;
        enum DisplayMode display_mode;

        // Device Storage
        DisplayFlags next_device(Device*);
        DisplayFlags prev_device(Device*);
        DisplayFlags current_device(Device*);
        HRESULT add_device(Device);
        HRESULT remove_device(char[4]);
        HRESULT overwrite_device(Device);
        NUMBER device_count();

        // Device Modification
        HRESULT set_device_state(char[4], bool);
        HRESULT set_device_power(char[4], NUMBER);

        // eeprom
        HRESULT write_devices_to_eeprom();
        NUMBER read_devices_from_eeprom();

};

bool is_supported_char(char[], int, bool);

void fill_char_with_int(char[], int, int);


uintptr_t calculate_free_memory();

#endif