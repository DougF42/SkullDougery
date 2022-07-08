#include "Commands.h"
#include <cmath>
#include <iostream>
/**
 * SKULL coordinates are regarded as CARTESION coordinates:
 * Coordinates are assumed integer. However, we convert to double and
 *    round the result when converting between coordinate systems to
 *    avoid integer truncation.
 *
 * For the skull, These limits are (currently) pre-set to default values.
 * (eventually, we probably should allow these to be configured)
 *
 * The movement commands expect a desired position in the 
 * EXTERNAL coordinate system, which is also viewed as cartesian, and
 * defined when the 'Commands' class is initialized.
 * The translation  SKULL  <-->  EXTERNAL is done automatically.
 * 
 * A simplifying assumpyion: we expect the origin and axis orientation
 * are identical for both coordinate systems.\
 *
 * @param _ext_rot_range   external rotate coordinates range from x to y. x < y
 * @param _ext_nod_range   external nod    coordinates range from x to y. x < y
 */
Commands::Commands(int externRotLow, int externRotHigh, int extNodLow, int extNodHigh)
{
  prevSpeed = cv::Point2i(200,200); // Default speeds
  setAllLimits( -1200, 1200, -500, 500,
	        externRotLow, externRotHigh, extNodLow, extNodHigh);

}

Commands::~Commands()
{
  // Destructor.
}

/**
 * Set a new range for skull motion.
 * This is a front-end for setAllLimits that only changes 
 * the SKULL coordinate system.
 */
void Commands::setSkullRange(int rotLow, int rotHigh, int nodLow, int nodHigh)
{
  setAllLimits(rotLow, rotHigh, nodLow, nodHigh,
	       externRotRange.low, externRotRange.high, externNodRange.low, externNodRange.high);
}

/**
 * Set a new range for extern motion
 * This is a front-rnd for setAllLimits that ONLY changes
 * the external coordinate system.
 */
void Commands::setExtRange(int rotLow, int rotHigh, int nodLow, int nodHigh)
{
  setAllLimits(skullRotateRange.low, skullRotateRange.high, skullNodRange.low, skullNodRange.high,
	      rotLow, rotHigh, nodLow, nodHigh);
}


/**
 * Set the limits of the skull coordinate system, and
 * calculate the slope of the conversion from EXTERNAL to SKULL
 */
void Commands::setAllLimits( int ext_rotLow, int ext_rotHigh, int ext_nodLow, int ext_nodHigh,
		     int skul_rotLow, int skul_rotHigh, int skul_nodLow, int skul_nodHigh)
{
  externRotRange = Range(ext_rotLow, ext_rotHigh);
  externNodRange = Range(ext_nodLow, ext_nodHigh);
  skullRotateRange = Range(skul_rotLow, skul_rotHigh);
  skullNodRange    = Range(skul_nodLow, skul_nodHigh);
  
  double x = double (skullRotateRange.high - skullRotateRange.low) / double (externRotRange.high - externRotRange.low);
  slope.x = std::round(x);
  
  double y =  double(skullNodRange.high   - skullNodRange.low)    / double(externNodRange.high    - externNodRange.low);
  slope.y = std::round(y);
}



/*
 * convert from external coordinates to internal coordinates
 * @param extPos - the point we are converting from
 * @param intPos - output - the result
 */
void Commands::xlate(cv::Point2i &extPos, cv::Point2i *skullPos)
{
  //  long map(long x, long in_min, long in_max, long out_min, long out_max) {
  //  return   out_min +  (x - in_min) * (out_max - out_min) / (in_max - in_min) ;

  // ROTATE:
  skullPos->x = skullRotateRange.low +  std::round(( extPos.x - externRotRange.low) * slope.x);

  // NOD:
  skullPos->y = skullNodRange.low    +  std::round(( extPos.y - externNodRange.low)  * slope.y);
    return;
}


/**
 * Override (front end) for moveHeadTo
 * This uses the previous speed for the next move.
 */
void Commands::moveHeadTo( cv::Point2i newPos)
{
  moveHeadTo(newPos, &prevSpeed);
}


/**
 * Move the head to the requested position
 * @param extPosition - desired position in EXTERNAL coordinates
 * @param speed  - how fast to rotate and nod. If nullptr, we
 *     default to 200,200 - which is fairly slow.
 *     All speeds MUST be positive
 */
void Commands:: moveHeadTo( cv::Point2i extPos, cv::Point2i *_speed)
{
  cv::Point2i target;
  xlate ( extPos, &target);
  
  // Sanity check
  if ((_speed->x <0)|| (_speed->y <0))
    {
      std::cout<<"ERROR: Negative speed in Commands::moveHeadTo. Speed argument was: " << _speed <<std::endl;
      return;
    }
  
  // DO THE MOVE:
  char buf[128];
  sprintf(buf, "ROT RA%04i%05i\n", _speed->x, target.x);
  // TODO: SEND
  // TODO: WAIT ACK
  
   sprintf(buf, "NOD RA%04i%05i\n", _speed->y, target.y);
   // TODO: SEND
   // TODO: WAIT ACK

   // Save current speed.
  prevSpeed = *_speed;
}
