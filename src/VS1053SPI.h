
#pragma once

#if defined(ARDUINO)
# include "Arduino.h"
# include <SPI.h>
#else
# include "VS1053Ext.h"
#endif
/**
 * @brief Abstract SPI Driver for VS1053. We support different alternative implementations.
 * Outside of Arduino you need to provide your own
 * 
 * @tparam Driver 
 */

class VS1053SPI {
  public:
    virtual void beginTransaction() = 0;
    virtual void endTransaction() = 0;
    virtual void set_speed(uint32_t speed)= 0;

    virtual  void write(uint8_t data) = 0;
    virtual  void write16(uint16_t data)= 0;
    virtual  void write_bytes(const uint8_t * data, uint32_t size) = 0;
    virtual  uint8_t transfer(uint8_t data) = 0;
    virtual uint16_t read16(uint16_t port) = 0;

};


#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)

/**
 * @brief Arduino Driver for ESP32 
 * 
 */
class VS1053SPIESP32 : public VS1053SPI {
  protected:
    uint32_t speed = 200000;
  public:
    VS1053SPIESP32(SPIClass &spi=SPI){
        p_spi = &spi;
    }

    void beginTransaction()  override { 
        SPISettings settings(speed, MSBFIRST, SPI_MODE0);       
        p_spi->beginTransaction(settings);
    }
    void endTransaction()  override{SPI.endTransaction();}
    void set_speed(uint32_t value){ this->speed = value;}

     void write(uint8_t data)  override{
        p_spi->write(data);
    }

     uint8_t transfer(uint8_t data)  override{
        return p_spi->transfer(data);
    }

     void write16(uint16_t data)  override{
        p_spi->write16(data);
    }

     void write_bytes(const uint8_t * data, uint32_t size)  override{
        p_spi->writeBytes(data, size);
    }

    uint16_t read16(uint16_t port) override {
        return p_spi->transfer16(port);
    }
  protected:
    SPIClass *p_spi;

};

#elif defined(ARDUINO)

/**
 * @brief Generic SPI Driver for Arduino
 * 
 */
class VS1053SPIArduino : public VS1053SPI {
  protected:
    uint32_t speed = 200000;

  public:
    VS1053SPIArduino() = default;

    void beginTransaction()  override{ 
        SPISettings settings(speed, MSBFIRST, SPI_MODE0);       
        SPI.beginTransaction(settings);
    }
    void endTransaction()  override {SPI.endTransaction();}
    void set_speed(uint32_t value){ this->speed = value;}

     void write(uint8_t data)  override {
        (void)SPI.transfer(data);
    }

     uint8_t transfer(uint8_t data)  override{
        return SPI.transfer(data);
    }

     void write16(uint16_t data)  override{
        (void)SPI.transfer16(data);
    }

    uint16_t read16(uint16_t port) override {
        return SPI.transfer16(port);
    }

     void write_bytes(const uint8_t * data, uint32_t size)  override {
        SPI.transfer((void*)data, size);
        // for (int i = 0; i < size; ++i) {
        //     SPI.transfer(data[i]);
        // }
    }
};

#endif

