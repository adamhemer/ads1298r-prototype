
#include "Arduino.h"
#include "SPI.h"
#include "WiFi.h"
#include "esp_wpa2.h"   // wpa2 library for connections to Enterprise networks
#include "env.h"        // WiFi credentials - not version controlled
#include "ADS1298R.h"

#include <esp_adc_cal.h>    // Libraries for ESP ADC
#include <esp32-hal-adc.h>

#define PIN_PWDN    9  // D2
#define PIN_RST     10 // D3
#define PIN_START   12 // D5
#define PIN_CS      13 // 14
#define PIN_DRDY    21 // D7
#define PIN_DOUT    11 // MISO
#define PIN_DIN     47 // MOSI
#define PIN_SCLK    14 // SCK

// System Commands
#define SPI_WAKEUP 0x02
#define SPI_STANDBY 0x04
#define SPI_RESET 0x06
#define SPI_START 0x08
#define SPI_STOP 0x0A

// Data Read Commands
#define RDATAC 0x10
#define SDATAC 0x11
#define RDATA 0x12

// Register Read Commands

// SPI Settings
#define SPI_BAUD 2000000
#define SPI_BIT_ORDER MSBFIRST
#define SPI_MODE SPI_MODE1

// Minimum SPI clock = tSCLK < (tDR – 4tCLK) / (NBITS × NCHANNELS + 24) = (1/500 - 4 x 50x10^-9) / (8 x 24 + 24) = 108108 Hz
// Thus the SPI clock must be at minimum 110kHz to retrieve all the data between DRDY pulses

#define SERIAL_BAUD 115200

#define CHANNELS    8   // Do not modify - For testing with less channels only

#define ENABLE1     1
#define ENABLE2     2
#define REG_EN      38
#define READ_BAT    39
#define BAT_ADC     15 // 40 cant do adc, bridged to 15 with jumper wire
#define CHARGE_EN   41
#define TERM_OFF    42

#define LED1        4
#define LED2        5
#define BUTTON      6

#define VCAP1       8

// Battery
#define BAT_UVD         true        // Under-voltage Detection enable
#define BAT_LOW         3.4         // Cutoff voltage
#define BAT_UVD_FREQ    500 * 60    // Test frequency 1 min (500Hz * 60s)
#define BAT_M           1.2788      // Slope (m) of transform
#define BAT_C           -1.3179     // Intercept (c) of transform

// Calculation Values
#define BAT_MAX         4.2
#define BAT_MIN         3.2
#define BAT_DURATION    5.4 * 60    // 5.4 Hours


WiFiClient client;

// CHnSET : Test Signal 0x_5, Shorted 0x_1, Normal 0x_0.
// { 0x00, 0x00, 0b00000010, 0x05, 0x05, 0x05, 0x05, 0x05 }; // Respiration testing
// { 0x05, 0x00, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 }; // ECG on Channel 2, Others test signal.
// { 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 }; // All channels test signal.

uint8_t channelConfig[] = { 0x05, 0x00, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 }; // See datasheet for details on CHnSET

ADS1298R ads1298r(channelConfig, SPI_BAUD, SPI_BIT_ORDER, SPI_MODE);

WiFiCredentials bestNetwork;

long long batteryEmptyTime = LONG_LONG_MAX;

