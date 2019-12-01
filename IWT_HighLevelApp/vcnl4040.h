/**
  Adapted from: https://github.com/sparkfun/SparkFun_VCNL4040_Arduino_Library/blob/master/src/SparkFun_VCNL4040_Arduino_Library.cpp
  Development environment specifics:
  Azure Sphere 10.9

  Original comment:

  This is a library written for the VCNL4040 distance sensor.
  By Nathan Seidle @ SparkFun Electronics, April 17th, 2018
  The VCNL4040 is a simple IR presence and ambient light sensor. This 
  sensor is excellent for detecting if something has appeared in front 
  of the sensor. We often see this type of sensor on automatic towel 
  dispensers, automatic faucets, etc. You can detect objects qualitatively 
  up to 20cm away. This means you can detect if something is there, 
  and if it is closer or further away since the last reading, but it's 
  difficult to say it is 7.2cm away. If you need quantitative distance 
  readings (for example sensing that an object is 177mm away) check out 
  the SparkFun Time of Flight (ToF) sensors with mm accuracy.
  This library offers the full range of settings for the VCNL4040. Checkout
  the various examples provided with the library but also please give the datasheet
  a read.
  https://github.com/sparkfun/SparkFun_VCNL4040_Arduino_Library
  Development environment specifics:
  Arduino IDE 1.8.5
  SparkFun labored with love to create this code. Feel like supporting open
  source hardware? Buy a board from SparkFun!
  https://www.sparkfun.com/products/14690
*/

#ifndef VCNL4040_H
#define VCNL4040_H
#include "vcnl4040_hardware.h"

#include<stdint.h>
#include<stdbool.h>
#include <string.h>
#include <errno.h>

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"

#include <applibs/log.h>
#include <applibs/i2c.h>
#include "epoll_timerfd_utilities.h"


#define boolean bool

static const uint8_t VCNL4040_ALS_IT_MASK = (uint8_t)~((1 << 7) | (1 << 6));
static const uint8_t VCNL4040_ALS_IT_80MS = 0;
static const uint8_t VCNL4040_ALS_IT_160MS = (1 << 7);
static const uint8_t VCNL4040_ALS_IT_320MS = (1 << 6);
static const uint8_t VCNL4040_ALS_IT_640MS = (1 << 7) | (1 << 6);

static const uint8_t VCNL4040_ALS_PERS_MASK = (uint8_t)~((1 << 3) | (1 << 2));
static const uint8_t VCNL4040_ALS_PERS_1 = 0;
static const uint8_t VCNL4040_ALS_PERS_2 = (1 << 2);
static const uint8_t VCNL4040_ALS_PERS_4 = (1 << 3);
static const uint8_t VCNL4040_ALS_PERS_8 = (1 << 3) | (1 << 2);

static const uint8_t VCNL4040_ALS_INT_EN_MASK = (uint8_t)~((1 << 1));
static const uint8_t VCNL4040_ALS_INT_DISABLE = 0;
static const uint8_t VCNL4040_ALS_INT_ENABLE = (1 << 1);

static const uint8_t VCNL4040_ALS_SD_MASK = (uint8_t)~((1 << 0));
static const uint8_t VCNL4040_ALS_SD_POWER_ON = 0;
static const uint8_t VCNL4040_ALS_SD_POWER_OFF = (1 << 0);

static const uint8_t VCNL4040_PS_DUTY_MASK = (uint8_t)~((1 << 7) | (1 << 6));
static const uint8_t VCNL4040_PS_DUTY_40 = 0;
static const uint8_t VCNL4040_PS_DUTY_80 = (1 << 6);
static const uint8_t VCNL4040_PS_DUTY_160 = (1 << 7);
static const uint8_t VCNL4040_PS_DUTY_320 = (1 << 7) | (1 << 6);

static const uint8_t VCNL4040_PS_PERS_MASK = (uint8_t)~((1 << 5) | (1 << 4));
static const uint8_t VCNL4040_PS_PERS_1 = 0;
static const uint8_t VCNL4040_PS_PERS_2 = (1 << 4);
static const uint8_t VCNL4040_PS_PERS_3 = (1 << 5);
static const uint8_t VCNL4040_PS_PERS_4 = (1 << 5) | (1 << 4);

