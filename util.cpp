#include "device.h"
#include "util.h"
#include "errors.h"

#include <Adafruit_RGBLCDShield.h>
#include <Arduino.h>
#include <EEPROM.h>


extern char *__brkval;

SmartHomeState::SmartHomeState() {
    this->num_devices = 0;
    memset(this->devices_slots_free, true, MAX_CAPACITY);
    this-> current_device_index = 0;
};

NUMBER SmartHomeState::next_free_index() {
    for (NUMBER i=0; i < MAX_CAPACITY; i++) {
        if (devices_slots_free[i]) {
            return i;
        }
    }
    return -1;
};

NUMBER SmartHomeState::get_device_index_by_id(char id[4]) {
    for (NUMBER i = 0; i < MAX_CAPACITY; i++) {
        if (!this->devices_slots_free[i] && strcmp(this->devices[i].id, id) == 0) {
            return i;
        };
    };
    return -1; // not found
};

HRESULT SmartHomeState::add_device(Device device) {

    if (this->get_device_index_by_id(device.id) != -1) {
        return E_STATE_CONFLICTING_DEVICE;
    };

    if (this->num_devices >= MAX_CAPACITY) {
        return E_STATE_CAPACITY_REACHED;
    };


    NUMBER i = this->insert(device.id); 


    if (!this->devices_slots_free[i]) {
        // if slot isnt free shuffle up
        if (!shuffle_up(i)) {
            // if we cant shuffle up shuffle down
            if (!shuffle_down(i)) {
                // if we cant shuffle down panic
                return E_STATE_PANIC;
            }
        }
    }

    this->devices[i] = device;
    this->devices_slots_free[i] = false;
    this->is_current = false;
    this-> num_devices += 1;
    return S_OK;
};

NUMBER SmartHomeState::insert(char id[4]){

    if (this->num_devices == 0) {
        return 0;
    };

    NUMBER last_taken_spot;

    for (NUMBER i = 0; i < MAX_CAPACITY; i ++) {
        if (this->devices_slots_free[i]) {
            continue;
        };
        last_taken_spot = i;

        // ID IS LOWER THAN DEVice at index i
        if (strcmp(this->devices[i].id, id) > 0) {
            return i;
        };

    };

    // we got to the end of the array so we
    // should insert after the last device

    if (last_taken_spot == MAX_CAPACITY-1) {
        return last_taken_spot; // we are going to have to shuffle down
    } else {
        return last_taken_spot+1;
    }
    
}

bool SmartHomeState::shuffle_up(NUMBER after) {
    NUMBER free_index = after + 1;
    while (free_index < MAX_CAPACITY) {
        if (this->devices_slots_free[free_index]) {
            break;
        };
        free_index ++;
    };

    if (free_index >= MAX_CAPACITY) {
        return false; // no space to shuffle up so we will have to shuffle down
    };

    // shuffle up between after-free_index both exclusive
    // we start from the top and move the element below
    // up into the free place
    for (NUMBER i = free_index; i > after; i--) {
        this->devices[i] = this->devices[i-1]; 
        this->devices_slots_free[i] = false;
        this->devices_slots_free[i-1] = true;
    };

    return true;
    
};

bool SmartHomeState::shuffle_down(NUMBER before) {
    NUMBER free_index = before - 1;
    while (free_index >= 0) {
        if (this->devices_slots_free[free_index]) {
            break;
        };
        free_index--;
    };


    // catch an overflow (0-1 is greated than max cap)
    if (free_index > MAX_CAPACITY) {
        return false; // no space to shuffle down so we will have to shuffle up
    };

    // shuffle down between free_index-before both exclusive
    // we start from the bottom and move the element above
    // down into the free place

    for (NUMBER i = free_index; i < before; i++) {
        this->devices[i] = this->devices[i+1]; 
        this->devices_slots_free[i] = false;
        this->devices_slots_free[i+1] = true;
    };

    return true;
}

HRESULT SmartHomeState::remove_device(char id[4]) {
    NUMBER i = get_device_index_by_id(id);
    if (i != -1) {
        this->devices_slots_free[i] = true;
        this->num_devices -=1;
        this->is_current = false;
        if (this->current_device_index==i) {
            this->current_device_index++;
        }
        return S_OK;
    };
    return E_STATE_NO_KNOWN_DEVICE;
}
HRESULT SmartHomeState::overwrite_device(Device device) {
    NUMBER index = this->get_device_index_by_id(device.id);
    if (index == -1) {
        return E_STATE_NO_KNOWN_DEVICE;
    };

    this->devices[index] = device;
    this->is_current = false;
    return S_OK;
}

NUMBER SmartHomeState::device_count() {
    return this->num_devices;
};

bool SmartHomeState::device_meets_state_criteria(NUMBER device_index) {
    switch (this->display_mode) {
        case ON_DEVICES:
            return this->devices[device_index].state == true;
        case OFF_DEVICES:
            return this->devices[device_index].state == false;
        default:
            return true;
    };
}

