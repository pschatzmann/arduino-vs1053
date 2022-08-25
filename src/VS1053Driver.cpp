/**
 * This is a driver library for VS1053 MP3 Codec Breakout
 * (Ogg Vorbis / MP3 / AAC / WMA / FLAC / MIDI Audio Codec Chip).
 * Adapted for Espressif ESP8266 and ESP32 boards.
 *
 * version 1.0.1
 *
 * Licensed under GNU GPLv3 <http://gplv3.fsf.org/>
 * Copyright © 2018
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
#include "VS1053Driver.h"


VS1053::VS1053(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin)
        : cs_pin(_cs_pin), dcs_pin(_dcs_pin), dreq_pin(_dreq_pin) {
}
VS1053::VS1053(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin, uint8_t _reset_pin)
        : cs_pin(_cs_pin), dcs_pin(_dcs_pin), dreq_pin(_dreq_pin), reset_pin(_reset_pin) {
}

uint16_t VS1053::readRegister(uint8_t _reg) const {
    uint16_t result;

    control_mode_on();
    spi.write(3);    // Read operation
    spi.write(_reg); // Register to write (0..0xF)
    // Note: transfer16 does not seem to work
    result = (spi.transfer(0xFF) << 8) | // Read 16 bits data
             (spi.transfer(0xFF));
    await_data_request(); // Wait for DREQ to be HIGH again
    control_mode_off();
    return result;
}

void VS1053::writeRegister(uint8_t _reg, uint16_t _value) const {
    control_mode_on();
    spi.write(2);        // Write operation
    spi.write(_reg);     // Register to write (0..0xF)
    spi.write16(_value); // Send 16 bits data
    await_data_request();
    control_mode_off();
}

void VS1053::sdi_send_buffer(uint8_t *data, size_t len) {
    size_t chunk_length; // Length of chunk 32 byte or shorter

    data_mode_on();
    while (len) // More to do?
    {
        await_data_request(); // Wait for space available
        chunk_length = len;
        if (len > vs1053_chunk_size) {
            chunk_length = vs1053_chunk_size;
        }
        len -= chunk_length;
        spi.write_bytes(data, chunk_length);
        data += chunk_length;
    }
    data_mode_off();
}

void VS1053::sdi_send_fillers(size_t len) {
    size_t chunk_length; // Length of chunk 32 byte or shorter

    data_mode_on();
    while (len) // More to do?
    {
        await_data_request(); // Wait for space available
        chunk_length = len;
        if (len > vs1053_chunk_size) {
            chunk_length = vs1053_chunk_size;
        }
        len -= chunk_length;
        while (chunk_length--) {
            spi.write(endFillByte);
        }
    }
    data_mode_off();
}

void VS1053::wram_write(uint16_t address, uint16_t data) {
    writeRegister(SCI_WRAMADDR, address);
    writeRegister(SCI_WRAM, data);
}

uint16_t VS1053::wram_read(uint16_t address) {
    writeRegister(SCI_WRAMADDR, address); // Start reading from WRAM
    return readRegister(SCI_WRAM);        // Read back result
}

bool VS1053::testComm(const char *header) {
    // Test the communication with the VS1053 module.  The result wille be returned.
    // If DREQ is low, there is problably no VS1053 connected.  Pull the line HIGH
    // in order to prevent an endless loop waiting for this signal.  The rest of the
    // software will still work, but readbacks from VS1053 will fail.
    int i; // Loop control
    uint16_t r1, r2, cnt = 0;
    uint16_t delta = 300; // 3 for fast SPI

    if (!digitalRead(dreq_pin)) {
        LOG("VS1053 not properly installed!");
        // Allow testing without the VS1053 module
        pinMode(dreq_pin, INPUT_PULLUP); // DREQ is now input with pull-up
        return false;                    // Return bad result
    }
    // Further TESTING.  Check if SCI bus can write and read without errors.
    // We will use the volume setting for this.
    // Will give warnings on serial output if DEBUG is active.
    // A maximum of 20 errors will be reported.
    if (strstr(header, "Fast")) {
        delta = 3; // Fast SPI, more loops
    }

    LOG("%s", header);  // Show a header

    for (i = 0; (i < 0xFFFF) && (cnt < 20); i += delta) {
        writeRegister(SCI_VOL, i);         // Write data to SCI_VOL
        r1 = readRegister(SCI_VOL);        // Read back for the first time
        r2 = readRegister(SCI_VOL);        // Read back a second time
        if (r1 != r2 || i != r1 || i != r2) // Check for 2 equal reads
        {
            LOG("VS1053 error retry SB:%04X R1:%04X R2:%04X", i, r1, r2);
            cnt++;
            delay(10);
        }
        yield(); // Allow ESP firmware to do some bookkeeping
    }
    return (cnt == 0); // Return the result
}

bool VS1053::begin() {
    LOG("begin");

    // support for optional custom reset pin when wiring is not possible
    if (reset_pin!=-1){
        pinMode(reset_pin, OUTPUT);
        digitalWrite(reset_pin, HIGH);
    }

    pinMode(dreq_pin, INPUT); // DREQ is an input
    pinMode(cs_pin, OUTPUT);  // The SCI and SDI signals
    pinMode(dcs_pin, OUTPUT);
    digitalWrite(dcs_pin, HIGH); // Start HIGH for SCI en SDI
    digitalWrite(cs_pin, HIGH);
    delay(100);
    LOG("Reset VS1053...");
    digitalWrite(dcs_pin, LOW); // Low & Low will bring reset pin low
    digitalWrite(cs_pin, LOW);
    delay(500);
    LOG("End reset VS1053...");
    digitalWrite(dcs_pin, HIGH); // Back to normal again
    digitalWrite(cs_pin, HIGH);
    delay(500);
    // Init SPI in slow mode ( 0.2 MHz )
    spi.set_speed(200000);
    // printDetails("Right after reset/startup");
    delay(20);
    // printDetails("20 msec after reset");
    if (testComm("Slow SPI,Testing VS1053 read/write registers...")) {
        //softReset();
        // Switch on the analog parts
        writeRegister(SCI_AUDATA, 44101); // 44.1kHz stereo
        // The next clocksetting allows SPI clocking at 5 MHz, 4 MHz is safe then.
        writeRegister(SCI_CLOCKF, 6 << 12); // Normal clock settings multiplyer 3.0 = 12.2 MHz
        // SPI Clock to 4 MHz. Now you can set high speed SPI clock.
        spi.set_speed(4000000);
        writeRegister(SCI_MODE, _BV(SM_SDINEW) | _BV(SM_LINE1));
        testComm("Fast SPI, Testing VS1053 read/write registers again...");
        delay(10);
        await_data_request();
        endFillByte = wram_read(0x1E06) & 0xFF;
        LOG("endFillByte is %X", endFillByte);
        //printDetails("After last clocksetting") ;
        delay(100);
    }
    chip_version = getChipVersion();
    return true;
}

void VS1053::setVolume(uint8_t vol) {
    // Set volume.  Both left and right.
    // Input value is 0..100.  100 is the loudest.
    uint16_t valueL, valueR; // Values to send to SCI_VOL

    curvol = vol;                         // Save for later use
    valueL = vol;
    valueR = vol;

    if (curbalance < 0) {
        valueR = max(0, vol + curbalance);
    } else if (curbalance > 0) {
        valueL = max(0, vol - curbalance);
    }

    valueL = map(valueL, 0, 100, 0xFE, 0x00); // 0..100% to left channel
    valueR = map(valueR, 0, 100, 0xFE, 0x00); // 0..100% to right channel

    int16_t value = (valueL << 8) | valueR;
    writeRegister(SCI_VOL, value); // Volume left and right
    LOG("setVolume: %x", value);
}

void VS1053::setBalance(int8_t balance) {
    if (balance > 100) {
        curbalance = 100;
    } else if (balance < -100) {
        curbalance = -100;
    } else {
        curbalance = balance;
    }
}

void VS1053::setTone(uint8_t *rtone) { // Set bass/treble (4 nibbles)
    // Set tone characteristics.  See documentation for the 4 nibbles.
    uint16_t value = 0; // Value to send to SCI_BASS
    int i;              // Loop control

    for (i = 0; i < 4; i++) {
        value = (value << 4) | rtone[i]; // Shift next nibble in
    }
    writeRegister(SCI_BASS, value); // Volume left and right
}

uint8_t VS1053::getVolume() { // Get the currenet volume setting.
    return curvol;
}

int8_t VS1053::getBalance() { // Get the currenet balance setting.
    return curbalance;
}

void VS1053::startSong() {
    sdi_send_fillers(10);
}


void VS1053::playChunk(uint8_t *data, size_t len) {
    sdi_send_buffer(data, len);
}

void VS1053::stopSong() {
    uint16_t modereg; // Read from mode register
    int i;            // Loop control

    sdi_send_fillers(2052);
    delay(10);
    writeRegister(SCI_MODE, _BV(SM_SDINEW) | _BV(SM_CANCEL));
    for (i = 0; i < 200; i++) {
        sdi_send_fillers(32);
        modereg = readRegister(SCI_MODE); // Read status
        if ((modereg & _BV(SM_CANCEL)) == 0) {
            sdi_send_fillers(2052);
            LOG("Song stopped correctly after %d msec", i * 10);
            return;
        }
        delay(10);
    }
    printDetails("Song stopped incorrectly!");
}

void VS1053::softReset() {
    LOG("Performing soft-reset");
    writeRegister(SCI_MODE, _BV(SM_SDINEW) | _BV(SM_RESET));
    delay(10);
    await_data_request();
}

void VS1053::hardReset(){
    if (reset_pin!=-1){
        LOG("Performing hard-reset");
        digitalWrite(reset_pin, LOW);
        delay(500);
        digitalWrite(reset_pin, HIGH);
    } else {
        LOG("hard-reset not supported");
    }
}


/**
 * VLSI datasheet: "SM_STREAM activates VS1053b’s stream mode. In this mode, data should be sent with as
 * even intervals as possible and preferable in blocks of less than 512 bytes, and VS1053b makes
 * every attempt to keep its input buffer half full by changing its playback speed up to 5%. For best
 * quality sound, the average speed error should be within 0.5%, the bitrate should not exceed
 * 160 kbit/s and VBR should not be used. For details, see Application Notes for VS10XX. This
 * mode only works with MP3 and WAV files."
*/

