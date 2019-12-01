#pragma once

// If your application is going to connect to an IoT Central Application, then enable this define.  When
// enabled Device Twin JSON updates will conform to what IoT Central expects to confirm Device Twin settings
//#define IOT_CENTRAL_APPLICATION

// If your application is going to connect straight to a IoT Hub, then enable this define.
//#define IOT_HUB_APPLICATION

#if (defined(IOT_CENTRAL_APPLICATION) && defined(IOT_HUB_APPLICATION))
#error "Can not define both IoT Central and IoT Hub Applications at the same time only define one."
#endif 

#if (!defined(IOT_CENTRAL_APPLICATION) && !defined(IOT_HUB_APPLICATION))
#warning "Building application for no cloud connectivity"
#endif 

#ifdef IOT_CENTRAL_APPLICATION
#warning "Building for IoT Central Application"
#endif 

#ifdef IOT_HUB_APPLICATION
#warning "Building for IoT Hub Application"
#endif 

// If your hardware includes a reed switch in GPIO_42 enable this define
#define REED_SWITCH_INCLUDED

#ifdef REED_SWITCH_INCLUDED
#warning "Building for a reed switch or similar at GPIO42."
#endif 

#define VCNL4040_PROXIMITY_INCLUDED

#ifdef VCNL4040_PROXIMITY_INCLUDED
#warning "Building for a VCNL4040 sensor at ISU2."
#endif 
