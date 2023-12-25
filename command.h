#ifndef COMMAND_H
#define COMMAND_H
#include "errors.h"
#include "util.h"
#define CMD_OFFSET 6

enum CommandType: char {
    Add,
    State,
    Power,
    Remove,
    Write,
    NotACommand
};

class Command {
    private:
        Command(char*, CommandType);
        Command(CommandType type); // write command
        static enum CommandType char_to_command_type (char);
        HRESULT execute_add(SmartHomeState*, char[24]);
        HRESULT execute_state(SmartHomeState*, char[24]);
        HRESULT execute_power(SmartHomeState*, char[24]);
        HRESULT execute_remove(SmartHomeState*, char[24]);
        CommandType type;
    public:
        Command(); // for null instantiation to be overwritten by ::create
        char device_id[4];
        static HRESULT create(char[24], Command*);
        enum CommandType get_type();
        void get_device_id(char*);
        HRESULT execute(SmartHomeState*, char[24]);
};

#endif