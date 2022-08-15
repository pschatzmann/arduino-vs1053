
#pragma once

#if defined(ARDUINO)
#include "Arduino.h"
#include <SPI.h>

/**
 * @brief SPI Dirver for VS1053. We support different alternative implementations
 * 
 * @tparam Driver 
 */

template <class Driver>
class VS1053SPI {
  public:
    void beginTransaction() const{ driver.beginTransaction();}
    void endTransaction() const {driver.endTransaction();}
    void set_speed(int speed){ driver.set_speed(speed);}

    inline void write(uint8_t data) const {driver.write(data);}
    inline void write16(uint16_t data) const {driver.write16(data);}
    inline void write_bytes(const uint8_t * data, uint32_t size) const {driver.write_bytes(data,size);}
    inline uint8_t transfer(uint8_t data) const { return driver.transfer(data);}
    inline uint16_t transfer16(uint16_t data) const {return driver.transfer(data);}
    uint16_t read16(uint16_t port) {return driver.read16(port);}

  protected:
    Driver driver;
};


/**
 * @brief Generic SPI Driver for Arduino
 * 
 */
class VS1053SPIArduino {
  protected:
    SPISettings settings = SPISettings(200000, MSBFIRST, SPI_MODE0);

  public:
    void beginTransaction() const{ SPI.beginTransaction(settings);}
    void endTransaction() const{SPI.endTransaction();}
    void set_speed(int speed){ settings._clock=speed;}

    inline void write(uint8_t data) const {
        (void)SPI.transfer(data);
    }

    inline uint8_t transfer(uint8_t data) const {
        return SPI.transfer(data);
    }

    inline void write16(uint16_t data) const {
        (void)SPI.transfer16(data);
    }

    uint16_t read16(uint16_t port) {
        return SPI.transfer16(port);
    }

    inline void write_bytes(const uint8_t * data, uint32_t size) const {
        for (int i = 0; i < size; ++i) {
            SPI.transfer(data[i]);
        }
    }

};


#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)

/**
 * @brief Arduono Driver for ESP32 
 * 
 */
class VS1053SPIESP32 {
  protected:
    SPISettings settings = SPISettings(200000, MSBFIRST, SPI_MODE0);
  public:
    void beginTransaction() const { SPI.beginTransaction(settings);}
    void endTransaction() const {SPI.endTransaction();}
    void set_speed(int speed){ settings._clock=speed;}

    inline void write(uint8_t data) const {
        SPI.write(data);
    }

    inline uint8_t transfer(uint8_t data) const {
        return SPI.transfer(data);
    }

    inline void write16(uint16_t data) const {
        SPI.write16(data);
    }

    inline void write_bytes(const uint8_t * data, uint32_t size) const {
        SPI.writeBytes(data, size);
    }

    uint16_t read16(uint16_t port) {
        return SPI.transfer16(port);
    }

};

#endif
#endif
