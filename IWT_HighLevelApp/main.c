/* Enrique Albertos.
   Licensed under the MIT License. */

// Securely prove that you have been to a site at a specific time with JSON Web Tokens (JWT) signed in QR format
// This appliaction creates signed JWTs in QR format and display them in an ink display
// Format of the tokens
// HEADER:ALGORITHM & TOKEN TYPE
//{
//"alg": "HS256",
//"typ" : "JWT"
//}
//PAYLOAD:DATA
//{
//  "jti": "e2da7773-a7cd-44a0-85d7-43fe0654cb22-2c0e818c-5dbda386",
//  "iat" : 1572709254
//}
//VERIFY SIGNATURE
//HMACSHA256(
//	base64UrlEncode(header) + "." +
//	base64UrlEncode(payload),
//	your-secret-key)
// 
// jti: JWT ID claim provides a unique identifier for the JWT
// Jti: device_id-random-unix_time
// iat: “Issued at” time, in Unix time, at which the token was issued
//
// Hardware: WaveShare 1.54inch e-Paper V2, Active Matrix Electrophoretic Display (AMEPD)
// https://www.waveshare.com/wiki/1.54inch_e-Paper_Module
//========================================================
// | NOTES            | AVNET KIT | Pin   | Mikro Bus    |
//========================================================
// | Reset            | GPIO_16   | RST   |  2   | RST   |
// | SPI chip select  | GPIO_34   | CS    |  3   | CS    |
// | SPI clock        | GPIO_31   | SCK   |  4   | SCK   |
// | SPI data input   | GPIO_32   | SDI   |  6   | MOSI  |
// | Power supply     | 3V3       | +3.3V |  7   | 3.3V  |
// | Ground	          | GND       | GND   |  8   | GND   |
// | Data / ConfigPWM | GPIO_0    | D / C | 16   | PWM   |	
// | Busy indicator   | GPIO_2    | BSY   | 15   | INT   |
//========================================================

// Reed Switch
// DEFINE REED_SWITCH_INCLUDED in build_options.h
// 3.3v  ---> RESISTOR 4K7 ----> GPIO_42 --> REED SWITCH --> GND
//
#include "applibs_versions.h"
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include <applibs/log.h>
#include <applibs/spi.h>
#include <applibs/gpio.h>

#include "mt3620_rdb.h"
#include "epaper_hardware.h"

#include "epd/EPD_1in54.h"
#include "gui/GUI_Paint.h"

#include "qr/qrcodegen.h"
#include "iwt_base64.h"
#include "iwt_crypto.h"
#include "iwt_image.h"

#include "build_options.h"
#include "epoll_timerfd_utilities.h"
#include "deviceTwin.h"
#include "iwt_display.h"
#include <applibs/wificonfig.h>
#include <azureiot/iothub_device_client_ll.h>
#include "azure_iot_utilities.h"
#include "connection_strings.h"



// Provide local access to variables in other files
extern twin_t twinArray[];
extern int twinArraySize;
extern IOTHUB_DEVICE_CLIENT_LL_HANDLE iothubClientHandle;

// Array with messages from Azure
extern uint8_t oled_ms1[CLOUD_MSG_SIZE] = "THANKS!";
extern uint8_t oled_ms2[CLOUD_MSG_SIZE] = "RECYCLING";
extern uint8_t oled_ms3[CLOUD_MSG_SIZE] = "GIVES";
extern uint8_t oled_ms4[CLOUD_MSG_SIZE] = "QR PRIZES";

// File descriptors - initialized to invalid value

int userLedRedFd = -1;
int userLedGreenFd = -1;
int userLedBlueFd = -1;
int appLedFd = -1;
int wifiLedFd = -1;
int clickSocket1Relay1Fd = -1;
int clickSocket1Relay2Fd = -1;

static int epollFd = -1;
static int epaperSpiDcFd = -1;
static int epaperSpiBusyFd = -1;

static int epaperSpiResetFd = -1;
static int spiFd = -1;
static int buttonPollTimerFd = -1;
static int buttonAGpioFd = -1;
static int buttonBGpioFd = -1;

static int gotoMainScreenTimerFd = -1;


#ifdef REED_SWITCH_INCLUDED
static int reedSwitchFd = -1;
static GPIO_Value_Type reedSwitchState = GPIO_Value_Low;
#endif

#define BUFFER_SIZE 2048

// "iat" (Issued At) 
// Claim The "iat" (issued at) claim identifies the time at which the JWT was
// issued.This claim can be used to determine the age of the JWT.Its
// value MUST be a number containing a NumericDate value.

