/**
    @file

    @brief
        Example program which demonstrates using configuration functions
        to change the functionality of serial ports.

    @verbatim

	Because the board is jumperless, a means must be provided to
	change the serial port modes (RS232, RS422, etc).  That means is
	through manipulation of the MPIO pins in a specific sequence.  This
	example program demonstrates how that is accomplished, using the
	SER35330 library.

	To use this example:

			./config-example [dev file]

	The [dev file] is the device file for the gpio controlling the board.

    @endverbatim

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

    $Id: config-example.c 156431 2026-07-31 15:19:12Z Nsmith $
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <getopt.h>
#include <limits.h>

#include "ser25330-library.h"

#define DEFAULT_DEVICE_NAME "/dev/gpiochip0";

#define MAX_OPEN_TTYS 8
struct ttyrecs {
	int ttyfileptr;
	char ttynom[250];
	int RTS_state;
};

struct ttyrecs ttyfiletab[MAX_OPEN_TTYS];

/* 
  scan backwards through a string to find t he numeric
  value at the end.

   ex /dev/tty34 - returns an integer of 34 
     *numbegin will point to the '3' character.

   returns -1 on failure (numbegin will returned as null)
   it is assumed the user will never have a tty1000 or
   greater. 
 */
int scanttyindex(char *sptr, char **numbegin)
{
	int ret;
	unsigned long convert;
	char *invalid_char_p;
	int eolp = strlen(sptr);

	if (eolp > 0) {
		ret = eolp - 1;	// start one character back.
		do {
			if (isdigit(sptr[ret])) {
				ret--;
			} else {
				// number not found...just
				// stop the loop
				break;
			}

		} while (ret >= 0);

		// at this point ret == -1 is a valid value
		// however it is also an error at the end
		// the next if statment will clean this up
		// after this if statement ret = -1 means
		// an error.
		if ((ret == -1) || (ret != (eolp - 1))) {
			// some part of a number found 
			// starting at ret +1
			ret++;
			*numbegin = &sptr[ret];
			convert = strtoul(*numbegin, &invalid_char_p, 10);
			// testing all error cases.
			if ((convert == ULONG_MAX) && (errno == ERANGE))
				ret = -1;
			if (*invalid_char_p != '\0')
				ret = -1;
			if ((ret > -1) && (convert < 1000))
				ret = (int)convert;
			else
				ret = -1;
		} else {
			ret = -1;
		}

	} else {
		ret = -1;
	}

	if (ret == -1)
		*numbegin = NULL;

	return ret;
}

/*
  return a file handle or -1 for error
 */
int Opentty(char *ttyfilename, int portindex, char *fnameactual)
{
	char *restringptr;
	char filenamecpy[200];
	int ttyindex;
	int fd = -1;

	do {
		// is the ttyfilename close to legit?? Is it /dev/ttyx ?
		if (strlen(ttyfilename) < 9)
			break;
		if ((portindex < 0) || (portindex > 7))
			break;
		strncpy(filenamecpy, ttyfilename, 200);

		ttyindex = scanttyindex(filenamecpy, &restringptr);
		if (ttyindex < 0)
			break;

		// reform the index by chopping of the base port index
		// and attaching the port offset (0-7) so we can
		// adjust the proper RTS line.
		ttyindex += portindex;
		*restringptr = '\0';
		sprintf(restringptr, "%d", ttyindex);

		strncpy(fnameactual, filenamecpy, 200);

		// Open the /dev/ file object that represents port 0 on the board.
		// All configuration actions have to be executed on the first port.
		fd = open(filenamecpy, O_RDWR | O_NOCTTY | O_NONBLOCK);

	} while (0);

	return fd;
}

int SetPortRts(int fd, int rtsVal)
{
	int portFlags;

	if (fd < 0)
		return 0;

	if (ioctl(fd, TIOCMGET, &portFlags) == -1) {
		return 0;
	}

	if (rtsVal == 1) {
		portFlags |= TIOCM_RTS;
	} else {
		portFlags &= ~TIOCM_RTS;
	}

	if (ioctl(fd, TIOCMSET, &portFlags) == -1) {
		return 0;
	}

	return 1;
}

int GetPortRts(int fd, int *rtsVal)
{
	int portFlags;

	if (fd < 0)
		return 0;

	if (ioctl(fd, TIOCMGET, &portFlags) == -1) {
		return 0;
	}

	*rtsVal = (portFlags & TIOCM_RTS);

	return 1;
}

