#include <SPI.h>
#include "Arduino.h"
#include "stdint.h"

#define PIN_PWDN    D2
#define PIN_RST     D3
#define PIN_START   D5
#define PIN_CS      14
#define PIN_DRDY    D7
#define PIN_DOUT    MISO
#define PIN_DIN     MOSI
#define PIN_SCLK    SCK

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


;class ADS1298R {
public:
    ADS1298R(int baud, int bitOrder, int mode);

    void init();
    void initLoop();

    void readDataContinuous();
    void stopDataContinuous();

    void writeRegister(uint8_t register, uint8_t value);

    void writeRegisters(uint8_t start_reg, int num_regs, uint8_t* value);

    uint8_t readRegister(uint8_t register);

    uint8_t* readRegisters(uint8_t start_reg, int num_regs);

private:
    SPISettings busSettings;

};