//"jti" (JWT ID) Claim
//The "jti" (JWT ID) claim provides a unique identifier for the JWT.
//The identifier value MUST be assigned in a manner that ensures that
//there is a negligible probability that the same value will be
//accidentally assigned to a different data object; if the application
//uses multiple issuers, collisions MUST be prevented among values
//produced by different issuers as well.The "jti" claim can be used
//to prevent the JWT from being replayed.The "jti" value is a case-
//sensitive string.

// Define the Json string format for the JSON WEB TOKEN
const char cstrJWTPayloadJson[] = "{\"jti\":\"%s-%08x-%08x\",\"iat\":%d}";

// Define the Json string format for the button press data
const char cstrButtonTelemetryJson[] = "{\"%s\":\"%d\"}";


static const char cstrJWTHeaderJson[] = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
static char deviceId[200];
static char key[200];

// Set up a timer to return to main screen
struct timespec gotoMainScreenPeriod = { 10, 0 };
static const struct timespec nullPeriod = { 0, 0 };


#define JSON_BUFFER_SIZE 204
static long lastJwtId;

// Support functions.
static void TerminationHandler(int signalNumber);

static int paintScreen(int (*paint)(void));

static int InitPeripheralsAndHandlers(void);
static void ClosePeripheralsAndHandlers(void);

static void getTimeUtc(char* displayTimeBuffer);
static void ButtonTimerEventHandler(EventData* eventData);
static void GotoMainScreenTimerEventHandler(EventData* eventData);


// Button state variables, initilize them to button not-pressed (High)
static GPIO_Value_Type buttonAState = GPIO_Value_High;
static GPIO_Value_Type buttonBState = GPIO_Value_High;
static SpiMasterConfigType spiMasterConfig;

#if (defined(IOT_CENTRAL_APPLICATION) || defined(IOT_HUB_APPLICATION))
bool versionStringSent = false;
#endif

// Termination state
static volatile sig_atomic_t terminationRequired = false;

// event handler data structures. Only the event handler field needs to be populated.
static EventData buttonEventData = { .eventHandler = &ButtonTimerEventHandler };

// event handler data structures. Only the event handler field needs to be populated.
static EventData gotoMainScreenEventData = { .eventHandler = &GotoMainScreenTimerEventHandler };

/// <summary>
///     Signal handler for termination requests. This handler must be async-signal-safe.
/// </summary>
static void TerminationHandler(int signalNumber)
{
    // Don't use Log_Debug here, as it is not guaranteed to be async-signal-safe.
    terminationRequired = true;
}

/// <summary>
///     Get UTC time in format: "Sat Nov 2 19:51:45 2019"
/// </summary>
static void getTimeUtc(char* displayTimeBuffer) {
	struct timespec currentTime;
	if (clock_gettime(CLOCK_REALTIME, &currentTime) == -1) {
		Log_Debug("ERROR: clock_gettime failed with error code: %s (%d).\n", strerror(errno),
			errno);
		terminationRequired = true;
		return;
	}
	else {

		if (!asctime_r((gmtime(&currentTime.tv_sec)), (char* restrict) displayTimeBuffer)) {
			Log_Debug("ERROR: asctime_r failed with error code: %s (%d).\n", strerror(errno),
				errno);
			terminationRequired = true;
			return;
		}
		Log_Debug("UTC:            %s", displayTimeBuffer);
	}
}

/// <summary>
///     Get UTC time as an integer, unix format
/// </summary>
static time_t getUnixTime(void) {
	struct timespec currentTime;
	if (clock_gettime(CLOCK_REALTIME, &currentTime) == -1) {
		Log_Debug("ERROR: clock_gettime failed with error code: %s (%d).\n", strerror(errno),
			errno);
		terminationRequired = true;
		return -1;
	}
	return currentTime.tv_sec;
}

/// <summary>
///     Paint idle screen. Asks for pressing A Button
/// </summary>
int paintIdleScreen(void) {
#ifdef REED_SWITCH_INCLUDED
	Paint_DrawBitMap(gImage_qrcycleHead);
#endif // REED_SWITCH_INCLUDED
#ifndef REED_SWITCH_INCLUDED
	Paint_DrawBitMap(pressA_image);
#endif // !REED_SWITCH_INCLUDED
	
	return 0;
}

