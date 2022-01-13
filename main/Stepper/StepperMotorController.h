//==================================================================================================================
//
//  FILE   : StepperMotorController.h
//
//  NOTES  : Stepper Motor Controller, Version 2022-1-12
//           This firmware is used to operate a single stepper driver/motor from an Arduino board.
//           (see the .cpp file for details)
//
//  AUTHOR : Bill Daniels
//           Copyright (c) 2013-2021, D+S Tech Labs Incorporated
//           All Rights Reserved
//
//==================================================================================================================
//
//  This class communicates (by GPIO pins) with micro-stepping motor drivers that take Enable, Direction
//  and Pulse (step) signals.  A typical stepper motor driver would be the MA860H Microstep Driver.
//
//  A "digital" stepper motor driver requires three(3) GPIO pins to operate: Enable, Direction and Pulse (step)
//  The default board pins are the following, but can be set using Constructor params:
//
//  Enable    = digital pin 2 (D2)
//  Direction = digital pin 3 (D3)
//  Pulse     = digital pin 4 (D4)
//
//  A "non-digital" stepper motor driver requires four(4) Arduino pins to operate: A+ A- B+ B-
//  Using the Arduino NANO board:
//
//  A+  = digital pin 5 (D5)
//  A-  = digital pin 6 (D6)
//  B+  = digital pin 7 (D7)
//  B-  = digital pin 8 (D8)
//
//  (GND) should be used for common ground.
//  Future connections will include LOW and HIGH Limit Switch inputs.
//
//------------------------------------------------------------------------------------------------------------------
//
//  Motor rotation velocity follows a trapezoidal shape.
//  A linear ramp-up/ramp-down rate is set with the SetRamp() method.
//
//                                   .--------------------------------.    <-- full velocity
//  A ramp value of 0                |                                |
//  specifies no ramping:            |                                |
//  (not recommended !!!)            |                                |
//                                 --------------------------------------
//
//                                       .------------------------.        <-- full velocity
//  A ramp value of 5                   /                          \.
//  specifies moderate ramping:        /                            \.
//  This is the default at startup    /                              \.
//                                 --------------------------------------
//
//                                           .----------------.            <-- full velocity
//  A ramp value of 9                      /                    \.
//  specifies gradual ramping:           /                        \.
//                                     /                            \.
//                                 --------------------------------------
//
//  Use low values (2, 3, ..) for fast accelerations with light loads and high values (.., 8, 9) for slow accelerations
//  with heavy loads.  It is highly recommended to use slow acceleration when moving high inertial loads.
//
//                                            ----------    <-- full velocity
//  If there is not enough time to achieve
//  full velocity, then rotation velocity         /\.
//  follows a "stunted" triangle path:           /  \.
//                                            ----------
//
//  Once a ramp value is set, all rotate commands will use that ramp value.
//  The default ramp value at start-up is 5.
//
//------------------------------------------------------------------------------------------------------------------
//
//  All five Rotate methods return the string "RCp" to indicate completion of rotation (RC = ROTATE COMPLETE),
//  where p is the new ABSOLUTE POSITION of the motor.  For example, receiving the string "RC-5430" indicates
//  rotation is complete and the motor is now at -5430 steps from its HOME position (negative meaning counter-clockwise).
//
//  If a Rotate method is called while the motor is already running, then the current rotation is
//  interrupted and the new Rotate method is executed from the motor's current position.
//
//------------------------------------------------------------------------------------------------------------------
//
//  The motor's HOME position is always zero (0).
//  Clockwise rotation uses positive step counts.
//  Counter-clockwise rotation uses negative step counts.
//
//  The default LOWER and UPPER Range Limits are set to +2 billion and -2 billion at start up.
//  It is assumed the user will set these limits from a configuration file or other means.
//  If a range limit has been reached (the motor's Absolute Position has reached the set
//  LOWER LIMIT or UPPER LIMIT), then the motor's motion is stopped and a Range Error "RE"
//  message is returned.
//
//------------------------------------------------------------------------------------------------------------------
//
//  This class also has a method for operating the stepper motor by executing String commands.
//  Use the ExecuteCommand() method passing the following 2-char commands:
//
//    EN    = ENABLE                - Enables the motor driver (energizes the motor)
//    DI    = DISABLE               - Disables the motor driver (releases the motor)
//    SH    = SET HOME POSITION     - Sets the current position of the motor as its HOME position (Sets Absolute position to zero)
//    SL... = SET LOWER LIMIT       - Sets the LOWER LIMIT (minimum Absolute Position) of the motor's range
//    SU... = SET UPPER LIMIT       - Sets the UPPER LIMIT (maximum Absolute Position) of the motor's range
//    SRr   = SET RAMP              - Sets the trapezoidal velocity RAMP (up/down) for smooth motor start and stop
//    RA... = ROTATE ABSOLUTE       - Rotates motor to an Absolute target position from its HOME position
//    RR... = ROTATE RELATIVE       - Rotates motor clockwise or counter-clockwise any number of steps from its current position
//    RH    = ROTATE HOME           - Rotates motor to its HOME position
//    RL    = ROTATE LOWER LIMIT    - Rotates motor to its LOWER LIMIT position
//    RU    = ROTATE UPPER LIMIT    - Rotates motor to its UPPER LIMIT position
//    ES    = E-STOP                - Stops the motor immediately and releases motor (emergency stop)
//    GA    = GET ABSOLUTE position - Returns the motor's current step position relative to its HOME position
//    GR    = GET RELATIVE position - Returns the motor's current step position relative to its last targeted position
//    GL    = GET LOWER LIMIT       - Returns the motor's Absolute LOWER LIMIT position
//    GU    = GET UPPER LIMIT       - Returns the motor's Absolute UPPER LIMIT position
//    GT    = GET TIME              - Returns the remaining time in ms for motion to complete
//    GV    = GET VERSION           - Returns this firmware's current version
//    BL    = BLINK LED             - Blink the onboard LED to indicate identification
//
//    where r is the velocity ramp rate (0-9).
//
//    ---------------------------------------------------------------
//     Command String Format: (no spaces between fields)
//    ---------------------------------------------------------------
//                              cc vvvv sssssssssss...<lf>
//                              |   |        |
//      Command/Query ----------*   |        |
//      [2-chars]                   |        |
//         EN = ENABLE              |        |
//         DI = DISABLE             |        |
//         SH = SET HOME            |        |
//         SL = SET LOWER LIMIT     |        |
//         SU = SET UPPER LIMIT     |        |
//         SR = SET RAMP            |        |
//         RA = ROTATE ABSOLUTE     |        |
//         RR = ROTATE RELATIVE     |        |
//         RH = ROTATE HOME         |        |
//         RL = ROTATE LOWER LIMIT  |        |
//         RU = ROTATE UPPER LIMIT  |        |
//         ES = E-STOP              |        |
//         GA = GET ABS POSITION    |        |
//         GR = GET REL POSITION    |        |
//         GL = GET LOWER LIMIT     |        |
//         GU = GET UPPER LIMIT     |        |
//         GT = GET TIME            |        |
//         GV = GET VERSION         |        |
//         BL = BLINK               |        |
//                                  |        |
//      Velocity (steps per sec) ---*        |
//         [4-digits] 1___ - 9999   |        |
//         Right-padded with spaces |        |
//                                  |        |
//      Ramp Slope -----------------*        |
//         [1-digit] 0 - 9                   |
//         (For SR command only)             |
//                                           |  ----.
//      Absolute or Relative Step Position --*      |
//         [1 to 10-digits plus sign]               |
//         No padding necessary                     |--- For ROTATE commands only
//         -2147483648 to 2147483647 (long)         |
//         Positive values = Clockwise              |
//         Negative values = Counter-Clockwise      |
//                                              ----*
//    ---------------------------------------------------------------
//     Examples: (quotes are not included in string)
//    ---------------------------------------------------------------
//     "EN"            - ENABLE                - Enable the stepper motor driver
//     "DI"            - DISABLE               - Disable the stepper motor driver
//     "SR6"           - SET RAMP              - Set the velocity ramp-up/ramp-down rate to 6
//     "SH"            - SET HOME              - Set the current position of the motor as its HOME position (which is zero)
//     "RA500 2000"    - ROTATE ABSOLUTE       - Rotate the motor at 500 steps per second, to Absolute position of +2000 steps clockwise from HOME
//     "RR3210-12000"  - ROTATE RELATIVE       - Rotate the motor at 3210 steps per second, -12000 steps counter-clockwise from its current position
//     "RH"            - ROTATE HOME           - Rotate motor back to its HOME position (0)
//     "RL"            - ROTATE to LOWER LIMIT - Rotate motor to its LOWER LIMIT position
//     "ES"            - EMERGENCY STOP        - Immediately stop the motor and cancel rotation command
//     "GR"            - GET RELATIVE POSITION - Get the current relative step position of the motor
//
//  This class may also be queried for position, range limits, remaining motion time and firmware version with the following commands:
//
//     GA = GET ABSOLUTE position - Returns the motor's current step position relative to its HOME position
//     GR = GET RELATIVE position - Returns the motor's current step position relative to its last targeted position
//     GL = GET LOWER LIMIT       - Returns the motor's Absolute LOWER LIMIT position
//     GU = GET UPPER LIMIT       - Returns the motor's Absolute UPPER LIMIT position
//     GT = GET TIME              - Returns the remaining time in ms for motion to complete
//     GV = GET VERSION           - Returns this firmware's current version
//
//  The returned result is a string with the following format:
//     APs... = Absolute Position of motor is s steps from its HOME position
//     RPs... = Relative Position of motor is s steps from its last targeted position
//     LLs... = LOWER LIMIT of motor relative to its HOME position
//     ULs... = UPPER LIMIT of motor relative to its HOME position
//  where s is a positive or negative long integer (depending on clockwise or counter-clockwise position)
//  and represents a number of steps which may be 1 to 10 digits plus a possible sign.
//
//
//==================================================================================================================

