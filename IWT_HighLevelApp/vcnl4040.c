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


#include "vcnl4040.h"



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


//Check comm with sensor and set it to default init settings
boolean vcnl4040_begin(const int isu) {
	

	initI2c(isu);

	vcnl4040_readWhoAmI();

	//Configure the various parts of the sensor
	vcnl4040_setLEDCurrent(200); //Max IR LED current

	vcnl4040_setIRDutyCycle(40); //Set to highest duty cycle

	vcnl4040_setProxIntegrationTime(8); //Set to max integration

	vcnl4040_setProxResolution(16); //Set to 16-bit output

	vcnl4040_enableSmartPersistance(); //Turn on smart presistance

	vcnl4040_powerOnProximity(); //Turn on prox sensing

	//vcnl4040_setAmbientIntegrationTime(VCNL4040_ALS_IT_80MS); //Keep it short
	//vcnl4040_powerOnAmbient(); //Turn on ambient sensing

	return (true);
}

//Test to see if the device is responding
boolean vcnl4040_isConnected(void) {
  // do nothing
  return (true);
}

//Set the duty cycle of the IR LED. The higher the duty
//ratio, the faster the response time achieved with higher power
//consumption. For example, PS_Duty = 1/320, peak IRED current = 100 mA,
//averaged current consumption is 100 mA/320 = 0.3125 mA.
void vcnl4040_setIRDutyCycle(uint16_t dutyValue)
{
  if(dutyValue > 320 - 1) dutyValue = VCNL4040_PS_DUTY_320;
  else if(dutyValue > 160 - 1) dutyValue = VCNL4040_PS_DUTY_160;
  else if(dutyValue > 80 - 1) dutyValue = VCNL4040_PS_DUTY_80;
  else dutyValue = VCNL4040_PS_DUTY_40;
  
  bitMask(VCNL4040_PS_CONF1, LOWER, VCNL4040_PS_DUTY_MASK, dutyValue);
}

//Set the Prox interrupt persistance value
//The PS persistence function (PS_PERS, 1, 2, 3, 4) helps to avoid
//false trigger of the PS INT. It defines the amount of
//consecutive hits needed in order for a PS interrupt event to be triggered.
void vcnl4040_setProxInterruptPersistance(uint8_t persValue)
{
  bitMask(VCNL4040_PS_CONF1, LOWER, VCNL4040_PS_PERS_MASK, persValue);
}

//Set the Ambient interrupt persistance value
//The ALS persistence function (ALS_PERS, 1, 2, 4, 8) helps to avoid
//false trigger of the ALS INT. It defines the amount of
//consecutive hits needed in order for a ALS interrupt event to be triggered.
void vcnl4040_setAmbientInterruptPersistance(uint8_t persValue)
{
  bitMask(VCNL4040_ALS_CONF, LOWER, VCNL4040_ALS_PERS_MASK, persValue);
}

void vcnl4040_enableAmbientInterrupts(void)
{
  bitMask(VCNL4040_ALS_CONF, LOWER, VCNL4040_ALS_INT_EN_MASK, VCNL4040_ALS_INT_ENABLE);
}
void vcnl4040_disableAmbientInterrupts(void)
{
  bitMask(VCNL4040_ALS_CONF, LOWER, VCNL4040_ALS_INT_EN_MASK, VCNL4040_ALS_INT_DISABLE);
}

//Power on or off the ambient light sensing portion of the sensor
void vcnl4040_powerOnAmbient(void)
{
  bitMask(VCNL4040_ALS_CONF, LOWER, VCNL4040_ALS_SD_MASK, VCNL4040_ALS_SD_POWER_ON);
}
void vcnl4040_powerOffAmbient(void)
{
  bitMask(VCNL4040_ALS_CONF, LOWER, VCNL4040_ALS_SD_MASK, VCNL4040_ALS_SD_POWER_OFF);
}