/// <summary>
///     Paint Clock screen. 
/// </summary>
int paintClockScreen(void) {
	char outstr[200];
	timer_t t;
	struct tm* tmp;
	t = time(NULL);
	tmp = localtime(&t);
	if (tmp == NULL) {
		Log_Debug("PaintClockScreen localtime returned NULL");
		terminationRequired = true;
		return -1;
	}
	if (strftime(outstr, sizeof(outstr), "%H:%M", tmp) == 0) {
		Log_Debug("PaintClockScreen strftime returned 0");
		terminationRequired = true;
		return -1;
	}
	
	Paint_DrawRectangle(4, 4, EPD_WIDTH - 4, EPD_HEIGHT - 4, BLACK, DRAW_FILL_EMPTY, DOT_PIXEL_4X4);
	Paint_DrawCircle(EPD_WIDTH / 2, EPD_HEIGHT / 2, EPD_WIDTH / 2 - 4, BLACK, DRAW_FILL_FULL, DOT_PIXEL_1X1);
	Paint_DrawCircle(EPD_WIDTH / 2, EPD_HEIGHT / 2, EPD_WIDTH / 2 - 40, WHITE, DRAW_FILL_FULL, DOT_PIXEL_1X1);

	Paint_DrawString_EN((EPD_WIDTH - Font24.Width * 5)/2, (EPD_HEIGHT - Font24.Height) / 2, outstr,&Font24, WHITE, BLACK);
	return 0;
}

/// <summary>
///     Paint Messages screen. 
/// </summary>
int paintMessagesScreen(void) {
	const sFONT font = Font20;
	Paint_DrawRectangle(4, 4, EPD_WIDTH-4, EPD_HEIGHT-4, BLACK, DRAW_FILL_EMPTY, DOT_PIXEL_4X4);
	int lineStartY = (EPD_HEIGHT - font.Height * 4) / 2;
	int lineStartX = (EPD_WIDTH - strlen(oled_ms1) * font.Width) / 2;
	Paint_DrawString_EN(lineStartX, lineStartY, oled_ms1, &font, WHITE, BLACK);
	lineStartY += font.Height;
	lineStartX = (EPD_WIDTH - strlen(oled_ms2) * font.Width) / 2;
	Paint_DrawString_EN(lineStartX, lineStartY, oled_ms2, &font, WHITE, BLACK);
	lineStartY += font.Height;
	lineStartX = (EPD_WIDTH - strlen(oled_ms3) * font.Width) / 2;
	Paint_DrawString_EN(lineStartX, lineStartY, oled_ms3, &font, WHITE, BLACK);
	lineStartY += font.Height;
	lineStartX = (EPD_WIDTH - strlen(oled_ms4) * font.Width) / 2;
	Paint_DrawString_EN(lineStartX, lineStartY, oled_ms4, &font, WHITE, BLACK);
	return 0;
}

