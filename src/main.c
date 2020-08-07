/*
 * Author:		Mark Hinkle
 * Date:		August 2nd, 2020
 * 
 * Filename:	main.c
 * Desc:		This main file is intended to collect the functionality
 * 			of all other implementation files and organize them using
 * 			POSIX threads to create an application that captures, sharpens,
 * 			and stores images at various frequencies. This program is 
 * 			intended to meet the requirements of the final project for
 * 			ECEN 5623 - Real Time Embedded Systems
 * 
 * !! Shoutouts !!
 * 
 * <!> Sam Siewert - this file is fundamentally built upon several POSIX
 * 		thread related files which were written by Sam for ECEN 5623 at 
 * 		the University of Colorado Boulder
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
#include "mem.h"
#include "sharpen.h"
#include "capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
//
#include "time.h"


// Macros
//


// Typedefs & Enums
//



/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
int main(int argc, char ** argv)
{
    int framecnt = 1;
    struct timespec frame_time;
    
    capture_init(RES_LOW);

    capture_photo();
    
	//Must be semaphore protected
	unsigned char test_buf[IMG_BUF_SIZE(HRES_L, VRES_L)];
	capture_update(test_buf, sizeof(test_buf));
    
    // record when process was called
    clock_gettime(CLOCK_REALTIME, &frame_time);  
	
	//Test if it worked
	mem_store(test_buf, sizeof(test_buf), framecnt, &frame_time); //framecnt == iteration_cnt     
     
    
    capture_uninit();
    
    fprintf(stderr, "\n");
    return 0;	 
}

