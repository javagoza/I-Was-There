/************************************************************************
* Adapted from: https://github.com/sparkfun/SparkFun_VCNL4040_Arduino_Library/blob/master/src/SparkFun_VCNL4040_Arduino_Library.cpp
************************************************************************/

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"

#include <applibs/log.h>
#include <applibs/i2c.h>

#include "mt3620_avnet_dev.h"
#include "deviceTwin.h"
#include "azure_iot_utilities.h"
#include "build_options.h"

#include "vcnl4040.h"

void bitMask(uint8_t commandAddress, bool commandHeight, uint8_t mask, uint8_t thing);
int writeCommand(const uint8_t commandCode, const uint16_t value);
int writeCommandLower(uint8_t commandCode, uint8_t newValue);
uint16_t readCommand(uint8_t commandCode);
uint16_t getID(void);


#define ENABLE_READ_WRITE_DEBUG

const uint8_t VCNL4040_ADDR = 0x60; //7-bit unshifted I2C address of VCNL4040

//Used to select between upper and lower byte of command register
#define LOWER true
#define UPPER false


//VCNL4040 Command Codes
#define VCNL4040_ALS_CONF 0x00
#define VCNL4040_ALS_THDH 0x01
#define VCNL4040_ALS_THDL 0x02
#define VCNL4040_PS_CONF1 0x03 //Lower
#define VCNL4040_PS_CONF2 0x03 //Upper
#define VCNL4040_PS_CONF3 0x04 //Lower
#define VCNL4040_PS_MS 0x04 //Upper
#define VCNL4040_PS_CANC 0x05
#define VCNL4040_PS_THDL 0x06
#define VCNL4040_PS_THDH 0x07
#define VCNL4040_PS_DATA 0x08
#define VCNL4040_ALS_DATA 0x09
#define VCNL4040_WHITE_DATA 0x0A
#define VCNL4040_INT_FLAG 0x0B //Upper
#define VCNL4040_ID 0x0C

//Extern variables
int i2cFd = -1;


const uint8_t vcnl4040Address = 0x60;     // Addr = 0x60

/// <summary>
///    Checks the number of transferred bytes for I2C functions and prints an error
///    message if the functions failed or if the number of bytes is different than
///    expected number of bytes to be transferred.
/// </summary>
/// <returns>true on success, or false on failure</returns>
static bool CheckTransferSize(const char* desc, size_t expectedBytes, ssize_t actualBytes)
{
	if (actualBytes < 0) {
		Log_Debug("ERROR: %s: errno=%d (%s)\n", desc, errno, strerror(errno));
		return false;
	}

	if (actualBytes != (ssize_t)expectedBytes) {
		Log_Debug("ERROR: %s: transferred %zd bytes; expected %zd\n", desc, actualBytes,
			expectedBytes);
		return false;
	}

	return true;
}

static int ReadWhoAmI(void)
{
	
	static const uint8_t whoAmIRegId = VCNL4040_ID;
	static const uint16_t expectedWhoAmI = 0x0186;
	uint16_t actualWhoAmI;

	// Read register value using AppLibs combination read and write API.
	ssize_t transferredBytes =
		I2CMaster_WriteThenRead(i2cFd, vcnl4040Address, &whoAmIRegId, sizeof(whoAmIRegId),
			&actualWhoAmI, sizeof(actualWhoAmI));
	if (!CheckTransferSize("I2CMaster_WriteThenRead (WHO_AM_I)",
		sizeof(whoAmIRegId) + sizeof(actualWhoAmI), transferredBytes)) {
		return -1;
	}
	Log_Debug("INFO: WHO_AM_I=0x%02x (I2CMaster_WriteThenRead)\n", actualWhoAmI);
	if (actualWhoAmI != expectedWhoAmI) {
		Log_Debug("ERROR: Unexpected WHO_AM_I value.\n");
		return -1;
	}

	return 0;
}


//Set the duty cycle of the IR LED. The higher the duty
//ratio, the faster the response time achieved with higher power
//consumption. For example, PS_Duty = 1/320, peak IRED current = 100 mA,
//averaged current consumption is 100 mA/320 = 0.3125 mA.
void setIRDutyCycle(uint16_t dutyValue)
{
	if (dutyValue > 320 - 1) dutyValue = VCNL4040_PS_DUTY_320;
	else if (dutyValue > 160 - 1) dutyValue = VCNL4040_PS_DUTY_160;
	else if (dutyValue > 80 - 1) dutyValue = VCNL4040_PS_DUTY_80;
	else dutyValue = VCNL4040_PS_DUTY_40;

	bitMask(VCNL4040_PS_CONF1, LOWER, VCNL4040_PS_DUTY_MASK, dutyValue);
}