/// <summary>
///     Paint QR Screen incluiding date time and I was there string
/// </summary>
/// <returns>0 on success, or -1 on failure< / returns>
int paintQrScreen(void) {
	long unixTime = getUnixTime();

	int jwtUid = rand();
	char* jwebPayload = (char*)malloc(BUFFER_SIZE * sizeof(char));
	if (jwebPayload == NULL) {
		Log_Debug("Failed to apply jwebPayload memory...\r\n");
		return -1;
	}
	sprintf(jwebPayload, cstrJWTPayloadJson, deviceId, jwtUid, unixTime, unixTime);
	lastJwtId = jwtUid;

	char* jwebPayloadBase64 = (char*)malloc(BUFFER_SIZE * sizeof(char));
	if (jwebPayloadBase64 == NULL) {
		Log_Debug("Failed to apply jwebPayloadBase64 memory...\r\n");
		return -1;
	}
	jwt_urlsafe_base64_encode(jwebPayloadBase64, jwebPayload, strlen(jwebPayload));

	free(jwebPayload);
	
	char* jwebHeaderTokenBase64 = (char*)malloc(BUFFER_SIZE * sizeof(char));
	if (jwebHeaderTokenBase64 == NULL) {
		Log_Debug("Failed to apply jwebHeaderTokenBase64 memory...\r\n");
		return -1;
	}
	jwt_urlsafe_base64_encode(jwebHeaderTokenBase64, cstrJWTHeaderJson, strlen(cstrJWTHeaderJson));

	char* jwebtoken = (char*)malloc(BUFFER_SIZE * sizeof(char));
	if (jwebtoken == NULL) {
		Log_Debug("Failed to apply jwebtoken memory...\r\n");
		return -1;
	}
	sprintf(jwebtoken, "%s.%s", jwebHeaderTokenBase64, jwebPayloadBase64);
	free(jwebPayloadBase64);
	free(jwebHeaderTokenBase64);

	char* jwebHmacBinary = (char*)malloc(BUFFER_SIZE * sizeof(char));
	if (jwebHmacBinary == NULL) {
		Log_Debug("Failed to apply jwebHmacBinary memory...\r\n");
		return -1;
	}
	Log_Debug("WebToken: %s, Length: %d\n", jwebtoken, strlen(jwebtoken));
	iwt_crypto_hmacsha256(jwebHmacBinary, jwebtoken, strlen(jwebtoken), key);

	char* jwebHmacBase64 = (char*)malloc(BUFFER_SIZE * sizeof(char));
	if (jwebHmacBase64 == NULL) {
		Log_Debug("Failed to apply jwebHmacBase64 memory...\r\n");
		return -1;
	}
	jwt_urlsafe_base64_encode(jwebHmacBase64, jwebHmacBinary, SHA256_DIGEST_SIZE);
	free(jwebHmacBinary);

	char* jwebtokensigned = (char*)malloc(BUFFER_SIZE * sizeof(char));
	if (jwebtokensigned == NULL) {
		Log_Debug("Failed to apply jwebtokensigned memory...\r\n");
		return -1;
	}
	sprintf(jwebtokensigned, "%s.%s", jwebtoken, jwebHmacBase64);
	free(jwebHmacBase64);
	free(jwebtoken);
		
	enum qrcodegen_Ecc errCorLvl = qrcodegen_Ecc_LOW;  // Error correction level

    // Make and print the QR Code symbol
	uint8_t* qrcode = (uint8_t*) malloc(qrcodegen_BUFFER_LEN_MAX * sizeof(uint8_t));
	if (qrcode == NULL) {
		Log_Debug("Failed to apply qrcode memory...\r\n");
		return -1;
	}
	uint8_t* tempBuffer = (uint8_t*)malloc(qrcodegen_BUFFER_LEN_MAX * sizeof(uint8_t));
	if (tempBuffer == NULL) {
		Log_Debug("Failed to apply tempBuffer memory...\r\n");
		return -1;
	}

	bool ok = qrcodegen_encodeText(jwebtokensigned, tempBuffer, qrcode, errCorLvl,
		qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);
	//Log_Debug("JWT Signed %s\n", jwebtokensigned);

	free(jwebtokensigned);
	
	if (ok) {
		char* displayTimeBuffer= (char*)malloc(26 * sizeof(char));
		if (displayTimeBuffer == NULL) {
			Log_Debug("Failed to apply displayTimeBuffer memory...\r\n");
			return -1;
		}
		getTimeUtc(displayTimeBuffer);
		displayTimeBuffer[24] = '\0';
		sFONT headerFont = Font12;
		int size = qrcodegen_getSize(qrcode);
		int boxSize = (EPD_WIDTH - (4 * headerFont.Height)) / size;
		int border = boxSize + (EPD_WIDTH - boxSize * size) / 2;

		Paint_DrawRectangle(1, 1, EPD_WIDTH, EPD_HEIGHT, BLACK, DRAW_FILL_FULL, DOT_STYLE_DFT);
		Paint_DrawRectangle(border- 2*boxSize, border - 2*boxSize,
			size * boxSize + border + 2 * boxSize -1,
			size * boxSize + border + 2 * boxSize -1, WHITE, DRAW_FILL_FULL, DOT_STYLE_DFT);
		Paint_DrawString_EN((EPD_WIDTH - (24 * headerFont.Width)) / 2, headerFont.Height / 2, displayTimeBuffer, &Font12, BLACK, WHITE);
		free(displayTimeBuffer);
		Paint_DrawString_EN((EPD_WIDTH - (11 * headerFont.Width)) / 2, EPD_HEIGHT - (headerFont.Height + headerFont.Height / 2), "I Was There", &Font12, BLACK, WHITE);

		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				if (qrcodegen_getModule(qrcode, x, y)) {
					Paint_DrawPoint(x * boxSize + border, y * boxSize + border, BLACK, boxSize, DOT_STYLE_DFT);
				}
				else {
					Paint_DrawPoint(x * boxSize + border, y * boxSize + border, WHITE, boxSize, DOT_STYLE_DFT);
				}
			}
		}
		free(qrcode);
		free(tempBuffer);
	}
	return 0;
}

