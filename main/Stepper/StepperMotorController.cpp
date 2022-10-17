//==================================================================================================================
//
//  FILE   : StepperMotorController.cpp
//
//  NOTES  : Stepper Motor Controller, Version 2022-1-12
//           This firmware is used to operate a single stepper driver/motor from an Arduino board.
//           (see the header file for details)
//
//  AUTHOR : Bill Daniels
//           Copyright (c) 2013-2022, D+S Tech Labs Incorporated
//           All Rights Reserved
//
//==================================================================================================================

#include "Arduino.h"
#include "esp_log.h"
#include "string.h"
#include "StepperMotorController.h"
#include "climits"
#include "freertos/FreeRTOS.h"

//--- Globals ----------------------------------------------

const char * StepperMotorController::Version =
  "Stepper Motor Controller 2022-1-12\nCopyright 2013-2022, D+S Tech Labs, Inc.\nAll Rights Reserved.";


//==========================================================
//  Constructor
//==========================================================
StepperMotorController::StepperMotorController(DriverTypes driverType, int pin1, int pin2, int pin3, int pin4, int ledPin)
{
  DriverType = driverType;
  
  // Save the specified pins for the both driver types
  EnablePin    = pin1;
  DirectionPin = pin2;
  StepPin      = pin3;
  motor_pin_1  = pin1;
  motor_pin_2  = pin2;
  motor_pin_3  = pin3;
  motor_pin_4  = pin4;
  LEDPin       = ledPin;

  // Set all pin modes as DIGITAL OUTPUT's
  pinMode(pin1  , OUTPUT);
  pinMode(pin2  , OUTPUT);
  pinMode(pin3  , OUTPUT);
  if ((driverType==UNIPOLAR) || (driverType==BIPOLAR))  pinMode(pin4  , OUTPUT);
  if (ledPin != STEPPER_NO_LED) pinMode(LEDPin, OUTPUT);

  // Initialize pins
  digitalWrite(pin1, LOW);
  digitalWrite(pin2, LOW);
  digitalWrite(pin3, LOW);
  if ((driverType==UNIPOLAR) || (driverType==BIPOLAR))  digitalWrite(pin4, LOW);
  if (ledPin != STEPPER_NO_LED) digitalWrite(LEDPin, LOW);

  // If DIGITAL, start with motor disabled
  if (DriverType == DIGITAL)
    digitalWrite(EnablePin, HIGH);

	// Set motor state and step position/timing
  Homed             = false;
  MotorState        = DISABLED;
  StepIncrement     = 1L;
  AbsolutePosition  = 0L;
  DeltaPosition     = 0L;
  TargetPosition    = 0L;
  LowerLimit        = -2000000000L;
  UpperLimit        =  2000000000L;
  RampSteps         = 0L;
  RampDownStep      = 0L;
  Velocity          = 0L;
  VelocityIncrement = RampScale * 5L;  // Default ramp scale of 5
  NextPosition      = 0L;
  NextStepMicros    = -1L;
  RunReturn         = RUN_OK;

  MaxVelocity       = 0;
  TargetOrSteps     = 0;
  TotalSteps        = 0;
  StepCount         = 0;

}

/**
 * @brief Dump all class variables to LOGD.
 * DEF: Remove before flight!
 */
const static char *DTAG="STEPPER:";

void StepperMotorController::Dump()
{
	ESP_LOGD("- - -","- - -");
	// TIME:
	ESP_LOGD(DTAG, "NOW=%ld   extStepMicros=%ld  NextStep-NOW=%ld ", micros(), NextStepMicros, NextStepMicros-micros());

	// POSITION
	ESP_LOGD(DTAG, "POSITION: AbsPos=%ld  DeltaPos=%ld  Target=%ld ",
			AbsolutePosition, DeltaPosition, TargetPosition);

	ESP_LOGD(DTAG,"LIMITS: LowerLimit=%ld  UpperLimit=%ld", LowerLimit, UpperLimit);

	LowerLimit        = -2000000000L;
	UpperLimit        =  2000000000L;

	  // VELOCITY
	ESP_LOGD(DTAG, "VELOCITY1: MaxVel=%ld  Velocity=%ld  RampSteps=%ld RampDownStep=%ld",
			MaxVelocity, Velocity, RampSteps, RampDownStep);

	ESP_LOGD(DTAG, "VELOCITY2: StepIncrement %ld VelocityIncr: %ld ",
			StepIncrement, VelocityIncrement);

	ESP_LOGD(DTAG, "XXX: TargetOrSteps %ld    TotalSteps %ld  StepCount %ld",
	  TargetOrSteps ,	  TotalSteps ,	  StepCount);


	// OTHER
	ESP_LOGD(DTAG, "Homed=%d  MotorState=%d RunReturn=%d",
			Homed, static_cast<int>(MotorState), static_cast<int>(RunReturn));
	ESP_LOGD("- - -","- - -");
}
//=========================================================
// GetTimeToNextStep
//    This is used to determine how long before we need
// to call 'Run' again.
//
// @return 0 if not running. Time to next event otherwise
//=========================================================
unsigned long  StepperMotorController::GetTimeToNextStep   () {
	return (NextStepMicros);
}

