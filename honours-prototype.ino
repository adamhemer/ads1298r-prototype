
#include "Arduino.h"
#include "SPI.h"
#include "WiFi.h"
#include "WiFiMulti.h"
#include "esp_wpa2.h" //wpa2 library for connections to Enterprise networks


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

#define SERIAL_BAUD 1000000

#define CHANNELS    8 // For testing with less channels


WiFiMulti WiFiMulti;
WiFiClient client;


#define EAP_ANONYMOUS_IDENTITY "anonymous@flinders.edu.au" //anonymous@example.com, or you can use also nickname@example.com
#define EAP_IDENTITY "heme0012@flinders.edu.au" //nickname@example.com, at some organizations should work nickname only without realm, but it is not recommended
#define EAP_PASSWORD "" //password for eduroam account
#define EAP_USERNAME "heme0012" // the Username is the same as the Identity in most eduroam networks.
const char* ssid = "eduroam"; // eduroam SSID



void setup()
{

    // Start serial to PC
    Serial.begin(SERIAL_BAUD);
    Serial.println("Starting sequence...");

    SPI.begin(18, 19, 23);

    // Set pinmodes
    pinMode(PIN_PWDN, OUTPUT);
    pinMode(PIN_RST, OUTPUT);
    pinMode(PIN_START, OUTPUT);
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_DRDY, INPUT);
    // pinMode(PIN_DOUT,   INPUT);
    // pinMode(PIN_DIN,    OUTPUT);
    // pinMode(PIN_SCLK,   OUTPUT);

    // ADS1298R Startup Proceedure
    digitalWrite(PIN_PWDN, LOW);
    digitalWrite(PIN_RST, LOW);
    digitalWrite(PIN_START, LOW);
    digitalWrite(PIN_CS, LOW);
    // digitalWrite(PIN_DIN,   LOW);
    // digitalWrite(PIN_SCLK,  LOW);

    delay(100); // Oscillator Wakeup

    digitalWrite(PIN_PWDN, HIGH);
    digitalWrite(PIN_RST, HIGH);

    delay(1000); // Wait for tPOR and VCAP1

    // Send reset pulse // 1 clk ~= 0.5us
    digitalWrite(PIN_RST, LOW);
    delayMicroseconds(1);
    digitalWrite(PIN_RST, HIGH);

    delayMicroseconds(10); // Wait for reset time (18 clocks)

    delayMicroseconds(10); // Extra time to be safe

    // SDATAC
    SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(SDATAC);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    delayMicroseconds(1000); // Wait for device to change mode

    // WREG CONFIG3 0xC0 - Turn on internal reference
    SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(0b01000011);
    SPI.transfer(0x00);
    SPI.transfer(0xCC); // C0 internal reference, 4C rld stuff, CC for both
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    delayMicroseconds(10000);

    // WREG CONFIG1 0x86
    SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(0b01000001);
    SPI.transfer(0x00);
    SPI.transfer(0x86);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    delayMicroseconds(1000);

    // WREG CONFIG2 0x00
    SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(0b01000010);
    SPI.transfer(0x00);
    SPI.transfer(0x10);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    delayMicroseconds(1000);

    // WREG CHnSET 0x01
    SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(0x45); // CH1SET
    SPI.transfer(0x07); // Write 8 in a row
    for (int i = 0; i < 8; i++)
    {
        if (i == 1) {
            SPI.transfer(0x00); // 0x01 shorts to ref, 0x05 to internal test, 0x00 for normal
        } else {
            SPI.transfer(0x05); // 0x01 shorts to ref, 0x05 to internal test, 0x00 for normal
        }
    }
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    delayMicroseconds(1000);
    
    // WREG RLD_SENSP 0x01
    SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(0x4D);
    SPI.transfer(0x00);
    SPI.transfer(0x02);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    delayMicroseconds(1000);

    // WREG RLD_SENSN 0x01
    SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(0x4E);
    SPI.transfer(0x00);
    SPI.transfer(0x02);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    delayMicroseconds(1000);


    // GET ID
    SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(0x20);
    SPI.transfer(0x00);
    int id = SPI.transfer(0x00);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    Serial.print("Device has ID: ");
    Serial.println(id);

    delay(1);

    // RDATAC
    SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
    digitalWrite(PIN_CS, LOW);
    delayMicroseconds(1);
    SPI.transfer(RDATAC);
    delayMicroseconds(1);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    delay(100);

    digitalWrite(PIN_START, HIGH);

    delay(10);

    // id = 69;
    // SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
    // digitalWrite(PIN_CS, LOW);
    // SPI.transfer(0x20);
    // SPI.transfer(0x00);
    // id = SPI.transfer(0x00);
    // digitalWrite(PIN_CS, HIGH);
    // SPI.endTransaction();
    // Serial.print("Device has ID: ");
    // Serial.println(id);

    // // SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
    // // digitalWrite(PIN_CS, LOW);
    // // delayMicroseconds(1);
    // SPI.transfer(0b00100000);
    // id = SPI.transfer(0b00000000);
    // delayMicroseconds(1);
    // digitalWrite(PIN_CS, HIGH);

    // SPI.endTransaction();

    // Serial.println(id);



    // ======== Wifi setup ========
    pinMode(D9, OUTPUT);
    digitalWrite(D9, LOW);
    
    Serial.begin(115200);
    delay(10);


    // -------- Connect to Wifi ---------
    
    // WiFiMulti.addAP("Telstra9BD817", "97n89kdsht"); // Home
    // WiFiMulti.addAP("Galaxy A33 5GD849", "oilyloki"); // Hotspot
    // // WiFiMulti.addAP("TelstraD7E509", "jfm2xmt7hs"); // Emily's

    // Serial.print("Waiting for WiFi... ");

    // while(WiFiMulti.run() != WL_CONNECTED) {
    //     Serial.print(".");
    //     delay(500);
    // }

    // Serial.println("");
    // Serial.println("WiFi connected");
    // Serial.println("IP address: ");
    // Serial.println(WiFi.localIP());

    // digitalWrite(D9, HIGH);

    // delay(500);

    Serial.print(F("Connecting to network: "));
    Serial.println(ssid);
    WiFi.disconnect(true);  //disconnect from WiFi to set new WiFi connection

    WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD); 


    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(F("."));
    }
    Serial.println("");
    Serial.println(F("WiFi is connected!"));
    Serial.println(F("IP address set: "));
    Serial.println(WiFi.localIP()); 


    // -------- Connect to Server ---------

    // const uint16_t port = 41000;
    const uint16_t port = 4500;
    // const char * host = "192.168.0.101"; // Home
    // const char * host = "192.168.121.65"; // Hotspot
    // const char * host = "192.168.0.45"; // Emily's
    const char * host = "10.30.7.30"; // eduroam

    Serial.print("Connecting to ");
    Serial.println(host);

    while (!client.connect(host, port)) {
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

        // Send RDATA to manually read
        // SPI.transfer(RDATA);


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

            // double voltage = value * 74.056 / 1000000.0;

            // Serial.print(value);
            // Serial.print(", ");

            // Serial.print(arr[i * 3 + 0]);
            // Serial.print(", ");
            // Serial.print(arr[i * 3 + 1]);
            // Serial.print(", ");
            // Serial.print(arr[i * 3 + 2]);
            // Serial.print(", ");
        }

        client.write((const uint8_t *) &SPI_values_read, sizeof(SPI_values_read));
        // Serial.println();
    }
}
