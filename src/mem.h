#ifndef _MEM_H
#define _MEM_H

/*
 * Author:		Mark Hinkle
 * Date:		August 2nd, 2020
 * 
 * Filename:	mem.h
 * Desc:		This header file is intended to provide access to the 
 * 			functions provided in mem.c, as well as include other
 * 			libraries that make the operation of this code possible
 * 
 * !! Shoutouts !!
 * 
 * <!> Sam Siewert - this file is built thanks to several memory examples 
 * 		which were written by Sam for ECEN 5623 at the University of 
 * 		Colorado Boulder
 * 
 * ## Disclaimer ##
 * 
 * 		This code is provided open source as-is, and the author is not 
 * 	responsible for any errors or faults that may develop in this 
 * 	application 
 * 
 */   
 
// Includes
//
#include <time.h>


// Macros
//
#define HRES_L_STR			"640"
#define VRES_L_STR			"480"

// Typedefs & Enums
//

// Function Declarations
void mem_store(const void *p, int size, unsigned int tag, struct timespec *time);

#endif /* _MEM_H */