//Sets the integration time for the ambient light sensor
void vcnl4040_setAmbientIntegrationTime(uint16_t timeValue)
{
  if(timeValue > 640 - 1) timeValue = VCNL4040_ALS_IT_640MS;
  else if(timeValue > 320 - 1) timeValue = VCNL4040_ALS_IT_320MS;
  else if(timeValue > 160 - 1) timeValue = VCNL4040_ALS_IT_160MS;
  else timeValue = VCNL4040_ALS_IT_80MS;

  bitMask(VCNL4040_ALS_CONF, LOWER, VCNL4040_ALS_IT_MASK, timeValue);
}

//Sets the integration time for the proximity sensor
void vcnl4040_setProxIntegrationTime(uint8_t timeValue)
{
  if(timeValue > 8 - 1) timeValue = VCNL4040_PS_IT_8T;
  else if(timeValue > 4 - 1) timeValue = VCNL4040_PS_IT_4T;
  else if(timeValue > 3 - 1) timeValue = VCNL4040_PS_IT_3T;
  else if(timeValue > 2 - 1) timeValue = VCNL4040_PS_IT_2T;
  else timeValue = VCNL4040_PS_IT_1T;

  bitMask(VCNL4040_PS_CONF1, LOWER, VCNL4040_PS_IT_MASK, timeValue);
}

//Power on the prox sensing portion of the device
void vcnl4040_powerOnProximity(void)
{
  bitMask(VCNL4040_PS_CONF1, LOWER, VCNL4040_PS_SD_MASK, VCNL4040_PS_SD_POWER_ON);
}

//Power off the prox sensing portion of the device
void vcnl4040_powerOffProximity(void)
{
  bitMask(VCNL4040_PS_CONF1, LOWER, VCNL4040_PS_SD_MASK, VCNL4040_PS_SD_POWER_OFF);
}

//Sets the proximity resolution
void vcnl4040_setProxResolution(uint8_t resolutionValue)
{
	if(resolutionValue > 16 - 1) resolutionValue = VCNL4040_PS_HD_16_BIT;
	else resolutionValue = VCNL4040_PS_HD_12_BIT;
	
  bitMask(VCNL4040_PS_CONF2, UPPER, VCNL4040_PS_HD_MASK, resolutionValue);
}

//Sets the proximity interrupt type
void vcnl4040_setProxInterruptType(uint8_t interruptValue)
{
  bitMask(VCNL4040_PS_CONF2, UPPER, VCNL4040_PS_INT_MASK, interruptValue);
}

//Enable smart persistance
//To accelerate the PS response time, smart
//persistence prevents the misjudgment of proximity sensing
//but also keeps a fast response time.
void vcnl4040_enableSmartPersistance(void)
{
  bitMask(VCNL4040_PS_CONF3, LOWER, VCNL4040_PS_SMART_PERS_MASK, VCNL4040_PS_SMART_PERS_ENABLE);
}
void vcnl4040_disableSmartPersistance(void)
{
  bitMask(VCNL4040_PS_CONF3, LOWER, VCNL4040_PS_SMART_PERS_MASK, VCNL4040_PS_SMART_PERS_DISABLE);
}

//Enable active force mode
//An extreme power saving way to use PS is to apply PS active force mode.
//Anytime host would like to request one proximity measurement,
//enable the active force mode. This
//triggers a single PS measurement, which can be read from the PS result registers.
//VCNL4040 stays in standby mode constantly.
void vcnl4040_enableActiveForceMode(void)
{
  bitMask(VCNL4040_PS_CONF3, LOWER, VCNL4040_PS_AF_MASK, VCNL4040_PS_AF_ENABLE);
}
void vcnl4040_disableActiveForceMode(void)
{
  bitMask(VCNL4040_PS_CONF3, LOWER, VCNL4040_PS_AF_MASK, VCNL4040_PS_AF_DISABLE);
}

//Set trigger bit so sensor takes a force mode measurement and returns to standby
void vcnl4040_takeSingleProxMeasurement(void)
{
  bitMask(VCNL4040_PS_CONF3, LOWER, VCNL4040_PS_TRIG_MASK, VCNL4040_PS_TRIG_TRIGGER);
}

//Enable the white measurement channel
void vcnl4040_enableWhiteChannel(void)
{
  bitMask(VCNL4040_PS_MS, UPPER, VCNL4040_WHITE_EN_MASK, VCNL4040_WHITE_ENABLE);
}
void vcnl4040_disableWhiteChannel(void)
{
  bitMask(VCNL4040_PS_MS, UPPER, VCNL4040_WHITE_EN_MASK, VCNL4040_WHITE_ENABLE);
}

