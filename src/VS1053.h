/**
 * This is a driver library for VS1053 MP3 Codec Breakout
 * (Ogg Vorbis / MP3 / AAC / WMA / FLAC / MIDI Audio Codec Chip).
 * Adapted for Espressif ESP8266 and ESP32 boards.
 *
 * version 1.0.1
 *
 * Licensed under GNU GPLv3 <http://gplv3.fsf.org/>
 * Copyright © 2017
 *
 * @authors baldram, edzelf, MagicCube, maniacbug
 *
 * Development log:
 *  - 2011: initial VS1053 Arduino library
 *          originally written by J. Coliz (github: @maniacbug),
 *  - 2016: refactored and integrated into Esp-radio sketch
 *          by Ed Smallenburg (github: @edzelf)
 *  - 2017: refactored to use as PlatformIO library
 *          by Marcin Szalomski (github: @baldram | twitter: @baldram)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License or later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef VS1053_H
#define VS1053_H

#include <Arduino.h>
#include "ConsoleLogger.h"
#include "VS1053SPI.h"
#include "patches/vs1053b-patches.h"
#include "patches/vs1053-input.h"

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

enum VS1053_I2S_RATE {
    VS1053_I2S_RATE_192_KHZ,
    VS1053_I2S_RATE_96_KHZ,
    VS1053_I2S_RATE_48_KHZ
};

enum VS1053_INPUT {
    VS1053_MIC,
    VS1053_AUX,
    VS1053_ALL
};

struct VS1053 {
    /**
     * @brief Amplitude and Frequency Limit 
     * 
     */
    class VS1053EquilizerValue {
    public:
        VS1053EquilizerValue(uint8_t limit){
            this->freq_limit = limit;
        }
        /// Frequency Limit in hz: range 0 to 15000
        uint16_t freq_limit=0; 
        /// Amplitude value: range 0 to 100 
        uint8_t amplitude=0;  
        /// Register value for amplitude
        uint16_t scaledAmplitude(){
            if (amplitude>100) amplitude = 100;
            return static_cast<float>(amplitude)/100.0*15;
        }
        /// Register value for Frequency Limit
        uint16_t scaledFreq(float scale){
            uint16_t result = static_cast<float>(freq_limit) / scale;
            if (result>15) result = 15;
            return result;
        }
    };
    /**
     * @brief Equilizer Functionality using the SCI_BASS Register
     */

    class VS1053Equilizer {
    protected:
        VS1053EquilizerValue v_bass{3}; // 30 hz
        VS1053EquilizerValue v_treble{15}; // 15000 hz


    public:
        /// Provides the bass VS1053EquilizerValue
        VS1053EquilizerValue &bass() {
            return v_bass;
        }

        /// Provides the treble VS1053EquilizerValue
        VS1053EquilizerValue &treble() {
            return v_treble;
        }

        /// Provides the VS1053 SCI_BASS Register Value
        uint16_t value() {
            uint16_t result=0;
            result |= v_treble.scaledAmplitude() << 12;
            result |= v_treble.scaledFreq(1000) << 8;
            result |= v_bass.scaledAmplitude() << 4;
            result |= v_bass.scaledFreq(10);
            return result;
        }
    };

protected:
    uint8_t cs_pin;                         // Pin where CS line is connected
    uint8_t dcs_pin;                        // Pin where DCS line is connected
    uint8_t dreq_pin;                       // Pin where DREQ line is connected
    uint8_t curvol;                         // Current volume setting 0..100%
    int16_t reset_pin = -1;                 // Custom Reset Pin (optional)
    int8_t  curbalance = 0;                 // Current balance setting -100..100
                                            // (-100 = right channel silent, 100 = left channel silent)
    const uint8_t vs1053_chunk_size = 32;
    SPISettings VS1053_SPI;                 // SPI settings for this slave
    uint8_t endFillByte;                    // Byte to send when stopping song
    VS1053Equilizer equilizer;
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    VS1053SPI<VS1053SPIESP32> spi;
#else
    VS1053SPI<VS1053SPIArduino> spi;
