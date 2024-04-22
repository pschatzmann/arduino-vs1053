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
 * @authors baldram, edzelf, MagicCube, maniacbug, pschatzmann
 *
 * Development log:
 *  - 2011: initial VS1053 Arduino library
 *          originally written by J. Coliz (github: @maniacbug),
 *  - 2016: refactored and integrated into Esp-radio sketch
 *          by Ed Smallenburg (github: @edzelf)
 *  - 2017: refactored to use as PlatformIO library
 *          by Marcin Szalomski (github: @baldram | twitter: @baldram)
 *  - 2022: Refactord to provide first class Arduino support for all processors!
 *          Redesigned SPI interface
 *          Support for Realtime Midi
 *          Support for Reading  from the microphone
 *          Some additional methods to handle treble, bass, earphones
 *          Support for cmake 
 *          using arduino_vs1053 namespace
 *          by Phil Schatzmann
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

#pragma once


#include "VS1053Config.h"
#include "VS1053Logger.h"
#include "VS1053SPI.h"
#include "VS1053Recording.h"
#include "patches/vs1053b-patches.h"
#include "patches_in/vs1003b-pcm.h"
#include "patches_in/vs1053b-pcm.h"
#include "patches_midi/rtmidi1053b.h"
#include "patches_midi/rtmidi1003b.h"

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

namespace arduino_vs1053 {


/** @file */

/// I2S Rate
enum VS1053_I2S_RATE {
    VS1053_I2S_RATE_192_KHZ,
    VS1053_I2S_RATE_96_KHZ,
    VS1053_I2S_RATE_48_KHZ
};

/// Actual Mode: Input, Output, Output Streaming Midi
enum VS1053_MODE {
    VS1053_NA,
    VS1053_OUT,
    VS1053_IN,
    VS1053_MIDI,
};

/// Earspeaker settings
enum VS1053_EARSPEAKER {
    VS1053_EARSPEAKER_OFF = 0,
    VS1053_EARSPEAKER_MIN,
    VS1053_EARSPEAKER_ON,
    VS1053_EARSPEAKER_MAX
};

/**
 * @brief Main class for controlling VS1053 and VS1003 modules
 * 
 */
class VS1053 {

    /**
     * @brief Amplitude and Frequency Limit 
     */
    struct VS1053EquilizerValue {
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

    /**
     * @brief Some Additional logic for the VS1003 to manage the complicated clock values. 
     * @author pschatzmann
     */
    class VS1003Clock {
      public:
        VS1003Clock() = default;

        int setSampleRate(int sample_rate){        
            int diff_min = sample_rate; // max value for difference
            int sample_rate_result = -1;
            multiplier = -1;

            // find the combination with the smallet difference
            for (int j=0;j<7;j++){
                for (int div=4;div<=126;div++){
                    float mf = multiplier_factors[j];
                    uint16_t sc_mult = sc_multipliers[j];
                    int sample_rate_eff = getSampleRate(div, mf);

                    if (sample_rate_eff < sample_rate){
                        // increasing dividers will make the sample rate just smaller, wo we break
                        break;
                    }

                    int diff = abs(sample_rate_eff - sample_rate);
                    VS1053_LOGD("--> div: %d - mult: %f (%x) -> %d", div, mf, sc_mult, sample_rate_eff);
                    if (diff<diff_min){
                        VS1053_LOGD("==> div: %d - mult: %f (%x) -> %d", div, mf, sc_mult, sample_rate_eff);
                        diff_min = diff;
                        this->divider = div;
                        this->multiplier = sc_mult;
                        this->multiplier_factor = mf;
                        sample_rate_result = sample_rate_eff;
                    }
                    // if we found the correct rate we are done
                    if (diff==0) return sample_rate_result; 
                }
            }
            return multiplier==-1? -1: sample_rate_result;
        }

        uint16_t getMultiplierRegisterValue() {
            return multiplier;
        }

        float getMultiplier() {
            return multiplier_factor;
        }

        int8_t getDivider() {
            return divider; // 4. If SCI_AICTRL0 contains 0, the default divider value 12 is used.
        }

      protected:
        const uint16_t SC_1003_MULT_1 = 0x0000;
        const uint16_t SC_1003_MULT_2 = 0x2000;
        const uint16_t SC_1003_MULT_25 = 0x4000;
        const uint16_t SC_1003_MULT_3 = 0x6000;
        const uint16_t SC_1003_MULT_35 = 0x8000;
        const uint16_t SC_1003_MULT_4 = 0xa000;
        const uint16_t SC_1003_MULT_45 = 0xc000;
        const uint16_t SC_1003_MULT_5 = 0xe000;

        float clock_rate = 12288000;
        int divider = -1;
        int multiplier = -1;
        const uint16_t sc_multipliers[7] = { SC_1003_MULT_2,SC_1003_MULT_25,SC_1003_MULT_3, SC_1003_MULT_35, SC_1003_MULT_4, SC_1003_MULT_45, SC_1003_MULT_5};
        float multiplier_factor;
        const float multiplier_factors[7] = { 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0};

        int getSampleRate(int div, float multiplierValue){
            return (multiplierValue * clock_rate) / 256 / div;
        }

    };


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
    const uint8_t SM_ADPCM = 12;            // Bitnumber in SCI_MODE for PCM/ADPCM recording active 

