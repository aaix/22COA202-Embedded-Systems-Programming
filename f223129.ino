#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <Arduino.h>
#include <utility/Adafruit_MCP23017.h>

#include "command.h"
#include "device.h"
#include "errors.h"
#include "util.h"

#define FEATURES "BASIC & UDCHARS, FREERAM, HCI, SCROLL, EEPROM"
#define WHITE '\7'
#define YELLOW '\3'
#define GREEN '\2'
#define PURPLE '\5'

// LCD
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// GLOBAL STATE
SmartHomeState state = SmartHomeState();

// DISPLAY STATE
DisplayFlags current_display_flags = NO_DEVICES;
DisplayMode prev_display_mode = ALL_DEVICES;

// BUTTON STATE
unsigned long input_available_after = 0;

// SCROLLING
unsigned long next_scroll_update;
// this theoretically can still be 16 long but it ensures memory safety
// should we have a non null terminated string copied in ...
char scrolling_text[16+4] = {0}; // 16 chars + blank to show end of scroll
NUMBER scrolling_index = 0;

void flush_serial() {
    while(Serial.available()) {
        Serial.read();
    };
}

void wait_for_sync() {
    while (Serial.read() != 'X') {
        Serial.write('Q');
        delay(1000);
        while(Serial.available()) {
            if (Serial.read() == 'X') {
                return;
            };
        };

    };
};


void setup() {
    Serial.begin(9600);
    lcd.begin(16, 2);
    lcd.clear();
    lcd.setBacklight(PURPLE);
    wait_for_sync();
    flush_serial();

    Serial.println();
    Serial.println(F("Reading EEPROM ... this may take a while."));

    unsigned NUMBER eeprom_devices = state.read_devices_from_eeprom();

    Serial.print(F("Loaded "));
    Serial.print(eeprom_devices);
    Serial.println(F(" Devices"));

    Serial.println(F(FEATURES));
    lcd.setBacklight(WHITE);
    // Stop the LCD displaying gibberish when it is passed a NULL


    // custom characters for the display
    // this doesnt work with progmem so we declare it here
    // so it can go out of scope once we are finished setting up
    const uint8_t UP_ARROW[8] = {4,14,21,4,4,4,4,0};
    const uint8_t DOWN_ARROW[8] = {0,4,4,4,4,21,14,4};
    const uint8_t NULL_CHAR[8] = {0,0,0,0,0,0,0,0};
    const uint8_t DEGREE_CHAR[8] = {7,5,7,0,0,0,0,0};



    lcd.createChar(0, NULL_CHAR);
    lcd.createChar(1, UP_ARROW);
    lcd.createChar(2, DOWN_ARROW);
    lcd.createChar(3, DEGREE_CHAR);

};

void loop() {

    // Read and execute commands FIRST
    // THEN update display (from buttons)
    if (Serial.available() >= MIN_COMMAND_LEN) {

        delay(100); // wait a bit here to ENSURE we have the entire byte sequence
        char command_buffer[24] = {0};
        // The longest valid command is 23, chars
        // so read 23 chars and flush the rest of
        // the buffer to discard the rest (if any)
        Serial.readBytes(command_buffer, 23);
        
        // discard
        flush_serial();


        Command command;

        HRESULT create_hresult = Command::create(command_buffer, &command); 
        
        if (create_hresult != S_OK) {
            Serial.print(F("CREATE ERROR : "));
            Serial.println(create_hresult);
            return;
        };



        HRESULT exec_hresult;

        if (command.get_type() == CommandType::Write) {
            Serial.print(F("Writing ... "));
        }

        exec_hresult = command.execute(&state, command_buffer);

        if (exec_hresult != S_OK) {
            Serial.print(F("EXEC ERROR : "));
            Serial.println(exec_hresult);
            return;
        };

        Serial.println(F("OK"));

    };
    unsigned char button_state = lcd.readButtons();
    state.update_pressed_buttons(button_state);

    // process buttons
    if (!button_presses_disabled()) {
        process_buttons(button_state);
    }

    if (state.display_mode != STUDENT_ID) {
        update_display(button_state);
    };
};