void VS1053::streamModeOn() {
    LOG("Performing streamModeOn");
    writeRegister(SCI_MODE, _BV(SM_SDINEW) | _BV(SM_STREAM));
    delay(10);
    await_data_request();
}

void VS1053::streamModeOff() {
    LOG("Performing streamModeOff");
    writeRegister(SCI_MODE, _BV(SM_SDINEW));
    delay(10);
    await_data_request();
}

void VS1053::printDetails(const char *header) {
    uint16_t regbuf[16];
    uint8_t i;

    LOG("%s", header);
    LOG("REG   Contents");
    LOG("---   -----");
    for (i = 0; i <= SCI_num_registers; i++) {
        regbuf[i] = readRegister(i);
    }
    for (i = 0; i <= SCI_num_registers; i++) {
        delay(5);
        LOG("%3X - %5X", i, regbuf[i]);
    }
}

/**
 * An optional switch.
 * Most VS1053 modules will start up in MIDI mode. The result is that there is no audio when playing MP3.
 * You can modify the board, but there is a more elegant way without soldering.
 * No side effects for boards which do not need this switch. It means you can call it just in case.
 *
 * Read more here: http://www.bajdi.com/lcsoft-vs1053-mp3-module/#comment-33773
 */
void VS1053::switchToMp3Mode() {
    wram_write(ADDR_REG_GPIO_DDR_RW, 3); // GPIO DDR = 3
    wram_write(ADDR_REG_GPIO_ODATA_RW, 0); // GPIO ODATA = 0
    delay(100);
    LOG("Switched to mp3 mode");
    softReset();
}