static const uint8_t VCNL4040_PS_IT_MASK = (uint8_t)~((1 << 3) | (1 << 2) | (1 << 1));
static const uint8_t VCNL4040_PS_IT_1T = 0;
static const uint8_t VCNL4040_PS_IT_15T = (1 << 1);
static const uint8_t VCNL4040_PS_IT_2T = (1 << 2);
static const uint8_t VCNL4040_PS_IT_25T = (1 << 2) | (1 << 1);
static const uint8_t VCNL4040_PS_IT_3T = (1 << 3);
static const uint8_t VCNL4040_PS_IT_35T = (1 << 3) | (1 << 1);
static const uint8_t VCNL4040_PS_IT_4T = (1 << 3) | (1 << 2);
static const uint8_t VCNL4040_PS_IT_8T = (1 << 3) | (1 << 2) | (1 << 1);

static const uint8_t VCNL4040_PS_SD_MASK = (uint8_t)~((1 << 0));
static const uint8_t VCNL4040_PS_SD_POWER_ON = 0;
static const uint8_t VCNL4040_PS_SD_POWER_OFF = (1 << 0);

static const uint8_t VCNL4040_PS_HD_MASK = (uint8_t)~((1 << 3));
static const uint8_t VCNL4040_PS_HD_12_BIT = 0;
static const uint8_t VCNL4040_PS_HD_16_BIT = (1 << 3);

static const uint8_t VCNL4040_PS_INT_MASK = (uint8_t)~((1 << 1) | (1 << 0));
static const uint8_t VCNL4040_PS_INT_DISABLE = 0;
static const uint8_t VCNL4040_PS_INT_CLOSE = (1 << 0);
static const uint8_t VCNL4040_PS_INT_AWAY = (1 << 1);
static const uint8_t VCNL4040_PS_INT_BOTH = (1 << 1) | (1 << 0);

static const uint8_t VCNL4040_PS_SMART_PERS_MASK = (uint8_t)~((1 << 4));
static const uint8_t VCNL4040_PS_SMART_PERS_DISABLE = 0;
static const uint8_t VCNL4040_PS_SMART_PERS_ENABLE = (1 << 1);

static const uint8_t VCNL4040_PS_AF_MASK = (uint8_t)~((1 << 3));
static const uint8_t VCNL4040_PS_AF_DISABLE = 0;
static const uint8_t VCNL4040_PS_AF_ENABLE = (1 << 3);

static const uint8_t VCNL4040_PS_TRIG_MASK = (uint8_t)~((1 << 3));
static const uint8_t VCNL4040_PS_TRIG_TRIGGER = (1 << 2);

static const uint8_t VCNL4040_WHITE_EN_MASK = (uint8_t)~((1 << 7));
static const uint8_t VCNL4040_WHITE_ENABLE = 0;
static const uint8_t VCNL4040_WHITE_DISABLE = (1 << 7);

static const uint8_t VCNL4040_PS_MS_MASK = (uint8_t)~((1 << 6));
static const uint8_t VCNL4040_PS_MS_DISABLE = 0;
static const uint8_t VCNL4040_PS_MS_ENABLE = (1 << 6);

static const uint8_t VCNL4040_LED_I_MASK = (uint8_t)~((1 << 2) | (1 << 1) | (1 << 0));
static const uint8_t VCNL4040_LED_50MA = 0;
static const uint8_t VCNL4040_LED_75MA = (1 << 0);
static const uint8_t VCNL4040_LED_100MA = (1 << 1);
static const uint8_t VCNL4040_LED_120MA = (1 << 1) | (1 << 0);
static const uint8_t VCNL4040_LED_140MA = (1 << 2);
static const uint8_t VCNL4040_LED_160MA = (1 << 2) | (1 << 0);
static const uint8_t VCNL4040_LED_180MA = (1 << 2) | (1 << 1);
static const uint8_t VCNL4040_LED_200MA = (1 << 2) | (1 << 1) | (1 << 0);