void setup()
{
    // Battery charging pins
    pinMode(ENABLE1, OUTPUT);
    pinMode(ENABLE2, OUTPUT);
    pinMode(REG_EN, OUTPUT);
    pinMode(READ_BAT, OUTPUT);
    pinMode(CHARGE_EN, OUTPUT);
    pinMode(TERM_OFF, OUTPUT);

    pinMode(BAT_ADC, INPUT);
    
    // Pin to measure VCAP1 on ADS1298R
    pinMode(VCAP1, INPUT);
    analogReadResolution(12);
    adcAttachPin(VCAP1);

    // User IO
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(BUTTON, INPUT);

    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);

    // Default values
    digitalWrite(ENABLE1, HIGH);    // Fast charging enabled (500mA)
    digitalWrite(ENABLE2, LOW);
    digitalWrite(REG_EN, HIGH);     // Regulator enable, pull low to turn off device
    digitalWrite(READ_BAT, LOW);    // Read bat, set high then read from BAT_ADC
    digitalWrite(CHARGE_EN, LOW);   // Charging enabled
    digitalWrite(TERM_OFF, LOW);    // Termination timers enabled


    // ======== Serial setup ========
    Serial.begin(SERIAL_BAUD);
    delay(3000);
    Serial.println("+-------------------------------------+");
    Serial.println("|        Music From Biosignals        |");
    Serial.println("+-------------------------------------+");
    Serial.println("|      On-Body Collection Device      |");
    Serial.println("|             Adam Hemer              |");
    Serial.println("|         Flinders University         |");
    Serial.println("+-------------------------------------+");
    Serial.println();
    
    // ======== Battery Check ========
    Serial.println("Getting battery voltage...");
    double voltage = getBatteryVoltageCorrected();      // Get battery voltage
    Serial.print("Battery Voltage: ");
    Serial.println(voltage);

    if (voltage < BAT_LOW) {
        Serial.print("Battery voltage too low! Shutting down...");
        delay(1000);
        digitalWrite(REG_EN, LOW);                      // Shutdown voltage regulator
    }

    int batteryTime = getBatteryTimeRemaining();        // Minutes till battery predicted empty
    long long remainingMS = batteryTime * 60 * 1000;    // Milliseconds till battery predicted empty
    batteryEmptyTime = millis() + remainingMS;          // Add to current time to find empty time

    Serial.print("Estimated Time Remaining: ");
    Serial.print(batteryTime);
    Serial.print(" m, (");
    Serial.print(batteryTime/60.0);
    Serial.println(" h)");


    // ======== ADS1298R setup ========
    Serial.println("Starting ADS1298R boot sequence...");

    digitalWrite(LED1, HIGH);
    ads1298r.init();
    digitalWrite(LED1, LOW);


    // ======== Wifi setup ========
    digitalWrite(LED2, LOW);
    delay(10);


    // ======== Find Networks ========
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int n = WiFi.scanNetworks();
    if (n < 0) {
        Serial.println("No networks detected");
    } else {
        Serial.print(n);
        Serial.println(" networks found!");
        // Iterate from smallest to largest signal strength
        for (int i = n - 1; i >= 0; i--) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.print((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
            delay(10);

            // Check if network is in the list of known networks
            for (int j = 0; j < numNetworks; j++) {
                if (WiFi.SSID(i) == allNetworks[j].ssid) {
                    bestNetwork = allNetworks[j];           // Update best network
                    Serial.print(" - credentials known");
                }
            }
            Serial.println();
        }
    }
    // Networks iterated weakest to strongest so bestNetwork will be the highest strength, known network.
    Serial.println("");
    Serial.print("Best network with known credentials is ");
    Serial.println(bestNetwork.ssid);


    // ======== Connect to Wifi ========
    
    WiFi.disconnect(true);  //disconnect from WiFi to set new WiFi connection
    
    if (bestNetwork.enterprise) {
        Serial.println("Connecting to enterprise network: ");
        WiFi.begin(bestNetwork.ssid, WPA2_AUTH_PEAP, bestNetwork.identity, bestNetwork.username, bestNetwork.password); 
    } else {
        Serial.print(F("Connecting to network: "));
        Serial.println(bestNetwork.ssid);
        WiFi.begin(bestNetwork.ssid, bestNetwork.password);
    }


    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(F("."));                   // Loading bar "...." in serial monitor while connecting
        delay(250);
        digitalWrite(LED2, LOW);
        delay(250);
        digitalWrite(LED2, HIGH);

    }

    // Triple flash to indicate connected
    for (int i = 0; i < 3; i++) {
        delay(100);
        digitalWrite(LED2, LOW);
        delay(100);
        digitalWrite(LED2, HIGH);
    }
    

    Serial.println("");
    Serial.println(F("WiFi is connected!"));
    Serial.println(F("IP address set: "));
    Serial.println(WiFi.localIP()); 
    digitalWrite(LED2, HIGH);                   // Visual indication that wifi is connected


    // -------- Connect to Server ---------
    Serial.print("Connecting to ");
    Serial.println(bestNetwork.host);

    while (!client.connect(bestNetwork.host, bestNetwork.port)) {
        Serial.println("Connection failed.");
        Serial.println("Waiting 2 seconds before retrying...");
        digitalWrite(LED1, LOW);
        delay(2000);
        digitalWrite(LED1, HIGH);
    }

    // Triple flash to indicate connected
    for (int i = 0; i < 3; i++) {
        delay(100);
        digitalWrite(LED1, LOW);
        delay(100);
        digitalWrite(LED1, HIGH);
    }

    // -------- Send Device Details ---------
    sendClientHeader();
}