DisplayFlags SmartHomeState::current_device(Device* device) {
    // we can cheat here by getting the device before the current device+1
    // instead of recalculating extra state
    this->current_device_index += 1;
    DisplayFlags flags = this->prev_device(device);

    if (flags != NO_DEVICES) {
        return flags;
    } else {
        this->current_device_index = 0;
        return this->next_device(device);
    }
};

DisplayFlags SmartHomeState::next_device(Device* device) {
    if (this->num_devices == 0) {
        return NO_DEVICES;
    };

    NUMBER flags = AT_TOP | AT_BOTTOM;
    bool found_device = false;

    // scan for device after current pointer
    // then scan for a next device incase we arent at the top or bottom
    for (NUMBER i = 1 + this->current_device_index; i < MAX_CAPACITY; i++) {

        if (!this->devices_slots_free[i] && this->device_meets_state_criteria(i)) {
            if (!found_device) {
                *device = this->devices[i];
                this->current_device_index = i;
                found_device = true;
            } else {
                flags = flags & ~AT_BOTTOM;
                break; // no need to search more than 1 device below
            };
        };
    };

    if (!found_device) {
        return NO_DEVICES;
    };

    // index 0 always at top
    // prevents overflow
    if (this->current_device_index != 0) {
        for (NUMBER i = this->current_device_index-1; i >= 0; i--) {
            if (!this->devices_slots_free[i] && this->device_meets_state_criteria(i)) {
                flags = flags & ~AT_TOP;
                break;
            };
        };
    } else {
        flags = flags & ~AT_TOP;
    }

    switch (device->type) {
        case Thermostat:
        case Light:
        case Speaker:
            flags = flags | DISPLAY_POWER;
    };


    return flags;
};

DisplayFlags SmartHomeState::prev_device(Device* device) {
    if (this->num_devices == 0) {
        return NO_DEVICES;
    };

    NUMBER flags = AT_TOP | AT_BOTTOM;
    bool found_device = false;

    // scan for a device before the current index
    // then again for another device to see if we are at the bottom
    for (NUMBER i = this->current_device_index-1; i >= 0; i--) {

        if (!this->devices_slots_free[i] && this->device_meets_state_criteria(i)) {
            if (!found_device) {
                *device = this->devices[i];
                this->current_device_index = i;
                found_device = true;
            } else {
                flags = flags & ~AT_TOP;
                break; // no need to search more than 1 device below
            };
        };
    };

    if (!found_device) {
        return NO_DEVICES;
    };

    // scan 
    for (NUMBER i = this->current_device_index+1; i < MAX_CAPACITY; i++) {
        if (!this->devices_slots_free[i] && this->device_meets_state_criteria(i)) {
            flags = flags & ~AT_BOTTOM;
            break;
        };
    };

    switch (device->type) {
        case Thermostat:
        case Light:
        case Speaker:
            flags = flags | DISPLAY_POWER;
    };


    return flags;
}

HRESULT SmartHomeState::set_device_state(char id[4], bool state) {
    NUMBER index = this->get_device_index_by_id(id);
    if (index == -1) {
        return E_STATE_NO_KNOWN_DEVICE;
    };

    this->devices[index].state = state;
    this->is_current = false;
    return S_OK;
};

HRESULT SmartHomeState::set_device_power(char id[4], NUMBER power) {
    NUMBER index = this->get_device_index_by_id(id);

    if (index == -1) {
        return E_STATE_NO_KNOWN_DEVICE;
    };

    switch (this->devices[index].type) {
        case Thermostat:
            if (power < 9 || power > 32) {
                return E_COMMAND_VALUE_OUT_OF_RANGE;
            };
            break;
        case Speaker:
        case Light:
            if (power < 0 || power > 100) {
                return E_COMMAND_VALUE_OUT_OF_RANGE;
            };
            break;
        default:
            return E_COMMAND_DEVICE_FEATURE_MISMATCH;
    };

    this->devices[index].power = power;
    this->is_current = false;
    return S_OK;
};

void SmartHomeState::update_pressed_buttons(int state) {
    unsigned long current_timestamp = millis();

    const int buttons[NUM_BUTTONS] = {
        BUTTON_UP,
        BUTTON_DOWN,
        BUTTON_LEFT,
        BUTTON_RIGHT,
        BUTTON_SELECT,
    };

    for (NUMBER i = 0; i < NUM_BUTTONS; i++) {

        bool stored_as_down = (this->buttons_down_since[i] != 0);
        bool button_is_down = (state & buttons[i]);

        if (button_is_down && !stored_as_down) {
            this->buttons_down_since[i] = current_timestamp;
        } else if (!button_is_down && stored_as_down) {
            this->buttons_down_since[i] = 0;
        };
    };

};

