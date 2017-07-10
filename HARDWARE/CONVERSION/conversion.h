#ifndef __CONVERSION_H
#define __CONVERSION_H	 
#include "sys.h"
extern void GPS_transformation(double WGS84_Lat,double WGS84_Lon,double * BD_09_Lat,double * BD_09_Lon);   //GPS真实坐标转化
extern double Cal_Distance(double lat1, double lng1, double lat2, double lng2);                            //计算两坐标点的距离
#endif