/// <summary>
///     Generic method to paint a screen in the e-paper screen
///     Inputs: The function that paints the screen
/// </summary>
/// <returns>0 on success, or -1 on failure< / returns>
static int paintScreen(int (*paint)(void) ){
	if (EPD_Init(LUT_FULL_UPDATE(), &spiMasterConfig) != 0) {
		Log_Debug("e-Paper init failed\r\n");
		return -1;
	}
	//Create a new image cache
	UBYTE* BlackImage;
	/* you have to edit the startup_stm32fxxx.s file and set a big enough heap size */
	UWORD Imagesize = ((EPD_WIDTH % 8 == 0) ? (EPD_WIDTH / 8) : (EPD_WIDTH / 8 + 1)) * EPD_HEIGHT;
	if ((BlackImage = (UBYTE*)malloc(Imagesize)) == NULL) {
		Log_Debug("Failed to apply for black memory...\r\n");
		return -1;
	}
	Paint_NewImage(BlackImage, EPD_WIDTH, EPD_HEIGHT, 0, BLACK);
	Paint_Clear(WHITE);
	int result = (*paint)();
	EPD_Display(BlackImage);
	free(BlackImage);
	BlackImage = NULL;
	EPD_Sleep();
	return result;
}


/// <summary>
///     Set up SIGTERM termination handler, initialize peripherals, and set up event handlers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static int InitPeripheralsAndHandlers(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = TerminationHandler;
    sigaction(SIGTERM, &action, NULL);

    epollFd = CreateEpollFd();
    if (epollFd < 0) {
        return -1;
    }

	// Open button A GPIO as input
	Log_Debug("Opening Starter Kit Button A as input.\n");
	buttonAGpioFd = GPIO_OpenAsInput(MT3620_RDB_BUTTON_A);
	if (buttonAGpioFd < 0) {
		Log_Debug("ERROR: Could not open button A GPIO: %s (%d).\n", strerror(errno), errno);
		return -1;
	}

#ifdef REED_SWITCH_INCLUDED
	// Open reed sensor GPIO as input
	Log_Debug("Opening Reed Switch GPIO as input.\n");
	reedSwitchFd = GPIO_OpenAsInput(MT3620_GPIO42);
	if (reedSwitchFd < 0) {
		Log_Debug("ERROR: Could not open Reed Switch GPIO: %s (%d).\n", strerror(errno), errno);
		return -1;
	}

