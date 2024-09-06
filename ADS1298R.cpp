
#include "ADS1298R.h"
#include "Arduino.h"
#include "SPI.h"

#define SPI_BAUD 2000000
#define SPI_BIT_ORDER MSBFIRST
#define SPI_MODE SPI_MODE1

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

void ADS1298R::init() {
    
    // SPI.begin(PIN_SCLK, PIN_DOUT, PIN_DIN);
    SPI.begin(18, 19, 23);

    // Set pinmodes
    pinMode(PIN_PWDN, OUTPUT);
    pinMode(PIN_RST, OUTPUT);
    pinMode(PIN_START, OUTPUT);
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_DRDY, INPUT);

    // ADS1298R Startup Proceedure
    digitalWrite(PIN_PWDN, LOW);
    digitalWrite(PIN_RST, LOW);
    digitalWrite(PIN_START, LOW);
    digitalWrite(PIN_CS, LOW);

    delay(100); // Oscillator Wakeup

    digitalWrite(PIN_PWDN, HIGH);
    digitalWrite(PIN_RST, HIGH);

    delay(1000); // Wait for tPOR and VCAP1

    // Send reset pulse // 1 clk ~= 0.5us
    digitalWrite(PIN_RST, LOW);
    delayMicroseconds(1);
    digitalWrite(PIN_RST, HIGH);

    delayMicroseconds(20); // Wait for reset time (18 clocks ~= 10us)

    // SDATAC
    stopDataContinuous();

    // WREG CONFIG3 0xC0 - Turn on internal reference
    writeRegister(CONFIG3, 0xCC);                            // C0 internal reference, 4C rld stuff, CC for both

    // WREG CONFIG1 0x86
    writeRegister(CONFIG1, 0x86);

    // WREG CONFIG2 0x10
    writeRegister(CONFIG2, 0x10);

    // WREG CHnSET 0x01
    uint8_t channelSettings[] = { 0x05, 0x00, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 };
    writeRegisters(CH1SET, 8, channelSettings);
    
    // WREG RLD_SENSP 0x01
    writeRegister(RLD_SENSP, 0x02);

    // WREG RLD_SENSN 0x01
    writeRegister(RLD_SENSN, 0x02);

    // WREG RESP 0b111 011 10 = 0xEE
    // writeRegister(RESP, 0xEE);

    // WREG CONFIG4 0b000 0 0 0 0 0 // Default, dont actually need to write
    writeRegister(CONFIG4, 0x00);

    // GET ID
    int id = readRegister(0x00); 
    Serial.print("Device has ID: ");
    Serial.println(id);

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




