*****************************************************************************
** ChibiOS/RT port for ARM-Cortex-M3 ADICUP360.                            **
*****************************************************************************

** TARGET **

The demo runs on an ADICUP360 board.

** The Demo **

The demo flashes the two on-board LEDs using a thread, the serial port SD0
(UART0, mapped on the User COM port) is started. The RT and OSLIB test suites
are not included, they do not fit in the 8K RAM of the ADuCM360.

** Board setup **

To redirect P0.6 and P0.7 to the User COM port it is required to setup
the switch matrix as:
 - S1 -> Don't Care
 - S2 -> 0
 - S3 -> Don't Care
 - S4 -> 0
   
** Build Procedure **

The demo has been tested by using the free Codesourcery GCC-based toolchain
and YAGARTO.
Just modify the TRGT line in the makefile in order to use different GCC ports.

** Notes **

Some files used by the demo are not part of ChibiOS/RT but are copyright of
Analog Devices and are licensed under a different license.
Also note that not all the files present within the Analog Devices 
Cross Core Embedded Studio are distributed with ChibiOS/RT, 
you can find the whole library on the ADI web site:

                             http://www.analog.com