int load_all_ttys(char *ttydev)
{
	int ports;
	int fptr;
	int ret = -1;

	do {

		for (ports = 0; ports < MAX_OPEN_TTYS; ports++) {
			ttyfiletab[ports].ttynom[0] = '\0';
			ttyfiletab[ports].ttyfileptr = -1;
			fptr = Opentty(ttydev, ports, ttyfiletab[ports].ttynom);
			if (fptr < 0) {
				break;
			}
			ttyfiletab[ports].ttyfileptr = fptr;
		}

		if (ports < MAX_OPEN_TTYS) {
			// we didn't open them all ...so close them
			int pclose;
			for (pclose = 0; pclose <= ports; pclose++) {
				close(ttyfiletab[pclose].ttyfileptr);
				ttyfiletab[pclose].ttyfileptr = -1;
			}
			break;
		} else {
			ret = 0;
		}

	} while (0);

	return ret;
}

void print_port_settings(int fileDesc, char *ttyfile)
{

	int index = 0;
	int success = 0;
	Settings_ModeType mode;
	int termination = 0;
	int rs485Mode = 0;

	printf("\n===================================================\n");
	printf(" Port Settings\n\n");

	for (index = 0; index < SETTINGS_PORT_MAX; index++) {
		success =
		    Settings_GetPortConfig(fileDesc, index, &mode,
					   &termination);

		if (!success) {
			printf
			    ("Error calling GetPortConfig on port %d. Error code %d.\n",
			     index, success);
			exit(EXIT_FAILURE);
		}

		switch (mode) {
		case SETTINGS_PORT_MODE_RS232:
			printf("Port %d, Mode RS232, ", index);
			break;
		case SETTINGS_PORT_MODE_RS422:
			printf("Port %d, Mode RS422, ", index);
			break;
		case SETTINGS_PORT_MODE_RS485:
			printf("Port %d, Mode RS485, ", index);
			rs485Mode = 1;
			break;
		case SETTINGS_PORT_MODE_RS485_RTS_INV:
			printf("Port %d, Mode RS485 (RTS Inverted), ", index);
			rs485Mode = 1;
			break;
		default:
			printf("Port %d, Unknown mode (%d), ", index, mode);
			break;

		}

		if (termination) {
			printf("Termination Enabled");
		} else {
			printf("Termination Disabled");
		}
		termination = 0;

		// We can only change the RTS on the current serial port we're on, which
		// must be port 0, since that is the port we have to use to change
		// configuration settings via the MPIO.
		if (rs485Mode) {
			if (ttyfiletab[index].ttyfileptr >= 0) {
				success =
				    GetPortRts(ttyfiletab[index].ttyfileptr,
					       &ttyfiletab[index].RTS_state);
				if (success) {
					printf(",  RTS: ");
					if (ttyfiletab[index].RTS_state) {
						printf("Enabled");
					} else {
						printf("Disabled");
					}
				} else {
					printf(", RTS: Unknown (error)");
				}
			}
		}

		printf("\n");

	}

}

void eat_newlines()
{

	char my_char = '0';

	while (my_char != '\n') {
		my_char = getchar();
	}

}

void usage()
{

	fprintf(stderr,
		"\n\nUsage: config-example [--help] [--gpio GPIOFILE] [--ttyfile TTYFILENAME]\n");
	fprintf(stderr,
		"    --help:     Display usage information and exit.\n");
	fprintf(stderr, "    --gpio:  Use specified gpio control file.\n");
	fprintf(stderr,
		"        GPIOFILE        This should be the gpio target file for the serial port board.\n");
	fprintf(stderr, "                    ex:  /dev/gpiochip0\n");
	fprintf(stderr, "    --ttyfile:  Use specified Base TTY file.\n");
	fprintf(stderr,
		"        TTYFILENAME:   This should be the first serial port on the board.\n");
	fprintf(stderr, "                    ex:  /dev/ttyS4\n");

	fprintf(stderr, "\n\n");

}

/**
*******************************************************************************
@brief
    Main program code.

@param
    argument_count

    Number of command line arguments passed to executable.

@param
    arguments

    Address of array containing command line arguments.

@retval
    EXIT_SUCCESS

    Success.

@retval
    EXIT_FAILURE

    Failure.
 ******************************************************************************/
