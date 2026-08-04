
/**
	@file

	@brief
		SER25330 user library source code

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

	$Id: librtd-ser25330.c 156431 2026-07-31 15:19:12Z Nsmith $
*/

#include <linux/kernel.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "exar_regs.h"
#include "ser25330-library.h"

#define RW_BUFFER_SIZE	5

//Accessing MPIO registers from the user space:
// define these in the app.
/* EXAR ioctls */

struct xrioctl_rw_reg {
	unsigned char reg;
	unsigned char regvalue;
};

/*
	BEGIN Driver access functions.
*/

/*
	Reads a byte from the set of config registers in the Exar chip.

	descriptor - A valid, open handle to the first port on the Exar chip
		being accessed.
	Offset - The offset of the register to read.
	Value - A pointer to a location that will be filled with the byte read
		from the Exar chip.

	Returns 1 on success and 0 otherwise.
*/
int Config_Read(int descriptor, unsigned char exar_register_offset,
		unsigned char *Value)
{
	int success;

	struct xrioctl_rw_reg data_exchange;

	data_exchange.reg = exar_register_offset;

	success = ioctl(descriptor, IOCTL_EXAR_READ_REG, &data_exchange);

	*Value = data_exchange.regvalue;

	return (success == 0);
}

/*
	Writes a byte to the set of config registers in the Exar chip.

	descriptor - A valid, open handle to the first port on the Exar chip
		being accessed.
	Offset - The offset of the register to be written to.
	Value - The value to write to the config register.

	Returns 1 on success and 0 otherwise.
*/
int Config_Write(int descriptor, unsigned char exar_register_offset,
		 unsigned char Value)
{
	int success;
	struct xrioctl_rw_reg data_exchange;

	data_exchange.reg = exar_register_offset;
	data_exchange.regvalue = Value;

	success = ioctl(descriptor, IOCTL_EXAR_WRITE_REG, &data_exchange);

	return (success == 0);
}

/*
	Reads, modifies and writes a byte to the set of config registers in the
	Exar chip.

	descriptor - A valid, open handle to the first port on the Exar chip
		being accessed.
	Offset - The offset of the register to be modified.
	Value - The value to write to the config register.
	Mask - A mask value the describes the bits to be modified. If a mask bit is
		a one, the corresponding bit in the register will be changed to the
		corresponding bit in Value. If a mask bit is zero, the corresponding
		bit in the register will not be changed.

	Returns 1 on success and 0 otherwise.
*/
int Config_Modify(int descriptor, unsigned char exar_register_offset,
		  unsigned char Value, unsigned char Mask)
{
	int success;

	struct xrioctl_rw_reg data_exchange;

	data_exchange.reg = exar_register_offset;

	success = ioctl(descriptor, IOCTL_EXAR_READ_REG, &data_exchange);

	if (success == 0) {
		// Modify
		Value = (data_exchange.regvalue & ~Mask) | (Value & Mask);

		// Write
		data_exchange.regvalue = Value;

		success =
		    ioctl(descriptor, IOCTL_EXAR_WRITE_REG, &data_exchange);
	}

	return (success == 0);
}

/*
	END Driver access functions.
*/

/*
	BEGIN MPIO/EEPROM interface functions.
*/