#ifndef SMC_H
#define SMC_H

#define HOME_LIMIT_SPEED  4000L
#define PULSE_WIDTH       5     // 5-microseconds (check your driver's pulse width requirement)

enum DriverTypes
{
  DIGITAL,
  NON_DIGITAL
};

enum MotorStates
{
  ENABLED,   // Motor driver is enabled, this is the normal idle/holding state
  DISABLED,  // Motor driver is disable, motor can be manually rotated
  RUNNING,   // Motor is running (rotating) and is currently executing a Rotate command
  STOPPED    // Motor is in an E-STOP condition (Emergency Stop)
};

enum RunReturn_t
{
	RUN_OK,          // Sitting idle or still running
	RUN_COMPLETE,    // Rotate Complete
	RUN_RANGE_ERROR  // Range Error (tried to pass a limit)
};


//=========================================================
//  class StepperMotorController
//=========================================================

class StepperMotorController
{
  private:
    static const char * Version;

    DriverTypes  DriverType = DIGITAL;   // Default is a "digital" stepper driver (enable, direction, pulse)
    MotorStates  MotorState = DISABLED;  // Default is disabled (unlocked)

    // Logic Pins For Digital Stepper Drivers:
    int  EnablePin, DirectionPin, StepPin;
    int  LEDPin;

