

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





class ADS1298R {
public:
    ADS1298R();

    void init();




private:

}



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