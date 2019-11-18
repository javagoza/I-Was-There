# I WAS THERE


|Library   |Purpose  |
|---------|---------|
|log     |  Displays messages in the Visual Studio Device Output window during debugging  |
|spi    | Manages SPI interfaces |

## Prerequisites

 This sample requires the following hardware:

- AVNET Azure Sphere MT3620 Kit board
- Jumper wires to connect the boards.


## To build and run

1. Set up your Azure Sphere device and development environment as described in the [Azure Sphere documentation](https://docs.microsoft.com/azure-sphere/install/install).
1. Even if you've performed this set up previously, ensure you have Azure Sphere SDK version 19.10 or above. In an Azure Sphere Developer Command Prompt, run **azsphere show-version** to check. Download and install the [latest SDK](https://aka.ms/AzureSphereSDKDownload) as needed.
1. Connect your Azure Sphere device to your PC by USB.
1. Enable [application development](https://docs.microsoft.com/azure-sphere/quickstarts/qs-blink-application#prepare-your-device-for-development-and-debugging), if you have not already done so:

   `azsphere device prep-debug`
1. Clone the repo and find the IWT_HighLevelApp sample in the SPI folder.
1. In Visual Studio, open IWT_HighLevelApp.sln and press F5 to compile and build the solution and load it onto the device for debugging.


## License
For details on license, see LICENSE.txt in this directory.
