#include "command.h"
#include "errors.h"
#include "util.h"
#include "device.h"
#include <Arduino.h>
#include <string.h>

Command::Command() {};

Command::Command(char id[4], CommandType type) {
    strncpy(this->device_id, id, 4);
    this->type = type;
}

Command::Command(CommandType type) {
    this->type = type;
}


CommandType Command::get_type() {
    return this->type;
};

void Command::get_device_id(char* buf) {
    strncpy(buf, this->device_id, 4);
}


CommandType Command::char_to_command_type(char c) {
    switch (c) {
        case 'A':
            return Add;
        case 'S':
            return State;
        case 'P':
            return Power;
        case 'R':
            return Remove;
        default:
            return NotACommand;
    };
    
}

HRESULT Command::create(char str[24], Command* command) {

    if (strcmp(str, "WRITE") == 0) {
        *command = Command(CommandType::Write);
        return S_OK;
    };

  
    if (strlen(str) < MIN_COMMAND_LEN) {
        return E_COMMAND_GENERAL_INVALID;
    };

    // REMOVE COMMAND MAY NOT HAVE A SECOND -
    if (str[1] != '-' || (str[5] != '-' && str[5] != 0)) {
        return E_COMMAND_FORMAT_INVALID;
    };


    CommandType type = Command::char_to_command_type(str[0]);
    if (type == CommandType::NotACommand) {
        return E_COMMAND_NOT_A_COMMAND;
    };

    // Copy 3 bytes after an offset of 2
    // initialise device_id as 4 zeros for null-termination
    char device_id[4] = {0};
    strncpy(device_id, str + 2, 3);



    if (!is_supported_char(device_id, 4, false)) {
        return E_COMMAND_UNSUPPORTED_CHARS;
    }

    *command = Command(
        device_id,
        type
    );

    return S_OK;
}


HRESULT Command::execute(SmartHomeState* state, char command_buffer[24]) {
    switch (this->type) {
        case Add:
            return this->execute_add(state, command_buffer);
        case State:
            return this->execute_state(state, command_buffer);

        case Power:
            return this->execute_power(state, command_buffer);

        case Remove:
            return this->execute_remove(state, command_buffer);
        
        case Write:
            return state->write_devices_to_eeprom();

        default:
            return E_COMMAND_NOT_A_COMMAND;
    }
};

HRESULT Command::execute_add(SmartHomeState* state, char command_buffer[24]) {
    DeviceType type = char_to_device_type(command_buffer[0 + CMD_OFFSET]);

    if (type == NotADevice) {
        return E_COMMAND_UNKNOWN_DEVICE_TYPE;
    };

    char location[16]= {0};

    if (command_buffer[CMD_OFFSET + 1] != '-') {
        return E_COMMAND_FORMAT_INVALID;
    };

    memcpy(location, command_buffer + CMD_OFFSET + 2, 15);

    if (strlen(location) < 1) {
        return E_COMMAND_VALUE_OUT_OF_RANGE;
    };

    if (!is_supported_char(location, 15, true)) {
        return E_COMMAND_UNSUPPORTED_CHARS;
    };

    char power;
    switch (type) {
        case Thermostat:
            power = 22;
            break;
        default:
            power = 100;
            break;
    };

    Device device = Device {
        {0},
        type,
        {0},
        false,
        power,
    };
    memcpy(device.id, this->device_id, 3);
    memcpy(device.location, location, 15);
    HRESULT hresult = state->add_device(device);
    if (hresult == E_STATE_CONFLICTING_DEVICE) {
        return state->overwrite_device(device);
    } else {
        return hresult;
    };
};

HRESULT Command::execute_state(SmartHomeState* state, char command_buffer[24]) {
    char state_buf[4] = {0};
    memcpy(state_buf, command_buffer + CMD_OFFSET, 3);

    bool device_state;
    if (strcmp(state_buf, "OFF") == 0) {
        device_state = false;
    } else if (strcmp(state_buf, "ON") == 0) {
        device_state = true;
        } else {
        return E_COMMAND_UNKNOWN_STATE;
    };

    return state->set_device_state(this->device_id, device_state);
};

HRESULT Command::execute_power(SmartHomeState* state, char command_buffer[24]) {
    char power_buf[4] = {0};
    memcpy(power_buf, command_buffer + CMD_OFFSET, 3);

    int power = atoi(power_buf);

    return state->set_device_power(this->device_id, power);
};

HRESULT Command::execute_remove(SmartHomeState* state, char command_buffer[24]) {
    return state->remove_device(this->device_id);
};