//Set the Prox interrupt persistance value
//The PS persistence function (PS_PERS, 1, 2, 3, 4) helps to avoid
//false trigger of the PS INT. It defines the amount of
//consecutive hits needed in order for a PS interrupt event to be triggered.
void setProxInterruptPersistance(uint8_t persValue)
{
	bitMask(VCNL4040_PS_CONF1, LOWER, VCNL4040_PS_PERS_MASK, persValue);
}

//Set the Ambient interrupt persistance value
//The ALS persistence function (ALS_PERS, 1, 2, 4, 8) helps to avoid
//false trigger of the ALS INT. It defines the amount of
//consecutive hits needed in order for a ALS interrupt event to be triggered.
void setAmbientInterruptPersistance(uint8_t persValue)
{
	bitMask(VCNL4040_ALS_CONF, LOWER, VCNL4040_ALS_PERS_MASK, persValue);
}

void enableAmbientInterrupts(void)
{
	bitMask(VCNL4040_ALS_CONF, LOWER, VCNL4040_ALS_INT_EN_MASK, VCNL4040_ALS_INT_ENABLE);
}
void disableAmbientInterrupts(void)
{
	bitMask(VCNL4040_ALS_CONF, LOWER, VCNL4040_ALS_INT_EN_MASK, VCNL4040_ALS_INT_DISABLE);
}

//Power on or off the ambient light sensing portion of the sensor
void powerOnAmbient(void)
{
	bitMask(VCNL4040_ALS_CONF, LOWER, VCNL4040_ALS_SD_MASK, VCNL4040_ALS_SD_POWER_ON);
}
void powerOffAmbient(void)
{
	bitMask(VCNL4040_ALS_CONF, LOWER, VCNL4040_ALS_SD_MASK, VCNL4040_ALS_SD_POWER_OFF);
}

//Sets the integration time for the ambient light sensor
void setAmbientIntegrationTime(uint16_t timeValue)
{
	if (timeValue > 640 - 1) timeValue = VCNL4040_ALS_IT_640MS;
	else if (timeValue > 320 - 1) timeValue = VCNL4040_ALS_IT_320MS;
	else if (timeValue > 160 - 1) timeValue = VCNL4040_ALS_IT_160MS;
	else timeValue = VCNL4040_ALS_IT_80MS;

	bitMask(VCNL4040_ALS_CONF, LOWER, VCNL4040_ALS_IT_MASK, timeValue);
}

//Sets the integration time for the proximity sensor
void setProxIntegrationTime(uint8_t timeValue)
{
	if (timeValue > 8 - 1) timeValue = VCNL4040_PS_IT_8T;
	else if (timeValue > 4 - 1) timeValue = VCNL4040_PS_IT_4T;
	else if (timeValue > 3 - 1) timeValue = VCNL4040_PS_IT_3T;
	else if (timeValue > 2 - 1) timeValue = VCNL4040_PS_IT_2T;
	else timeValue = VCNL4040_PS_IT_1T;

	bitMask(VCNL4040_PS_CONF1, LOWER, VCNL4040_PS_IT_MASK, timeValue);
}

//Power on the prox sensing portion of the device
void powerOnProximity(void)
{
	bitMask(VCNL4040_PS_CONF1, LOWER, VCNL4040_PS_SD_MASK, VCNL4040_PS_SD_POWER_ON);
}

//Power off the prox sensing portion of the device
void powerOffProximity(void)
{
	bitMask(VCNL4040_PS_CONF1, LOWER, VCNL4040_PS_SD_MASK, VCNL4040_PS_SD_POWER_OFF);
}

//Sets the proximity resolution
void setProxResolution(uint8_t resolutionValue)
{
	if (resolutionValue > 16 - 1) resolutionValue = VCNL4040_PS_HD_16_BIT;
	else resolutionValue = VCNL4040_PS_HD_12_BIT;

	bitMask(VCNL4040_PS_CONF2, UPPER, VCNL4040_PS_HD_MASK, resolutionValue);
}

//Sets the proximity interrupt type
void setProxInterruptType(uint8_t interruptValue)
{
	bitMask(VCNL4040_PS_CONF2, UPPER, VCNL4040_PS_INT_MASK, interruptValue);
}

//Enable smart persistance
//To accelerate the PS response time, smart
//persistence prevents the misjudgment of proximity sensing
//but also keeps a fast response time.
void enableSmartPersistance(void)
{
	bitMask(VCNL4040_PS_CONF3, LOWER, VCNL4040_PS_SMART_PERS_MASK, VCNL4040_PS_SMART_PERS_ENABLE);
}
void disableSmartPersistance(void)
{
	bitMask(VCNL4040_PS_CONF3, LOWER, VCNL4040_PS_SMART_PERS_MASK, VCNL4040_PS_SMART_PERS_DISABLE);
}

