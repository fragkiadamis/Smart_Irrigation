#include "smart_greenhouse_mcu.h"

/*
 Determine The serial that is going to be used
 as the bluetooth serial according to the type
 of the MCU.
*/
#if defined(ESP_PLATFORM)
    #define BTSerial Serial2 // Set BTSerial as Serial2 alias
#elif defined(__AVR_ATmega328P__)
    #include <SoftwareSerial.h>
    SoftwareSerial BTSerial(BT_RX, BT_TX); // Set software serial as BTSerial
#endif

void SmartGreenHouseMCU::setupHardwareSerial(void) {
    // Setup hardware serial
    Serial.begin(BAUD_RATE);
    while(!Serial);
    Serial.flush();
}

void SmartGreenHouseMCU::setupBTSerial(void) {
    // Setup bluetooth serial, according to MCU type
    #if defined(ESP_PLATFORM)
        BTSerial.begin(38400, SERIAL_8N1, 16, 17);
    #elif defined(__AVR_ATmega328P__)
        BTSerial.begin(BT_BAUD_RATE);
    #endif
    while(!BTSerial);
    BTSerial.flush();
}

bool SmartGreenHouseMCU::hasMessage(void) {
    return BTSerial.available();
}
    
void SmartGreenHouseMCU::receive(char *buffer) {
    uint8_t size = 0;
    unsigned long time = micros();

    while(micros() - time <= SERIAL_TIMEOUT) {
        if (BTSerial.available()) {
        char c = BTSerial.read();
        
        // If the buffer is almost full or end of line, stop
        if(size == (SERIAL_BUFFER_SIZE - 1) || c == '\n')
            break;
            
            buffer[size++] = c;
            time = micros(); // Every time a new character arrives update the timeout
        }
    }
    buffer[size] = '\0';
}

void SmartGreenHouseMCU::send(float value) {
    BTSerial.println(value);
}

void SmartGreenHouseMCU::mcu_sleep(void) {
    // sleep_enable(); // Enabling sleep mode
    // set_sleep_mode(SLEEP_MODE_ADC); // Setting the sleep mode.
    // sleep_cpu(); // Activating sleep mode
    // sleep_disable();
}