//=========================================================
//  Run:
//  Must be continuously called inside your loop function
//  There should be no delay between calls
//
//  Return values:
//    "RCp" - Rotate Complete where p is the Absolute Position
//    "RE"  - Range Error (tried to pass a limit)
//    ""    - Empty string (sitting idle or still running)
//=========================================================
RunReturn_t StepperMotorController::Run ()
{
	RunReturn = RUN_OK;

	// If motor is RUNNING and it's time for it to step, then issue step pulse
	if (Homed && MotorState == RUNNING)
	{
		// Is it time to step?
		if (micros () >= NextStepMicros)
		{
			// Check range limits
			NextPosition = AbsolutePosition + StepIncrement; // +1 for clockwise rotations, -1 for counter-clockwise
			if (NextPosition < LowerLimit || NextPosition > UpperLimit)
			{
				ESP_LOGD("DRIVER:","OUT-OF-RANGE");
				// Next step will be out of range,
				// So stop motor and return range error
				MotorState = ENABLED;
				RunReturn = RUN_RANGE_ERROR;  // Range Error
			}
			else  // Perform single step
			{
				if (DriverType == DIGITAL)
				{
					//=============================================================================
					// For Digital Stepper Drivers:
					//   Turn on the Step (Pulse) pin for a short period (pulse width)
					//   Perform FAST step pulse using direct port register
					//=============================================================================

					// PORTD |= B00010000;   // Turn on Pulse pin (Arduino NANO Digital Pin 4)
					// delayMicroseconds (PULSE_WIDTH);
					// PORTD &= ~B00010000;  // Turn off Pulse pin

					digitalWrite (StepPin, HIGH );
					delayMicroseconds (PULSE_WIDTH );
					digitalWrite (StepPin, LOW );
				}

				else
				//=============================================================================
				// For BIPOLAR Stepper Drivers:
				//   Each of the four coils must be turned on/off in sequence
				//=============================================================================
				if (DriverType == BIPOLAR)
				{
					switch (NextPosition % 4)
					{
						case 0:  // 1010
							digitalWrite (motor_pin_1, HIGH );
							digitalWrite (motor_pin_2, LOW );
							digitalWrite (motor_pin_3, HIGH );
							digitalWrite (motor_pin_4, LOW );
							break;
							
						case 1:  // 0110
						case -3:
							digitalWrite (motor_pin_1, LOW );
							digitalWrite (motor_pin_2, HIGH );
							digitalWrite (motor_pin_3, HIGH );
							digitalWrite (motor_pin_4, LOW );
							break;
							
						case 2:  // 0101
						case -2:
							digitalWrite (motor_pin_1, LOW );
							digitalWrite (motor_pin_2, HIGH );
							digitalWrite (motor_pin_3, LOW );
							digitalWrite (motor_pin_4, HIGH );
							break;
							
						case 3:  // 1001
						case -1:
							digitalWrite (motor_pin_1, HIGH );
							digitalWrite (motor_pin_2, LOW );
							digitalWrite (motor_pin_3, LOW );
							digitalWrite (motor_pin_4, HIGH );
							break;
					}
				}

				else
				{
					//=============================================================================
					// For UNIPOLAR Stepper Drivers:
					//   Each of the four coils must be turned on/off in sequence
					//=============================================================================
					if (DriverType == UNIPOLAR)
					{
						switch (NextPosition % 8)
						// For 4-wire stepper motors
						{
							case (0): //1000
								digitalWrite (motor_pin_1, HIGH );
								digitalWrite (motor_pin_2, LOW );
								digitalWrite (motor_pin_3, LOW );
								digitalWrite (motor_pin_4, LOW );
								break;

							case (1): //1100
							case(-7):
								digitalWrite (motor_pin_1, HIGH );
								digitalWrite (motor_pin_2, HIGH );
								digitalWrite (motor_pin_3, LOW );
								digitalWrite (motor_pin_4, LOW );
								break;

							case (2): //0100
							case(-6):
								digitalWrite (motor_pin_1, LOW );
								digitalWrite (motor_pin_2, HIGH );
								digitalWrite (motor_pin_3, LOW );
								digitalWrite (motor_pin_4, LOW );
								break;

							case (3): //0110
							case (-5):
								digitalWrite (motor_pin_1, LOW );
								digitalWrite (motor_pin_2, HIGH );
								digitalWrite (motor_pin_3, HIGH );
								digitalWrite (motor_pin_4, LOW );
								break;

							case (4): //0010
							case (-4):
								digitalWrite (motor_pin_1, LOW );
								digitalWrite (motor_pin_2, LOW );
								digitalWrite (motor_pin_3, HIGH );
								digitalWrite (motor_pin_4, LOW );
								break;

							case (5): //0011
							case (-3):
								digitalWrite (motor_pin_1, LOW );
								digitalWrite (motor_pin_2, LOW );
								digitalWrite (motor_pin_3, HIGH );
								digitalWrite (motor_pin_4, HIGH );
								break;

							case (6): //0001
							case (-2):
								digitalWrite (motor_pin_1, LOW );
								digitalWrite (motor_pin_2, LOW );
								digitalWrite (motor_pin_3, LOW );
								digitalWrite (motor_pin_4, HIGH );
								break;

							case (7): //1001
							case(-1):
								digitalWrite (motor_pin_1, HIGH );
								digitalWrite (motor_pin_2, LOW );
								digitalWrite (motor_pin_3, LOW );
								digitalWrite (motor_pin_4, HIGH );
								break;
						}
					}
				}

				// Increment positions
				AbsolutePosition = NextPosition;
				DeltaPosition += StepIncrement;

				// Has motor reached the target position?
				if (AbsolutePosition == TargetPosition)
				{
					// Yes, stop motor and indicate completion with position
					MotorState = ENABLED;
					RunReturn = RUN_COMPLETE;
				}
				else
				{
					// No, so continue motion
					StepCount = abs (DeltaPosition );
					if (StepCount <= RampSteps)
					{
						// Ramping up
						Velocity += VelocityIncrement;
					}
					else if (StepCount > RampDownStep)
					{
						// Ramping down
						Velocity -= VelocityIncrement;
					}

					// Set time for next step
					if (Velocity != 0)
					{
						NextStepMicros += 1000000L / Velocity;
					}
					else
					{
						NextStepMicros += 5L; // DEF ensure SOME time until next
					}
				}
			}
		}
	}

	return RunReturn;
}

