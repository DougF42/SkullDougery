/**
 * NewDACOutput.h
 *
 *  Created on: Oct 22, 2022
 *      Author: doug
 *
 */
// #pragma once
#include "freertos/FreeRTOS.h"
#include "esp_intr_alloc.h"
#include "DACOutput.h"

#ifndef MAIN_AUDIO_NEWDACOUTPUT_H_
#define MAIN_AUDIO_NEWDACOUTPUT_H_

#include "../audio/Output.h"

class NewDACOutput
{
public:
	NewDACOutput ();
	virtual ~NewDACOutput ();
};

#endif /* MAIN_AUDIO_NEWDACOUTPUT_H_ */