#endif


 public:
    // SCI Register
    const uint8_t SCI_MODE = 0x0;
    const uint8_t SCI_STATUS = 0x1;
    const uint8_t SCI_BASS = 0x2;
    const uint8_t SCI_CLOCKF = 0x3;
    const uint8_t SCI_DECODE_TIME = 0x4;        // current decoded time in full seconds
    const uint8_t SCI_AUDATA = 0x5;
    const uint8_t SCI_WRAM = 0x6;
    const uint8_t SCI_WRAMADDR = 0x7;
    const uint8_t SCI_AIADDR = 0xA;
    const uint8_t SCI_VOL = 0xB;
    const uint8_t SCI_AICTRL0 = 0xC;
    const uint8_t SCI_AICTRL1 = 0xD;
    const uint8_t SCI_AICTRL2 = 0xE;
    const uint8_t SCI_AICTRL3 = 0xF;
    const uint8_t SCI_num_registers = 0xF;
    // Stream header data
    const uint8_t SCI_HDAT0 = 0x8;          // Stream header data 0
    const uint8_t SCI_HDAT1 = 0x9;          // Stream header data 1

    // SCI_MODE bits
    const uint8_t SM_SDINEW = 11;           // Bitnumber in SCI_MODE always on
    const uint8_t SM_RESET = 2;             // Bitnumber in SCI_MODE soft reset
    const uint8_t SM_CANCEL = 3;            // Bitnumber in SCI_MODE cancel song
    const uint8_t SM_TESTS = 5;             // Bitnumber in SCI_MODE for tests
    const uint8_t SM_LINE1 = 14;            // Bitnumber in SCI_MODE for Line input
    const uint8_t SM_STREAM = 6;            // Bitnumber in SCI_MODE for Streaming Mode
    const uint8_t SM_ADPCM = 12;            // PCM/ADPCM recording active

    const uint16_t ADDR_REG_GPIO_DDR_RW = 0xc017;
    const uint16_t ADDR_REG_GPIO_VAL_R = 0xc018;
    const uint16_t ADDR_REG_GPIO_ODATA_RW = 0xc019;
    const uint16_t ADDR_REG_I2S_CONFIG_RW = 0xc040;

    // Timer settings  for VS1053 and VS1063 */
    const uint16_t INT_ENABLE = 0xC01A;
    const uint16_t SC_MULT_53_35X = 0x8000;
    const uint16_t SC_ADD_53_10X = 0x0800;


protected:
    inline void await_data_request() const {
        while (!digitalRead(dreq_pin)) {
            yield();                        // Very short delay
        }
    }

    inline void control_mode_on() const {
        spi.beginTransaction();   // Prevent other SPI users
        digitalWrite(dcs_pin, HIGH);        // Bring slave in control mode
        digitalWrite(cs_pin, LOW);
    }

    inline void control_mode_off() const {
        digitalWrite(cs_pin, HIGH);         // End control mode
        spi.endTransaction();               // Allow other SPI users
    }

    inline void data_mode_on() const {
        spi.beginTransaction();   // Prevent other SPI users
        digitalWrite(cs_pin, HIGH);         // Bring slave in data mode
        digitalWrite(dcs_pin, LOW);
    }

    inline void data_mode_off() const {
        digitalWrite(dcs_pin, HIGH);        // End data mode
        spi.endTransaction();               // Allow other SPI users
    }

    uint16_t read_register(uint8_t _reg) const;

    void sdi_send_buffer(uint8_t *data, size_t len);

    void sdi_send_fillers(size_t length);



    void wram_write(uint16_t address, uint16_t data);

    uint16_t wram_read(uint16_t address);