//=== startRotation =====================================

void StepperMotorController::startRotation ()
{
  // Determine number of steps in ramp and set starting speed
  if (VelocityIncrement == 0L)
  {
    // Immediate full speed, no ramping
    RampSteps = 0L;
    Velocity  = MaxVelocity;  // Start at full velocity
  }
  else
  {
    // Ramp up
    RampSteps = MaxVelocity / VelocityIncrement;
    if (RampSteps == 0L)
      Velocity = MaxVelocity;  // Start at slow value
    else
      Velocity = 0L;  // Start from a stand-still
  }

  // Set on what step to start ramping down
  if (TotalSteps > 2L * RampSteps)
    RampDownStep = TotalSteps - RampSteps;  // Normal trapezoid velocity
  else
    RampDownStep = RampSteps = TotalSteps / 2L;  // Stunted triangle velocity

  // Set Direction
  if (TargetPosition > AbsolutePosition)
  {
    StepIncrement = 1L;
    if (DriverType == DIGITAL) digitalWrite(DirectionPin, LOW);
  }
  else if (TargetPosition < AbsolutePosition)
  {
    StepIncrement = -1L;
    if (DriverType == DIGITAL) digitalWrite(DirectionPin, HIGH);
  }
  else
  {
    // Motor is already at Target position
    MotorState = ENABLED;
    return;
  }

  // Start rotation
  DeltaPosition  = 0L;
  NextStepMicros = micros() + 10L;  // Direction must be set 10-microseconds before stepping
  MotorState     = RUNNING;
}

//=== Enable ============================================

void StepperMotorController::Enable ()
{
  // Enable motor driver
	if (DriverType == DIGITAL)
	{
		digitalWrite(EnablePin, LOW);
	}

  MotorState = ENABLED;
}

//=== Disable ===========================================

void StepperMotorController::Disable ()
{
  // Disable motor driver
  if (DriverType == DIGITAL) {
	  digitalWrite(EnablePin, HIGH);
  } else {
	  digitalWrite(motor_pin_1, LOW);
	  digitalWrite(motor_pin_2, LOW);
	  digitalWrite(motor_pin_3, LOW);
	  if ((DriverType==UNIPOLAR) || (DriverType==BIPOLAR))
		  digitalWrite(motor_pin_4, LOW);
  }

  MotorState = DISABLED;
  Homed      = false;  // When motor is free to move, the HOME position is lost
}

