#include "master.h"

// Initialize communication protocol and serial handling
SmartGreenHouseMCU sghMCU;

// State flasgs
bool acIsEnabled = false;
bool openWindowIsAllowed = true;

String sendCmdAndGetRes(String cmd) {
    sghMCU.send(cmd);
    while (!sghMCU.hasMessage());
    char res[SERIAL_BUFFER_SIZE] = {0};
    sghMCU.receive(res);
    return String(res);
}

// Request the inside and the outside temperature.
void requestTemperatures(float *innerTemperature, float *outerTemperature) {
    String cmd = "";

    // Read inner and outer temperature.
    cmd = String(sghMCU.DHT_SENS) + '|' + String(sghMCU.INNER_TEMP);    // Create command,
    *innerTemperature = sendCmdAndGetRes(cmd).toFloat();                // And send it.
    cmd = String(String(sghMCU.OUTER_TEMP));
    *outerTemperature = sendCmdAndGetRes(cmd).toFloat();
}

// Request the inside / outside luminosity ratio of the greenhouse
void requestLuminosity(float *luminosityRatio) {
    String cmd = "";

    // Read inner and outer temperature.
    cmd = String(String(sghMCU.LUM));                   // Create command,
    *luminosityRatio = sendCmdAndGetRes(cmd).toFloat();   // And send it.
}

// Determine window position by reading the capacitance sensors
void determineWindowPosition(bool *fullyOpen, bool *halfOpen, bool *closed) {
    *fullyOpen = (touchRead(UPPER_END) < TOUCH_THRESHOLD) ? true : false;
    *halfOpen = (touchRead(MIDDLE) < TOUCH_THRESHOLD) ? true : false;
    *closed = (touchRead(LOWER_END) < TOUCH_THRESHOLD) ? true : false;
}

// Control the window shutter.
void controlShutter(uint8_t sensor, uint8_t pin1Val, uint8_t pin2Val) {
    digitalWrite(SHUTTER_MOTOR_PIN1, pin1Val);
    digitalWrite(SHUTTER_MOTOR_PIN2, pin2Val);

    // Wait until the window touches the middle or the upper sensor (whatever is defined)
    while (touchRead(sensor) > TOUCH_THRESHOLD);
    // And stop the shutter
    digitalWrite(SHUTTER_MOTOR_PIN1, LOW);
    digitalWrite(SHUTTER_MOTOR_PIN2, LOW);
}

// Start air-condition
void enableAircondition(bool cooling) {
    // Setup or change mode (cooling or heating).
    if (cooling)
        digitalWrite(AC_COOLING, HIGH);
    else
        digitalWrite(AC_HEATING, HIGH);

    // If it is not already on, then turn it on...
    if (!acIsEnabled) {
        digitalWrite(AC_MOTOR_PIN1, HIGH);
        digitalWrite(AC_MOTOR_PIN2, LOW);
    }
    
    acIsEnabled = true;
}

// Stop the aircondition
void stopAircondition(void) {
    digitalWrite(AC_COOLING, LOW);
    digitalWrite(AC_HEATING, LOW);

    digitalWrite(AC_MOTOR_PIN1, LOW);
    digitalWrite(AC_MOTOR_PIN2, LOW);
    
    acIsEnabled = false;
}

// Enable actuators if needed to handle atmosphere.
void handleAtmosphere(void) {
    // Initialize variables for atmospheric values.
    float innerTemperature = 0, outerTemperature = 0;
    float luminosityRatio = 0;
    // Initialze window positions.
    bool windowIsOpen = false, windowInMiddle = false, windowIsClosed = false;

    // Get temperatures.
    requestTemperatures(&innerTemperature, &outerTemperature);
    // Get luminosity ratio
    requestLuminosity(&luminosityRatio);
    // Determine window position.
    determineWindowPosition(&windowIsOpen, &windowInMiddle, &windowIsClosed);

    // When the temperature is inside this span, handle temperature by opening / closing the window.
    if (LOW_OUT_TEMP_THRESHOLD <= outerTemperature <= HIGH_OUT_TEMP_THRESHOLD) {
        // Start by stoping the AC if it was on from a previous circle.
        if (acIsEnabled)
            stopAircondition();

        // In case where the inner temperature is higher more
        // than 1 degree from the external temperature, open the window.
        if ((innerTemperature > outerTemperature + 1) && !windowIsOpen) {
            controlShutter(UPPER_END, HIGH, LOW);

            // Allow window opening
            openWindowIsAllowed = true;
        }
        // In case where the inner temperature is lower more
        // than 1 degree from the external temperature, close the window.
        else if ((innerTemperature < outerTemperature - 1) && !windowIsClosed)
            controlShutter(LOWER_END, LOW, HIGH);

    // Else handle temperature using the air-condition with the window closed.
    } else {
        if (!windowIsClosed)
            controlShutter(LOWER_END, LOW, HIGH);
        
        // Enable air-condition, and determine cooling or heating.
        enableAircondition((innerTemperature > outerTemperature) ? true : false);
        // Do not allow window opening
        openWindowIsAllowed = false;
    }

    // Re-new the values that represent the window position
    determineWindowPosition(&windowIsOpen, &windowInMiddle, &windowIsClosed);
    
    // When the light intensity is inside this span, re-adjust window position for optimal light intensity
    if ((LOW_LUMINOSITY_THRESHOLD <= luminosityRatio <= HIGH_LUMINOSITY_THRESHOLD) && openWindowIsAllowed) {
        if ((LOW_LUMINOSITY_THRESHOLD <= luminosityRatio < 60) && !windowIsOpen)
            controlShutter(UPPER_END, HIGH, LOW); // Fully open the window
        else if (60 <= luminosityRatio <= HIGH_LUMINOSITY_THRESHOLD) {
            if (!windowInMiddle) {
                // Set the window in the middle position by half-opening it or half-closing it
                if (windowIsClosed)
                    controlShutter(MIDDLE, HIGH, LOW); // Half-open
                else if (windowIsOpen)
                    controlShutter(MIDDLE, LOW, HIGH); // Half-close
            }
        }
    } else {
        if (luminosityRatio < LOW_LUMINOSITY_THRESHOLD)
            digitalWrite(ARTIFICIAL_LIGHTING, HIGH);
        else if (luminosityRatio > HIGH_LUMINOSITY_THRESHOLD)
            digitalWrite(ARTIFICIAL_LIGHTING, LOW);
    }
}

void setup() {
    // Setup hardware and bluetooth serial
    sghMCU.setupHardwareSerial();
    sghMCU.setupBTSerial();

    // Set sleep timer
    // esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

    delay(1000);
    Serial.println("Master MCU is ready!");
}

void loop() {
    handleAtmosphere();

    Serial.println("Going to sleep now");
    delay(1000);
    // esp_light_sleep_start();
}