//Enable the proximity detection logic output mode
//When this mode is selected, the INT pin is pulled low when an object is
//close to the sensor (value is above high
//threshold) and is reset to high when the object moves away (value is
//below low threshold). Register: PS_THDH / PS_THDL
//define where these threshold levels are set.
void vcnl4040_enableProxLogicMode(void)
{
  bitMask(VCNL4040_PS_MS, UPPER, VCNL4040_PS_MS_MASK, VCNL4040_PS_MS_ENABLE);
}
void vcnl4040_disableProxLogicMode(void)
{
  bitMask(VCNL4040_PS_MS, UPPER, VCNL4040_PS_MS_MASK, VCNL4040_PS_MS_DISABLE);
}

//Set the IR LED sink current to one of 8 settings
void vcnl4040_setLEDCurrent(uint8_t currentValue)
{
	if(currentValue > 200 - 1) currentValue = VCNL4040_LED_200MA;
	else if(currentValue > 180 - 1) currentValue = VCNL4040_LED_180MA;
	else if(currentValue > 160 - 1) currentValue = VCNL4040_LED_160MA;
	else if(currentValue > 140 - 1) currentValue = VCNL4040_LED_140MA;
	else if(currentValue > 120 - 1) currentValue = VCNL4040_LED_120MA;
	else if(currentValue > 100 - 1) currentValue = VCNL4040_LED_100MA;
	else if(currentValue > 75 - 1) currentValue = VCNL4040_LED_75MA;
	else currentValue = VCNL4040_LED_50MA;

	bitMask(VCNL4040_PS_MS, UPPER, VCNL4040_LED_I_MASK, currentValue);
}

//Set the proximity sensing cancelation value - helps reduce cross talk
//with ambient light
void vcnl4040_setProxCancellation(uint16_t cancelValue)
{
  writeCommand(VCNL4040_PS_CANC, cancelValue);
}

//Value that ALS must go above to trigger an interrupt
void vcnl4040_setALSHighThreshold(uint16_t threshold)
{
  writeCommand(VCNL4040_ALS_THDH, threshold);
}

//Value that ALS must go below to trigger an interrupt
void vcnl4040_setALSLowThreshold(uint16_t threshold)
{
  writeCommand(VCNL4040_ALS_THDL, threshold);
}

//Value that Proximity Sensing must go above to trigger an interrupt
void vcnl4040_setProxHighThreshold(uint16_t threshold)
{
  writeCommand(VCNL4040_PS_THDH, threshold);
}

//Value that Proximity Sensing must go below to trigger an interrupt
void vcnl4040_setProxLowThreshold(uint16_t threshold)
{
  writeCommand(VCNL4040_PS_THDL, threshold);
}

//Read the Proximity value
uint16_t vcnl4040_getProximity(void)
{
  return (readCommand(VCNL4040_PS_DATA));
}

//Read the Ambient light value
uint16_t vcnl4040_getAmbient(void)
{
  return (readCommand(VCNL4040_ALS_DATA));
}

//Read the White light value
uint16_t vcnl4040_getWhite(void)
{
  return (readCommand(VCNL4040_WHITE_DATA));
}

//Read the sensors ID
uint16_t vcnl4040_getID(void)
{
  return (readCommand(VCNL4040_ID));
}

//Returns true if the prox value rises above the upper threshold
boolean vcnl4040_isClose(void)
{
  uint8_t interruptFlags = readCommandUpper(VCNL4040_INT_FLAG);
  return (interruptFlags & VCNL4040_INT_FLAG_CLOSE);
}

//Returns true if the prox value drops below the lower threshold
boolean vcnl4040_isAway(void)
{
  uint8_t interruptFlags = readCommandUpper(VCNL4040_INT_FLAG);
  return (interruptFlags & VCNL4040_INT_FLAG_AWAY);
}

//Returns true if the prox value rises above the upper threshold
boolean vcnl4040_isLight(void)
{
  uint8_t interruptFlags = readCommandUpper(VCNL4040_INT_FLAG);
  return (interruptFlags & VCNL4040_INT_FLAG_ALS_HIGH);
}