// This function may not be correct when a button is pressed down
// at one of every EXACTLY maxsize(unsigned long) ms
// but this is 1ms every ~50 days and is probably accceptable
bool SmartHomeState::button_down_for(int button, unsigned long time) {
    unsigned long current_timestamp = millis();

    const int buttons[NUM_BUTTONS] = {
        BUTTON_UP,
        BUTTON_DOWN,
        BUTTON_LEFT,
        BUTTON_RIGHT,
        BUTTON_SELECT,
    };

    for (NUMBER i = 0; i < NUM_BUTTONS; i++) {
        //Select the button
        // this could be done with a map
        // but that allocates on the heap
        if (buttons[i] & button) {
            unsigned long stored_timestamp = this->buttons_down_since[i];
            return (stored_timestamp && ((current_timestamp - stored_timestamp) > time));
        };
    };
    return false;

};

// writes over ANYTHING in the eeprom
HRESULT SmartHomeState::write_devices_to_eeprom() {
    unsigned int eeprom_pointer = 0;

    unsigned int eeprom_length = EEPROM.length()-1;


    for (NUMBER i = 0; i < MAX_CAPACITY; i++) {

        if (this->devices_slots_free[i]) {
            continue;
        }

        unsigned char size = sizeof this->devices[i];


        if ((eeprom_pointer + size + 4) > eeprom_length) {
            return E_STATE_EEPROM_FULL;
        };


        // [SIG1, SIG2, SIZE, DEVICE... , TERMIATOR]
        EEPROM.put(eeprom_pointer++, EEPROM_SIG_BYTE_1);
        EEPROM.put(eeprom_pointer++, EEPROM_SIG_BYTE_2);
        EEPROM.put(eeprom_pointer++, size);
        EEPROM.put(eeprom_pointer, this->devices[i]);
        
        eeprom_pointer += size;

        EEPROM.put(eeprom_pointer++, EEPROM_TERMINATING_BYTE);

    };

    return S_OK;
}

NUMBER SmartHomeState::read_devices_from_eeprom() {

    NUMBER devices_read = 0;

    for (unsigned int eeprom_pointer = 0; eeprom_pointer < EEPROM.length(); eeprom_pointer++) {

        if (EEPROM.read(eeprom_pointer) != EEPROM_SIG_BYTE_1) {
            continue;
        };
            if (EEPROM.read(eeprom_pointer+1) != EEPROM_SIG_BYTE_2) {
            continue;
        };

        unsigned char size = EEPROM.read(eeprom_pointer+2);

        // this should remove almost every false positive
        if (EEPROM.read(eeprom_pointer+3+size) != EEPROM_TERMINATING_BYTE) {
            continue;
        };

        Device device;
        EEPROM.get(eeprom_pointer+3, device);

        if (this->add_device(device) == S_OK) {
            devices_read++;
        };
        
        // set the pointer to the terminating byte
        // because the next iteration will add 1 to the pointer
        eeprom_pointer += 3+size;
    };

    return devices_read;
}

// UNSAFE - len must be appropriate
// len (may) include the null terminator
bool is_supported_char(char str[], int len, bool allow_lower) {
    for (int i = 0; i < len; i++) {

        int c = int(str[i]);

        if (c == 0) { // No point checking past null terminator
            return true;
        };

        // Could apply some boolean logic to normalise/simplify
        // but it is humanly understandable this way
        // if NOT (65 < c < 90 OR 96 < c < 123)
        // basically if c is not A-Z or not a-z respectively
        if (allow_lower) {
            if (96 < c && c < 123) {
                continue;
            };
        };

        if ( (c < 65 || c > 90) ) {
            return false;
        };
    };
    return true;
};

// as log functions are often VERY expensive (can be up to 30x more cycles)

// for all circumstances using an approximate guess based on
// the fast log 2 using the leading zeros method would be better
// but considering our integers are (at most) 100
// this cheaper is function to count
// if we only need a few instructions to check 1, 10, 100
// compared to significantly more instructions for a constant time variant


unsigned int fast_int_log_10(unsigned int x) {
    if (x < 10) {
        return 1;
    } else if (x < 100) {
        return 2;
    } else {
        return 3;
    }
}

void fill_char_with_int(char str[], int x, int digits) {

    // we use digits as an array indexer
    // to avoid having to reverse the array
    // we can only do this because we know how
    // many digits we want to display/pad
    unsigned NUMBER digit_index = digits - 1;
    unsigned NUMBER leading_zeros = digits - fast_int_log_10(x);


    for (unsigned NUMBER i = 0; i < digits; i++) {
        unsigned NUMBER remainder = x % 10;
        x = x / 10;
        str[digit_index] = char(remainder) + '0';
        digit_index -=1;
    };


    for (unsigned NUMBER i = 0; i < leading_zeros; i++) {
        str[i] = ' ';
    };
};

// the code on learn didnt work for me (idk why)
// maybe because i have not allocated on the heap (i hope..)
uintptr_t calculate_free_memory() {
    // ref to stack var can be treated
    // as the top of stack pointer
    char sp;

    if ((uintptr_t) __brkval) {
        // BOUNDRY BETWEEN STACK AND HEAP
        return &sp - __brkval;
    } else {
        // start of the heap allocation
        //return &sp - __malloc_heap_start;

        // work out ourselves
        uintptr_t ptr = malloc(1);
        free((void *) ptr);

        return &sp - ptr;;
    };
};

