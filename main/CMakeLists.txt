
idf_component_register(SRCS "main.cpp" "SPIFFS.cpp" "PwmDriver.cpp" "Interpolate.cpp"
		"audio/I2SOutput2.cpp"  "audio/SndPlayer.cpp"
		"MotionSequencer.cpp"
		"Sequencer/DeviceDef.cpp" "Sequencer/Message.cpp" "Sequencer/SwitchBoard.cpp"
		"Network/WiFiHub.cpp" "Network/UDPServer.cpp" "CmdDecoder.cpp" "Parameters/RmNvs.cpp"
		"Stepper/Arduino.cpp" "Stepper/StepperMotorController.cpp" "Stepper/StepperDriver.cpp"
		 
                    INCLUDE_DIRS "")
                    
spiffs_create_partition_image(spiff ../data FLASH_IN_PROJECT)
