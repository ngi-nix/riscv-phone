#include <stdlib.h>

#include <esp_log.h>

#include "eos.h"
#include "i2c.h"

static const char *TAG = "EOS DRV2605L";

#define DRV2605L_ADDR                0x5A

#define DRV2605_REG_STATUS 0x00       ///< Status register
#define DRV2605_REG_MODE 0x01         ///< Mode register
#define DRV2605_MODE_INTTRIG 0x00     ///< Internal trigger mode
#define DRV2605_MODE_EXTTRIGEDGE 0x01 ///< External edge trigger mode
#define DRV2605_MODE_EXTTRIGLVL 0x02  ///< External level trigger mode
#define DRV2605_MODE_PWMANALOG 0x03   ///< PWM/Analog input mode
#define DRV2605_MODE_AUDIOVIBE 0x04   ///< Audio-to-vibe mode
#define DRV2605_MODE_REALTIME 0x05    ///< Real-time playback (RTP) mode
#define DRV2605_MODE_DIAGNOS 0x06     ///< Diagnostics mode
#define DRV2605_MODE_AUTOCAL 0x07     ///< Auto calibration mode

#define DRV2605_REG_RTPIN 0x02        ///< Real-time playback input register
#define DRV2605_REG_LIBRARY 0x03      ///< Waveform library selection register
#define DRV2605_REG_WAVESEQ1 0x04     ///< Waveform sequence register 1
#define DRV2605_REG_WAVESEQ2 0x05     ///< Waveform sequence register 2
#define DRV2605_REG_WAVESEQ3 0x06     ///< Waveform sequence register 3
#define DRV2605_REG_WAVESEQ4 0x07     ///< Waveform sequence register 4
#define DRV2605_REG_WAVESEQ5 0x08     ///< Waveform sequence register 5
#define DRV2605_REG_WAVESEQ6 0x09     ///< Waveform sequence register 6
#define DRV2605_REG_WAVESEQ7 0x0A     ///< Waveform sequence register 7
#define DRV2605_REG_WAVESEQ8 0x0B     ///< Waveform sequence register 8

#define DRV2605_REG_GO 0x0C           ///< Go register
#define DRV2605_REG_OVERDRIVE 0x0D    ///< Overdrive time offset register
#define DRV2605_REG_SUSTAINPOS 0x0E   ///< Sustain time offset, positive register
#define DRV2605_REG_SUSTAINNEG 0x0F   ///< Sustain time offset, negative register
#define DRV2605_REG_BREAK 0x10        ///< Brake time offset register
#define DRV2605_REG_AUDIOCTRL 0x11    ///< Audio-to-vibe control register
#define DRV2605_REG_AUDIOLVL 0x12     ///< Audio-to-vibe minimum input level register
#define DRV2605_REG_AUDIOMAX 0x13     ///< Audio-to-vibe maximum input level register
#define DRV2605_REG_AUDIOOUTMIN 0x14  ///< Audio-to-vibe minimum output drive register
#define DRV2605_REG_AUDIOOUTMAX 0x15  ///< Audio-to-vibe maximum output drive register
#define DRV2605_REG_RATEDV 0x16       ///< Rated voltage register
#define DRV2605_REG_CLAMPV 0x17       ///< Overdrive clamp voltage register
#define DRV2605_REG_AUTOCALCOMP 0x18  ///< Auto-calibration compensation result register
#define DRV2605_REG_AUTOCALEMP 0x19   ///< Auto-calibration back-EMF result register
#define DRV2605_REG_FEEDBACK 0x1A     ///< Feedback control register
#define DRV2605_REG_CONTROL1 0x1B     ///< Control1 Register
#define DRV2605_REG_CONTROL2 0x1C     ///< Control2 Register
#define DRV2605_REG_CONTROL3 0x1D     ///< Control3 Register
#define DRV2605_REG_CONTROL4 0x1E     ///< Control4 Register
#define DRV2605_REG_VBAT 0x21         ///< Vbat voltage-monitor register
#define DRV2605_REG_LRARESON 0x22     ///< LRA resonance-period register

void eos_drv2605l_test(void) {
    uint8_t data = 0;

    int ret = eos_i2c_read(DRV2605L_ADDR, DRV2605_REG_STATUS, &data, 1);
    if (ret) ESP_LOGE(TAG, "I2C ERROR!");

    eos_i2c_write8(DRV2605L_ADDR, DRV2605_REG_MODE, 0x00);      // out of standby
    eos_i2c_write8(DRV2605L_ADDR, DRV2605_REG_RTPIN, 0x00);     // no real-time-playback

    eos_i2c_write8(DRV2605L_ADDR, DRV2605_REG_WAVESEQ1, 1);     // strong click
    eos_i2c_write8(DRV2605L_ADDR, DRV2605_REG_WAVESEQ2, 0);     // end sequence

    eos_i2c_write8(DRV2605L_ADDR, DRV2605_REG_OVERDRIVE, 0);    // no overdrive

    eos_i2c_write8(DRV2605L_ADDR, DRV2605_REG_SUSTAINPOS, 0);
    eos_i2c_write8(DRV2605L_ADDR, DRV2605_REG_SUSTAINNEG, 0);
    eos_i2c_write8(DRV2605L_ADDR, DRV2605_REG_BREAK, 0);
    eos_i2c_write8(DRV2605L_ADDR, DRV2605_REG_AUDIOMAX, 0x64);

    // LRA open loop
    eos_i2c_write8(DRV2605L_ADDR, DRV2605_REG_FEEDBACK, eos_i2c_read8(DRV2605L_ADDR, DRV2605_REG_FEEDBACK) | 0x80);     // turn on N_ERM_LRA
    eos_i2c_write8(DRV2605L_ADDR, DRV2605_REG_CONTROL3, eos_i2c_read8(DRV2605L_ADDR, DRV2605_REG_CONTROL3) | 0x01);     // turn on LRA_OPEN_LOOP

    eos_i2c_write8(DRV2605L_ADDR, DRV2605_REG_LIBRARY, 6);      // set LRA library
    eos_i2c_write8(DRV2605L_ADDR, DRV2605_REG_GO, 1);           // go

}