//=== SetHomePosition ===================================

void StepperMotorController::SetHomePosition ()
{
  // Set current position as HOME position
  AbsolutePosition = 0L;
  DeltaPosition    = 0L;
  Homed            = true;

  // Enable the motor
  Enable ();
}

//=== SetLowerLimit =====================================

void StepperMotorController::SetLowerLimit (long lowerLimit)
{
  LowerLimit = lowerLimit;
}

//=== SetUpperLimit =====================================

void StepperMotorController::SetUpperLimit (long upperLimit)
{
  UpperLimit = upperLimit;
}

//=== SetRamp ===========================================

void StepperMotorController::SetRamp (int ramp)
{
  // Check specified ramp factor
  if (ramp >= 0 && ramp <= 9)
  {
    // Set velocity slope (increment)
    if (ramp == 0)
      VelocityIncrement = 0L;  // constant full velocity
    else
      VelocityIncrement = RampScale * (10L - (long)ramp);
  }
}

//=== RotateAbsolute ====================================

void StepperMotorController::RotateAbsolute (long newPosition, int stepsPerSecond)
{
  TargetPosition = newPosition;  // Set Absolute Position
  MaxVelocity    = stepsPerSecond;
  TotalSteps     = abs(TargetPosition - AbsolutePosition);

  startRotation();
}

//==== RotateRelative ===================================

void StepperMotorController::RotateRelative (long numSteps, int stepsPerSecond)
{
  // If numSteps is positive (> 0) then motor rotates clockwise, else counter-clockwise
  if (numSteps != 0)
  {
    TargetPosition = AbsolutePosition + numSteps;
    MaxVelocity    = stepsPerSecond;
    TotalSteps     = abs(numSteps);

    startRotation();
  }
}

//=== RotateToHome ======================================

void StepperMotorController::RotateToHome()
{
  MaxVelocity    = HOME_LIMIT_SPEED;
  TargetPosition = 0L;  // HOME position
  TotalSteps     = abs(AbsolutePosition);

  startRotation();
}

//=== RotateToLowerLimit ================================

void StepperMotorController::RotateToLowerLimit()
{
  MaxVelocity    = HOME_LIMIT_SPEED;
  TargetPosition = LowerLimit;
  TotalSteps     = abs(AbsolutePosition - LowerLimit);

  startRotation();
}

//=== RototaeToUpperLimit ===============================

void StepperMotorController::RotateToUpperLimit()
{
  MaxVelocity    = HOME_LIMIT_SPEED;
  TargetPosition = UpperLimit;
  TotalSteps     = abs(AbsolutePosition - UpperLimit);

  startRotation();
}

//=== EStop =============================================

void StepperMotorController::EStop()
{
  // Emergency Stop
  // ((( Requires Re-Enable and Re-Homing of motor )))
	if (DriverType==DIGITAL) {
		digitalWrite(StepPin  , LOW );   // Pulse Off for digital drivers
		digitalWrite(EnablePin, HIGH);   // Disengage
	} else {
		digitalWrite(motor_pin_1, LOW);  // Coil Off
		digitalWrite(motor_pin_2, LOW);  // Coil Off
		digitalWrite(motor_pin_3, LOW);  // Coil Off
		digitalWrite(motor_pin_4, LOW);  // Coil Off
	}
  MotorState = STOPPED;
  Homed = false;
  TargetPosition = AbsolutePosition;
}

//=== IsHomed ===========================================

bool StepperMotorController::IsHomed()
{
  // Return homed state
  return Homed;
}

//=== GetState ==========================================

MotorStates StepperMotorController::GetState()
{
  // Return current motor state
  return MotorState;
}

//=== GetAbsolutePosition ===============================

long StepperMotorController::GetAbsolutePosition()
{
  // Return current position from HOME
  return AbsolutePosition;
}

//=== GetRelativePosition ===============================

long StepperMotorController::GetRelativePosition()
{
  // Return number of steps moved from last position
  return DeltaPosition;
}

//=== GetLowerLimit =====================================

long StepperMotorController::GetLowerLimit()
{
  return LowerLimit;
}

//=== GetUpperLimit =====================================

long StepperMotorController::GetUpperLimit()
{
  return UpperLimit;
}

//=== GetRemainingTime ==================================

