#include <SPI.h>
#include "Arduino.h"
#include "stdint.h"

#define PIN_PWDN    9    //D2
#define PIN_RST     18    //D3
#define PIN_START   12    //D5
#define PIN_CS      13    //14
#define PIN_DRDY    21    //D7
#define PIN_DOUT    47    //MISO
#define PIN_DIN     11    //MOSI
#define PIN_SCLK    14    //SCK

// System Commands
#define SPI_WAKEUP  0x02
#define SPI_STANDBY 0x04
#define SPI_RESET   0x06
#define SPI_START   0x08
#define SPI_STOP    0x0A

// Data Read Commands
#define RDATAC  0x10
#define SDATAC  0x11
#define RDATA   0x12

// Register Commands
#define RREG    0b00100000
#define WREG    0b01000000

/// ---- Register Names ----
// Device Settings (Readonly)
#define ID          0x00

// Global Settings Across Channels
#define CONFIG1     0x01
#define CONFIG2     0x02
#define CONFIG3     0x03
#define LOFF        0x04

// Channel-Specific Settings
#define CH1SET      0x05
#define CH2SET      0x06
#define CH3SET      0x07
#define CH4SET      0x08
#define CH5SET      0x09
#define CH6SET      0x0A
#define CH7SET      0x0B
#define CH8SET      0x0C
#define RLD_SENSP   0x0D
#define RLD_SENSN   0x0E
#define LOFF_SENSN  0x0F
#define LOFF_SENSP  0x10
#define LOFF_FLIP   0x11

// Lead-Off Status Registers (Readonly)
#define LOFF_STATP  0x12
#define LOFF_STATN  0x13

// GPIO and Other Registers
#define GPIO        0x14
#define PACE        0x15
#define RESP        0x16
#define CONFIG4     0x17
#define WCT1        0x18
#define WCT2        0x19


class ADS1298R {
public:
    ADS1298R(const uint8_t* channelSettings, int baud, int bitOrder, int mode); // Constructor, defaults: 2000000, MSBFIRST [=1], SPI_MODE1 [=1]

    void init();                    // Configures the ESP and ADC with pin and config settings, runs the initialsation loop.
    void initLoop();                // Follows the boot sequence as outlined in the ADS1298R datasheet, checks that the ID is read correctly and retries if not.

    void readDataContinuous();      // Enables continuous data collection at the configured SPS speed
    void stopDataContinuous();      // Disables continuous data collection

    void writeRegister(uint8_t reg, uint8_t value);                         // Write a single byte register

    void writeRegisters(uint8_t start_reg, int num_regs, uint8_t* value);   // Write multiple registers

    uint8_t readRegister(uint8_t reg);                                      // Read a single register

    uint8_t* readRegisters(uint8_t start_reg, int num_regs);                // Read multiple registers

    void setChannelConfigs(uint8_t* channel_settings);                      // Set configuration of channels, can use shorthands below or custom array.
    void writeChannelConfigs();                                             // Writes channel config to device. 

    // Shorthand for channel settings
    const static uint8_t CHSET_ALL_TEST[8];
    const static uint8_t CHSET_CH2_ONLY[8];
    const static uint8_t CHSET_ALL_ON[8];

private:
    SPISettings busSettings;
    uint8_t* channelSettings;

};