/*
	Controls the CS line going to the EEPROM.

	descriptor - A valid, open handle to the first port on the Exar chip
		being accessed.
	Enable - Pass in 1 to set CS high or pass in 0 to set CS low.

	Returns 1 on success and 0 otherwise.
*/
int MPIO_SetCS(int descriptor, int Enable)
{
	int success = 1;
	int ret;

	unsigned char portbuf[RW_BUFFER_SIZE];
	portbuf[0] = 'p';
	portbuf[1] = MPIO_MASK_CS;
	portbuf[2] = 0;
	if (Enable) {
		// set CS        
		portbuf[3] = 0xFF;
		ret = write(descriptor, portbuf, RW_BUFFER_SIZE);
	} else {
		// clear CS
		portbuf[3] = 0x00;
		ret = write(descriptor, portbuf, RW_BUFFER_SIZE);
	}

	if (ret < 0)
		success = 0;

	return success;
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
	int success = 1;
	unsigned char portbuf[RW_BUFFER_SIZE];
	int ret;

	portbuf[0] = 'p';
	portbuf[1] = MPIO_MASK_DI;
	portbuf[2] = 0;
	do {
		// Set DI to bit 0 of Value
		portbuf[3] = (Value & 0x01 ? 0xFF : 0);
		ret = write(descriptor, portbuf, RW_BUFFER_SIZE);
		if (ret < 0)
			success = 0;
		if (!success)
			break;

		// Set SK high
		portbuf[1] = MPIO_MASK_SK;
		portbuf[3] = 0xff;
		ret = write(descriptor, portbuf, RW_BUFFER_SIZE);
		if (ret < 0)
			success = 0;
		if (!success)
			break;

		// Wait for 1 millisecond
		usleep(1000);

		// Set SK low
		portbuf[3] = 0x00;
		ret = write(descriptor, portbuf, RW_BUFFER_SIZE);
		if (ret < 0)
			success = 0;
		if (!success)
			break;

		// Wait for 1 millisecond
		usleep(1000);

	} while (0);

	return success;
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
	int success = 1;
	unsigned char portbuf[RW_BUFFER_SIZE];
	unsigned short value;
	int ret;

	portbuf[0] = 'p';
	portbuf[1] = MPIO_MASK_SK;
	portbuf[2] = 0;

	do {
		// Set SK high
		portbuf[3] = 0xff;
		ret = write(descriptor, portbuf, RW_BUFFER_SIZE);
		if (ret < 0)
			success = 0;
		if (!success)
			break;

		// Wait for 1 millisecond
		usleep(1000);

		// Read DO
		ret = read(descriptor, &value, sizeof(value));
		if (ret < 0)
			success = 0;
		if (!success)
			break;

		// Set SK low
		portbuf[3] = 0;
		ret = write(descriptor, portbuf, RW_BUFFER_SIZE);
		if (ret < 0)
			success = 0;
		if (!success)
			break;

		// Wait for 1 millisecond
		usleep(1000);

		// Return DO in bit 0 of Value
		*Value = (value & MPIO_MASK_DO) >> MPIO_SHIFT_DO;

	} while (0);

	return success;
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
int Settings_Open(int descriptor, int *device_handle)
{
	int success = 1;
	unsigned char portbuf[RW_BUFFER_SIZE];
	int ret = 0;

	*device_handle = -1;
	portbuf[0] = 'd';
	portbuf[2] = 0;
	do {
		// Set MPIO bits 0, 2, 3 and 4 to output (SK, DI, CS and EEPROM access)
		// and all other bits to input
		/*success = Config_Write(descriptor, EXAR_OFFSET_MPIOSEL, 0xE2); */
		portbuf[1] = 0xFF;
		portbuf[3] = 0xE2;
		ret = write(descriptor, portbuf, RW_BUFFER_SIZE);
		if (ret < 0)
			success = 0;
		if (!success)
			break;

		// Set outputs to 0 (including setting the EEPROM access bit low)
		/*success = Config_Write(descriptor, EXAR_OFFSET_MPIOLVL, 0); */

		portbuf[0] = 'p';
		portbuf[1] = 0x1D;
		portbuf[3] = 0;

		ret = write(descriptor, portbuf, RW_BUFFER_SIZE);
		if (ret < 0)
			success = 0;
		if (!success)
			break;

	} while (0);

	if (success)
		*device_handle = descriptor;

	return success;
}

/*
	Closes access to EEPROM-stored settings. This causes the EPLD to read and
	apply the settings from the EEPROM.
*/
int Settings_Close(int descriptor)
{
	int success = 1;
	int ret;

	unsigned char portbuf[RW_BUFFER_SIZE];
	portbuf[0] = 'p';
	portbuf[1] = MPIO_MASK_EEPROM_ACCESS;
	portbuf[3] = 0xFF;
	portbuf[2] = 0;
	do {
		// Set the 'EEPROM access' bit high
		ret = write(descriptor, portbuf, RW_BUFFER_SIZE);
		if (ret < 0)
			success = 0;
		if (!success)
			break;

		// Set all bits to input
		portbuf[0] = 'd';
		portbuf[1] = 0xFF;
		portbuf[3] = 0xFF;

		/*      success = Config_Write(descriptor, EXAR_OFFSET_MPIOSEL, 0xFF); */
		ret = write(descriptor, portbuf, RW_BUFFER_SIZE);
		if (ret < 0)
			success = 0;
		if (!success)
			break;

	} while (0);

	return success;
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

		success = Config_Read(descriptor, EXAR_OFFSET_DEVREV, &value);

		if (!success)
			break;
		Info->device_rev = value;

		success = Config_Read(descriptor, EXAR_OFFSET_DEVID, &value);

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
	int status = 1;
	unsigned short value = 0;
	unsigned char fpga_rev;
	int ret;

	do {
		ret = read(descriptor, &value, sizeof(value));
		if (ret < 0) {
			status = 0;
			break;
		}

		value >>= 8;
		fpga_rev = (unsigned char)(value >> 4);

		if (fpga_rev < USER_ID_JUMPER_MIN_FPGA_REV) {
			status = 0;
			break;
		}
		// mask the bits we need
		*Value = value & MPIO_MASK_USER_ID_JUMPER;

	} while (0);

	return status;
}

/*
    END other MPIO functions.
*/