static const uint8_t VCNL4040_INT_FLAG_ALS_LOW = (1 << 5);
static const uint8_t VCNL4040_INT_FLAG_ALS_HIGH = (1 << 4);
static const uint8_t VCNL4040_INT_FLAG_CLOSE = (1 << 1);
static const uint8_t VCNL4040_INT_FLAG_AWAY = (1 << 0);



boolean vcnl4040_begin(const int isu);
boolean vcnl4040_isConnected(void); //True if sensor responded to I2C query

void vcnl4040_setIRDutyCycle(uint16_t dutyValue);

void vcnl4040_setProxInterruptPersistance(uint8_t persValue);
void vcnl4040_setAmbientInterruptPersistance(uint8_t persValue);

void vcnl4040_setProxIntegrationTime(uint8_t timeValue); //Sets the integration time for the proximity sensor
void vcnl4040_setAmbientIntegrationTime(uint16_t timeValue);

void vcnl4040_powerOnProximity(void); //Power on the prox sensing portion of the device
void vcnl4040_powerOffProximity(void); //Power off the prox sensing portion of the device

void vcnl4040_powerOnAmbient(void); //Power on the ALS sensing portion of the device
void vcnl4040_powerOffAmbient(void); //Power off the ALS sensing portion of the device

void vcnl4040_setProxResolution(uint8_t resolutionValue);//Sets the proximity resolution to 12 or 16 bit

void vcnl4040_enableAmbientInterrupts(void);
void vcnl4040_disableAmbientInterrupts(void);

void vcnl4040_enableSmartPersistance(void);
void vcnl4040_disableSmartPersistance(void);

void vcnl4040_enableActiveForceMode(void);
void vcnl4040_disableActiveForceMode(void);
void vcnl4040_takeSingleProxMeasurement(void);

void vcnl4040_enableWhiteChannel(void);
void vcnl4040_disableWhiteChannel(void);

void vcnl4040_enableProxLogicMode(void);
void vcnl4040_disableProxLogicMode(void);

void vcnl4040_setLEDCurrent(uint8_t currentValue);

void vcnl4040_setProxCancellation(uint16_t cancelValue);
void vcnl4040_setALSHighThreshold(uint16_t threshold);
void vcnl4040_setALSLowThreshold(uint16_t threshold);
void vcnl4040_setProxHighThreshold(uint16_t threshold);
void vcnl4040_setProxLowThreshold(uint16_t threshold);

uint16_t vcnl4040_getProximity(void);
uint16_t vcnl4040_getAmbient(void);
uint16_t vcnl4040_getWhite(void);
uint16_t vcnl4040_getID(void);

void vcnl4040_setProxInterruptType(uint8_t interruptValue); //Enable four prox interrupt types
boolean vcnl4040_isClose(void); //Interrupt flag: True if prox value greater than high threshold
boolean vcnl4040_isAway(void); //Interrupt flag: True if prox value lower than low threshold
boolean vcnl4040_isLight(void); //Interrupt flag: True if ALS value higher than high threshold
boolean vcnl4040_isDark(void); //Interrupt flag: True if ALS value lower than low threshold

int vcnl4040_readWhoAmI(void);
void vcnl4040_closeI2c(void);

static uint16_t readCommand(uint8_t commandCode);
static uint8_t readCommandLower(uint8_t commandCode);
static uint8_t readCommandUpper(uint8_t commandCode);

static int writeCommand(const uint8_t regAddress, const uint16_t value);
static boolean writeCommandLower(uint8_t commandCode, uint8_t newValue);
static boolean writeCommandUpper(uint8_t commandCode, uint8_t newValue);

static void bitMask(uint8_t commandAddress, boolean commandHeight, uint8_t mask, uint8_t thing);

//////
static int initI2c(const int isu);
static bool CheckTransferSize(const char* desc, size_t expectedBytes, ssize_t actualBytes);
static int writeCommand(const uint8_t regAddress, const uint16_t value);

	
#endif // VCNL4040_H