#endif // REED_SWITCH_INCLUDED

	// Open button B GPIO as input
	Log_Debug("Opening Starter Kit Button B as input.\n");
	buttonBGpioFd = GPIO_OpenAsInput(MT3620_RDB_BUTTON_B);
	if (buttonBGpioFd < 0) {
		Log_Debug("ERROR: Could not open button B GPIO: %s (%d).\n", strerror(errno), errno);
		return -1;
	}

	// Set up a timer to poll the buttons
	struct timespec buttonPressCheckPeriod = { 0, 1000000 };
	buttonPollTimerFd =
		CreateTimerFdAndAddToEpoll(epollFd, &buttonPressCheckPeriod, &buttonEventData, EPOLLIN);
	if (buttonPollTimerFd < 0) {
		return -1;
	}

	gotoMainScreenTimerFd =
		CreateTimerFdAndAddToEpoll(epollFd, &nullPeriod, &gotoMainScreenEventData, EPOLLIN);
	if (gotoMainScreenTimerFd < 0) {
		return -1;
	}
	
	epaperSpiBusyFd = GPIO_OpenAsInput(SAMPLE_EPAPER_BUSY);
	if (epaperSpiBusyFd < 0) {
		Log_Debug("ERROR: SAMPLE_EPAPER_BUSY = %d errno = %s (%d)\n", epaperSpiBusyFd, strerror(errno),
			errno);
		return -1;
	}

	epaperSpiDcFd = GPIO_OpenAsOutput(SAMPLE_EPAPER_DATA_CONFIG,GPIO_OutputMode_PushPull,GPIO_Value_Low);
	if (epaperSpiDcFd < 0) {
		Log_Debug("ERROR: SAMPLE_EPAPER_DATA_CONFIG = %d errno = %s (%d)\n", epaperSpiDcFd, strerror(errno),
			errno);
		return -1;
	}
	   
	epaperSpiResetFd = GPIO_OpenAsOutput(SAMPLE_EPAPER_RESET, GPIO_OutputMode_PushPull, GPIO_Value_Low);
	if (epaperSpiResetFd < 0) {
		Log_Debug("ERROR: SAMPLE_EPAPER_CS = %d errno = %s (%d)\n", epaperSpiResetFd, strerror(errno),
			errno);
		return -1;
	}
	   	 
    SPIMaster_Config config;
    int ret = SPIMaster_InitConfig(&config);
    if (ret != 0) {
        Log_Debug("ERROR: SPIMaster_InitConfig = %d errno = %s (%d)\n", ret, strerror(errno),
                  errno);
        return -1;
    }
    config.csPolarity = SPI_ChipSelectPolarity_ActiveLow;
    spiFd = SPIMaster_Open(SAMPLE_EPAPER_SPI, SAMPLE_EPAPER_SPI_CS, &config);
    if (spiFd < 0) {
        Log_Debug("ERROR: SPIMaster_Open: errno=%d (%s)\n", errno, strerror(errno));
        return -1;
    }

    int result = SPIMaster_SetBusSpeed(spiFd, 200000);
    if (result != 0) {
        Log_Debug("ERROR: SPIMaster_SetBusSpeed: errno=%d (%s)\n", errno, strerror(errno));
        return -1;
    }

	//  SPI0 is commonly used, in which CPHL =	0, CPOL = 0.
    result = SPIMaster_SetMode(spiFd, SPI_Mode_0);
    if (result != 0) {
        Log_Debug("ERROR: SPIMaster_SetMode: errno=%d (%s)\n", errno, strerror(errno));
        return -1;
    }

	// In the write mode, SDA is shifted into an 8-bit shift register on each rising edge 
	// of SCL in the order of D7, D6, ... D0.
	// The level of D / C# should be kept over the whole byte.
	result = SPIMaster_SetBitOrder(spiFd, SPI_BitOrder_MsbFirst);
	if (result != 0) {
		Log_Debug("ERROR: SPIMaster_SetBitOrder: errno=%d (%s)\n", errno, strerror(errno));
		return -1;
	}
		
	spiMasterConfig.spiFd = spiFd;
	spiMasterConfig.dcFd = epaperSpiDcFd;
	spiMasterConfig.resetFd = epaperSpiResetFd;
	spiMasterConfig.busyFd = epaperSpiBusyFd;

	if (EPD_Init(LUT_FULL_UPDATE(), &spiMasterConfig) != 0) {
		Log_Debug("e-Paper init failed\r\n");
		return -1;
	}

	// Traverse the twin Array and for each GPIO item in the list open the file descriptor
	for (int i = 0; i < twinArraySize; i++) {

		// Verify that this entry is a GPIO entry
		if (twinArray[i].twinGPIO != NO_GPIO_ASSOCIATED_WITH_TWIN) {

			*twinArray[i].twinFd = -1;

			// For each item in the data structure, initialize the file descriptor and open the GPIO for output.  Initilize each GPIO to its specific inactive state.
			*twinArray[i].twinFd = (int)GPIO_OpenAsOutput(twinArray[i].twinGPIO, GPIO_OutputMode_PushPull, twinArray[i].active_high ? GPIO_Value_Low : GPIO_Value_High);

			if (*twinArray[i].twinFd < 0) {
				Log_Debug("ERROR: Could not open LED %d: %s (%d).\n", twinArray[i].twinGPIO, strerror(errno), errno);
				return -1;
			}
		}
	}

	// Tell the system about the callback function that gets called when we receive a device twin update message from Azure
	AzureIoT_SetDeviceTwinUpdateCallback(&deviceTwinChangedHandler);


    return 0;
}


