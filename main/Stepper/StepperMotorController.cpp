//==================================================================================================================
//
//  FILE   : StepperMotorController.cpp
//
//  NOTES  : Stepper Motor Controller, Version 2022-1-8
//           This firmware is used to operate a single stepper driver/motor from an Arduino board.
//           (see the header file for details)
//
//  AUTHOR : Bill Daniels
//           Copyright (c) 2013-2022, D+S Tech Labs Incorporated
//           All Rights Reserved
//
//==================================================================================================================

#include "Arduino.h"
#include "StepperMotorController.h"

//==========================================================
//  Constructor
//==========================================================
StepperMotorController::StepperMotorController (int enablePin, int directionPin, int stepPin, int ledPin)
{
  // Save the specified pins
  EnablePin    = enablePin;
  DirectionPin = directionPin;
  StepPin      = stepPin;
  LEDPin       = ledPin;

  // Set all pin modes as DIGITAL OUTPUT's
  pinMode(EnablePin   , OUTPUT);
  pinMode(DirectionPin, OUTPUT);
  pinMode(StepPin     , OUTPUT);
  pinMode(LEDPin      , OUTPUT);
  pinMode(motor_pin_1 , OUTPUT);
  pinMode(motor_pin_2 , OUTPUT);
  pinMode(motor_pin_3 , OUTPUT);
  pinMode(motor_pin_4 , OUTPUT);

  // Initialize pins
  digitalWrite(EnablePin   , HIGH);  // Start with driver disabled
  digitalWrite(DirectionPin, LOW );
  digitalWrite(StepPin     , LOW );  // No pulse
  digitalWrite(LEDPin      , LOW );  // Turn off LED
  digitalWrite(motor_pin_1 , LOW );  // Coil off
  digitalWrite(motor_pin_2 , LOW );  // Coil off
  digitalWrite(motor_pin_3 , LOW );  // Coil off
  digitalWrite(motor_pin_4 , LOW );  // Coil off

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
  RunReturn         = "";

//  // Flash LED to indicate board is ready
//  for (int i=0; i<5; i++)
//  {
//    digitalWrite(LEDPin, HIGH); delay(100);
//    digitalWrite(LEDPin, LOW ); delay(400);
//  }
}