void process_buttons(unsigned char buttons) {

    // dont lock input if the user has unpressed the button
    // this makes it easier if the user is pressing buttons in quick succession
    if (buttons == 0) {
        unlock_buttons();
    };


    if (state.button_down_for(BUTTON_SELECT, 1000)) {
        // this stops the display flashing
        // by only drawing if the display mode has changed
        if (state.display_mode != STUDENT_ID) {
            prev_display_mode = state.display_mode;
            state.display_mode = STUDENT_ID;
            lcd.clear();
            lcd.setBacklight(PURPLE);
            lcd.home();
            lcd.print(F("F223129"));
            lcd.setCursor(0, 1); // next row
            int free_sram = calculate_free_memory();

            lcd.print(F("FREE: "));
            lcd.print(free_sram);
            lcd.print('B');
            return;
        };
        return; // return here to stop other display modifications
        
        // if display mode is student id and button isnt pressed
    } else if (state.display_mode == STUDENT_ID) {
        state.display_mode = prev_display_mode;
        state.is_current = false;
    };

    if (buttons & BUTTON_RIGHT) {
        // ON DEVICES ONLY
        if (state.display_mode == ON_DEVICES) {
            state.display_mode = ALL_DEVICES;
            display_message("ALL DEVICES", WHITE);
        } else {
            state.display_mode = ON_DEVICES;
            display_message("ON DEVICES", GREEN);
        };

        delay(400);
        state.is_current = false;

    } else if (buttons & BUTTON_LEFT) {
        // OFF DEVICES ONLY
        if (state.display_mode == OFF_DEVICES) {
            state.display_mode = ALL_DEVICES;
            display_message("ALL DEVICES", WHITE);
        } else {
           state.display_mode = OFF_DEVICES;
            display_message("OFF DEVICES", YELLOW);
        }
        delay(400);
        state.is_current = false;
    
    };
}

// basically used for making the user experience better
// e.g its harder to accidentally scroll 2 devices
void lock_buttons_for(unsigned long duration) {

    // dont lock if already locked
    if (button_presses_disabled()) {
        return;
    };
    input_available_after = millis() + duration;
};

void unlock_buttons() {
    input_available_after = millis();
}

bool button_presses_disabled() {
    return millis() < input_available_after;
}


void display_message(char message[], unsigned char colour) {
    // set the scrolling text to "" to disable scrolling
    // setting the first char to the null terminator effectively
    // blanks out the string
    scrolling_text[0] = 0;
    lcd.clear();
    lcd.home();
    lcd.print(message);
    lcd.setBacklight(colour);
}

void draw_display(
    char id[4],
    char location[16], // 11 chars max
    DeviceType type,
    bool state,
    int power,
    DisplayFlags flags
) {
    char line1[17] = {0};
    char line2[17] = {0};
    // we dont need to initialise to spaces
    // because null is defined as a blank char

    if (!(flags & AT_TOP)) {
        line1[0] = 1; //UP ARROW
    };
    if (!(flags & AT_BOTTOM)) {
        line2[0] = 2; //DOWN ARROW
    }

    //Device ID
    strncpy(line1+1, id, 3);

    //Device Location
    strncpy(line1+5, location, 11);

    switch (type) {
        case Speaker:
            line2[1] = 'S';
            break;
        case Socket:
            line2[1] = 'O';
            break;
        case Light:
            line2[1] = 'L';
            break;
        case Thermostat:
            line2[1] = 'T';
            break;
        case Camera:
            line2[1] = 'C';
            break;
    };

    if (state) {
        strncpy(line2+3, " ON", 3);
        lcd.setBacklight(GREEN); //GREEN
    } else {
        strncpy(line2+3, "OFF", 3);
        lcd.setBacklight(YELLOW); //YELLOW
    };

    if (flags & DISPLAY_POWER) {
        char power_buf[4] = {0};

        switch (type) {
            case Speaker:
            case Light:
                fill_char_with_int(power_buf, power, 3);
                power_buf[3] = '%';
                break;
            case Thermostat:
                fill_char_with_int(power_buf, power, 2);
                power_buf[2] = 3; // degree
                power_buf[3] = 'C';
                break;
        };
        strncpy(line2+7, power_buf, 4);
    };

  
    //Copy buffers to display

    lcd.home();

    for (NUMBER i = 0; i < 16; i++) {
      lcd.write(line1[i]);
    };

    lcd.setCursor(0, 1);

    for (NUMBER i = 0; i < 16; i++) {
      lcd.write(line2[i]);
    };
    scrolling_index = 0;
    strncpy(scrolling_text, location, 15);
}

