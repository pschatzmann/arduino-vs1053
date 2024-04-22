#include "VS1053Driver.h"

namespace arduino_vs1053 {


VS1053::VS1053(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin, uint8_t _reset_pin, VS1053_SPI *_p_spi)
        : cs_pin(_cs_pin), dcs_pin(_dcs_pin), dreq_pin(_dreq_pin), reset_pin(_reset_pin), p_spi(_p_spi) {

    if (p_spi==nullptr){
// if spi parameter is undifined, we use the system specific default drivers
#if USE_ESP_SPI_CUSTOM && (defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266))
        static VS1053_SPIESP32 spi;
        p_spi = &spi;
#elif defined(ARDUINO)
        static VS1053_SPIArduino spi;
        p_spi = &spi;
#endif

    }
}

#ifdef ARDUINO

VS1053::VS1053(uint8_t _cs_pin, uint8_t _dcs_pin, uint8_t _dreq_pin, uint8_t _reset_pin, SPIClass &spi)
        : cs_pin(_cs_pin), dcs_pin(_dcs_pin), dreq_pin(_dreq_pin), reset_pin(_reset_pin) {
    static VS1053_SPIArduino vs_spi(spi);
    p_spi = &vs_spi;
}

#endif


uint16_t VS1053::readRegister(uint8_t _reg) const {
    uint16_t result;

    control_mode_on();
    p_spi->write(3);    // Read operation
    p_spi->write(_reg); // Register to write (0..0xF)
    // Note: transfer16 does not seem to work
    result = (p_spi->transfer(0xFF) << 8) | // Read 16 bits data
             (p_spi->transfer(0xFF));
    await_data_request(); // Wait for DREQ to be HIGH again
    control_mode_off();
    return result;
}

void VS1053::writeRegister(uint8_t _reg, uint16_t _value) const {
    control_mode_on();
    p_spi->write(2);        // Write operation
    p_spi->write(_reg);     // Register to write (0..0xF)
    p_spi->write16(_value); // Send 16 bits data
    await_data_request();
    control_mode_off();
}

void VS1053::set_flag(uint16_t &reg_value, uint16_t flag, bool active){
    if (active){
        reg_value |= flag; // setting bit
    } else {
        reg_value &= ~(flag);  // clearing bit 0x1800
    }
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
        p_spi->write_bytes(data, chunk_length);
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
            p_spi->write(endFillByte);
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
    // Test the communication with the VS1053 module.  The result will be returned.
    // If DREQ is low, there is problably no VS1053 connected.  Pull the line HIGH
    // in order to prevent an endless loop waiting for this signal.  The rest of the
    // software will still work, but readbacks from VS1053 will fail.
    int i; // Loop control
    uint16_t r1, r2, cnt = 0;
    uint16_t delta = 300; // 3 for fast SPI
    delay(100);

    if (!digitalRead(dreq_pin)) {
        VS1053_LOGW("VS1053 not properly installed!");
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

    VS1053_LOGD("%s", header);  // Show a header

    for (i = 0; (i < 0xFFFF) && (cnt < 20); i += delta) {
        writeRegister(SCI_VOL, i);         // Write data to SCI_VOL
        r1 = readRegister(SCI_VOL);        // Read back for the first time
        r2 = readRegister(SCI_VOL);        // Read back a second time
        if (r1 != r2 || i != r1 || i != r2) // Check for 2 equal reads
        {
            VS1053_LOGW("VS1053 error retry SB:%04X R1:%04X R2:%04X", i, r1, r2);
            cnt++;
            delay(10);
        }
        yield(); // Allow ESP firmware to do some bookkeeping
    }
    return (cnt == 0); // Return the result
}

bool VS1053::begin() {
    VS1053_LOGD("begin");
    bool result = false;
    // support for optional custom reset pin when wiring is not possible
    if (reset_pin!=-1){
        pinMode(reset_pin, OUTPUT);
        digitalWrite(reset_pin, HIGH);
        delay(500);
    }

    pinMode(dreq_pin, INPUT); // DREQ is an input
    pinMode(cs_pin, OUTPUT);  // The SCI and SDI signals
    pinMode(dcs_pin, OUTPUT);
    digitalWrite(dcs_pin, HIGH); // Start HIGH for SCI en SDI
    digitalWrite(cs_pin, HIGH);
    delay(100);
    VS1053_LOGI("Reset...");
    digitalWrite(dcs_pin, LOW); // Low & Low will bring reset pin low
    digitalWrite(cs_pin, LOW);
    delay(500);
    VS1053_LOGI("End reset...");
    digitalWrite(dcs_pin, HIGH); // Back to normal again
    digitalWrite(cs_pin, HIGH);
    delay(500);
    // Init SPI in slow mode ( 0.2 MHz )
    p_spi->set_speed(200000);
    // printDetails("Right after reset/startup");
    delay(20);
    // printDetails("20 msec after reset");
    if (testComm("Slow SPI,Testing read/write registers...")) {
        //softReset();
        result = true;
        // Switch on the anaVS1053_LOGD parts
        writeRegister(SCI_AUDATA, 44101); // 44.1kHz stereo
        // The next clocksetting allows SPI clocking at 5 MHz, 4 MHz is safe then.
        writeRegister(SCI_CLOCKF, 6 << 12); // Normal clock settings multiplyer 3.0 = 12.2 MHz
        // SPI Clock to 4 MHz. Now you can set high speed SPI clock.
        p_spi->set_speed(4000000);
        writeRegister(SCI_MODE, _BV(SM_SDINEW) | _BV(SM_LINE1));
        testComm("Fast SPI, Testing read/write registers again...");
        delay(10);
        await_data_request();
        endFillByte = wram_read(0x1E06) & 0xFF;
        VS1053_LOGD("endFillByte is %X", endFillByte);
        //printDetails("After last clocksetting") ;
        delay(100);
    } 
    mode = VS1053_NA; // default mode

    // set and print chip version
    chip_version = getChipVersion();
    switch(chip_version){
        case 3: {
          const char* chip = "VS1003";
          VS1053_LOGI("%s (%d)",chip ,chip_version);  
        } break;   
        case 4: {
          const char* chip = "VS1053";
          VS1053_LOGI("%s (%d)",chip ,chip_version);  
        } break; 
        default:
          const char* chip = "Unsuppored chip";
          VS1053_LOGI("%s: (%d)",chip, chip_version);    
          result = false;
          break;
    }
    return result;
}
    
bool VS1053::beginOutput(){
    mode = VS1053_OUT;
    begin();
    startSong();
    switchToMp3Mode(); // optional, some boards require this    
    if (chip_version == 4) { // Only perform an update if we really are using a VS1053, not. eg. VS1003
        loadDefaultVs1053Patches(); 
    }
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
    VS1053_LOGI("setVolume: %x", value);
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
    //sdi_send_buffer(data, len);
    writeAudio(data, len);
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
            VS1053_LOGI("Song stopped correctly after %d msec", i * 10);
            return;
        }
        delay(10);
    }
    printDetails("Song stopped incorrectly!");
}