unsigned long StepperMotorController::GetRemainingTime()
{
  // Return remaining time (in milliseconds) for motion to complete
  if (MotorState != RUNNING)
    return 0L;

  unsigned long numSteps = abs(AbsolutePosition - TargetPosition);
  unsigned long remTime  = 1000L * numSteps / MaxVelocity + 500L;  // +500 for ramping

  return remTime;
}

//=== GetVersion ========================================

const char * StepperMotorController::GetVersion()
{
  return Version;
}

//=== BlinkLED ==========================================

void StepperMotorController::BlinkLED()
{
  // Blink onboard LED (if defined)
  if (LEDPin < 0 ) return;
  for (int i=0; i<10; i++)
  {
    digitalWrite(LEDPin, HIGH);
    delay(20);
    digitalWrite(LEDPin, LOW);
    delay(80);
  }
}


//========================================================
//  ExecuteCommand
//========================================================

const char * StepperMotorController::ExecuteCommand(const char *packet)
{
  char  command[3];
  int   ramp, velocity;
  long  limit, targetOrNumSteps;
  static char  returnString[40];

  // Command string must be at least 2 chars
  if (strlen(packet) < 2) return "";

  // Set 2-Char Command and parse all commands
  // DEF: strncpy (command, packet, 2);
  command[0]=toupper( packet[0]);
  command[1]=toupper( packet[1]);
  command[2] = 0;


  //======================================================
  //  Emergency Stop (ESTOP)
  //  I check this first for quick processing.
  //  When an E-Stop is called, you must re-Enable the
  //  driver and re-Set the motor's Home position for
  //  motion to resume.
  //======================================================
  if (strcmp(command, "ES") == 0)
    EStop();

  //======================================================
  //  Enable / Disable
  //======================================================
  else if (strcmp(command, "EN") == 0)
    Enable();
  else if (strcmp(command, "DI") == 0)
    Disable();

  //======================================================
  //  Set HOME Position, LOWER and UPPER Limits
  //======================================================
  else if (strcmp(command, "SH") == 0)
    SetHomePosition();
  else if (strcmp(command, "SL") == 0 || strcmp(command, "SU") == 0)
  {
    // Check for value
    if (strlen(packet) < 3)
      return "E:Missing limit value";

    limit = strtol(packet+2, NULL, 10);

    if (strcmp(command, "SL") == 0)
      SetLowerLimit(limit);
    else
      SetUpperLimit(limit);
  }

  //======================================================
  //  Set Velocity Ramp Factor
  //======================================================
  else if (strcmp(command, "SR") == 0)
  {
    // Check for value
    if (strlen(packet) != 3)
      return "E:Missing ramp value 0-9";

    ramp = atoi(packet+2);

    // Check specified ramp value
    if (ramp >= 0 && ramp <= 9)
      SetRamp(ramp);
  }

  //======================================================
  //  Rotate Commands
  //======================================================
  else if (strcmp(command, "RH") == 0)
    RotateToHome();
  else if (strcmp(command, "RL") == 0)
    RotateToLowerLimit();
  else if (strcmp(command, "RU") == 0)
    RotateToUpperLimit();
  else if (strcmp(command, "RA") == 0 || strcmp(command, "RR") == 0)
  {
    // Rotate command must be at least 7 chars
    if (strlen(packet) < 7)
      return "E:Bad command";

    // Parse max velocity and target/numSteps
    char velString[5];  // Velocity is 4-chars 0001..9999
    strncpy (velString, packet+2, 4);
    velString[4] = 0;
    velocity = strtol(velString, NULL, 10);
    targetOrNumSteps = strtol(packet+6, NULL, 10);  // Target position or number of steps is remainder of packet
    if (strcmp(command, "RA") == 0)
      RotateAbsolute(targetOrNumSteps, velocity);
    else
      RotateRelative(targetOrNumSteps, velocity);
  }

  //======================================================
  //  Query Commands and Blink
  //======================================================
  else if (strcmp(command, "GA") == 0)
    return itoa(GetAbsolutePosition(), returnString, 10);
  else if (strcmp(command, "GR") == 0)
    return itoa(GetRelativePosition(), returnString, 10);
  else if (strcmp(command, "GL") == 0)
    return itoa(GetLowerLimit(), returnString, 10);
  else if (strcmp(command, "GU") == 0)
    return itoa(GetUpperLimit(), returnString, 10);
  else if (strcmp(command, "GT") == 0)
    return itoa(GetRemainingTime(), returnString, 10);
  else if (strcmp(command, "GV") == 0)
    return (char *)GetVersion();
  else if (strcmp(command, "BL") == 0)
    BlinkLED();
  else
    return "E:Unknown command";

  return "";
}