void VS1053::disableI2sOut() {
    wram_write(ADDR_REG_I2S_CONFIG_RW, 0x0000);

    // configure GPIO0 4-7 (I2S) as input (default)
    // leave other GPIOs unchanged
    uint16_t cur_ddr = wram_read(ADDR_REG_GPIO_DDR_RW);
    wram_write(ADDR_REG_GPIO_DDR_RW, cur_ddr & ~0x00f0);
}

void VS1053::enableI2sOut(VS1053_I2S_RATE i2sRate) {
    // configure GPIO0 4-7 (I2S) as output
    // leave other GPIOs unchanged
    uint16_t cur_ddr = wram_read(ADDR_REG_GPIO_DDR_RW);
    wram_write(ADDR_REG_GPIO_DDR_RW, cur_ddr | 0x00f0);

    uint16_t i2s_config = 0x000c; // Enable MCLK(3); I2S(2)
    switch (i2sRate) {
        case VS1053_I2S_RATE_192_KHZ:
            i2s_config |= 0x0002;
            break;
        case VS1053_I2S_RATE_96_KHZ:
            i2s_config |= 0x0001;
            break;
        default:
        case VS1053_I2S_RATE_48_KHZ:
            // 0x0000
            break;
    }

    wram_write(ADDR_REG_I2S_CONFIG_RW, i2s_config );
}

