idf_component_register(SRCS "main.cpp" "config.cpp" "SPIFFS.cpp" "SPIFFS.cpp" "PwmDriver.cpp" "Interpolate.cpp"
		"audio/DACOutput.cpp" "audio/I2SOutput.cpp" "audio/Output.cpp" "SndPlayer.cpp"
		"Sequencer/DeviceDef.cpp" "Sequencer/Message.cpp" "Sequencer/SwitchBoard.cpp"
                    INCLUDE_DIRS "")
spiffs_create_partition_image(spiff ../data FLASH_IN_PROJECT)