    // Logic Pins For Non-Digital Stepper Drivers:
    int  motor_pin_1;
    int  motor_pin_2;
    int  motor_pin_3;
    int  motor_pin_4;

    bool        Homed = false;  // The motor must be "Homed" before use
    const long  RampScale = 5L;
    long        MaxVelocity, TargetOrSteps, TotalSteps, StepCount;

    long           StepIncrement;      // 1 for clockwise rotations, -1 for counter-clockwise
    long           AbsolutePosition;   // Number of steps from HOME position
    long           DeltaPosition;      // Number of steps from last position
    long           TargetPosition;     // Target position for end of rotation
    long           LowerLimit;         // Minimum step position
    long           UpperLimit;         // Maximum step position
    long           RampSteps;          // Total number of steps during ramping
    long           RampDownStep;       // Step at which to start ramping down
    long           Velocity;           // Current velocity of motor
    long           VelocityIncrement;  // Velocity adjustment for ramping (determined by ramp factor)
    long           NextPosition;       // Position after next step
    unsigned long  NextStepMicros;     // Target micros for next step
    RunReturn_t    RunReturn = RUN_OK; // Return from Run method

    void startRotation();

  public:
    StepperMotorController(DriverTypes driverType, int pin1=2, int pin2=3, int pin3=4, int pin4=5, int ledPin=13);
    // For Digital Drivers pin1, pin2, pin3 are for Enable, Direction and Step pins respectively
    // For Non-Digital Drivers pin1, pin2, pin3, pin4 are for each coil of the motor

    RunReturn_t    Run();  // Keeps the motor running (must be called from your loop() function with no delay)

    void           Enable              ();                                      // Enables the motor driver (energizes the motor)
    void           Disable             ();                                      // Disables the motor driver (releases the motor)

    void           SetHomePosition     ();                                      // Sets the current position of the motor as its HOME position (Sets Absolute position to zero)
    void           SetLowerLimit       (long lowerLimit);                       // Sets the LOWER LIMIT (minimum Absolute Position) of the motor's range
    void           SetUpperLimit       (long upperLimit);                       // Sets the UPPER LIMIT (maximum Absolute Position) of the motor's range
    void           SetRamp             (int ramp);                              // Sets the trapezoidal velocity RAMP (up/down) for smooth motor start and stop

    void           RotateAbsolute      (long absPosition, int stepsPerSecond);  // Rotates motor to an Absolute target position from its HOME position
    void           RotateRelative      (long numSteps, int stepsPerSecond);     // Rotates motor clockwise(+) or counter-clockwise(-) any number of steps from its current position
    void           RotateToHome        ();                                      // Rotates motor to its HOME position
    void           RotateToLowerLimit  ();                                      // Rotates motor to its LOWER LIMIT position
    void           RotateToUpperLimit  ();                                      // Rotates motor to its UPPER LIMIT position
    void           EStop               ();                                      // Stops the motor immediately (emergency stop)

    bool           IsHomed             ();                                      // Returns true or false
    MotorStates    GetState            ();                                      // Returns current state of motor
    long           GetAbsolutePosition ();                                      // Returns the motor's current step position relative to its HOME position
    long           GetRelativePosition ();                                      // Returns the motor's current step position relative to its last targeted position
    long           GetLowerLimit       ();                                      // Returns the motor's Absolute LOWER LIMIT position
    long           GetUpperLimit       ();                                      // Returns the motor's Absolute UPPER LIMIT position
    unsigned long  GetRemainingTime    ();                                      // Return the remaining time in ms when rotation will complete
    const char *   GetVersion          ();                                      // Returns this firmware's current version
    void           BlinkLED            ();                                      // Blink the onboard LED to indicate identification
    unsigned long  GetTimeToNextStep   ();                                      // How long before the next step...

    char *         ExecuteCommand      (char *command);                         // Execute a stepper motor function by string command (see notes above)
};

#endif