/**
 * A lightweight method to check if VS1053 is correctly wired up (power supply and connection to SPI interface).
 *
 * @return true if the chip is wired up correctly
 */
bool VS1053::isChipConnected() {
    uint16_t status = readRegister(SCI_STATUS);

    return !(status == 0 || status == 0xFFFF);
}

/**
 * get the Version Number for the VLSI chip
 * VLSI datasheet: 0 for VS1001, 1 for VS1011, 2 for VS1002, 3 for VS1003, 4 for VS1053 and VS8053,
 * 5 for VS1033, 7 for VS1103, and 6 for VS1063. 
 */
uint16_t VS1053::getChipVersion() {
    uint16_t status = readRegister(SCI_STATUS);
       
    return ( (status & 0x00F0) >> 4);
}

/**
 * Provides current decoded time in full seconds (from SCI_DECODE_TIME register value)
 *
 * When decoding correct data, current decoded time is shown in SCI_DECODE_TIME
 * register in full seconds. The user may change the value of this register.
 * In that case the new value should be written twice to make absolutely certain
 * that the change is not overwritten by the firmware. A write to SCI_DECODE_TIME
 * also resets the byteRate calculation.
 *
 * SCI_DECODE_TIME is reset at every hardware and software reset. It is no longer
 * cleared when decoding of a file ends to allow the decode time to proceed
 * automatically with looped files and with seamless playback of multiple files.
 * With fast playback (see the playSpeed extra parameter) the decode time also
 * counts faster. Some codecs (WMA and Ogg Vorbis) can also indicate the absolute
 * play position, see the positionMsec extra parameter in section 10.11.
 *
 * @see VS1053b Datasheet (1.31) / 9.6.5 SCI_DECODE_TIME (RW)
 *
 * @return current decoded time in full seconds
 */
uint16_t VS1053::getDecodedTime() {
    return readRegister(SCI_DECODE_TIME);
}

/**
 * Clears decoded time (sets SCI_DECODE_TIME register to 0x00)
 *
 * The user may change the value of this register. In that case the new value
 * should be written twice to make absolutely certain that the change is not
 * overwritten by the firmware. A write to SCI_DECODE_TIME also resets the
 * byteRate calculation.
 */
void VS1053::clearDecodedTime() {
    writeRegister(SCI_DECODE_TIME, 0x00);
    writeRegister(SCI_DECODE_TIME, 0x00);
}

/**
 * Fine tune the data rate
 */
void VS1053::adjustRate(long ppm2) {
    writeRegister(SCI_WRAMADDR, 0x1e07);
    writeRegister(SCI_WRAM, ppm2);
    writeRegister(SCI_WRAM, ppm2 >> 16);
    // oldClock4KHz = 0 forces  adjustment calculation when rate checked.
    writeRegister(SCI_WRAMADDR, 0x5b1c);
    writeRegister(SCI_WRAM, 0);
    // Write to AUDATA or CLOCKF checks rate and recalculates adjustment.
    writeRegister(SCI_AUDATA, readRegister(SCI_AUDATA));
}

/**
 * Load a patch or plugin
 *
 * Patches can be found on the VLSI Website http://www.vlsi.fi/en/support/software/vs10xxpatches.html
 *  
 * Please note that loadUserCode only works for compressed plugins (file ending .plg). 
 * To include them, rename them to file ending .h 
 * Please also note that, in order to avoid multiple definitions, if you are using more than one patch, 
 * it is necessary to rename the name of the array plugin[] and the name of PLUGIN_SIZE to names of your choice.
 * example: after renaming plugin[] to plugin_myname[] and PLUGIN_SIZE to PLUGIN_MYNAME_SIZE 
 * the method is called by loadUserCode(plugin_myname, PLUGIN_MYNAME_SIZE)
 * It is also possible to just rename the array plugin[] to a name of your choice
 * example: after renaming plugin[] to plugin_myname[]  
 * the method is called by loadUserCode(plugin_myname, sizeof(plugin_myname)/sizeof(plugin_myname[0]))
 */
