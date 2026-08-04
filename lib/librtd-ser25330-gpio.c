
/**
	@file

	@brief
		SER25330 user library source code using the EXAR gpio driver in the kernel

	@verbatim
	--------------------------------------------------------------------------
	This file and its contents are copyright (C) RTD Embedded Technologies,
	Inc.  All Rights Reserved.

	This software is licensed as described in the RTD End-User Software License
	Agreement.  For a copy of this agreement, refer to the file LICENSE.TXT
	(which should be included with this software) or contact RTD Embedded
	Technologies, Inc.
	--------------------------------------------------------------------------
	@endverbatim

	$Id: librtd-ser25330-gpio.c 120280 2019-06-03 21:05:41Z amarchini $
*/

#include <linux/kernel.h>
#include <linux/version.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <linux/gpio.h>
#include <errno.h>

#include "exar_regs.h"
#include "ser25330-library.h"

#define RW_BUFFER_SIZE	5

// Setup for 8 boards for now
#define MAX_SER_BOARDS 8

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
#define GPIO_V1
#endif

struct ser_board_desc_rec {
	int file_handle;

#ifdef GPIO_V1
	// global file pointer for the output pins
	struct gpiohandle_request g_out_pins;
	// global file pointer for the input pin
	struct gpiohandle_request g_in_pins;
#else
	// global file pointer for the output pins
	struct gpio_v2_line_request g_out_pins;
	// global file pointer for the input pin
	struct gpio_v2_line_request g_in_pins;
#endif
	uint8_t OutputHi;
	uint8_t OutputLo;
};

struct ser_board_desc_rec serx5330_board[MAX_SER_BOARDS] =
    {[0 ... (MAX_SER_BOARDS - 1)].file_handle = -1 };

/*
	BEGIN MPIO/EEPROM interface functions.
*/

