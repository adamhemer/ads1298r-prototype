
#include "Arduino.h"
#include "SPI.h"

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


#define CHANNELS    4 // For testing with less channels

void setup()
{

    // Start serial to PC
    Serial.begin(115200);
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
    SPI.transfer(0xC0);
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
        SPI.transfer(0x05); // 0x01 shorts to ref, 0x05 to internal test, 0x00 for normal
    }
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
    // SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
    // digitalWrite(PIN_CS, LOW);
    // delayMicroseconds(1);
    // SPI.transfer(RDATAC);
    // delayMicroseconds(1);
    // digitalWrite(PIN_CS, HIGH);
    // SPI.endTransaction();

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
}

void loop()
{
    // delay(1);
    if (!digitalRead(PIN_DRDY))
    {
        // Store incoming values
        int arr[24] = {0};

        // Start transaction
        SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
        digitalWrite(PIN_CS, LOW);
        delayMicroseconds(1);

        // Send RDATA to manually read
        SPI.transfer(RDATA);


        // Read status bytes
        int status0 = SPI.transfer(0);
        int status1 = SPI.transfer(0);
        int status2 = SPI.transfer(0);

        // Read channel 1 - 4
        for (int i = 0; i < 3 * CHANNELS; i++)
        {
            arr[i] = SPI.transfer(0);
        }

        // End transaction
        digitalWrite(PIN_CS, HIGH);
        SPI.endTransaction();

        for (int i = 0; i < CHANNELS; i++) {
            // Calculate sign extension
            int32_t extension = arr[i * 3 + 0] & 0x80 ? 0xFF : 0x00;

            // Calculate value
            int32_t value = (int32_t)arr[i * 3 + 2] | ((int32_t)arr[i * 3 + 1] << 8) | ((int32_t)arr[i * 3 + 0] << 16) | ((int32_t)extension << 24);

            // double voltage = value * 74.056 / 1000000.0;

            Serial.print(value);
            Serial.print(", ");
            // Serial.print(arr[0]);
            // Serial.print(", ");
            // Serial.print(arr[1]);
            // Serial.print(", ");
            // Serial.print(arr[2]);
            // Serial.print(", ");
        }

        Serial.println();
    }
}
