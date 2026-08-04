
#ifndef EXAR_REGS__H
#define EXAR_REGS__H

#define FIOQSIZE 0x5460
#define IOCTL_EXAR_READ_REG (FIOQSIZE + 1)
#define IOCTL_EXAR_WRITE_REG (FIOQSIZE + 2)

#define EXAR_OFFSET_DEVREV 0x8C
#define EXAR_OFFSET_DEVID 0x8D
#define EXAR_OFFSET_MPIOLVL 0x90
#define EXAR_OFFSET_MPIOSEL 0x93
#define EXAR_OFFSET_MPIOLVL_15_8 0x96	// Bits 15:8
#define EXAR_OFFSET_MPIOSEL_15_8 0x99	// Bits 15:8

// The number of ports that settings can be altered for.
#define NUMBER_OF_PORTS 8

// The minimum FPGA revision number that supports the User ID jumper feature.
#define USER_ID_JUMPER_MIN_FPGA_REV 2

// Defines the mask for the MPIO pin connected to 'SK'.
#define MPIO_MASK_SK            0x01

// Defines the mask for the MPIO pin connected to 'DO'.
#define MPIO_MASK_DO            0x02

// Defines the number of bits needed to shift from bit zero to the bit for 'DO'.
#define MPIO_SHIFT_DO           1

// Defines the mask for the MPIO pin connected to 'DI'.
#define MPIO_MASK_DI            0x04

// Defines the number of bits needed to shift from bit zero to the bit for 'DI'.
#define MPIO_SHIFT_DI           2

// Defines the mask for the MPIO pin connected to 'CS'.
#define MPIO_MASK_CS            0x08

// defines the mask for the MPIO User ID Jumper feature
#define MPIO_MASK_USER_ID_JUMPER 0x0F

// Defines the mask for the MPIO pin connected to the EPLD that allows EEPROM
//  access when pulled low.
#define MPIO_MASK_EEPROM_ACCESS 0x10

// Defines the offset into the EEPROM where the User Notes are stored.
#define EEPROM_OFFSET_USER_NOTES    0x00

// Defines the offset into the EEPROM where Revision Control info is stored.
#define EEPROM_OFFSET_REV_CONTROL   0x10

// Defines the size, in bytes, of the area where Revision Control info is
//  stored.
#define EEPROM_SIZE_REV_CONTROL     0x01

// Defines the offset into the EEPROM where Port Config info is stored.
#define EEPROM_OFFSET_PORT_CONFIG   0x11

// Defines the size, in bytes, of the area where Port Config info is stored.
#define EEPROM_SIZE_PORT_CONFIG     0x04

// Defines the mask for the mode bits in the nibble of a port's config.
#define EEPROM_MASK_PORT_MODE       0x07

// Defines the mask for the termination bit in the nibble of a port's config.
// NOTE: When this bit is one, termination is DISABLED. When this bit is zero,
//  termination is ENABLED.
#define EEPROM_MASK_PORT_TERM       0x08

// Defines the offset into the 'extra' memory of the EEPROM.
#define EEPROM_OFFSET_EXTRA_MEM     0x16

// Defines the size, in bytes, of the 'extra' memory of the EEPROM.
#define EEPROM_SIZE_EXTRA_MEM       0xDA

#endif