void VS1053::loadUserCode(const unsigned short* plugin, unsigned short plugin_size) {
    LOG("Loading User Code");
    int i = 0;
    while (i < plugin_size) {
        unsigned short addr, n, val;
        addr = plugin[i++];
        n = plugin[i++];
        if (n & 0x8000U) { /* RLE run, replicate n samples */
            n &= 0x7FFF;
            val = plugin[i++];
            while (n--) {
                writeRegister(addr, val);
            }
        } else {           /* Copy run, copy n samples */
            while (n--) {
                val = plugin[i++];
                writeRegister(addr, val);
            }
        }
    }
    LOG("User Code - done");
}

/**
 * Load the latest generic firmware patch
 */
void VS1053::loadDefaultVs1053Patches() {
    if (getChipVersion() == 4) { // Only perform an update if we really are using a VS1053, not. eg. VS1003
        LOG("loadDefaultVs1053Patches");
        loadUserCode(PATCHES, PATCHES_SIZE);
    }
}

/// Provides the treble amplitude value
uint8_t VS1053::treble() {
    return equilizer.treble().amplitude;
}

/// Sets the treble amplitude value (range 0 to 100)
void VS1053::setTreble(uint8_t value){
    if (value>100) value = 100;
    equilizer.treble().amplitude = value;
    writeRegister(SCI_BASS, equilizer.value());
}

/// Provides the Bass amplitude value 
uint8_t VS1053::bass() {
    return equilizer.bass().amplitude;
}

/// Sets the bass amplitude value (range 0 to 100)
void VS1053::setBass(uint8_t value){
    if (value>100) value = 100;
    equilizer.bass().amplitude = value;
    writeRegister(SCI_BASS, equilizer.value());
}

/// Sets the treble frequency limit in hz (range 0 to 15000)
void VS1053::setTrebleFrequencyLimit(uint16_t value){
    equilizer.bass().freq_limit = value;
    writeRegister(SCI_BASS, equilizer.value());
}

/// Sets the bass frequency limit in hz (range 0 to 15000)
void VS1053::setBassFrequencyLimit(uint16_t value){
    equilizer.bass().freq_limit = value;
    writeRegister(SCI_BASS, equilizer.value());
}


/// Stops the recording of sound
void VS1053::end() {
    uint16_t sci = readRegister(SCI_MODE);
    // clear SM_ADPCM bit
    sci &= ~(1U << SM_ADPCM);
    writeRegister(SCI_MODE, sci);
    softReset();
}


bool VS1053::beginMIDI() {
    LOG("beginMIDI");
                        
    // initialize the player
    begin();  
    mode = VS1053_MIDI;

    await_data_request();

    switch(chip_version){
        case 3: {
            loadUserCode(MIDI1003, MIDI1003_SIZE); 
            writeRegister(0xA , 0x30);  // setting VS1003 Start adress for user code
            LOG("MIDI plugin VS1003 loaded");  
            int check = readRegister(SCI_AUDATA);
            LOG("Midi %s", check==0xac45?"active":"inactive");
            } break;   
        case 4: {
            loadUserCode(MIDI1053, MIDI1053_SIZE); 
            writeRegister(0xA , 0x50);  // setting VS1053 Start adress for user code
            LOG("MIDI plugin VS1053 loaded");  
            int check = readRegister(SCI_AUDATA);
            LOG("Midi %s", check==0xac45?"active":"inactive");
            } break;   

        default:
           LOG("Please check whether your device is properly connected!");    
           break;
    }
}

/**
 * send a MIDI message
 *
 * a MIDI message ranges from 1 byte to three bytes
 * the first byte consists of 4 command bits and 4 channel bits
 * 
 * based on talkMIDI function in MP3_Shield_RealtimeMIDI demo by Matthias Neeracher,
 * which is based on Nathan Seidle's Sparkfun Electronics example
 */
 
void VS1053::sendMidiMessage(uint8_t cmd, uint8_t data1, uint8_t data2) {
     if (mode != VS1053_MIDI){
        LOG("beginMidi not called");
        return;
     }
    int len = 4;
    uint8_t data[6] ={0x00, cmd, 0x00, data1};
    // Some commands only have one data byte. All cmds less than 0xBn have 2 data bytes 
    // (sort of: http://253.ccarh.org/handout/midiprotocol/)
    if( (cmd & 0xF0) <= 0xB0 || (cmd & 0xF0) >= 0xE0) {
      data[4]=0x00;
      data[5]=data2;
      len = 6;
    } 
    sdi_send_buffer(data, len);
}


