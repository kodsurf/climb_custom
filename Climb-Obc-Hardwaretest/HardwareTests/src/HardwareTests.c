/*
===============================================================================
 Name        : HardwareTests.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#include "globals.h"				// Go there to switch the used hardware board.

#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#endif
#endif

#include "retarget.h"
#include <cr_section_macros.h>

// module includes
#include "mod/main.h"


int main(void) {

#if defined (__USE_LPCOPEN)
#if defined (HW_USED)
	// Our own board abstraction.
	ClimbBoardInit();
#endif
	// The original LpcOpen way of Chip initialize if no board is defined.
	// Some routines rely on SystemCoreClock variable ...???...
    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();
#endif

    MainInit();

    // Enter an infinite loop
    while(1) {
    	MainMain();
    }
    return 0 ;
}
