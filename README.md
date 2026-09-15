# Linux Software (SERx5330)

> SWP-700010119 rev D
> 
> Version v02.01.00.134090

Copyright (C), RTD Embedded Technologies, Inc.  All Rights Reserved.

This software package is dual-licensed.  Source code that is compiled for
kernel mode execution is licensed under the GNU General Public License
version 2. For a copy of this license, refer to the file
LICENSE_GPLv2.TXT (which should be included with this software) or contact
the Free Software Foundation. Source code that is compiled for user mode
execution is licensed under the RTD End-User Software License Agreement.
For a copy of this license, refer to LICENSE.TXT or contact RTD Embedded
Technologies, Inc. Using this software indicates agreement with the
license terms listed above.

## Table of Contents

- [Supported Hardware](#supported-hardware)
- [Supported Kernel Versions](#supported-kernel-versions)
- [Supported CPU Architecture](#supported-cpu-architecture)
- [Supported Compilers](#supported-compilers)
- [Driver](#driver)
- [Library Interface](#library-interface)
- [How to Find Serial Devices](#how-to-find-serial-devices)
- [Example Programs](#example-programs)
- [Header Files](#header-files)
- [Known Limitations](#known-limitations)
- [Getting Technical Support](#getting-technical-support)

## Supported Hardware

This software supports the following boards:

- [SER25330](https://www.rtd.com/PC104/UM/network/SERx5330.htm)
- [SER35330](https://www.rtd.com/PC104/UM/network/SERx5330.htm)
- [SER35220](https://www.rtd.com/PC104/UM/network/SERx5320.htm)
- [SER35320](https://www.rtd.com/PC104/UM/network/SERx5320.htm)


## Supported Kernel Versions

This software has been tested with the following Linux distributions and kernel
versions:

	* Red Hat Enterprise Linux 8.10 (kernel 4.18)
	* Ubuntu 24.04 LTS (kernel 6.17.0) 

 Due to API differences between kernel versions, RTD cannot guarantee 
 compatibility with kernels and distributions not listed above.  If a user 
 wishes to use an unsupported kernel/distribution, it may be necessary to 
 modify the driver module code and/or Makefiles for the specific Linux 
 environment.
 

## Supported CPU Architecture
  
 This software has been validated on the following CPU architectures.

- Intel x86 (64-bit) multicore


## Supported Compilers

The library software and example programs were compiled using the GNU gcc 
compiler under Ubuntu 24.04 LTS


## Driver

### For Kernel Versions after 4.11
DO NOT USE THIS DRIVER after kernel 4.11. Kernel versions above 4.11 have
the necessary driver support included.

### For Kernel Versions between kernel 3.8 and 4.11
The enclosed driver will be used with the Exar PCIe driver included with the
linux kernel starting around 3.8.

### For Kernel Versions before kernel 3.8
See known limitations.


## Library Interface

The directory `lib/` contains source code related to the user library.

The library contains functions that can be accessed by a user program 
to change the configuration of ports on the SER25330 card.  The library
functions are not required to utilize the ports as normal serial ports.

The library must be built before compiling the example programs or any
application that uses it.

For kernel versions between 3.8 and 4.11 build the library using 
the command:

```
$ make -f Make-rtd
```

For kernel versions after 4.11 build the library using the command:
```
$ make -f Make-gpio
```


## How to Find Serial Devices

Finding which serial device is which is most easily done by executing:

```
$ sudo demesg | grep ttyS
```

You should see outputs such as the follows:

```
0000:04:00.0: ttyS4 at MMIO 0xc0700000 (irq = 40, base_baud = 7812500) is a XR17V35X
```

Note the first number is the PCI lane that the device appears on and that it
is of the device type XR17V35X. You can confirm this with:

```
$ lspci
```

Some PCI devices will have multiple serial ports associated with them. In 
this case you will find that the lowest assigned serial port will correspond 
to the lowest connector number on the board.  e.g. if ttyS4 is the lowest 
serial number assigned to an XR17V35X, it will connect to CN 14 or port 0 on
a SER35330.

If there are multiple SERX5330 Boards on a PCIe-104 stack (or similar) you
can differentiate the devices by what PCI lane the device consumes with 
`lspci`.


## Example Programs

The directory `examples/` contains source code related to the example programs,
which demonstrate how to use features of the SER25330 board, specifically to
change the configuration of ports to RS232, RS422, and RS485.

For kernel versions between 3.8 and 4.11 build the library using 
the command:

```
$ make -f make_rtd
```

For kernel versions after 4.11 build the library using the command:
```
$ make -f make_gpio
```

The following files are provided in `examples/`:

### make_rtd

Make description file for building the example program for use with
the rtd exar-gpio extention driver.


### make_gpio

Make description file for building the example program for use with
the gpio driver built into the kernel.


### [config-example.c](examples/config-example.c)

Example program which demonstrates how to use library functions to
change the mode  of the serial ports from RS232 through RS485.

Setup: No setup required.

Usage:
Pass it the device file of the gpio device for
the targeted board. Changing the configuration of the ports is handled
through text prompts in the example. The rtd exar-gpio exension driver 
will require root access as follows:

```sh
$ sudo ./config-example --gpio /dev/exar-rtd0
```

In the case of the kenerl gpio driver use the command:

```sh
$ sudo ./config-example /dev/gpiochip0
```

If you are trying to change the RTS behaviour, you need to run the example
program like so:

```sh
$ sudo ./config-example-gpio  --ttyfile=/dev/ttySX
```
(where ttySX is the desired serial port)


## Header Files

The directory `include/` contains all header files needed by the example
programs, library, and user applications.


## Known Limitations

Prior to kernel 3.8 this board requires the Exar PCIe UART driver module 
(SWP-700010072). It should be included on the driver CD included with the 
board.
The driver module is also available directly from Exar. It can be found
at the following URL:
https://www.maxlinear.com/support/design-tools/software-drivers

The driver module must be loaded before running any program which accesses a
SER25330 device.

To configure the board you will need to use the enclosed package :
SER25330_Linux_v01.01.00.tar.gz


## Getting Technical Support

If you require additional support with this product, or any other products from
RTD Embedded Technologies, contact us using the information below:

RTD Embedded Technologies, Inc.\
103 Innovation Boulevard\
State College, PA 16803 USA

Telephone: (814) 234-8087\
Fax: (814) 234-5218\
Sales Information and Quotes: sales@rtd.com\
Technical Assistance: techsupport@rtd.com\
Web Site: http://www.rtd.com