void VS1053::writeAudio(uint8_t*data, size_t len){
     if (mode != VS1053_MIDI){
        uint8_t tmp[len*2];
        for (int j=0;j<len;j++){
            tmp[j*2] = 0;
            tmp[j+1] = data[j];
        }
        sdi_send_buffer(tmp, len*2);
     } else {
        sdi_send_buffer(data, len);
     }
}

/// Starts the recording of sound as WAV data
bool VS1053::beginInput(bool wavHeader) {
    LOG("beginInput");
    bool result = false;

    // regular setup
    begin();

    switch (chip_version){
        case 3:
            result = begin_input_vs1003(wavHeader);
            mode = VS1053_IN;
            break;

        case 4:
            result = begin_input_vs1053(wavHeader);
            mode = VS1053_IN;
            break;

        default:
            LOG("Only vs1053 supported - version %d", chip_version);
            result =false;
            break;
    }


    return result;
}


bool VS1053::begin_input_vs1053(bool wavHeader){
    LOG("%s",__func__);
    // clear SM_ADPCM bit
    uint16_t sci = readRegister(SCI_MODE);
    sci &= ~(1U << SM_ADPCM);
    writeRegister(SCI_MODE, sci);

    // set the clock to a value between just below 55.3 MHz and 67.6 MHz.
    // SC_MULT 3.5 & SC_ADD 1x (9.6.4 SCI_CLOCKF (RW))
    writeRegister(SCI_CLOCKF, SC_MULT_53_35X | SC_ADD_53_10X);

    // Set SCI_BASS (2) to 0.
    setBassFrequencyLimit(0);
    setBass(0);
    // Disable any potential user application
    writeRegister(SCI_AIADDR, 0);

    // Disable all interrupts except the SCI interrupt
    writeRegister(INT_ENABLE, 0x2); // Intenable, Enable Data interrupt
    writeRegister(SCI_WRAMADDR, 0xC01A);
    writeRegister(SCI_WRAM, 0x2);

    delay(20);

    // load plugin profile
    loadUserCode(pcm48s, 0xC01A);   

    // Set bit SM_ADPCM (12) in register SCI_MODE (0) to 1.
    sci |= 1U << SM_ADPCM;
    writeRegister(SCI_MODE, sci);

    // Set recording level control registers SCI_AICTRL1 (13) and SCI_AICTRL2 (14).
    writeRegister(SCI_AICTRL1, 0);
    writeRegister(SCI_AICTRL2, 4096);
    writeRegister(SCI_AICTRL3, 0);

    // activate encoder
    writeRegister(SCI_AIADDR, 0x34);

    if (!wavHeader){
        // remove wav header 44 bytes
        uint8_t tmp[44];
        int open = 44;
        while(open>0){
            open -= readBytes(tmp, open);
        }
    }
}

bool VS1053::begin_input_vs1003(bool wavHeader){
    LOG("%s",__func__);
//1) Load the patch using either the plugin format (vs1003b-pcm.plg)
//   or the loading tables (vs1003b-pcm.c)
    loadUserCode(pcm1003, PLUGIN_SIZE_pcm1003);   

//2) Configure the encoding normally, for example
//   CLOCKF=0x4000
//   AICTRL0=0x000c
//   AICTRL1=0x0800
//   MODE=0x1800

// sampleing rate = internal clock / 256 * divider value (SCI_AICTRL0)

    writeRegister(SCI_CLOCKF, 0x4430);
    delay(100);
    writeRegister(SCI_AICTRL0, 12); // clock divider: -> 12=8kHz 8=12kHz 6=16kHz */
    delay(100);
    writeRegister(SCI_AICTRL1, rec.recording_gain);
    delay(100);
    writeRegister(SCI_MODE, 0x1800);
    delay(100);

//3) Start the encoding mode by writing AIADDR=0x0030
    writeRegister(SCI_AIADDR, 0x0030);
}


/// Provides the number of bytes which are available in the read buffer
size_t VS1053::available() {
    if (mode!=VS1053_IN) return 0;

    size_t available = readRegister(SCI_HDAT1)*2;
    if (available>1024*2){
        LOG("Invalid value: %d", available);
        available = 0;
    }
    return available;
}

/// Provides the audio data as WAV
size_t VS1053::readBytes(uint8_t*data, size_t len){
    if (mode!=VS1053_IN) return 0;

    size_t read = min(len, available());
    size_t result = 0;
    int16_t *p_word = (int16_t*)data;
    for (int j=0; j<= read/2;j++){
        *p_word++ = readRegister(SCI_HDAT0);
        result+=2;
    }
    return result;
}



