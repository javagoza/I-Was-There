#include <stdint.h>
#include "iwt_display.h"
#include <math.h>

uint8_t oled_state = 0;

// Data of sensors (Acceleration, Gyro, Temperature, Presure)
sensor_var sensor_data;

// Data of network status
network_var network_data;

// Data of light sensor
float light_sensor;

// Altitude
extern float altitude;

// Array with messages from Azure
extern uint8_t oled_ms1[CLOUD_MSG_SIZE];
extern uint8_t oled_ms2[CLOUD_MSG_SIZE];
extern uint8_t oled_ms3[CLOUD_MSG_SIZE];
extern uint8_t oled_ms4[CLOUD_MSG_SIZE];

// Status variables of I2C bus and RT core
extern uint8_t RTCore_status;
extern uint8_t lsm6dso_status;
extern uint8_t lps22hh_status;


/**
  * @brief  Get the channel at given frequency
  * @param  freq_MHz: Frequency in MHz
  * @retval Channel.
  */
uint16_t get_channel(uint16_t freq_MHz)
{
	if (freq_MHz < 5000 && freq_MHz > 2400)
	{
		// channel of in 2.4 GHz band
		freq_MHz -= 2407;
	}
	else if (freq_MHz > 5000)
	{
		// channel of in 5 GHz band
		freq_MHz -= 5000;
	}
	else
	{
		// channel not in 2.4 or 5 GHz bands
		freq_MHz = 0;
	}

	freq_MHz /= 5;

	return freq_MHz;
}






// reverses a string 'str' of length 'len' 
static void reverse(uint8_t* str, int32_t len)
{
	int32_t i = 0;
	int32_t j = len - 1;
	int32_t temp;

	while (i < j)
	{
		temp = str[i];
		str[i] = str[j];
		str[j] = temp;
		i++; j--;
	}
}

/**
  * @brief  Converts a given integer x to string uint8_t[]
  * @param  x: x integer input
  * @param  str: uint8_t array output
  * @param  d: Number of zeros added
  * @retval i: number of digits
  */
int32_t intToStr(int32_t x, uint8_t str[], int32_t d)
{
	int32_t i = 0;
	uint8_t flag_neg = 0;

	if (x < 0)
	{
		flag_neg = 1;
		x *= -1;
	}
	while (x)
	{
		str[i++] = (x % 10) + '0';
		x = x / 10;
	}

	// If number of digits required is more, then 
	// add 0s at the beginning 
	while (i < d)
	{
		str[i++] = '0';
	}

	if (flag_neg)
	{
		str[i] = '-';
		i++;
	}

	reverse(str, i);
	str[i] = '\0';
	return i;
}

/**
  * @brief  Converts a given integer x to string uint8_t[]
  * @param  n: float number to convert
  * @param  res:
  * @param  afterpoint:
  * @retval None.
  */
void ftoa(float n, uint8_t* res, int32_t afterpoint)
{
	// Extract integer part 
	int32_t ipart = (int32_t)n;

	// Extract floating part 
	float fpart = n - (float)ipart;

	int32_t i;

	if (ipart < 0)
	{
		res[0] = '-';
		res++;
		ipart *= -1;
	}

	if (fpart < 0)
	{
		fpart *= -1;

		if (ipart == 0)
		{
			res[0] = '-';
			res++;
		}
	}

	// convert integer part to string 
	i = intToStr(ipart, res, 1);

	// check for display option after point 
	if (afterpoint != 0)
	{
		res[i] = '.';  // add dot 

		// Get the value of fraction part upto given no. 
		// of points after dot. The third parameter is needed 
		// to handle cases like 233.007 
		fpart = fpart * pow(10, afterpoint);

		intToStr((int32_t)fpart, res + i + 1, afterpoint);
	}
}


uint8_t get_str_size(uint8_t* str)
{
	uint8_t legth = 0;
	while (*(str) != NULL)
	{
		str++;
		legth++;
	}
	return legth;
}