//=========================================================
//  Run:
//  Must be continuously called inside your loop function
//  There should be no delay between calls
//
//  Return values:
//    "RCp" - Rotate Complete where p is the Absolute Position
//    "RE"  - Range Error (tried to pass a limit)
//    ""    - Empty String (sitting idle or still running)
//=========================================================
String StepperMotorController::Run()
{
  RunReturn = "";
  
  // If motor is RUNNING and it's time for it to step, then issue step pulse
  if (Homed && MotorState == RUNNING)
  {
    // Is it time to step?
    if (micros() >= NextStepMicros)
    {
      // Check range limits
      NextPosition = AbsolutePosition + StepIncrement;  // +1 for clockwise rotations, -1 for counter-clockwise
      if (NextPosition < LowerLimit || NextPosition > UpperLimit)
      {
        // Next step will be out of range,
        // So stop motor and return range error
        MotorState = ENABLED;
        RunReturn = "RE";  // Range Error
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

          digitalWrite(StepPin, HIGH);
          delayMicroseconds(PULSE_WIDTH);
          digitalWrite(StepPin, LOW);
        }
        else
        {
          //=============================================================================
          // For Non-Digital Stepper Drivers:
          //   Each of the four coils must be turned on/off in sequence
          //=============================================================================
          switch (NextPosition % 4)  // For 4-wire stepper motors
          {
            case 0:  // 1010
              digitalWrite(motor_pin_1, HIGH);
              digitalWrite(motor_pin_2, LOW);
              digitalWrite(motor_pin_3, HIGH);
              digitalWrite(motor_pin_4, LOW);
              break;
            case 1:  // 0110
              digitalWrite(motor_pin_1, LOW);
              digitalWrite(motor_pin_2, HIGH);
              digitalWrite(motor_pin_3, HIGH);
              digitalWrite(motor_pin_4, LOW);
              break;
            case 2:  // 0101
              digitalWrite(motor_pin_1, LOW);
              digitalWrite(motor_pin_2, HIGH);
              digitalWrite(motor_pin_3, LOW);
              digitalWrite(motor_pin_4, HIGH);
              break;
            case 3:  // 1001
              digitalWrite(motor_pin_1, HIGH);
              digitalWrite(motor_pin_2, LOW);
              digitalWrite(motor_pin_3, LOW);
              digitalWrite(motor_pin_4, HIGH);
              break;
          }
        }

        // Increment positions
        AbsolutePosition = NextPosition;
        DeltaPosition   += StepIncrement;

        // Has motor reached the target position?
        if (AbsolutePosition == TargetPosition)
        {
          // Yes, stop motor and indicate completion with position
          MotorState = ENABLED;
          RunReturn = "RC" + String(AbsolutePosition);
        }
        else
        {
          // No, so continue motion
          StepCount = abs(DeltaPosition);
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
          NextStepMicros += 1000000L / Velocity;
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
    digitalWrite(DirectionPin, LOW);
  }
  else if (TargetPosition < AbsolutePosition)
  {
    StepIncrement = -1L;
    digitalWrite(DirectionPin, HIGH);
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
  digitalWrite(EnablePin, LOW);
  MotorState = ENABLED;
}

//=== Disable ===========================================

void StepperMotorController::Disable ()
{
  // Disable motor driver
  digitalWrite(EnablePin, HIGH);
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
  digitalWrite(StepPin  , LOW );   // Pulse Off for digital drivers
  digitalWrite(EnablePin, HIGH);   // Disengage
  digitalWrite(motor_pin_1, LOW);  // Coil Off
  digitalWrite(motor_pin_2, LOW);  // Coil Off
  digitalWrite(motor_pin_3, LOW);  // Coil Off
  digitalWrite(motor_pin_4, LOW);  // Coil Off

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

String StepperMotorController::GetVersion()
{
  return Version;
}

//=== BlinkLED ==========================================

void StepperMotorController::BlinkLED()
{
  // Blink onboard LED
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

String StepperMotorController::ExecuteCommand(String packet)
{
  String  command;
  int     ramp, velocity;
  long    limit, targetOrNumSteps;
  
  // Command string must be at least 2 chars
  if (packet.length() < 2) return "";
  
  // Set 2-Char Command and parse all commands
  command = packet.substring(0, 2);

  //======================================================
  //  Emergency Stop (ESTOP)
  //  I check this first for quick processing.
  //  When an E-Stop is called, you must re-Enable the
  //  driver and re-Set the motor's Home position for
  //  motion to resume.
  //======================================================
  if (command == "ES")
    EStop();
      
  //======================================================
  //  Enable / Disable
  //======================================================
  else if (command == "EN")
    Enable();
  else if (command == "DI")
    Disable();

  //======================================================
  //  Set HOME Position, LOWER and UPPER Limits
  //======================================================
  else if (command == "SH")
    SetHomePosition();
  else if (command == "SL" || command == "SU")
  {
    // Check for value
    if (packet.length() < 3)
      return "Missing limit value";

    limit = (long)((packet.substring(2)).toInt());

    if (command == "SL")
      SetLowerLimit(limit);
    else
      SetUpperLimit(limit);
  }
  
  //======================================================
  //  Set Velocity Ramp Factor
  //======================================================
  else if (command == "SR")
  {
    // Check for value
    if (packet.length() != 3)
      return "Missing ramp value 0-9";
     
    ramp = (packet.substring(2)).toInt();
    
    // Check specified ramp value
    if (ramp >= 0 && ramp <= 9)
      SetRamp(ramp);
  }

  //======================================================
  //  Rotate Commands
  //======================================================
  else if (command == "RH")
    RotateToHome();
  else if (command == "RL")
    RotateToLowerLimit();
  else if (command == "RU")
    RotateToUpperLimit();
  else if (command == "RA" || command == "RR")
  {
    // Rotate command must be at least 7 chars
    if (packet.length() < 7)
      return "Bad command";
    
    // Parse max velocity and target/numSteps
    velocity         = (long)((packet.substring(2, 6)).toInt());  // Velocity is 4-chars 0001..9999
    targetOrNumSteps = (long)((packet.substring(6)).toInt());     // Target position or number of steps is remainder of packet

    if (command == "RA")
      RotateAbsolute(targetOrNumSteps, velocity);
    else
      RotateRelative(targetOrNumSteps, velocity);
  }
  
  //======================================================
  //  Query Commands and Blink
  //======================================================
  else if (command == "GA")
    return String(GetAbsolutePosition());
  else if (command == "GR")
    return String(GetRelativePosition());
  else if (command == "GL")
    return String(GetLowerLimit());
  else if (command == "GU")
    return String(GetUpperLimit());
  else if (command == "GT")
    return String(GetRemainingTime());
  else if (command == "GV")
    return GetVersion();
  else if (command == "BL")
    BlinkLED();
  else
    return "Unknown command";

  return "";
}
