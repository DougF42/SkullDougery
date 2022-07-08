/**
 * Given the reported position of things, move
 * the head to match.
 */

#include "Configuration.h"
#include <opencv2/core.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

/*
 * A simple class for storing ranges
 * Basicxly, two members - low, high
 */
class Range {
public:
  int low;
  int high;

Range()
  {
    low=0;
    high=0;
  }
  
  Range(long _low, long _high)
  {
    low=_low;
    high=_high;
  }
  
};
  

  
class Commands {
 private:
  // Tracking position limits...
  Range skullRotateRange;
  Range skullNodRange;

  // EXTERNAL coordinate system
  Range externRotRange;
  Range externNodRange;

  void setAllLimits( int ext_rotLow, int ext_rotHigh, int ext_nodLow, int ext_nodHigh,
		     int skul_rotLow, int skul_rotHigh, int skul_nodLow, int skul_nodHigh);
  void xlate(cv::Point2i &extPos, cv::Point2i *skullPos);

  cv::Point2d slope; // Slope of rot and nod
  cv::Point2i prevSpeed;

public:
  Commands(int externRotLow, int externRotHigh, int extNodLow, int extNodHigh);
  ~Commands();
  void setSkullRange(int rotLow, int rotHigh, int nodLow, int nodHigh);
  void setExtRange(int rotLow, int rotHigh, int nodLow, int nodHigh);

  void moveHeadTo( cv::Point2i newPos);
  void moveHeadTo( cv::Point2i newpos, cv::Point2i *_speed);
};
