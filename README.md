# I WAS THERE 

Repository for the projects submitted to:

- Hackster.io Secure Everything with Azure Sphere with Avnet and Microsoft Contest <br>
  https://www.hackster.io/contests/SecureEverything
- Element 14 Sensing the World Challenge <br>
  https://www.element14.com/community/docs/DOC-92683/l/sensing-the-world-challenge

The challenge is powered by the Avnet Azure Sphere MT3620 Starter Kit, a solution for creating secured IoT edge devices with a ton of potential applications.

## I WAS THERE... RECYCLING THE WASTE <img src="media/qrcycle.bmp" width="40" alt="logo">

Securely prove that you have been to a site at a specific time with JSON Web Tokens (JWT) signed in QR format
This appliaction creates signed JWTs in QR format and display them in an ink display with an Azure Sphere Device:
Azure Sphere MT3620 Starter Kit

Hackster.io Secure Everything with Azure Sphere: [I WAS THERE... RECYCLING THE WASTE](https://www.hackster.io/javagoza/i-was-there-recycling-the-waste-fcc895)

## I WAS THERE... SENSING CITIZENS RECYCLING <img src="media/PapeleraBateria.jpg" width="40" alt="logo">

Remotely monitor the uses of recycling bins. Azure IoT Central

Device Template: [Device Template](json/waste_bin.json)

Element14 Sensing the World Challenge: [I WAS THERE... SENSING CITIZENS RECYCLING](https://www.element14.com/community/groups/azuresphere/blog/2019/11/28/i-was-here)

<p align="center"><img src="media/sensingcitizensazureiotcentral.png" width="600" alt="logo"></p>

## Prerequisites

 This sample requires the following hardware:

- AVNET Azure Sphere MT3620 Starter Kit
- WaveShare 1.54inch e-Paper V2, Active Matrix Electrophoretic Display (AMEPD)
- SparkFun Proximity Sensor Breakout - 20cm, VCNL4040 (Qwiic)
- Jumper wires to connect the boards.

|Library   |Purpose  |
|---------|---------|
|log      |  Displays messages in the Visual Studio Device Output window during debugging  |
|spi      |  Manages SPI interfaces |
|i2c      |  Manages I2C interfaces |
|wolfssl  |  Manages cryptography |
|time     |  Manages clocks and time  |

**Note**
You can configure hardware included in [IWT_HighLevelApp/build_options.h](IWT_HighLevelApp/build_options.h)

### AVNET Azure Sphere MT3620 Starter Kit

The Avnet Azure Sphere MT3620 Starter Kit supports rapid prototyping of highly secure, end-to-end IoT implementations using Microsoft’s Azure Sphere. The small form-factor carrier board includes a production-ready MT3620 Sphere module with Wi-Fi connectivity, along with multiple expansion interfaces for easy integration of sensors, displays, motors, relays, and more. 

https://www.element14.com/community/community/designcenter/azure-sphere-starter-kits

### WolfSSL

https://github.com/wolfSSL/wolfssl

### WaveShare 1.54inch e-Paper V2 e-Paper Display

WaveShare 1.54inch e-Paper V2, Active Matrix Electrophoretic Display (AMEPD)

https://www.waveshare.com/wiki/1.54inch_e-Paper_Module

### SparkFun Proximity Sensor Breakout - 20cm, VCNL4040 (Qwiic)

https://www.sparkfun.com/products/15177

https://www.vishay.com/docs/84307/designingvcnl4040.pdf


## To build and run

1. Set up your Azure Sphere device and development environment as described in the [Azure Sphere documentation](https://docs.microsoft.com/azure-sphere/install/install).
1. Even if you've performed this set up previously, ensure you have Azure Sphere SDK version 19.10 or above. In an Azure Sphere Developer Command Prompt, run **azsphere show-version** to check. Download and install the [latest SDK](https://aka.ms/AzureSphereSDKDownload) as needed.
1. Connect your Azure Sphere device to your PC by USB.
1. Enable [application development](https://docs.microsoft.com/azure-sphere/quickstarts/qs-blink-application#prepare-your-device-for-development-and-debugging), if you have not already done so:

   `azsphere device prep-debug`
1. Clone the repo and find the IWT_HighLevelApp.
1. Clone the wolfSSL repo and copy the wolfSSL into the IWT_HighLevelApp sample in the IWT_HighLevelApp folder.
1. In Visual Studio, open IWT_HighLevelApp.sln and press F5 to compile and build the solution and load it onto the device for debugging.


## Connecting WaveShare 1.54inch e-Paper V2, SPI: 

|NOTES |AVNET KIT |Pin | |Mikro Bus |
|------|----------|----|-|----------|
|Reset            |GPIO_16   |RST   |2   |RST   |
|SPI chip select  |GPIO_34   |CS    |3   |CS    |
|SPI clock        |GPIO_31   |SCK   |4   |SCK   |
|SPI data input   |GPIO_32   |SDI   |6   |MOSI  |
|Power supply     |3V3       |+3.3V |7   |3.3V  |
|Ground	          |GND       |GND   |8   |GND   |
|Data / ConfigPWM |GPIO_0    |D / C |16   |PWM   |	
|Busy indicator   |GPIO_2    |BSY   |15   |INT   |

## Connecting the Reed Switch: 

Connect the Reed Switch to GPIO_42 and Ground with a pull-up resitor to 3.3v

|NOTES |AVNET KIT |Pin | |Mikro Bus |
|------|----------|----|-|----------|
|Reed Switch      |GPIO_42   |AN    |2   |Pull-up Resistor \\ Reed Sensor   |
|Power supply     |3V3       |+3.3V |7   |3.3V  |
|Ground	          |GND       |GND   |8   |GND   |

## Connecting VCNL4040 Proximity and Ambient Light Sensor, I2C:

Using a SparkFun Proximity Sensor Breakout - 20cm, VCNL4040 (Qwiic)

|NOTES |AVNET KIT |Pin | |Mikro Bus |
|------|----------|----|-|----------|
|Power supply     |3V3        |+3.3V  |7    |3.3V  |
|Ground	          |GND        |GND    |8    |GND   |
|I2C Data SDA2    |GPIO_38    |SDA2   |11   | SDA   |	
|I2C SCL          |GPIO_37    |SCL2   |12   | SCL   |	

## Schematics

<img src="media/IWT-Sketch_bb_rs_ps.png" width="600" alt="schematics">

##  Format of the tokens

* Header: algorithm & type
```json
{
"alg": "HS256",
"typ" : "JWT"
}
```

* Payload:data
```json
{
 "jti": "e2da7773-a7cd-44a0-85d7-43fe0654cb22-2c0e818c-5dbda386",
 "iat" : 1572709254
}
```

* Verify signature
```c
HMACSHA256(
	base64UrlEncode(header) + "." +
	base64UrlEncode(payload),
	your-secret-key)
 
```

## JWT Claims

 * jti: JWT ID claim provides a unique identifier for the JWT
 * iat: “Issued at” time, in Unix time, at which the token was issued
 
### "iat" (Issued At) 

Claim The "iat" (issued at) claim identifies the time at which the JWT was
issued.This claim can be used to determine the age of the JWT.Its
value MUST be a number containing a NumericDate value.

### "jti" (JWT ID) Claim

The "jti" (JWT ID) claim provides a unique identifier for the JWT.
The identifier value MUST be assigned in a manner that ensures that
there is a negligible probability that the same value will be
accidentally assigned to a different data object; if the application
uses multiple issuers, collisions MUST be prevented among values
produced by different issuers as well.The "jti" claim can be used
to prevent the JWT from being replayed.The "jti" value is a case-
sensitive string.

## License
For details on license, see LICENSE.txt in this directory.