//Returns true if the ALS value drops below the lower threshold
boolean vcnl4040_isDark(void)
{
  uint8_t interruptFlags = readCommandUpper(VCNL4040_INT_FLAG);
  return (interruptFlags & VCNL4040_INT_FLAG_ALS_LOW);
}

int vcnl4040_readWhoAmI(void)
{

	static const uint8_t whoAmIRegId = VCNL4040_ID;
	static const uint16_t expectedWhoAmI = 0x0186;
	uint16_t actualWhoAmI;

	// Read register value using AppLibs combination read and write API.
	ssize_t transferredBytes =
		I2CMaster_WriteThenRead(i2cFd, VCNL4040_ADDR, &whoAmIRegId, sizeof(whoAmIRegId),
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

/// <summary>
///     Closes the I2C interface File Descriptors.
/// </summary>
void vcnl4040_closeI2c(void) {

	CloseFdAndPrintError(i2cFd, "i2c");

}



//Given a command code (address) write to the lower byte without affecting the upper byte
static boolean writeCommandLower(uint8_t commandCode, uint8_t newValue)
{
  uint16_t commandValue = readCommand(commandCode);
  commandValue &= 0xFF00; //Remove lower 8 bits
  commandValue |= (uint16_t)newValue; //Mask in
  return (writeCommand(commandCode, commandValue));
}

//Given a command code (address) write to the upper byte without affecting the lower byte
static boolean writeCommandUpper(uint8_t commandCode, uint8_t newValue)
{
  uint16_t commandValue = readCommand(commandCode);
  commandValue &= 0x00FF; //Remove upper 8 bits
  commandValue |= (uint16_t)newValue << 8; //Mask in
  return (writeCommand(commandCode, commandValue));
}

//Given a command code (address) read the lower byte
static uint8_t readCommandLower(uint8_t commandCode)
{
  uint16_t commandValue = readCommand(commandCode);
  return (commandValue & 0xFF);
}

//Given a command code (address) read the upper byte
static uint8_t readCommandUpper(uint8_t commandCode)
{
  uint16_t commandValue = readCommand(commandCode);
  return (commandValue >> 8);
}

//Given a register, read it, mask it, and then set the thing
//commandHeight is used to select between the upper or lower byte of command register
//Example:
//Write dutyValue into PS_CONF1, lower byte, using the Duty_Mask
//bitMask(VCNL4040_PS_CONF1, LOWER, VCNL4040_PS_DUTY_MASK, dutyValue);
static void bitMask(uint8_t commandAddress, boolean commandHeight, uint8_t mask, uint8_t thing)
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


/// <summary>
///     Initializes the I2C interface.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static int initI2c(const int isu ) {

	// Begin MT3620 I2C init 
			
	Log_Debug("INFO: Begin I2C init [ISU=%d].\n", isu);


	i2cFd = I2CMaster_Open(isu);
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


//Reads two consecutive bytes from a given 'command code' location
static uint16_t readCommand(uint8_t commandCode)
{
	uint16_t answer;

	// Read register value using AppLibs combination read and write API.
	ssize_t transferredBytes =
		I2CMaster_WriteThenRead(i2cFd, VCNL4040_ADDR, &commandCode, sizeof(commandCode),
			&answer, sizeof(answer));
	if (!CheckTransferSize("I2CMaster_WriteThenRead (readCommand)",
		sizeof(commandCode) + sizeof(answer), transferredBytes)) {
		return -1;
	}
	return answer;
}

//Write two bytes to a given command code location (8 bits)
static int writeCommand(const uint8_t regAddress, const uint16_t value)
{
	Log_Debug("Write [RegAddress = 0x%02x, Value = 0x%04x].\n", regAddress, value);
	const uint8_t command[] = { regAddress, value & 0xFF,value >> 8 };
	ssize_t transferredBytes =
		I2CMaster_Write(i2cFd, VCNL4040_ADDR, command, sizeof(command));
	if (!CheckTransferSize("I2CMaster_Write (command)", sizeof(command), transferredBytes)) {
		return -1;
	}


	return (true);
}