void update_display(unsigned char button_state) {

    // if button up and not at top go to the previous device
    if (button_state & BUTTON_UP && !(current_display_flags & AT_TOP) && !button_presses_disabled()) {
        Device device;
        DisplayFlags flags = state.prev_device(&device);
        
        if (flags != NO_DEVICES) {
            current_display_flags = flags;
            draw_display(device.id, device.location, device.type, device.state, device.power, flags);
        };
        lock_buttons_for(150);


    // if button down and not at bottom go to the next device
    } else if (button_state & BUTTON_DOWN && !(current_display_flags & AT_BOTTOM) && !button_presses_disabled()) {
        Device device;
        DisplayFlags flags = state.next_device(&device);
        
        if (flags != NO_DEVICES) {
            current_display_flags = flags;
            draw_display(device.id, device.location, device.type, device.state, device.power, flags);
        };
        lock_buttons_for(150);
    
    // otherwise if we need to redraw the current device
    } else if (!state.is_current) {
        lcd.clear();
        lcd.setBacklight(WHITE);
        Device device;
        DisplayFlags flags = state.current_device(&device);
        
        // if there is a device to draw
        if (flags != NO_DEVICES) {
            current_display_flags = flags;
            draw_display(device.id, device.location, device.type, device.state, device.power, flags);
        }
        // if there are no devices but we are in on only mode
        else if (state.display_mode == ON_DEVICES) {
            display_message("NOTHINGS ON", GREEN);
        }
        // if there are no devices and we are in off only mode
        else if (state.display_mode == OFF_DEVICES) {
            display_message("NOTHINGS OFF", YELLOW);
        };
    } else {
        scroll_display_text();
    }

    //debug code prints device array

    //Serial.print("[");
    //for (int i = 0; i< MAX_CAPACITY; i++) {
    //    Serial.print(state.devices[i].id);
    //    Serial.print(", ");
    //};
    //Serial.print("]\n");
    //for (int i =0; i < (state.current_device_index*5 + 2); i++) {
    //    Serial.print(' ');
    //}
    //Serial.println('^');


    state.is_current = true;
};

void scroll_display_text() {
    if (millis() < next_scroll_update) {
        return;
    };
    dont_scroll_until(500); // 2chars a second

    NUMBER len = strlen(scrolling_text);

    NUMBER scrollable = len - 11+4; // we can fit 11 chars on screen and have 4 blanks at the end
 
    if (len <= 11) {
        return; // no need to scroll if fits
    };

    if (scrolling_index > scrollable) {
        dont_scroll_until(2000); // wait before start again
        scrolling_index = 0;
    };

    // print with the offset as an index
    lcd.setCursor(5, 0);
    for (NUMBER i = 0; i < len; i++) {
        // dont scroll after null terminator
        if (scrolling_text[i] == 0) {
            break;
        };
        lcd.write(scrolling_text[scrolling_index + i]);
    }

    scrolling_index++;
};

void dont_scroll_until(int duration) {
    next_scroll_update = millis() + duration;
}