void VS1053::softReset() {
    VS1053_LOGI("Performing soft-reset");
    writeRegister(SCI_MODE, _BV(SM_SDINEW) | _BV(SM_RESET));
    delay(10);
    await_data_request();
}

void VS1053::hardReset(){
    if (reset_pin!=-1){
        VS1053_LOGI("Performing hard-reset");
        digitalWrite(reset_pin, LOW);
        delay(500);
        digitalWrite(reset_pin, HIGH);
    } else {
        VS1053_LOGE("hard-reset only supported when reset_pin is defined");
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
    VS1053_LOGI("Performing streamModeOn");
    writeRegister(SCI_MODE, _BV(SM_SDINEW) | _BV(SM_STREAM));
    delay(10);
    await_data_request();
}

void VS1053::streamModeOff() {
    VS1053_LOGI("Performing streamModeOff");
    writeRegister(SCI_MODE, _BV(SM_SDINEW));
    delay(10);
    await_data_request();
}

void VS1053::printDetails(const char *header) {
    uint16_t regbuf[16];
    uint8_t i;

    VS1053_LOGI("%s", header);
    VS1053_LOGI("REG   Contents");
    VS1053_LOGI("---   -----");
    for (i = 0; i <= SCI_num_registers; i++) {
        regbuf[i] = readRegister(i);
    }
    for (i = 0; i <= SCI_num_registers; i++) {
        delay(5);
        VS1053_LOGI("%3X - %5X", i, regbuf[i]);
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
    VS1053_LOGI("Switched to mp3 mode");
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
    VS1053_LOGI("Loading User Code");
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
    VS1053_LOGD("User Code - done");
}

/**
 * Load the latest generic firmware patch
 */
bool VS1053::loadDefaultVs1053Patches() {
#if USE_PATCHES
    if (getChipVersion() == 4) { // Only perform an update if we really are using a VS1053, not. eg. VS1003
        VS1053_LOGD("loadDefaultVs1053Patches");
        loadUserCode(PATCHES, PATCHES_SIZE);
        return true;
    }
#endif
    return false;
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

bool VS1053::setEarSpeaker(VS1053_EARSPEAKER value){
    if (getChipVersion()!=4){
        VS1053_LOGE("Function not supported");
        return false;
    }
    int16_t mode = readRegister(SCI_MODE);
    switch(value){
        case VS1053_EARSPEAKER_MAX:
            writeRegister(SCI_MODE, mode | (SC_EAR_SPEAKER_HI | SC_EAR_SPEAKER_LO)); // extreme 3 - on on
            break;
        case VS1053_EARSPEAKER_ON:
            writeRegister(SCI_MODE, (mode | SC_EAR_SPEAKER_HI) & (~SC_EAR_SPEAKER_LO)); // normal 2 - off on
            break;
        case VS1053_EARSPEAKER_MIN:
            writeRegister(SCI_MODE, (mode | SC_EAR_SPEAKER_LO) & (~SC_EAR_SPEAKER_HI)); // minimal 1 - on off
            break;
        case VS1053_EARSPEAKER_OFF:
            writeRegister(SCI_MODE, (mode & (~SC_EAR_SPEAKER_HI)) & (~SC_EAR_SPEAKER_LO)); // off 0 - off off
            break;
    }
    return true;
}


/// Stops the recording of sound
void VS1053::end() {
    // clear SM_ADPCM bit
    uint16_t mode = readRegister(SCI_MODE);
    set_flag(mode, 1<<SM_ADPCM, false); // stop recoring
    writeRegister(SCI_MODE, mode);
    softReset();
}

#if USE_MIDI

bool VS1053::beginMidi() {
    VS1053_LOGI("beginMIDI");
    bool result = false;           
    // initialize the player
    begin();  

    await_data_request();

    switch(chip_version){
        case 3: {
            loadUserCode(MIDI1003, MIDI1003_SIZE); 
            writeRegister(0xA , 0x30);  // setting VS1003 Start adress for user code
            VS1053_LOGD("MIDI plugin VS1003 loaded");  
        } break;   
        case 4: {
            loadUserCode(MIDI1053, MIDI1053_SIZE); 
            writeRegister(0xA , 0x50);  // setting VS1053 Start adress for user code
            VS1053_LOGD("MIDI plugin VS1053 loaded");  
        } break; 
        default:
           VS1053_LOGE("Please check whether your device is properly connected!");    
           break;
    }

    delay(500);
    // check if midi is active
    int check = readRegister(SCI_AUDATA);
    if (check==0xac45){
        mode = VS1053_MIDI;
        result = true;
    }
    VS1053_LOGI("Midi %s", result ? "active":"inactive");
    return result;
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
        VS1053_LOGE("beginMidi not called");
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

#endif

void VS1053::writeAudio(uint8_t*data, size_t len){
     if (mode == VS1053_MIDI){
        uint8_t tmp[len*2];
        for (int j=0;j<len;j++){
            tmp[j*2] = 0;
            tmp[j*2+1] = data[j];
        }
        sdi_send_buffer(tmp, len*2);
     } else {
        sdi_send_buffer(data, len);
     }
}

/// Starts the recording of sound as WAV data
bool VS1053::beginInput(VS1053Recording &opt) {
    VS1053_LOGI("beginInput");
    bool result = false;

    // regular setup
    begin();

    switch (chip_version){
        case 3:
            result = begin_input_vs1003(opt);
            mode = VS1053_IN;
            break;

        case 4:
            result = begin_input_vs1053(opt);
            mode = VS1053_IN;
            break;

        default:
            VS1053_LOGE("Only vs1053 supported - version %d", chip_version);
            result =false;
            break;
    }
    return result;
}


bool VS1053::begin_input_vs1053(VS1053Recording &opt){
#ifdef USE_INPUT
    VS1053_LOGD("%s",__func__);
    // clear SM_ADPCM bit
    writeRegister(SCI_AICTRL0, opt.sampleRate());
    writeRegister(SCI_AICTRL1, opt.recordingGain());
    writeRegister(SCI_AICTRL2, opt.autoGainAmplification());

    // setup SCI_AICTRL3
    uint16_t ctrl3=0;
    if (opt.channels()==2){
        ctrl3 = 0; // joint stereo 
    } else {
        ctrl3 = opt.input==VS1053_AUX ? 3 : 2;  // select left or right channel
    }
    set_flag(ctrl3, 1<<2, 1); // Linear PCM Mode
    writeRegister(SCI_AICTRL3, ctrl3); 

    uint16_t mode = readRegister(SCI_MODE);
    set_flag(mode, 1<<SM_ADPCM, true); // activate pcm mode
    set_flag(mode, 1<<SM_RESET, true);
    set_flag(mode, 1<<SM_LINE1, opt.input==VS1053_AUX);

    writeRegister(SCI_MODE, mode);

    loadUserCode(pcm1053, PLUGIN_SIZE_pcm1053);   

    return true;
#else
    return false;
#endif
}

bool VS1053::begin_input_vs1003(VS1053Recording &opt){
    VS1053_LOGD("%s",__func__);
    // we might need to repeat some values per channel
    channels_multiplier = opt.channels();

//1) Load the patch using either the plugin format (vs1003b-pcm.plg)
//   or the loading tables (vs1003b-pcm.c)
    loadUserCode(pcm1003, PLUGIN_SIZE_pcm1003);   
    delay(100);

//2) Configure the encoding normally, for example
//   CLOCKF=0x4000
//   AICTRL0=0x000c
//   AICTRL1=0x0800
//   MODE=0x1800

    VS1003Clock calc;  // clock calculator
    int sample_rate_calc = calc.setSampleRate(opt.sample_rate);
    if (sample_rate_calc<0){
        VS1053_LOGD("Could not set sample rate");
        return false;
    }
    int16_t clock_freq = readRegister(SCI_CLOCKF) & 0x3FF;
    int16_t sci_clockf = clock_freq | calc.getMultiplierRegisterValue() ;
    VS1053_LOGD("clock_freq: %x", clock_freq);
    VS1053_LOGD("multipler: %x -  %f", calc.getMultiplierRegisterValue(), calc.getMultiplier());
    VS1053_LOGD("SCI_CLOCKF: %x", sci_clockf);
    VS1053_LOGD("divider: %d", calc.getDivider());
    VS1053_LOGD("sample_rate: %d", opt.sample_rate);
    VS1053_LOGD("sample_rate eff: %d", sample_rate_calc);

    writeRegister(SCI_CLOCKF, sci_clockf ); // e.g. 0x4430
    delay(100);
    writeRegister(SCI_AICTRL0, calc.getDivider()); // e.g. 12 / clock divider: -> 12=8kHz 8=12kHz 6=16kHz 
    delay(100);
    writeRegister(SCI_AICTRL1, opt.recording_gain);
    delay(100);

    // setting mic or aux as input
    uint16_t sci_mode = readRegister(SCI_MODE);
    set_flag(sci_mode, 1<<SM_ADPCM, true);
    set_flag(sci_mode, 1<<SM_LINE1, opt.input==VS1053_AUX);
    writeRegister(SCI_MODE, sci_mode);
    delay(100);

//3) Start the encoding mode by writing AIADDR=0x0030
    writeRegister(SCI_AIADDR, 0x0030);
    delay(100);

    // inform api about used sample rate
    opt.sample_rate = sample_rate_calc;
    return true;
}


/// Provides the number of bytes which are available in the read buffer
size_t VS1053::available() {
    if (mode!=VS1053_IN) return 0;

    size_t available = readRegister(SCI_HDAT1)*2;
    if (available>1024*2){
        //VS1053_LOGD("Invalid value: %d", available);
        available = 0; //1024*2;
    }
    return available * channels_multiplier;
}

/// Provides the audio data as PCM data
size_t VS1053::readBytes(uint8_t*data, size_t len){
    if (mode!=VS1053_IN) return 0;

    size_t read = min(len, available());
    size_t result = 0;
    int16_t *p_word = (int16_t*)data;
    size_t max_samples = read / 2 / channels_multiplier;
    for (int j=0; j<= max_samples;j++){
        int16_t tmp = readRegister(SCI_HDAT0);
        for (int j=0;j<channels_multiplier;j++){
            *p_word++ = tmp;
            result+=2;
        }
    }
    return result;
}

}


