#pragma once
#include "stdint.h"

/** @file */

namespace arduino_vs1053 {


/// Input from Aux or Microphone
enum VS1053_INPUT {
    VS1053_MIC = 0,
    VS1053_AUX = 1,
};

/**
 * @brief Relevant control data for recording audio from the vs1053
 * @author pschatzmann
 */
struct VS1053Recording {
    friend class VS1053;
    public:
        // values from 8000 to 48000
        void setSampleRate(uint16_t rate){
            sample_rate = rate;
            if (sample_rate>48000) sample_rate = 48000;
            if (sample_rate<8000) sample_rate = 8000;
        }

        uint16_t sampleRate() {
            return sample_rate;
        }

        void setChannels(uint8_t ch){
            channels_v = ch;
        }
        
        uint8_t channels() {
            return channels_v;
        }
        
        // values from 0 to 100
        void setRecordingGain(uint8_t gain){
            recording_gain = 1024 * gain / 100;
            if (recording_gain>1024) recording_gain = 1024;
            if (recording_gain<0) recording_gain = 0; // 0 = automatic gain control
        }

        uint8_t recordingGain() {
            return recording_gain;
        }

        // values from 0 to 100
        void setAutoGainAmplification(uint8_t amp){
            autogain_amplification = 65535 * amp / 100 ;
            if (autogain_amplification>65535) autogain_amplification = 65535;
            if (autogain_amplification<0) autogain_amplification = 0;
        }

        uint8_t autoGainAmplification() {
            return autogain_amplification;
        }

        void setInput(VS1053_INPUT in){
            input = in;
        }  

protected:
    uint16_t sample_rate = 8000;
    uint8_t channels_v = 1;
    uint16_t recording_gain = 0; // 
    uint16_t autogain_amplification = 0; // 
    VS1053_INPUT input = VS1053_MIC;
};   

}