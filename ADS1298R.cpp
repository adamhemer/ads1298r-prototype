
#include "ADS1298R.h"
#include "Arduino.h"
#include "SPI.h"

#include <esp_adc_cal.h>
#include <esp32-hal-adc.h>

#define SPI_BAUD 2000000
#define SPI_BIT_ORDER MSBFIRST
#define SPI_MODE SPI_MODE1

#define DEVICE_ID   210

extern SPIClass SPI;

ADS1298R::ADS1298R(int baud, int bitOrder, int mode) {

    busSettings = SPISettings(baud, bitOrder, mode);

};

void ADS1298R::readDataContinuous() {
    SPI.beginTransaction(busSettings);
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(RDATAC);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    delayMicroseconds(1000); // Wait for device to change mode
};

void ADS1298R::stopDataContinuous() {
    SPI.beginTransaction(busSettings);
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(SDATAC);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    delayMicroseconds(1000); // Wait for device to change mode
};

void ADS1298R::initLoop() {
    int id;
    int attempts = 0;
    const int maxAttempts = 4;
    do {
        Serial.println("Starting SPI.");
        SPI.begin(PIN_SCLK, PIN_DOUT, PIN_DIN); // This should work the same as below??
        //SPI.begin(18, 19, 23);

        // ADS1298R Startup Proceedure
        digitalWrite(PIN_PWDN, LOW);
        digitalWrite(PIN_RST, LOW);
        digitalWrite(PIN_START, LOW);
        digitalWrite(PIN_CS, LOW);

        Serial.println("Oscillator wakeup.");
        delay(200); // Oscillator Wakeup

        digitalWrite(PIN_PWDN, HIGH);
        digitalWrite(PIN_RST, HIGH);

        Serial.println("VCAP1 Settling.");
        delay(500); // Wait for tPOR and VCAP1

        double vcap_voltage = 0;
        while (vcap_voltage < 1.1) {
            vcap_voltage = ((double)adc1_get_raw(ADC1_CHANNEL_7) / 4096.0) * 3.3;
            Serial.print("VCAP1 voltage = ");
            Serial.println(vcap_voltage);
            delay(100);
        }
        Serial.println("VCAP1 voltage pass.");

        // Send reset pulse // 1 clk ~= 0.5us
        digitalWrite(PIN_RST, LOW);
        delayMicroseconds(2);
        digitalWrite(PIN_RST, HIGH);

        delayMicroseconds(50); // Wait for reset time (18 clocks ~= 10us)               // Maybe increase this time even more??

        // SDATAC
        stopDataContinuous();

        // WREG CONFIG3 0xC0 - Turn on internal reference  0b11001100
        delay(10);
        writeRegister(CONFIG3, 0xCC);//0x0b11001100);                            // C0 internal reference, 4C rld stuff, CC for both

        // WREG CONFIG1 0x86
        delay(10);
        writeRegister(CONFIG1, 0x86); // 0x86 = 500 in HP
        //writeRegister(CONFIG1, 0x05); // 500 in LP

        // WREG CONFIG2 0x10
        delay(10);
        writeRegister(CONFIG2, 0x10);

        // CHnSET
        // Channel Gain
        // 0x0_ = 6
        // 0x1_ = 1
        // 0x2_ = 2
        // 0x3_ = 3
        // 0x4_ = 4
        // 0x5_ = 8
        // 0x6_ = 12

        // WREG CHnSET 0x00 - Test Signal 0x_5, Shorted 0x_1, Normal 0x_0.
        // uint8_t channelSettings[] = { 0x00, 0x00, 0b00000010, 0x05, 0x05, 0x05, 0x05, 0x05 };
        // uint8_t channelSettings[] = { 0x05, 0x00, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 }; // ECG on Channel 2, Others test signal.
        uint8_t channelSettings[] = { 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 }; // All channels test signal.
        delay(10);
        writeRegisters(CH1SET, 8, channelSettings);

        // WREG RESP 0b11 1 100 10 0xF2
        //writeRegister(RESP, 0xF2);
        
        // WREG RLD_SENSP 0x02 - channel 2
        delay(10);
        writeRegister(RLD_SENSP, 0x02);

        // WREG RLD_SENSN 0x02 - channel 2
        delay(10);
        writeRegister(RLD_SENSN, 0x02);

        // GET ID
        id = readRegister(0x00);
        Serial.print("Device has ID: ");
        Serial.println(id);

        if (id != DEVICE_ID) {
            SPI.end();

            attempts++;
            if (attempts >= maxAttempts) {
                Serial.println("Device boot failed, max attempts exceeded!");
                ESP.restart();
            } else {
                Serial.println("Device boot failed, retrying startup proceedure...");
            }

            // Serial.println("Device boot failed, retrying startup proceedure...");
            SPI.end();
            delay(1000);
        }
    } while (id != DEVICE_ID);

    delay(1);

    // RDATAC
    readDataContinuous();

    delay(100);

    // Start conversion
    digitalWrite(PIN_START, HIGH);

    delay(10);
}

void ADS1298R::init() {
    
    SPI.begin(PIN_SCLK, PIN_DOUT, PIN_DIN);
    //SPI.begin(18, 19, 23);

    // Set pinmodes
    pinMode(PIN_PWDN, OUTPUT);
    pinMode(PIN_RST, OUTPUT);
    pinMode(PIN_START, OUTPUT);
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_DRDY, INPUT);

    // Setup ADS1298R and loop if the ID is read incorrectly.
    this->initLoop();

    delay(1);

    // RDATAC
    readDataContinuous();

    delay(100);

    // Start conversion
    digitalWrite(PIN_START, HIGH);

    delay(10);

};

/*
    writeRegister
        uint8_t reg         register address to write to
        uint8_t value       data to write
*/

void ADS1298R::writeRegister(uint8_t reg, uint8_t value) {

    SPI.beginTransaction(busSettings);
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(WREG | reg);
    SPI.transfer(0x00);
    SPI.transfer(value);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    delayMicroseconds(10000);
};

/*
    writeRegisters
        uint8_t start_reg   first register to write to
        int num_regs        number of sequential registers to write
        uint8_t value       data to write (as an array of length num_regs)
*/

void ADS1298R::writeRegisters(uint8_t start_reg, int num_regs, uint8_t* value) {

    SPI.beginTransaction(busSettings);
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(WREG | start_reg);
    SPI.transfer(num_regs - 1);

    for (uint8_t i = 0; i < num_regs; i++)
    {
        SPI.transfer(value[i]);
    }
    
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    delayMicroseconds(10000);

};

/*
    readRegister
        uint8_t reg         register address to read from

    return
        uint8_t             byte read from register
*/

uint8_t ADS1298R::readRegister(uint8_t reg) {

    SPI.beginTransaction(SPISettings(SPI_BAUD, SPI_BIT_ORDER, SPI_MODE));
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(RREG | reg);
    SPI.transfer(0x00);
    uint8_t readData = SPI.transfer(0x00);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    return readData;

};

/*
    readRegisters
        uint8_t start_reg   first register to read from
        int num_regs        number of sequential registers to read

    return
        uint8_t*            array of bytes read from registers
*/


uint8_t* ADS1298R::readRegisters(uint8_t start_reg, int num_regs) {

    SPI.beginTransaction(busSettings);
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(RREG | start_reg);
    SPI.transfer(num_regs - 1);

    uint8_t data[num_regs];
    for (uint8_t i = 0; i < num_regs; i++) {
        data[i] = SPI.transfer(0x00);
    }
    
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    return data;
};




