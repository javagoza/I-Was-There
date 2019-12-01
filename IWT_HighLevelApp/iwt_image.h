#pragma once
#include "build_options.h"

#ifdef REED_SWITCH_INCLUDED
extern const unsigned char gImage_qrcycleHead[];
#endif // REED_SWITCH_INCLUDED


#ifdef VCNL4040_PROXIMITY_INCLUDED
extern const unsigned char gImage_BinBattery[];
#endif // VCNL4040_PROXIMITY_INCLUDED


#ifndef REED_SWITCH_INCLUDED
extern const unsigned char pressA_image[];
#endif