/// <summary>
///     Handle button timer event: 
///		if the A button is pressed print QR on screen.
///		if the B button is pressed print idle screen.
/// </summary>
static void ButtonTimerEventHandler(EventData* eventData)
{
	bool sendTelemetryButtonA;

	if (ConsumeTimerFdEvent(buttonPollTimerFd) != 0) {
		terminationRequired = true;
		return;
	}

	int result;

#ifdef REED_SWITCH_INCLUDED
	// Check for reed switch open
	GPIO_Value_Type newReedSwitchState;
	result = GPIO_GetValue(reedSwitchFd, &newReedSwitchState);
	if (result != 0) {
		Log_Debug("ERROR: Could not read Reed Switch GPIO: %s (%d).\n", strerror(errno), errno);
		terminationRequired = true;
		return;
	}
	if (newReedSwitchState != reedSwitchState) {
		if (reedSwitchState == GPIO_Value_Low) {

			Log_Debug("Reed Switch opened!\n");
			// check if there is a message to show
			if (strlen(oled_ms1) > 0) {
				paintScreen(paintMessagesScreen);
			}

		}
		else {
			Log_Debug("Reed Switch A closed!\n");
			paintScreen(paintQrScreen);
			sendTelemetryButtonA = true;
			// Set timer to return to idle screen
			SetTimerFdToSingleExpiry(gotoMainScreenTimerFd, &gotoMainScreenPeriod);
		}
		// Update the static variable to use next time we enter this routine
		reedSwitchState = newReedSwitchState;
	}


#endif // REED_SWITCH_INCLUDED


	// Check for button A press
	GPIO_Value_Type newButtonAState;
	result = GPIO_GetValue(buttonAGpioFd, &newButtonAState);
	if (result != 0) {
		Log_Debug("ERROR: Could not read button GPIO: %s (%d).\n", strerror(errno), errno);
		terminationRequired = true;
		return;
	}

	// If the A button has just been pressed, print a new JWT QR on screen
	// The button has GPIO_Value_Low when pressed and GPIO_Value_High when released
	if (newButtonAState != buttonAState) {
		if (newButtonAState == GPIO_Value_Low) {

			Log_Debug("Button A pressed!\n");
			// check if there is a message to show
			if (strlen(oled_ms1) > 0) { 
				paintScreen(paintMessagesScreen);
			}
			
		}
		else {
			Log_Debug("Button A released!\n");
			paintScreen(paintQrScreen);
			sendTelemetryButtonA = true;
			// Set timer to return to idle screen
			SetTimerFdToSingleExpiry(gotoMainScreenTimerFd, &gotoMainScreenPeriod);
		}
		// Update the static variable to use next time we enter this routine
		buttonAState = newButtonAState;
	}

	// Check for button B press
	GPIO_Value_Type newButtonBState;
	result = GPIO_GetValue(buttonBGpioFd, &newButtonBState);
	if (result != 0) {
		Log_Debug("ERROR: Could not read button GPIO: %s (%d).\n", strerror(errno), errno);
		terminationRequired = true;
		return;
	}

	// If the B button has just been pressed/released goto idle screen.TODO: Reset timer.
	// The button has GPIO_Value_Low when pressed and GPIO_Value_High when released
	if (newButtonBState != buttonBState) {
		if (newButtonBState == GPIO_Value_Low) {
			Log_Debug("Button B pressed!\n");	
			paintScreen(paintClockScreen);
		}
		else {
			Log_Debug("Button B released!\n");
			// Set timer to return to idle screen
			SetTimerFdToSingleExpiry(gotoMainScreenTimerFd, &gotoMainScreenPeriod);
		}
		// Update the static variable to use next time we enter this routine
		buttonBState = newButtonBState;
	}
	// If either button was pressed, then enter the code to send the telemetry message
	if (sendTelemetryButtonA ) {

		char* pjsonBuffer = (char*)malloc(JSON_BUFFER_SIZE);
		if (pjsonBuffer == NULL) {
			Log_Debug("ERROR: not enough memory to send telemetry");
		}

		if (sendTelemetryButtonA) {
			// construct the telemetry message  for Button A
			snprintf(pjsonBuffer, JSON_BUFFER_SIZE, cstrButtonTelemetryJson, "buttonA", lastJwtId);
			Log_Debug("\n[Info] Sending telemetry %s\n", pjsonBuffer);
			AzureIoT_SendMessage(pjsonBuffer);
		}

		free(pjsonBuffer);
	}

}


/// <summary>
///     Handle button timer event: if the button is pressed, report the event to the IoT Hub.
/// </summary>
static void GotoMainScreenTimerEventHandler(EventData* eventData)
{
	Log_Debug("GotoMainScreenTimerEventHandler");
	if (ConsumeTimerFdEvent(gotoMainScreenTimerFd) != 0) {
		terminationRequired = true;
		return;
	}

	paintScreen(paintIdleScreen);

}