//Enable active force mode
//An extreme power saving way to use PS is to apply PS active force mode.
//Anytime host would like to request one proximity measurement,
//enable the active force mode. This
//triggers a single PS measurement, which can be read from the PS result registers.
//VCNL4040 stays in standby mode constantly.
void enableActiveForceMode(void)
{
	bitMask(VCNL4040_PS_CONF3, LOWER, VCNL4040_PS_AF_MASK, VCNL4040_PS_AF_ENABLE);
}
void disableActiveForceMode(void)
{
	bitMask(VCNL4040_PS_CONF3, LOWER, VCNL4040_PS_AF_MASK, VCNL4040_PS_AF_DISABLE);
}

//Set trigger bit so sensor takes a force mode measurement and returns to standby
void takeSingleProxMeasurement(void)
{
	bitMask(VCNL4040_PS_CONF3, LOWER, VCNL4040_PS_TRIG_MASK, VCNL4040_PS_TRIG_TRIGGER);
}

//Enable the white measurement channel
void enableWhiteChannel(void)
{
	bitMask(VCNL4040_PS_MS, UPPER, VCNL4040_WHITE_EN_MASK, VCNL4040_WHITE_ENABLE);
}
void disableWhiteChannel(void)
{
	bitMask(VCNL4040_PS_MS, UPPER, VCNL4040_WHITE_EN_MASK, VCNL4040_WHITE_ENABLE);
}

//Enable the proximity detection logic output mode
//When this mode is selected, the INT pin is pulled low when an object is
//close to the sensor (value is above high
//threshold) and is reset to high when the object moves away (value is
//below low threshold). Register: PS_THDH / PS_THDL
//define where these threshold levels are set.
void enableProxLogicMode(void)
{
	bitMask(VCNL4040_PS_MS, UPPER, VCNL4040_PS_MS_MASK, VCNL4040_PS_MS_ENABLE);
}
void disableProxLogicMode(void)
{
	bitMask(VCNL4040_PS_MS, UPPER, VCNL4040_PS_MS_MASK, VCNL4040_PS_MS_DISABLE);
}

//Set the IR LED sink current to one of 8 settings
void setLEDCurrent(uint8_t currentValue)
{
	if (currentValue > 200 - 1) currentValue = VCNL4040_LED_200MA;
	else if (currentValue > 180 - 1) currentValue = VCNL4040_LED_180MA;
	else if (currentValue > 160 - 1) currentValue = VCNL4040_LED_160MA;
	else if (currentValue > 140 - 1) currentValue = VCNL4040_LED_140MA;
	else if (currentValue > 120 - 1) currentValue = VCNL4040_LED_120MA;
	else if (currentValue > 100 - 1) currentValue = VCNL4040_LED_100MA;
	else if (currentValue > 75 - 1) currentValue = VCNL4040_LED_75MA;
	else currentValue = VCNL4040_LED_50MA;

	bitMask(VCNL4040_PS_MS, UPPER, VCNL4040_LED_I_MASK, currentValue);
}

//Set the proximity sensing cancelation value - helps reduce cross talk
//with ambient light
void setProxCancellation(uint16_t cancelValue)
{
	writeCommand(VCNL4040_PS_CANC, cancelValue);
}

//Value that ALS must go above to trigger an interrupt
void setALSHighThreshold(uint16_t threshold)
{
	writeCommand(VCNL4040_ALS_THDH, threshold);
}

//Value that ALS must go below to trigger an interrupt
void setALSLowThreshold(uint16_t threshold)
{
	writeCommand(VCNL4040_ALS_THDL, threshold);
}

//Value that Proximity Sensing must go above to trigger an interrupt
void setProxHighThreshold(uint16_t threshold)
{
	writeCommand(VCNL4040_PS_THDH, threshold);
}

//Value that Proximity Sensing must go below to trigger an interrupt
void setProxLowThreshold(uint16_t threshold)
{
	writeCommand(VCNL4040_PS_THDL, threshold);
}


uint16_t proximity_get()
{
	return (readCommand(VCNL4040_PS_DATA));
}

//Read the sensors ID
uint16_t getID()
{
	return (readCommand(VCNL4040_ID));
}


