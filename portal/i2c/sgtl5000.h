/* Audio Library for Teensy 3.X
* Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
*
* Development of this audio library was funded by PJRC.COM, LLC by sales of
* Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
* open source software by purchasing Teensy or other PJRC products.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice, development funding notice, and this permission
* notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#ifndef _SGTL5000_H
#define _SGTL5000_H

#include <stdbool.h>
#include <stdint.h>

#define AUDIO_INPUT_LINEIN  0
#define AUDIO_INPUT_MIC     1

//For Filter Type: 0 = LPF, 1 = HPF, 2 = BPF, 3 = NOTCH, 4 = PeakingEQ, 5 = LowShelf, 6 = HighShelf
#define FILTER_LOPASS 0
#define FILTER_HIPASS 1
#define FILTER_BANDPASS 2
#define FILTER_NOTCH 3
#define FILTER_PARAEQ 4
#define FILTER_LOSHELF 5
#define FILTER_HISHELF 6

//For frequency adjustment
#define FLAT_FREQUENCY 0
#define PARAMETRIC_EQUALIZER 1
#define TONE_CONTROLS 2
#define GRAPHIC_EQUALIZER 3

#define SGTL5000_I2C_ADDR_CS_LOW	0x0A  // CTRL_ADR0_CS pin low (normal configuration)
#define SGTL5000_I2C_ADDR_CS_HIGH	0x2A // CTRL_ADR0_CS  pin high

void sgtl5000_init(char * handle_name, uint8_t level);
bool sgtl5000_enable(void);
bool sgtl5000_volume(float n);
bool sgtl5000_inputLevel(float n);
bool sgtl5000_muteHeadphone(void);
bool sgtl5000_unmuteHeadphone(void);
bool sgtl5000_muteLineout(void);
bool sgtl5000_unmuteLineout(void);
bool sgtl5000_inputSelect(int n);
bool sgtl5000_volume_stereo(float left, float right);
bool sgtl5000_micGain(uint16_t dB);
bool sgtl5000_lineInLevel_stereo(uint8_t left, uint8_t right);
bool sgtl5000_lineInLevel(uint8_t n);
unsigned short sgtl5000_lineOutLevel(uint8_t n);
unsigned short sgtl5000_lineOutLevel_stereo(uint8_t left, uint8_t right);
unsigned short sgtl5000_dacVolume(float n);
unsigned short sgtl5000_dacVolume_stereo(float left, float right);
bool sgtl5000_dacVolumeRamp();
bool sgtl5000_dacVolumeRampLinear();
bool sgtl5000_dacVolumeRampDisable();
unsigned short sgtl5000_adcHighPassFilterEnable(void);
unsigned short sgtl5000_adcHighPassFilterFreeze(void);
unsigned short sgtl5000_adcHighPassFilterDisable(void);
unsigned short sgtl5000_audioPreProcessorEnable(void);
unsigned short sgtl5000_audioPostProcessorEnable(void);
unsigned short sgtl5000_audioProcessorDisable(void);
unsigned short sgtl5000_eqFilterCount(uint8_t n);
unsigned short sgtl5000_eqSelect(uint8_t n);
unsigned short sgtl5000_eqBand(uint8_t bandNum, float n);
void sgtl5000_eqBands_5(float bass, float mid_bass, float midrange, float mid_treble, float treble);
void sgtl5000_eqBands_2(float bass, float treble);
void sgtl5000_eqFilter(uint8_t filterNum, int *filterParameters);
unsigned short sgtl5000_autoVolumeControl(uint8_t maxGain, uint8_t lbiResponse, uint8_t hardLimit, float threshold, float attack, float decay);
unsigned short sgtl5000_autoVolumeEnable(void);
unsigned short sgtl5000_autoVolumeDisable(void);
unsigned short sgtl5000_enhanceBass(float lr_lev, float bass_lev);
unsigned short sgtl5000_enhanceBass_adv(float lr_lev, float bass_lev, uint8_t hpf_bypass, uint8_t cutoff);
unsigned short sgtl5000_enhanceBassEnable(void);
unsigned short sgtl5000_enhanceBassDisable(void);
unsigned short sgtl5000_surroundSound(uint8_t width);
unsigned short sgtl5000_surroundSound_adv(uint8_t width, uint8_t select);
unsigned short sgtl5000_surroundSoundEnable(void);
unsigned short sgtl5000_surroundSoundDisable(void);
void sgtl5000_killAutomation(void);
unsigned short sgtl5000_dapMix(float main, float mix);
void sgtl5000_calcBiquad(uint8_t filtertype, float fC, float dB_Gain, float Q, uint32_t quantization_unit, uint32_t fS, int *coef);

#endif
