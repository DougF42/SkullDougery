/**
 * NewDACOutput.cpp
 *
 *  Created on: Oct 22, 2022
 *      Author: doug
 *
 */

#include "NewDACOutput.h"
#include "freertos/FreeRTOS.h"
#include "esp_intr_alloc.h"
#include "DACOutput.h"

NewDACOutput::NewDACOutput (): Output(I2S_NUM_0)
{
	// TODO Auto-generated constructor stub

}

NewDACOutput::~NewDACOutput ()
{
	// TODO Auto-generated destructor stub
}

