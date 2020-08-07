/*
 * Author:		Mark Hinkle
 * Date:		August 2nd, 2020
 * 
 * Filename:	mem.c
 * Desc:		This implementation file is intended to provide an easy
 * 			interface for communicating with the memory peripherals on 
 * 			board the Raspberry Pi 4 in the most efficient way possible 
 * 
 * !! Shoutouts !!
 * 
 * <!> Sam Siewert - this file is fundamentally built upon several memory
 * 		access files which were written by Sam for ECEN 5623 at the 
 * 		University of Colorado Boulder
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
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
//
#include <string.h>
#include <fcntl.h>              /* low-level i/o */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


// Macros
//


// Typedefs & Enums
//


// Global Variables
//
char ppm_header[]="P6\n#9999999999 sec 9999999999 msec \n"HRES_L_STR"x"VRES_L_STR"\n255\n";
char ppm_dumpname[]="test00000000.ppm";



/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
void mem_store(const void *p, int size, unsigned int tag, struct timespec *time)
{
    int written, i, total, dumpfd;
   
    snprintf(&ppm_dumpname[4], 9, "%08d", tag);
    strncat(&ppm_dumpname[12], ".ppm", 5);
    dumpfd = open(ppm_dumpname, O_WRONLY | O_NONBLOCK | O_CREAT, 00666);

    snprintf(&ppm_header[4], 11, "%010d", (int)time->tv_sec);
    strncat(&ppm_header[14], " sec ", 5);
    snprintf(&ppm_header[19], 11, "%010d", (int)((time->tv_nsec)/1000000));
    strncat(&ppm_header[29], " msec \n"HRES_L_STR" "VRES_L_STR"\n255\n", 19);
    written=write(dumpfd, ppm_header, sizeof(ppm_header));

    total=0;

    do
    {
        written=write(dumpfd, p, size);
        total+=written;
    } while(total < size);

    printf("wrote %d bytes\n", total);

    close(dumpfd);
    
}



/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */

