
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
#define BAT_MIN         3.2         // Cutoff voltage
#define BAT_UVD_FREQ    500 * 60    // Test frequency 1 min (500Hz * 60s)
#define BAT_M           1.2788      // Slope (m) of transform
#define BAT_C           -1.3179     // Intercept (c) of transform


WiFiClient client;

ADS1298R ads1298r(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE);

WiFiCredentials bestNetwork;

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
    Serial.println("Starting sequence...");


    // ======== ADS1298R setup ========
    ads1298r.init();


    // ======== Wifi setup ========
    pinMode(LED1, OUTPUT);
    digitalWrite(LED1, LOW);
    delay(10);


    // ======== Find Networks ========
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int n = WiFi.scanNetworks();
    if (n < 0) {
        Serial.println("No netowrks detected");
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
        digitalWrite(LED1, LOW);
        delay(250);
        digitalWrite(LED1, HIGH);

    }
    Serial.println("");
    Serial.println(F("WiFi is connected!"));
    Serial.println(F("IP address set: "));
    Serial.println(WiFi.localIP()); 
    digitalWrite(LED1, HIGH);                   // Visual indication that wifi is connected


    // -------- Connect to Server ---------
    Serial.print("Connecting to ");
    Serial.println(bestNetwork.host);

    while (!client.connect(bestNetwork.host, bestNetwork.port)) {
        Serial.println("Connection failed.");
        Serial.println("Waiting 2 seconds before retrying...");
        delay(2000);
    }
}

int batteryCounter = 0;

// const int packetSize = 4;
// int packetAccum = 0;

// uint32_t sendBuffer[8 * packetSize] = {0};

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
        if (BAT_UVD) {
            batteryCounter++;
            if (batteryCounter > BAT_UVD_FREQ) {
                batteryCounter = 0;

                double voltage = BAT_M * getBatteryVoltage() + BAT_C;   // Get battery voltage and apply transform
                if (voltage < BAT_MIN) {
                    Serial.println("Low battery! Shutting down...");
                    delay(2000);
                    digitalWrite(REG_EN, LOW);  // Pull regulator enable pin low, disabling VCC
                };
            }
        }
    }

    // Auto reconnect
    if (!client.connected()) {
        Serial.println("Server connection lost, reconnecting...");
        while (!client.connect(bestNetwork.host, bestNetwork.port)) {
            Serial.println("Connection failed.");
            Serial.println("Waiting 2 seconds before retrying...");
            delay(2000);
        }
    }






    // digitalWrite(READ_BAT, HIGH);
    // delayMicroseconds(100);




    // digitalWrite(READ_BAT, LOW);

    // float mv = analogRead(BAT_ADC);// / 4096.0) * (37.0 / 27.0) * 3.3; // 0 to 4096 for 3.3V, adjust to voltage divider ratio. 
    // Serial.print(mv);
    // Serial.print(', ');
    // float vcap = analogRead(VCAP1);// / 4096.0) * 3.3; // 0 to 4096 for 3.3V
    // Serial.println(vcap);
}


double getBatteryVoltage() {
    digitalWrite(CHARGE_EN, HIGH); // Disable charging
    digitalWrite(READ_BAT, HIGH); // Enable resistor divider

    delayMicroseconds(100);
    // delay(10);
    double voltage = ((double)analogRead(BAT_ADC) / 4096.0) * (37.0 / 27.0) * 3.3; // 0 to 4096 for 3.3V, adjust to voltage divider ratio. 
    // Serial.println(mv);
    // Serial1.println(mv);
    // delay(10);

    digitalWrite(READ_BAT, LOW); // Disable resistor divider
    digitalWrite(CHARGE_EN, LOW); // Enable charging

    return voltage;
}