    const uint16_t ADDR_REG_GPIO_DDR_RW = 0xc017;
    const uint16_t ADDR_REG_GPIO_VAL_R = 0xc018;
    const uint16_t ADDR_REG_GPIO_ODATA_RW = 0xc019;
    const uint16_t ADDR_REG_I2S_CONFIG_RW = 0xc040;

    // Timer settings  for VS1053 and VS1063 */
    const uint16_t INT_ENABLE = 0xC01A;
    const uint16_t SC_MULT_53_35X = 0x8000;
    const uint16_t SC_ADD_53_10X = 0x0800;

    const uint16_t SC_EAR_SPEAKER_LO = 0x0010;
    const uint16_t SC_EAR_SPEAKER_HI = 0x0080;


    /// Constructor which allows a custom reset pin
    VS1053(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin, uint8_t _reset_pin=-1, VS1053_SPI *_p_spi=nullptr);

#ifdef ARDUINO
    /// Constructor which allows a custom reset pin and a Arduino SPI object
    VS1053(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin, uint8_t _reset_pin, SPIClass &_p_spi);

#endif

    /// Begin operation.  Sets pins correctly, and prepares SPI bus.
    bool begin();

    /// Prepares for regular output in decoding mode - (note that this method is also is calling begin())
    bool beginOutput();

    /// Prepare to start playing. Call this each time a new song starts
    void startSong();

    /// Play a chunk of data.  Copies the data to the chip.  Blocks until complete - also supports serial midi
    void writeAudio(uint8_t*data, size_t len);

    /// Legacy method - Play a chunk of data.  Copies the data to the chip.  Blocks until complete
    void playChunk(uint8_t *data, size_t len);
    
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

    /// Load a patch or plugin to fix bugs and/or extend functionality.
    // For more info about patches see http://www.vlsi.fi/en/support/software/vs10xxpatches.html
    void loadUserCode(const unsigned short* plugin, unsigned short plugin_size);

    /// Loads the latest generic firmware patch.
    bool loadDefaultVs1053Patches();


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

    /// Activate the ear speaker mode
    bool setEarSpeaker(VS1053_EARSPEAKER value);

    /// Stops the recording of sound - and resets the module
    void end();

#if USE_MIDI
    /// Starts the MIDI output processing
    bool beginMidi();

    /// performs a MIDI command
    void sendMidiMessage(uint8_t cmd, uint8_t data1, uint8_t data2);    
#endif

    /// Starts the recording of sound as WAV data
    bool beginInput(VS1053Recording &opt);

    /// Provides the number of bytes which are available in the read buffer
    size_t available();

    /// Provides the audio data as WAV
    size_t readBytes(uint8_t*data, size_t len);

    /// Reads a register value
    // A low level method which lets users access the internals of the VS1053.
    uint16_t readRegister(uint8_t _reg) const;

    /// Writes to VS10xx's SCI (serial command interface) SPI bus.
    // A low level method which lets users access the internals of the VS1053.
    void writeRegister(uint8_t _reg, uint16_t _value) const;


protected:
    uint8_t cs_pin;                         // Pin where CS line is connected
    uint8_t dcs_pin;                        // Pin where DCS line is connected
    uint8_t dreq_pin;                       // Pin where DREQ line is connected
    uint8_t curvol;                         // Current volume setting 0..100%
    int16_t reset_pin = -1;                 // Custom Reset Pin (optional)
    int8_t  curbalance = 0;                 // Current balance setting -100..100
                                            // (-100 = right channel silent, 100 = left channel silent)
    const uint8_t vs1053_chunk_size = 32;
    VS1053_SPI *p_spi = nullptr;             // SPI Driver
    uint8_t endFillByte;                    // Byte to send when stopping song
    VS1053Equilizer equilizer;
    VS1053_MODE mode;
    uint16_t chip_version = -1;
    uint8_t channels_multiplier = 1;        // Repeat read values for multiple channels



protected:

    inline void await_data_request() const {
        while (!digitalRead(dreq_pin)) {
            yield();                        // Very short delay
        }
    }

    inline void control_mode_on() const {
        p_spi->beginTransaction();   // Prevent other SPI users
        digitalWrite(dcs_pin, HIGH);        // Bring slave in control mode
        digitalWrite(cs_pin, LOW);
    }

    inline void control_mode_off() const {
        digitalWrite(cs_pin, HIGH);         // End control mode
        p_spi->endTransaction();               // Allow other SPI users
    }

    inline void data_mode_on() const {
        p_spi->beginTransaction();   // Prevent other SPI users
        digitalWrite(cs_pin, HIGH);         // Bring slave in data mode
        digitalWrite(dcs_pin, LOW);
    }

    inline void data_mode_off() const {
        digitalWrite(dcs_pin, HIGH);        // End data mode
        p_spi->endTransaction();               // Allow other SPI users
    }

    void sdi_send_buffer(uint8_t *data, size_t len);

    void sdi_send_fillers(size_t length);

    void wram_write(uint16_t address, uint16_t data);

    uint16_t wram_read(uint16_t address);

    bool begin_input_vs1053(VS1053Recording &opt);

    bool begin_input_vs1003(VS1053Recording &opt);

    void set_flag(uint16_t &value, uint16_t flag, bool active);
};

}
