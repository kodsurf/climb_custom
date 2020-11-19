/*
 * globals.h
 *
 *  Created on: 02.11.2019
 *      Author: Robert
 */

#ifndef GLOBALS_H_
#define GLOBALS_H_

#define SW_VERSION	"HWTest 0.2 Developer"

// Available board abstractions
#define OBC_BOARD	1
#define LPCX_BOARD	2

// Switch the board used here, and only here!
#define HW_USED	LPCX_BOARD
//#define HW_USED OBC_BOARD


#if HW_USED == LPCX_BOARD
	#include "hw_lpcx/lpcx_board.h"
	#define  BOARD_SHORT	"lpcx"
#elif HW_USED == OBC_BOARD
	#include "hw_obc/obc_board.h"
	#define  BOARD_SHORT	"obc"
	//#define	 RADIATION_TEST					// Remove this line for Image without starting radiation test scheduler module.
#endif

// Check for some settings we would like to keep
// (IDE Project level and OpenLPC) which the startup and lpc_chip_xx code could depend on.
#if defined (__cplusplus)
	#error "Project does not support any c++ code/modules!"
#endif
#if !defined (__REDLIB__)
	#warning "Project was developed and tested with redlib (nohost-nf)."
#endif
#if !defined (__USE_LPCOPEN)
	#warning "Project was developed and tested based on a LpcOpen Template using Chip but not board abstraction."
#endif
#if !defined (__CODE_RED)
	#warning "Project was developed and tested based on a LpcOpen Template using Chip but not board abstraction."
#endif
#if !defined (NO_BOARD_LIB)
	#warning "Project was developed and tested based on a LpcOpen Template using Chip but not board abstraction."
#endif
#if !defined (__GNUC__)
	#warning "Project was based on a LpcOpen Template with redlib (nohost-nf) -> retarget.h makes the defines needed for the stdio sys-out/in UART to work."
#endif
#if defined (DEBUG_SEMIHOSTING)
	#warning "Project was developed and tested with redlib (nohost-nf). Do not use semi_hosting."
#endif

// Chek for own board abstraction to be available
#if !defined (HW_USED)
	#Error "Choose your Hardware abstraction in line 15..17 of this file!"
#endif

#endif /* GLOBALS_H_ */