int EEPROM_WRITE_GPIO(int SerDevice)
{
	int ret;
#ifdef GPIO_V1
	struct gpiohandle_data data;
#else
	struct gpio_v2_line_values data;
#endif
	uint8_t g_OUTPUT = serx5330_board[SerDevice].OutputLo;

#ifdef GPIO_V1
	data.values[0] = (g_OUTPUT >> 0) & 0x01;	// SK
	// bit 1 = DO which is an input
	data.values[1] = (g_OUTPUT >> 2) & 0x01;	// DI
	data.values[2] = (g_OUTPUT >> 3) & 0x01;	// CS
	data.values[3] = (g_OUTPUT >> 4) & 0x01;	// EEPROM Enable
#else
	data.mask = 0x0f;	// Mask bits 0-3 to set them

	data.bits = ((g_OUTPUT >> 0) & 0x01)	// SK
	    | ((g_OUTPUT >> 2) & 0x01) << 1	// DI
	    | ((g_OUTPUT >> 3) & 0x01) << 2	// CS
	    | ((g_OUTPUT >> 4) & 0x01) << 3;	// EEPROM Enable
#endif

#ifdef GPIO_V1
	ret = ioctl(serx5330_board[SerDevice].g_out_pins.fd,
		    GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
#else
	ret = ioctl(serx5330_board[SerDevice].g_out_pins.fd,
		    GPIO_V2_LINE_SET_VALUES_IOCTL, &data);

	if (ret == -1) {
		printf("v2 Errno: %d\n", errno);
	}
#endif
	if (ret != 0)
		ret = 0;
	else
		ret = 1;
	return ret;
}

int EEPROM_READ_GPIO(int SerDevice, uint16_t *DATA)
{
	int ret;
#ifdef GPIO_V1
	struct gpiohandle_data data;
#else
	struct gpio_v2_line_values data;
	data.mask = 0x1f;
#endif

#ifdef GPIO_V1
	ret = ioctl(serx5330_board[SerDevice].g_in_pins.fd,
		    GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);

	*DATA = data.values[0];
#else
	ret = ioctl(serx5330_board[SerDevice].g_in_pins.fd,
		    GPIO_V2_LINE_GET_VALUES_IOCTL, &data);

	*DATA = data.bits & 0x01;
#endif
	if (ret != 0)
		ret = 0;
	else
		ret = 1;

	return ret;
}

/*
	Controls the CS line going to the EEPROM.

	descriptor - A valid index to the open gpiochip file
    
	Enable - Pass in 1 to set CS high or pass in 0 to set CS low.

	Returns 1 on success and 0 otherwise.
*/
int MPIO_SetCS(int descriptor, int Enable)
{

	if (Enable) {
		// set CS
		serx5330_board[descriptor].OutputLo |= MPIO_MASK_CS;
	} else {
		// clear CS
		serx5330_board[descriptor].OutputLo &= ~MPIO_MASK_CS;
	}

	return EEPROM_WRITE_GPIO(descriptor);
}

/*
	Clocks a single bit into the EEPROM via the DI line.

	descriptor - A valid, open handle to the first port on the Exar chip
		being accessed.
	Value - Bit zero of this value is the bit clocked in.

	Returns 1 on success and 0 otherwise.
*/
int MPIO_ClockIn(int descriptor, unsigned char Value)
{
	int ret;

	do {
		// Set DI to bit 0 of Value
		serx5330_board[descriptor].OutputLo &= ~MPIO_MASK_DI;
		if (Value)
			serx5330_board[descriptor].OutputLo |= MPIO_MASK_DI;
		ret = EEPROM_WRITE_GPIO(descriptor);
		if (ret == -1)
			break;

		// Set SK high
		serx5330_board[descriptor].OutputLo |= MPIO_MASK_SK;
		ret = EEPROM_WRITE_GPIO(descriptor);
		if (ret == -1)
			break;

		// Wait for 1 millisecond
		usleep(1000);

		// Set SK low
		serx5330_board[descriptor].OutputLo &= ~MPIO_MASK_SK;
		ret = EEPROM_WRITE_GPIO(descriptor);
		if (ret == -1)
			break;

		// Wait for 1 millisecond
		usleep(1000);

	} while (0);

	return ret;
}

/*
	Clocks a single bit out of the EEPROM via the DO line.

	descriptor - A valid, open handle to the first port on the Exar chip
		being accessed.
	Value - A pointer to a location whose bit zero is set to the bit clocked
		out of the EEPROM.

	Returns 1 on success and 0 otherwise.
*/
int MPIO_ClockOut(int descriptor, unsigned char *Value)
{
	uint16_t x;
	int ret;

	do {
		// Set SK high
		serx5330_board[descriptor].OutputLo |= MPIO_MASK_SK;
		ret = EEPROM_WRITE_GPIO(descriptor);
		if (ret == -1)
			break;

		// Wait for 1 millisecond
		usleep(1000);

		// Read DO
		ret = EEPROM_READ_GPIO(descriptor, &x);
		if (ret == -1)
			break;
		// Set SK low
		serx5330_board[descriptor].OutputLo &= ~MPIO_MASK_SK;
		ret = EEPROM_WRITE_GPIO(descriptor);
		if (ret == -1)
			break;

		// Wait for 1 millisecond
		usleep(1000);

		// Return DO in bit 0 of Value
		*Value = (uint8_t) x;

	} while (0);

	return ret;
}

/*
	Reads a byte from a specified location in the EEPROM.

	descriptor - A valid, open handle to the first port on the Exar chip
		being accessed.
	Address - The EEPROM location to read from.
	Value - A pointer to a location which will be filled with the byte read
		from the EEPROM.

	Returns 1 on success and 0 otherwise.
*/
int EEPROM_Read(int descriptor, unsigned char Address, unsigned char *Value)
{
	int success;
	unsigned char value = 0, i;

	do {
		// Set CS high
		success = MPIO_SetCS(descriptor, 1);
		if (!success)
			break;

		// Clock in command bits 1, 1, 0 to indicate a read operation
		success = MPIO_ClockIn(descriptor, 1);
		if (!success)
			break;

		success = MPIO_ClockIn(descriptor, 1);
		if (!success)
			break;

		success = MPIO_ClockIn(descriptor, 0);
		if (!success)
			break;

		// Clock in the address bits
		for (i = 0; i < 7; ++i) {
			success = MPIO_ClockIn(descriptor,
					       (Address & (0x40 >> i)) >> (6 -
									   i));
			if (!success)
				break;
		}
		if (!success)
			break;

		// Clock out the data bits
		*Value = 0;
		for (i = 0; i < 8; ++i) {
			success = MPIO_ClockOut(descriptor, &value);
			if (!success)
				break;

			*Value = (*Value << 1) | (value & 0x1);
		}

		// Set CS low
		success = MPIO_SetCS(descriptor, 0);
		if (!success)
			break;

	} while (0);

	return success;
}

/*
	Sends a command to the EEPROM to enable writing to it.

	descriptor - A valid, open handle to the first port on the Exar chip
		being accessed.

	Returns 1 on success and 0 otherwise.
*/
int EEPROM_WriteEnable(int descriptor)
{
	// Set CS high
	if (!MPIO_SetCS(descriptor, 1))
		return 0;

	// Clock in the command bits 1, 0, 0, 1, 1 to enable writing
	if (!MPIO_ClockIn(descriptor, 1) ||
	    !MPIO_ClockIn(descriptor, 0) ||
	    !MPIO_ClockIn(descriptor, 0) ||
	    !MPIO_ClockIn(descriptor, 1) || !MPIO_ClockIn(descriptor, 1)) {
		return 0;
	}
	// Clock in five 'dummy' bits
	if (!MPIO_ClockIn(descriptor, 0) ||
	    !MPIO_ClockIn(descriptor, 0) ||
	    !MPIO_ClockIn(descriptor, 0) ||
	    !MPIO_ClockIn(descriptor, 0) || !MPIO_ClockIn(descriptor, 0)) {
		return 0;
	}
	// Set CS low
	return MPIO_SetCS(descriptor, 0);
}

/*
	Sends a command to the EEPROM to disable writing to it.

	descriptor - A valid, open handle to the first port on the Exar chip
		being accessed.

	Returns 1 on success and 0 otherwise.
*/
int EEPROM_WriteDisable(int descriptor)
{
	// Set CS high
	if (!MPIO_SetCS(descriptor, 1))
		return 0;

	// Clock in the command bits 1, 0, 0, 0, 0 to enable writing
	if (!MPIO_ClockIn(descriptor, 1) ||
	    !MPIO_ClockIn(descriptor, 0) ||
	    !MPIO_ClockIn(descriptor, 0) ||
	    !MPIO_ClockIn(descriptor, 0) || !MPIO_ClockIn(descriptor, 0)) {
		return 0;
	}
	// Clock in five 'dummy' bits
	if (!MPIO_ClockIn(descriptor, 0) ||
	    !MPIO_ClockIn(descriptor, 0) ||
	    !MPIO_ClockIn(descriptor, 0) ||
	    !MPIO_ClockIn(descriptor, 0) || !MPIO_ClockIn(descriptor, 0)) {
		return 0;
	}
	// Set CS low
	return MPIO_SetCS(descriptor, 0);
}

/*
	Writes a byte to a specified location in the EEPROM.

	descriptor - A valid, open handle to the first port on the Exar chip
		being accessed.
	Address - The EEPROM location to write to.
	Value - The value to write to the EEPROM.

	Returns 1 on success and 0 otherwise.
*/
int EEPROM_Write(int descriptor, unsigned char Address, unsigned char Value)
{
	int success;
	unsigned char i;

	do {
		success = EEPROM_WriteEnable(descriptor);
		if (!success)
			break;

		// Set CS high
		success = MPIO_SetCS(descriptor, 1);
		if (!success)
			break;

		// Clock in the command bits 1, 0, 1
		if (!MPIO_ClockIn(descriptor, 1) ||
		    !MPIO_ClockIn(descriptor, 0) ||
		    !MPIO_ClockIn(descriptor, 1)) {
			success = 0;
			break;
		}
		// Clock in address bits
		for (i = 0; i < 7; ++i) {
			success = MPIO_ClockIn(descriptor,
					       (Address & (0x40 >> i)) >> (6 -
									   i));
			if (!success)
				break;
		}
		if (!success)
			break;

		// Clock in data bits
		for (i = 0; i < 8; ++i) {
			success = MPIO_ClockIn(descriptor,
					       (Value & (0x80 >> i)) >> (7 -
									 i));
			if (!success)
				break;
		}
		if (!success)
			break;

		// Set CS low
		success = MPIO_SetCS(descriptor, 0);
		if (!success)
			break;

		// Wait an arbitrary amount of time to allow the write to happen
		usleep(10000);

		success = EEPROM_WriteDisable(descriptor);
		if (!success)
			break;

	} while (0);

	return success;
}

/*
	END MPIO/EEPROM interface functions.
*/

/*
	BEGIN Settings Control functions.

	These functions are fully documented in the header file.
*/

/*
	Opens and prepares access to EEPROM-stored settings.
*/
int Settings_Open(int file_handle, int *device_handle)
{
	int lhfd = 0;
	int scan;
	struct gpiochip_info chipinfo;	// Chipinfo is the same for v1 & v2
#ifdef GPIO_V1
	struct gpiohandle_data data;
	// --file pointer for the output pins
	struct gpiohandle_request out_pins;
	// --file pointer for the input pin
	struct gpiohandle_request in_pins;
	struct gpioline_info lineinfo;
#else
	struct gpio_v2_line_values data;
	struct gpio_v2_line_info lineinfo;
	// --file pointer for the output pins
	struct gpio_v2_line_request out_pins;
	// --file pointer for the input pin
	struct gpio_v2_line_request in_pins;

	// all padding must be zero set
	memset(lineinfo.padding, 0, sizeof(lineinfo.padding));
	memset(out_pins.padding, 0, sizeof(out_pins.padding));
	memset(out_pins.config.padding, 0, sizeof(out_pins.config.padding));
	memset(in_pins.padding, 0, sizeof(in_pins.padding));
	memset(in_pins.config.padding, 0, sizeof(in_pins.config.padding));
#endif
	*device_handle = -1;

	do {
		// try to get output services 
		// Set MPIO bits 0, 2, 3 and 4 to output (SK, DI, CS and EEPROM access)
		// and all other bits to input

#ifdef GPIO_V1
		out_pins.lineoffsets[0] = 0;
		out_pins.lineoffsets[1] = 2;
		out_pins.lineoffsets[2] = 3;
		out_pins.lineoffsets[3] = 4;
		out_pins.lines = 4;
		out_pins.flags = GPIOHANDLE_REQUEST_OUTPUT;
		strcpy(out_pins.consumer_label, "eeprom_out");
#else
		out_pins.offsets[0] = 0;
		out_pins.offsets[1] = 2;
		out_pins.offsets[2] = 3;
		out_pins.offsets[3] = 4;
		out_pins.num_lines = 4;
		out_pins.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
		out_pins.config.num_attrs = 0;
		strcpy(out_pins.consumer, "eeprom_out");
#endif

		// Getting some additional Debugging information from the chip
		lhfd = ioctl(file_handle, GPIO_GET_CHIPINFO_IOCTL, &chipinfo);	// Chipinfo is the same for v1 & v2
		if (lhfd == -1) {
			printf("Errno: %d\n", errno);
			break;
		}
		// printf("Name: %s\n",chipinfo.name);
		// printf("label: %s\n",chipinfo.label);

		// Getting some additional Debugging information from the chip
#ifdef GPIO_V1
		lineinfo.line_offset = 0;
		lhfd = ioctl(file_handle, GPIO_GET_LINEINFO_IOCTL, &lineinfo);
		if (lhfd == -1) {
			printf("baba Errno: %d\n", errno);
			break;
		}
		// printf("Name: %s\n",lineinfo.name);
		// printf("consumer: %s\n",lineinfo.consumer);
		// printf("flags: %d\n",lineinfo.flags);
		// printf("line_offset: %d\n",lineinfo.line_offset);
#else
		lineinfo.offset = 0;
		lhfd =
		    ioctl(file_handle, GPIO_V2_GET_LINEINFO_IOCTL, &lineinfo);
		if (lhfd == -1) {
			printf("v2 Errno: %d\n", errno);
			break;
		}
		// printf("Name: %s\n",lineinfo.name);
		// printf("consumer: %s\n",lineinfo.consumer);
		// printf("flags: %lld\n",lineinfo.flags);
		// printf("line_offset: %d\n",lineinfo.offset);
#endif

#ifdef GPIO_V1
		lhfd = ioctl(file_handle, GPIO_GET_LINEHANDLE_IOCTL, &out_pins);
		if (lhfd == -1) {
			printf("Errno: %d\n", errno);
			break;
		}
		data.values[0] = 0;
		data.values[1] = 0;
		data.values[2] = 0;
		data.values[3] = 0;
#else
		lhfd = ioctl(file_handle, GPIO_V2_GET_LINE_IOCTL, &out_pins);
		if (lhfd == -1) {
			printf("v2 e1 Errno: %d\n", errno);
			break;
		}
		data.bits = 0x00;
		data.mask = 0x0f;	// 4 lines == bits 0-3, so mask these
#endif

#ifdef GPIO_V1
		lhfd =
		    ioctl(out_pins.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
		if (lhfd == -1) {
			printf("Errno: %d\n", errno);
			break;
		}
#else
		lhfd = ioctl(out_pins.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &data);
		if (lhfd == -1) {
			printf("v2 e2 Errno: %d\n", errno);
			break;
		}
#endif

		// try to get input services
		// Set MPIO bits 1 to input  (DO)
#ifdef GPIO_V1
		in_pins.lineoffsets[0] = 1;
		in_pins.lineoffsets[1] = 8;
		in_pins.lineoffsets[2] = 9;
		in_pins.lineoffsets[3] = 10;
		in_pins.lineoffsets[4] = 11;
		in_pins.lines = 5;
		in_pins.flags = GPIOHANDLE_REQUEST_INPUT;
		strcpy(in_pins.consumer_label, "eeprom_do");
#else
		in_pins.offsets[0] = 1;
		in_pins.offsets[1] = 8;
		in_pins.offsets[2] = 9;
		in_pins.offsets[3] = 10;
		in_pins.offsets[4] = 11;
		in_pins.num_lines = 5;
		in_pins.config.flags = GPIO_V2_LINE_FLAG_INPUT;
		in_pins.config.num_attrs = 0;
		strcpy(in_pins.consumer, "eeprom_do");
#endif

#ifdef GPIO_V1
		lhfd = ioctl(file_handle, GPIO_GET_LINEHANDLE_IOCTL, &in_pins);
		if (lhfd == -1) {
			break;
		}
#else
		lhfd = ioctl(file_handle, GPIO_V2_GET_LINE_IOCTL, &in_pins);
		if (lhfd == -1) {
			printf("v2 e3 Errno: %d\n", errno);
			break;
		}
#endif

		// locate an empty slot and save all the in a record
		for (scan = 0; scan < MAX_SER_BOARDS; scan++) {
			if (serx5330_board[scan].file_handle == -1) {
				// we found a slot
				serx5330_board[scan].file_handle = file_handle;
				serx5330_board[scan].g_out_pins = out_pins;
				serx5330_board[scan].g_in_pins = in_pins;
				serx5330_board[scan].OutputHi = 0;
				serx5330_board[scan].OutputLo = 0;
				*device_handle = scan;
				break;
			}
		}
		if (*device_handle == -1) {
			lhfd = -1;
		}

	} while (0);

	if (lhfd < 0)
		return 0;

	// success
	return 1;
}

/*
	Closes access to EEPROM-stored settings. This causes the EPLD to read and
	apply the settings from the EEPROM.
*/
int Settings_Close(int descriptor)
{
#ifdef GPIO_V1
	struct gpiohandle_data data;
#else
	struct gpio_v2_line_values data;
#endif
	int lhfd;

	do {
		// Set the 'EEPROM access' bit high
#ifdef GPIO_V1
		data.values[0] = 1;
		data.values[1] = 1;
		data.values[2] = 1;
		data.values[3] = 1;
		lhfd =
		    ioctl(serx5330_board[descriptor].g_out_pins.fd,
			  GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
		if (lhfd == -1)
			break;
#else
		data.mask = 0x0f;
		data.bits = 0x0f;
		lhfd =
		    ioctl(serx5330_board[descriptor].g_out_pins.fd,
			  GPIO_V2_LINE_SET_VALUES_IOCTL, &data);
		if (lhfd == -1)
			break;
#endif
		close(serx5330_board[descriptor].g_out_pins.fd);
		close(serx5330_board[descriptor].g_in_pins.fd);
		serx5330_board[descriptor].file_handle = -1;
		serx5330_board[descriptor].OutputHi = 0;
		serx5330_board[descriptor].OutputLo = 0;

	} while (0);

	if (lhfd < 0)
		return 0;

	// success
	return 1;

}

/*
	Gets the Revision Control byte and User Note from the settings EEPROM.
*/
int Settings_GetInfo(int descriptor, Settings_Info *Info)
{
	int success = 1;
	int i;

	do {
		// Check parameters
		if (Info == NULL) {
			success = 0;

			break;
		}
		// Read the User Notes section of the EEPROM
		for (i = 0; i < SETTINGS_USER_NOTE_SIZE; ++i) {
			success = EEPROM_Read(descriptor,
					      EEPROM_OFFSET_USER_NOTES + i,
					      &(Info->UserNotes[i]));
			if (!success)
				break;
		}
		if (!success)
			break;

	} while (0);

	return success;
}

/*
	Sets the User Notes in the settings.
*/
int Settings_SetInfo(int descriptor, Settings_Info *Info)
{
	int status = 1;
	int i;

	do {
		// Check parameters
		if (Info == NULL) {
			status = 0;

			break;
		}
		// Write the given User Notes info to the EEPROM
		for (i = 0; i < SETTINGS_USER_NOTE_SIZE; ++i) {
			status = EEPROM_Write(descriptor,
					      EEPROM_OFFSET_USER_NOTES + i,
					      Info->UserNotes[i]);
			if (!status) {
				break;
			}
		}
		if (!status) {
			break;
		}

	} while (0);

	return status;
}

/*
	Gets the Device Revision information from the settings EEPROM.
*/
int Settings_GetDevInfo(int descriptor, Device_Info *Info)
{
	int success = 1;

	unsigned char value = 0;

	do {
		// Check parameters
		if (Info == NULL) {
			success = 0;

			break;
		}
//              success = Config_Read(descriptor, EXAR_OFFSET_DEVREV, &value);

		if (!success)
			break;
		Info->device_rev = value;

//              success = Config_Read(descriptor, EXAR_OFFSET_DEVID, &value);

		if (!success)
			break;
		Info->device_id = value;

	} while (0);

	return success;
}

/*
	Gets the current settings configuration for a single port.
*/
int Settings_GetPortConfig(int descriptor,
			   int PortIndex,
			   Settings_ModeType *Mode, int *TermEnabled)
{
	int success = 1;
	unsigned char address, value;

	do {
		// Check parameters
		if ((PortIndex < 0) || (PortIndex >= SETTINGS_PORT_MAX) ||
		    (Mode == NULL) || (TermEnabled == NULL)) {
			success = 0;

			break;
		}
		// Compute the address to read
		address = EEPROM_OFFSET_PORT_CONFIG + (PortIndex / 2);

		// Read the port configuration byte
		success = EEPROM_Read(descriptor, address, &value);
		if (!success)
			break;

		// Extract the exact config values
		if (PortIndex % 2 == 0) {
			value >>= 4;
		} else {
			value &= 0x0F;
		}

		switch (value & EEPROM_MASK_PORT_MODE) {
		case SETTINGS_PORT_MODE_RS232:
			*Mode = SETTINGS_PORT_MODE_RS232;
			break;
		case SETTINGS_PORT_MODE_RS422:
			*Mode = SETTINGS_PORT_MODE_RS422;
			break;
		case SETTINGS_PORT_MODE_RS485:
			*Mode = SETTINGS_PORT_MODE_RS485;
			break;
		case SETTINGS_PORT_MODE_RS485_RTS_INV:
			*Mode = SETTINGS_PORT_MODE_RS485_RTS_INV;
			break;
		}

		if (value & EEPROM_MASK_PORT_TERM)
			*TermEnabled = 0;
		else
			*TermEnabled = 1;

	} while (0);

	return success;
}

/*
	Sets the settings configuration for a single port.
*/
int Settings_SetPortConfig(int descriptor,
			   int PortIndex,
			   Settings_ModeType Mode, int EnableTerm)
{
	int success = 1;
	unsigned char address, old_config, new_config;

	do {
		// Check parameters
		if ((PortIndex < 0) || (PortIndex >= SETTINGS_PORT_MAX) ||
		    ((Mode != SETTINGS_PORT_MODE_RS232) &&
		     (Mode != SETTINGS_PORT_MODE_RS422) &&
		     (Mode != SETTINGS_PORT_MODE_RS485) &&
		     (Mode != SETTINGS_PORT_MODE_RS485_RTS_INV))) {
			success = 0;
			break;
		}
		// Compute the address to read and write
		address = EEPROM_OFFSET_PORT_CONFIG + (PortIndex / 2);

		// Read the old config from the port configuration byte
		success = EEPROM_Read(descriptor, address, &old_config);
		if (!success)
			break;

		// Create the new config value from the config parameters
		new_config = (unsigned char)Mode;
		if (!EnableTerm)
			new_config |= EEPROM_MASK_PORT_TERM;

		// Combine it with the old config value, based on the location of the
		//  port's nibble
		if (PortIndex % 2 == 0) {
			// High nibble
			new_config = (old_config & 0x0F) | (new_config << 4);
		} else {
			// Low nibble
			new_config = (old_config & 0xF0) | new_config;
		}

		// Write out the new configuration byte
		success = EEPROM_Write(descriptor, address, new_config);
		if (!success)
			break;

	} while (0);

	return success;
}

/*
	END Settings Control functions.
*/

/*
    BEGIN other MPIO functions.
*/

/*
    Reads the 4 bits making up the User ID Jumper field.
*/
int MPIO_ReadUserIdJumper(int descriptor, unsigned char *Value)
{

	int ret;
#ifdef GPIO_V1
	struct gpiohandle_data data;

	ret = ioctl(serx5330_board[descriptor].g_in_pins.fd,
		    GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);

	*Value = data.values[4];
	*Value = (*Value << 1) | data.values[3];
	*Value = (*Value << 1) | data.values[2];
	*Value = (*Value << 1) | data.values[1];
#else
	struct gpio_v2_line_values data;
	data.mask = 0x1f;

	ret = ioctl(serx5330_board[descriptor].g_in_pins.fd,
		    GPIO_V2_LINE_GET_VALUES_IOCTL, &data);

	*Value = ((data.bits & 0x1e) >> 1);
#endif
	if (ret != 0)
		ret = 0;
	else
		ret = 1;

	return ret;
}

/*
    END other MPIO functions.
*/