int main(int argument_count, char **arguments)
{

	int fd;
	int dfd;		// device file descriptor. 
	Settings_Info info;
	Settings_ModeType mode;
	unsigned char byte_val;
	int success;
	int termination;
	int rtsVal = 0;
	char choice[20];
	int validMode = 0;
	int setRts = 0;
	int port;
	int ports;
	char userData[100];
	int index = 0;
	char single_ch = 0;
	int result = 0;

	int help_option_given = 0;
	int gpio_option_given = 0;
	int ttyfile_option_given = 0;

	char devName[250] = DEFAULT_DEVICE_NAME;
	char ttyfname[250] = "\0";

	struct option options[] = {
		{ "help", no_argument, 0, HELP_OPTION },
		{ "gpio", required_argument, 0, GPIO_FILE_OPTION },
		{ "ttyfile", required_argument, 0, TTY_FILE_OPTION },
		{ 0, 0, 0, 0 }
	};

	/*
	 * Show usage, parse arguments
	 */
	while (1) {
		/* Parse the next command line option and its arguments */
		result =
		    getopt_long(argument_count, arguments, "", options, NULL);

		/* If getopt_long() returned -1, then all options have been processed */
		if (result == -1) {
			break;
		}

		/* Figure out what getopt_long() found */
		switch (result) {
			/* 
			 * User entered '--help' 
			 */
		case HELP_OPTION:
			/* Refuse to accept duplicate '--help' options */
			if (help_option_given) {
				error(0, 0, "ERROR: Duplicate option '--help'");
				usage();
			}
			/* '--help' option has been seen */
			help_option_given = 0xFF;
			break;
		case GPIO_FILE_OPTION:
			if (gpio_option_given) {
				error(0, 0,
				      "ERROR: Duplicate option: '--gpio'");
				usage();
			}
			/* Copy the parameter string */
			strncpy(devName, optarg, sizeof(devName) - 1);
			gpio_option_given = 0xFF;
			break;
		case TTY_FILE_OPTION:
			if (ttyfile_option_given) {
				error(0, 0,
				      "ERROR: Duplicate option: '--ttyfile'");
				usage();
			}
			strncpy(ttyfname, optarg, sizeof(ttyfname) - 1);
			ttyfile_option_given = 0xFF;
			break;
		default:
			error(EXIT_FAILURE, 0,
			      "ERROR: Program generated unexpected value %#x",
			      result);
			break;
		}
	}

	/*
	 * Recognize '--help' option before any others
	 */
	if (help_option_given) {
		usage();
	}
	// if the tty port is supplied then attempt to 
	// open a table of tty ports so we can see and 
	// modify the RTS line. 
	// This is for testing, and has no affect after
	// the program is closed.
	for (ports = 0; ports < MAX_OPEN_TTYS; ports++) {
		ttyfiletab[ports].ttynom[0] = '\0';
		ttyfiletab[ports].ttyfileptr = -1;
	}

	if (ttyfile_option_given) {
		success = load_all_ttys(ttyfname);
		if (success == -1) {
			printf
			    ("\n\nUnable to open 8 ports starting at: %s\n\nVerify the correct staring tty for the board.\n\n",
			     ttyfname);
			return 1;
		}
	}
	// Open the /dev/ file object that represents port 0 on the board.
	// All configuration actions have to be executed on the first port.
	errno = 0;
	fd = open(devName, O_RDWR);

	if (fd < 0) {
		if (errno == ENOENT) {
			printf
			    ("\n\nFile does not exist: %s\n\nPlease make sure the Exar driver is loaded\n\n",
			     devName);
			return 1;
		}
		if (errno == EACCES) {
			printf
			    ("\n\nPermission denied accessing file: %s.\n\nPlease run as root.\n\n",
			     devName);
			return 1;
		}
		error(0, 0, "\nerrno: %d Error opening file: %s\n\n", errno,
		      devName);
		exit(EXIT_FAILURE);
	}

	/* Before dealing with the EEPROM through MPIO, we need to
	 * open it.
	 */
	success = Settings_Open(fd, &dfd);

	if (!success) {
		printf("Error opening EEPROM settings. Error code %d.\n",
		       success);
		exit(EXIT_FAILURE);
	}

	/* Read the user notes and revision area */
	success = Settings_GetInfo(dfd, &info);

	if (!success) {
		printf("Error calling GetInfo. Error code %d.\n", success);
		exit(EXIT_FAILURE);
	}

	printf("\nUser Notes: ");
	for (index = 0; index < 16; index++) {
		printf("%c", (char)info.UserNotes[index]);
	}
	printf("\n\n");

	success = MPIO_ReadUserIdJumper(dfd, &byte_val);
	if (success) {
		printf("User ID Jumper Val: 0x%x\n", byte_val);
	} else {
		printf("\n\n## Error reading user ID jumper\n");
	}

	print_port_settings(dfd, ttyfname);
	/*
	 * Once done with the EEPROM, close the settings
	 */
	success = Settings_Close(dfd);

	if (!success) {
		printf("Error closing EEPROM settings. Error code %d.\n",
		       success);
		exit(EXIT_FAILURE);
	}

	close(fd);
	printf
	    ("\nEnter port to change (0 - 7), 'u' to set user note, or 'x' to exit: ");

	result = scanf("%s", choice);

	if (result < 1) {
		exit(EXIT_FAILURE);
	}

	eat_newlines();

	while (choice[0] != 'x' && choice[0] != 'X') {

		// Open the /dev/ file object that represents port 0 on the board.
		// All configuration actions have to be executed on the first port.
		fd = open(devName, O_RDWR | O_NOCTTY | O_NONBLOCK);

		if (fd < 0) {
			printf(" Could not open file: %s\n Is the driver "
			       "loaded?  If so, try again as root.\n\n",
			       devName);
			exit(EXIT_FAILURE);
		}

		/* Before dealing with the EEPROM through MPIO, we need to
		 * open it.
		 */
		success = Settings_Open(fd, &dfd);

		if (!success) {
			printf
			    ("Error opening EEPROM settings. Error code %d.\n",
			     success);
			exit(EXIT_FAILURE);
		}

		if (choice[0] == 'u') {
			int char_count = 0;
			fputs("\nEnter user note (16 characters): ", stdout);
			fflush(stdout);
			index = 0;
			result = scanf("%c", &single_ch);
			while (char_count < 16 && single_ch != '\n') {
				userData[char_count++] = single_ch;
				result = scanf("%c", &single_ch);

			}

			if (single_ch != '\n') {
				eat_newlines();
			}

			for (index = 0; index < 16; index++) {
				info.UserNotes[index] = '\0';
			}

			for (index = 0; index < char_count; index++) {
				info.UserNotes[index] = userData[index];
			}

			success = Settings_SetInfo(dfd, &info);
			if (!success) {
				printf
				    ("Error setting User Notes. Error code %d.\n",
				     success);
				exit(EXIT_FAILURE);
			}

			/* Read the user notes and revision area */
			success = Settings_GetInfo(dfd, &info);

			if (!success) {
				printf
				    ("Error calling GetInfo. Error code %d.\n",
				     success);
				exit(EXIT_FAILURE);
			}

			printf("\nUser Notes: ");
			for (index = 0; index < 16; index++) {
				printf("%c", (char)info.UserNotes[index]);
			}

		} else if (choice[0] >= '0' && choice[0] <= '7') {
			port = choice[0] - '0';
			printf
			    ("\nEnter new mode for port %d (232, 422, 485, or 485INV)",
			     port);

			if (strlen(ttyfname) > 0) {
				printf(", or enter RTS to set: ");
			} else {
				printf(": ");
			}

			result = scanf("%s", choice);

			setRts = 0;
			validMode = 1;
			if (strcmp(choice, "232") == 0) {
				mode = SETTINGS_PORT_MODE_RS232;
			} else if (strcmp(choice, "422") == 0) {
				mode = SETTINGS_PORT_MODE_RS422;
			} else if (strcmp(choice, "485") == 0) {
				mode = SETTINGS_PORT_MODE_RS485;
			} else if (strcmp(choice, "485INV") == 0) {
				mode = SETTINGS_PORT_MODE_RS485_RTS_INV;
			} else if ((strcmp(choice, "RTS") == 0)) {
				setRts = 1;

			} else {
				printf
				    ("\nIncorrect choice made.  Please choose 232, 422, 485 or \n"
				     "485INV for new mode, or enter RTS to toggle port 0 bit.\n");
				validMode = 0;
			}

			if (validMode) {

				if (setRts == 0) {
					printf
					    ("Termination Enabled? (1 = yes, 0 = no): ");
					result = scanf("%s", choice);
					termination = (choice[0] == '1');

					success =
					    Settings_SetPortConfig(dfd, port,
								   mode,
								   termination);

					if (!success) {
						printf
						    ("Error calling SetPortConfig on port %d. Error code %d.\n",
						     port, success);
						exit(EXIT_FAILURE);
					}
				} else {
					if (!ttyfile_option_given) {
						printf
						    ("Need '--ttyfile' option to use RTS");
					} else {
						printf
						    ("Set port %d RTS (1 = enabled, 0 = disabled): ",
						     port);
						result = scanf("%s", choice);
						rtsVal = (choice[0] == '1');
						printf("*%d*", rtsVal);

						success =
						    SetPortRts(ttyfiletab
							       [port].ttyfileptr,
							       rtsVal);

						if (!success) {
							printf
							    ("Error trying to set port %d RTS to %d.\n",
							     port, rtsVal);
							exit(EXIT_FAILURE);
						}
					}
				}

			}

		} else {
			printf
			    ("\nInvalid port number.  Please select a port from 0 to 7.\n");
		}

		print_port_settings(dfd, ttyfname);
		/*
		 * Once done with the EEPROM, close the settings
		 */
		success = Settings_Close(dfd);

		if (!success) {
			printf
			    ("Error closing EEPROM settings. Error code %d.\n",
			     success);
			exit(EXIT_FAILURE);
		}

		close(fd);

		printf
		    ("\nEnter port to change (0 - 7), 'u' to set user note, or 'x' to exit: ");

		result = scanf("%s", choice);
		eat_newlines();

	}

	printf("\n\n");

	return EXIT_SUCCESS;
}