/// <summary>
///     Initializes the I2C interface.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
int initI2c(void) {

	// Begin MT3620 I2C init 
		
	Log_Debug("INFO: Begin MT3620 I2C init.\n");


	i2cFd = I2CMaster_Open(MT3620_RDB_HEADER4_ISU2_I2C);
	if (i2cFd < 0) {
		Log_Debug("ERROR: I2CMaster_Open: errno=%d (%s)\n", errno, strerror(errno));
		return -1;
	}

	int result = I2CMaster_SetBusSpeed(i2cFd, I2C_BUS_SPEED_STANDARD);
	if (result != 0) {
		Log_Debug("ERROR: I2CMaster_SetBusSpeed: errno=%d (%s)\n", errno, strerror(errno));
		return -1;
	}

	result = I2CMaster_SetTimeout(i2cFd, 100);
	if (result != 0) {
		Log_Debug("ERROR: I2CMaster_SetTimeout: errno=%d (%s)\n", errno, strerror(errno));
		return -1;
	}

	
	return 0;
}

/// <summary>
///     Closes the I2C interface File Descriptors.
/// </summary>
void closeI2c(void) {

	CloseFdAndPrintError(i2cFd, "i2c");

}



//Given a command code (address) write to the lower byte without affecting the upper byte
int writeCommandLower(uint8_t commandCode, uint8_t newValue)
{
	uint16_t commandValue = readCommand(commandCode);
	commandValue &= 0xFF00; //Remove lower 8 bits
	commandValue |= (uint16_t)newValue; //Mask in
	return (writeCommand(commandCode, commandValue));
}

//Write two bytes to a given command code location (8 bits)
int writeCommand(const uint8_t regAddress, const uint16_t value)
{
	Log_Debug("Write [RegAddress = 0x%02x, Value = 0x%04x].\n", regAddress, value);
	const uint8_t command[] = { regAddress, value & 0xFF,value >> 8 };
	ssize_t transferredBytes =
		I2CMaster_Write(i2cFd, vcnl4040Address, command, sizeof(command));
	if (!CheckTransferSize("I2CMaster_Write (command)", sizeof(command), transferredBytes)) {
		return -1;
	}


	return (true);
}

//Given a command code (address) write to the upper byte without affecting the lower byte
int writeCommandUpper(uint8_t commandCode, uint8_t newValue)
{
	uint16_t commandValue = readCommand(commandCode);
	commandValue &= 0x00FF; //Remove upper 8 bits
	commandValue |= (uint16_t)newValue << 8; //Mask in
	return (writeCommand(commandCode, commandValue));
}

//Given a command code (address) read the lower byte
uint8_t readCommandLower(uint8_t commandCode)
{
	uint16_t commandValue = readCommand(commandCode);
	return (commandValue & 0xFF);
}

//Given a command code (address) read the upper byte
uint8_t readCommandUpper(uint8_t commandCode)
{
	uint16_t commandValue = readCommand(commandCode);
	return (commandValue >> 8);
}

//Given a register, read it, mask it, and then set the thing
//commandHeight is used to select between the upper or lower byte of command register
//Example:
//Write dutyValue into PS_CONF1, lower byte, using the Duty_Mask
//bitMask(VCNL4040_PS_CONF1, LOWER, VCNL4040_PS_DUTY_MASK, dutyValue);
void bitMask(uint8_t commandAddress, bool commandHeight, uint8_t mask, uint8_t thing)
{
	// Grab current register context
	uint8_t registerContents;
	if (commandHeight == LOWER) registerContents = readCommandLower(commandAddress);
	else registerContents = readCommandUpper(commandAddress);

	// Zero-out the portions of the register we're interested in
	registerContents &= mask;

	// Mask in new thing
	registerContents |= thing;

	// Change contents
	if (commandHeight == LOWER) writeCommandLower(commandAddress, registerContents);
	else writeCommandUpper(commandAddress, registerContents);
}

//Reads two consecutive bytes from a given 'command code' location
uint16_t readCommand(uint8_t commandCode)
{
	uint16_t answer;

	// Read register value using AppLibs combination read and write API.
	ssize_t transferredBytes =
		I2CMaster_WriteThenRead(i2cFd, vcnl4040Address, &commandCode, sizeof(commandCode),
			&answer, sizeof(answer));
	if (!CheckTransferSize("I2CMaster_WriteThenRead (readCommand)",
		sizeof(commandCode) + sizeof(answer), transferredBytes)) {
		return -1;
	}
	return answer;
}


int proximity_init() {
	initI2c();

	ReadWhoAmI();

	////Configure the various parts of the sensor
	setLEDCurrent(200); //Max IR LED current

	setIRDutyCycle(40); //Set to highest duty cycle

	setProxIntegrationTime(8); //Set to max integration

	setProxResolution(16); //Set to 16-bit output

	enableSmartPersistance(); //Turn on smart presistance

	powerOnProximity(); //Turn on prox sensing


	return 0;
}