public:
    /// Constructor.  Only sets pin values.  Doesn't touch the chip.  Be sure to call begin()!
    VS1053(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin);

    /// Constructor which allows a custom reset pin
    VS1053(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin, uint8_t _reset_pin);

    /// Begin operation.  Sets pins correctly, and prepares SPI bus.
    void begin();

    /// Prepare to start playing. Call this each time a new song starts
    void startSong();

    /// Play a chunk of data.  Copies the data to the chip.  Blocks until complete
    void playChunk(uint8_t *data, size_t len);
    
    /// performs a MIDI command
    void sendMidiMessage(uint8_t cmd, uint8_t data1, uint8_t data2);    
	
    /// Finish playing a song. Call this after the last playChunk call
    void stopSong();

    /// Set the player volume.Level from 0-100, higher is louder
    void setVolume(uint8_t vol);

    /// Adjusting the left and right volume balance, higher to enhance the right side, lower to enhance the left side.
    void setBalance(int8_t balance);

    /// Set the player baas/treble, 4 nibbles for treble gain/freq and bass gain/freq
    void setTone(uint8_t *rtone);

    /// Get the currenet volume setting, higher is louder
    uint8_t getVolume();

    /// Get the currenet balance setting (-100..100)
    int8_t getBalance();

    /// Print configuration details to serial output.
    void printDetails(const char *header);

    /// Do a soft reset
    void softReset();

    /// Do a hardware reset
    void hardReset();

    /// Test communication with module
    bool testComm(const char *header);

    inline bool data_request() const {
        return (digitalRead(dreq_pin) == HIGH);
    }

    /// Fine tune the data rate
    void adjustRate(long ppm2);

    /// Streaming Mode On
    void streamModeOn();
    
    /// Default: Streaming Mode Off
    void streamModeOff();      

    /// An optional switch preventing the module starting up in MIDI mode
    void switchToMp3Mode();

    /// disable I2S output; this is the default state
    void disableI2sOut();

    /// enable I2S output (GPIO4=LRCLK/WSEL; GPIO5=MCLK; GPIO6=SCLK/BCLK; GPIO7=SDATA/DOUT)
    void enableI2sOut(VS1053_I2S_RATE i2sRate = VS1053_I2S_RATE_48_KHZ);

    /// Checks whether the VS1053 chip is connected and is able to exchange data to the ESP
    bool isChipConnected();

    /// gets Version of the VLSI chip being used
    uint16_t getChipVersion();    

    /// Provides SCI_DECODE_TIME register value
    uint16_t getDecodedTime();

    /// Clears SCI_DECODE_TIME register (sets 0x00)
    void clearDecodedTime();

    /// Writes to VS10xx's SCI (serial command interface) SPI bus.
    // A low level method which lets users access the internals of the VS1053.
    void writeRegister(uint8_t _reg, uint16_t _value) const;

    /// Load a patch or plugin to fix bugs and/or extend functionality.
    // For more info about patches see http://www.vlsi.fi/en/support/software/vs10xxpatches.html
    void loadUserCode(const unsigned short* plugin, unsigned short plugin_size);

    /// Loads the latest generic firmware patch.
    void loadDefaultVs1053Patches();


    /// Provides the treble amplitude value
    uint8_t treble();

    /// Sets the treble amplitude value (range 0 to 100)
    void setTreble(uint8_t value);

    /// Provides the Bass amplitude value 
    uint8_t bass();

    /// Sets the bass amplitude value (range 0 to 100)
    void setBass(uint8_t value);

    /// Sets the treble frequency limit in hz (range 0 to 15000)
    void setTrebleFrequencyLimit(uint16_t value);

    /// Sets the bass frequency limit in hz (range 0 to 15000)
    void setBassFrequencyLimit(uint16_t value);


    /// Starts the recording of sound as WAV data
    void beginInput(bool wavHeader=true);

    /// Stops the recording of sound
    void end();

    /// Provides the number of bytes which are available in the read buffer
    size_t available();

    /// Provides the audio data as WAV
    size_t readBytes(uint8_t*data, size_t len);

};

#endif
