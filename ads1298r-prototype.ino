
#include "Arduino.h"
#include "SPI.h"
#include "WiFi.h"
#include "WiFiMulti.h"
#include "esp_wpa2.h" //wpa2 library for connections to Enterprise networks
#include "env.h" // WiFi credentials - not version controlled
#include "ADS1298R.h"

#define PIN_PWDN D2
#define PIN_RST D3
#define PIN_START D5
#define PIN_CS 14
#define PIN_DRDY D7
#define PIN_DOUT MISO
#define PIN_DIN MOSI
#define PIN_SCLK SCK

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

#define CHANNELS    8 // For testing with less channels

// WiFiMulti WiFiMulti;
WiFiClient client;

// #define EAP_ANONYMOUS_IDENTITY "anonymous@flinders.edu.au" //anonymous@example.com, or you can use also nickname@example.com
// #define EAP_IDENTITY "heme0012@flinders.edu.au" //nickname@example.com, at some organizations should work nickname only without realm, but it is not recommended
// #define EAP_PASSWORD "" //password for eduroam account
// #define EAP_USERNAME "heme0012" // the Username is the same as the Identity in most eduroam networks.
// const char* ssid = "eduroam"; // eduroam SSID

ADS1298R ads1298r(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE);

void setup()
{

    // ======== Serial setup ========
    Serial.begin(SERIAL_BAUD);
    delay(3000);
    Serial.println("Starting sequence...");


    // ======== ADS1298R setup ========
    ads1298r.init();


    // ======== Wifi setup ========
    pinMode(D9, OUTPUT);
    digitalWrite(D9, LOW);
    delay(10);


    // ======== Find Networks ========

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    WiFiCredentials bestNetwork;

    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0) {
        Serial.println("no networks found");
    } else {
        Serial.print(n);
        Serial.println(" networks found");
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

            for (int j = 0; j < numNetworks; j++) {
                if (WiFi.SSID(i) == allNetworks[j].ssid) {
                    bestNetwork = allNetworks[j];
                    Serial.print(" - credentials known");
                }
            }
            Serial.println();
        }
    }
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
        delay(500);
        Serial.print(F("."));   // Loading bar "...." in serial monitor while connecting
    }
    Serial.println("");
    Serial.println(F("WiFi is connected!"));
    Serial.println(F("IP address set: "));
    Serial.println(WiFi.localIP()); 
    digitalWrite(D9, HIGH);                 // Visual indication that wifi is connected


    // -------- Connect to Server ---------
    Serial.print("Connecting to ");
    Serial.println(bestNetwork.host);

    while (!client.connect(bestNetwork.host, bestNetwork.port)) {
        Serial.println("Connection failed.");
        Serial.println("Waiting 2 seconds before retrying...");
        delay(2000);
        // return;
    }
}

void loop()
{
    // delay(1);
    if (!digitalRead(PIN_DRDY))
    {
        // Store incoming values
        int SPI_bytes_read[24] = {0};
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
        // Serial.println();
    }
}
