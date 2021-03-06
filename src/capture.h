/*
 * Author:		Mark Hinkle
 * Date:		August 2nd, 2020
 * 
 * Filename:	capture.h
 * Desc:		This header file is intended to provide access to the 
 * 			functions provided in capture.c, as well as include other
 * 			libraries that make the operation of this code possible
 * 
 * !! Shoutouts !!
 * 
 * <!> Sam Siewert - this file is built thanks to simple_capture.c
 * 		which was written by Sam for ECEN 5623 at the University of 
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

// Macros
//
#define HRES_L 				640
#define VRES_L 				480
//
#define IMG_BUF_SIZE(h,l)	(((h)*2*(l)*6)/4)

// Typedefs & Enums
//
typedef enum
{
	RES_LOW,
	RES_MED,
	RES_HIGH
} res_t;

// Function Declarations
//
void capture_init(res_t res);
void capture_uninit(void);
int capture_photo(void);
int capture_frame(void);
void capture_process(const void *p, int size);
int capture_update(void * dest, int dest_sz);