/// <summary>
///     Main entry point for this application.
/// </summary>
int main(int argc, char *argv[])
{
	//
	strcpy(deviceId, argv[2]);
	Log_Debug("Device ID: %s\n", deviceId);
	strcpy(key, argv[3]);
	// Variable to help us send the version string up only once
	bool networkConfigSent = false;
	char ssid[128];
	uint32_t frequency;
	char bssid[20];

	// Clear the ssid array
	memset(ssid, 0, 128);
    Log_Debug("EPAPER Sample application starting.\n");
	if (InitPeripheralsAndHandlers() != 0) {
        terminationRequired = true;
    }
	if (!terminationRequired) {
		// Init random seed with current time
		srand(getUnixTime() & 0xFFFFFFFF);
		paintScreen(paintIdleScreen);
	}

    // Use epoll to wait for events and trigger handlers, until an error or SIGTERM happens
    while (!terminationRequired) {
        if (WaitForEventAndCallHandler(epollFd) != 0) {
            terminationRequired = true;
        }


#if (defined(IOT_CENTRAL_APPLICATION) || defined(IOT_HUB_APPLICATION))
		// Setup the IoT Hub client.
		// Notes:
		// - it is safe to call this function even if the client has already been set up, as in
		//   this case it would have no effect;
		// - a failure to setup the client is a fatal error.
		if (!AzureIoT_SetupClient()) {
			Log_Debug("ERROR: Failed to set up IoT Hub client\n");
			break;
		}
#endif 

		WifiConfig_ConnectedNetwork network;
		int result = WifiConfig_GetCurrentNetwork(&network);

		if (result < 0)
		{
			// Log_Debug("INFO: Not currently connected to a WiFi network.\n");
			//// OLED
			strncpy(network_data.SSID, "Not Connected", 20);

			network_data.frequency_MHz = 0;

			network_data.rssi = 0;
		}
		else
		{

			frequency = network.frequencyMHz;
			snprintf(bssid, JSON_BUFFER_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x",
				network.bssid[0], network.bssid[1], network.bssid[2],
				network.bssid[3], network.bssid[4], network.bssid[5]);

			if ((strncmp(ssid, (char*)&network.ssid, network.ssidLength) != 0) || !networkConfigSent) {

				memset(ssid, 0, 128);
				strncpy(ssid, network.ssid, network.ssidLength);
				Log_Debug("SSID: %s\n", ssid);
				Log_Debug("Frequency: %dMHz\n", frequency);
				Log_Debug("bssid: %s\n", bssid);
				networkConfigSent = true;

#if (defined(IOT_CENTRAL_APPLICATION) || defined(IOT_HUB_APPLICATION))
				// Note that we send up this data to Azure if it changes, but the IoT Central Properties elements only 
				// show the data that was currenet when the device first connected to Azure.
				checkAndUpdateDeviceTwin("ssid", &ssid, TYPE_STRING, false);
				checkAndUpdateDeviceTwin("freq", &frequency, TYPE_INT, false);
				checkAndUpdateDeviceTwin("bssid", &bssid, TYPE_STRING, false);
#endif 
			}

			//// OLED

			memset(network_data.SSID, 0, WIFICONFIG_SSID_MAX_LENGTH);
			if (network.ssidLength <= SSID_MAX_LEGTH)
			{
				strncpy(network_data.SSID, network.ssid, network.ssidLength);
			}
			else
			{
				strncpy(network_data.SSID, network.ssid, SSID_MAX_LEGTH);
			}

			network_data.frequency_MHz = network.frequencyMHz;

			network_data.rssi = network.signalRssi;
		}
#if (defined(IOT_CENTRAL_APPLICATION) || defined(IOT_HUB_APPLICATION))
		if (iothubClientHandle != NULL && !versionStringSent) {

			checkAndUpdateDeviceTwin("versionString", argv[1], TYPE_STRING, false);
			versionStringSent = true;
		}

		// AzureIoT_DoPeriodicTasks() needs to be called frequently in order to keep active
		// the flow of data with the Azure IoT Hub
		AzureIoT_DoPeriodicTasks();
#endif



    }

    ClosePeripheralsAndHandlers();
    Log_Debug("Application exiting.\n");
    return 0;
}


/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
	Log_Debug("Closing file descriptors.\n");
	CloseFdAndPrintError(spiFd, "Spi");
	CloseFdAndPrintError(epaperSpiBusyFd, "Spi Busy");
	CloseFdAndPrintError(epaperSpiDcFd, "Spi DC");
	CloseFdAndPrintError(epaperSpiResetFd, "Spi Reset");
	CloseFdAndPrintError(epollFd, "Epoll");

	CloseFdAndPrintError(buttonPollTimerFd, "buttonPoll");
	CloseFdAndPrintError(buttonAGpioFd, "buttonA");
	CloseFdAndPrintError(buttonBGpioFd, "buttonB");
	CloseFdAndPrintError(gotoMainScreenTimerFd, "gotoMainScreen");
	for (int i = 0; i < twinArraySize; i++) {
		if (twinArray[i].twinGPIO != NO_GPIO_ASSOCIATED_WITH_TWIN) {
			CloseFdAndPrintError(*twinArray[i].twinFd, "twinArray");
		}
	}
#ifdef REED_SWITCH_INCLUDED
	CloseFdAndPrintError(reedSwitchFd, "ReedSwitch");
#endif // REED_SWITCH_INCLUDED


}