void loop()
{
    if (!digitalRead(PIN_DRDY))
    {
        // Store incoming values
        uint8_t SPI_bytes_read[24] = {0};
        uint32_t SPI_values_read[8] = {0};

        // Start transaction
        SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
        digitalWrite(PIN_CS, LOW);
        delayMicroseconds(1);

        // Read status bytes
        int status0 = SPI.transfer(0);
        int status1 = SPI.transfer(0);
        int status2 = SPI.transfer(0);

        // Read channel 1 - 4
        for (int i = 0; i < 3 * CHANNELS; i++)
        {
            SPI_bytes_read[i] = SPI.transfer(0);
        }

        // End transaction
        digitalWrite(PIN_CS, HIGH);
        SPI.endTransaction();

        for (int i = 0; i < CHANNELS; i++) {
            // Calculate sign extension
            int32_t extension = SPI_bytes_read[i * 3 + 0] & 0x80 ? 0xFF : 0x00;

            // Calculate value
            SPI_values_read[i] = (int32_t)SPI_bytes_read[i * 3 + 2] | ((int32_t)SPI_bytes_read[i * 3 + 1] << 8) | ((int32_t)SPI_bytes_read[i * 3 + 0] << 16) | ((int32_t)extension << 24);
        }

        client.write((const uint8_t *) &SPI_values_read, sizeof(SPI_values_read));

        // Low voltage shutdown check
        if (BAT_UVD && millis() > batteryEmptyTime) {   // May need to check less often, long long > long long might be expensive computationally
            
            double voltage = getBatteryVoltageCorrected();      // Get battery voltage

            if (voltage < BAT_LOW) {                            // If battery voltage too low
                Serial.print("Battery voltage too low! Shutting down...");
                digitalWrite(REG_EN, LOW);                      // Shutdown voltage regulator
            } else {    // If not
                        // Recalculate battery time
                int batteryTime = getBatteryTimeRemaining();        // Minutes till battery predicted empty
                long long remainingMS = batteryTime * 60 * 1000;    // Milliseconds till battery predicted empty
                batteryEmptyTime = millis() + remainingMS;          // Add to current time to find empty time

                Serial.print("Estimated Time Remaining: ");
                Serial.print(batteryTime);
                Serial.print(" m, (");
                Serial.print(batteryTime/60.0);
                Serial.println(" h)");
            }
        }
    }

    // Auto reconnect
    if (!client.connected()) {
        Serial.println("Server connection lost, reconnecting...");
        while (!client.connect(bestNetwork.host, bestNetwork.port)) {
            Serial.println("Connection failed.");
            Serial.println("Waiting 2 seconds before retrying...");
            digitalWrite(LED1, LOW);
            delay(2000);
            digitalWrite(LED1, HIGH);
        }

        // Triple flash to indicate connected
        for (int i = 0; i < 3; i++) {
            delay(100);
            digitalWrite(LED1, LOW);
            delay(100);
            digitalWrite(LED1, HIGH);
        }

        sendClientHeader();
    }
}

// Get battery voltage from ADC and apply voltage divider (10K - 27K) and power rail (3.3)
double getBatteryVoltage() {
    digitalWrite(CHARGE_EN, HIGH); // Disable charging
    digitalWrite(READ_BAT, HIGH); // Enable resistor divider

    delayMicroseconds(100);
    double voltage = ((double)analogRead(BAT_ADC) / 4096.0) * (37.0 / 27.0) * 3.3; // 0 to 4096 for 3.3V, adjust to voltage divider ratio. 

    digitalWrite(READ_BAT, LOW); // Disable resistor divider
    digitalWrite(CHARGE_EN, LOW); // Enable charging

    return voltage;
}

// Get battery voltage and correct based on linear transform
double getBatteryVoltageCorrected() {
    return BAT_M * getBatteryVoltage() + BAT_C;
}

// Very rough approximation of battery time remaining from a linearised model of data collected.
int getBatteryTimeRemaining() {
    double norm = (getBatteryVoltageCorrected() - BAT_MIN) / (BAT_MAX - BAT_MIN);    // Normalise battery voltage
    double minsRemaining = norm * BAT_DURATION;                 // Apply linear approximation

    return (int)minsRemaining;
}

// Header data sent to matlab with battery details and "STRT" command
void sendClientHeader() {
    Serial.println("Getting battery voltage...");
    double voltage = getBatteryVoltageCorrected();   // Get battery voltage and apply transform
    Serial.print("Battery Voltage: ");
    Serial.println(voltage);

    uint32_t voltageInt = (uint32_t)(voltage * 1000000.0);
    Serial.print("Battery Voltage (x1e6): ");
    Serial.println(voltageInt);
    uint8_t voltBuf[4];
    voltBuf[0] = voltageInt >> 24;
    voltBuf[1] = voltageInt >> 16;
    voltBuf[2] = voltageInt >>  8;
    voltBuf[3] = voltageInt;
    client.write((const uint8_t *) &voltBuf, sizeof(voltBuf));

    int mins = getBatteryTimeRemaining();
    Serial.print("Estimated Time Remaining: ");
    Serial.print(mins);
    Serial.print(" m, (");
    Serial.print(mins/60.0);
    Serial.println(" h)");

    long long remainingMS = mins * 60 * 1000;   // Milliseconds till battery predicted empty
    batteryEmptyTime = millis() + remainingMS;     // Add to current time to find empty time
            
    uint8_t minBuf[4] = { 0, 0, mins >> 8, mins % 255 };
    client.write((const uint8_t *) &minBuf, sizeof(minBuf));

    Serial.println();
    Serial.println("Starting data collection.");
    uint8_t startBuf[4] = { 0x53, 0x54, 0x52, 0x54 };           // STRT in hex
    client.write((const uint8_t *) &startBuf, sizeof(startBuf));
}