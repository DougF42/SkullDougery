#pragma once

#include "Output.h"
#define DEPRECIATE_OLD_I2S_DONT_USE

#ifndef DEPRECIATE_OLD_I2S_DONT_USE
/**
 * Base Class for both the ADC and I2S sampler
 **/
class I2SOutput : public Output
{
private:
    i2s_pin_config_t m_i2s_pins;

public:
    I2SOutput();
    void start(uint32_t sample_rate);
};
#endif
