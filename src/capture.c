/*
 * Author:		Mark Hinkle
 * Date:		August 2nd, 2020
 * 
 * Filename:	capture.c
 * Desc:		This implementation file is intended to provide an easy
 * 			interface to communicate with the V4L2 driver, which in turn
 * 			provides access to the PiCam perhiperal which is being used
 * 			to capture frames.
 * 
 * !! Shoutouts !!
 * 
 * <!> Sam Siewert - this file is fundamentally built upon simple_capture.c
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
#include "capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <syslog.h>
//
#include <getopt.h>				/* getopt_long() */	
//
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
//
#include <linux/videodev2.h>
//
#include <time.h>


// Macros
//
#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define COLOR_CONVERT
// control IO method...
#define IO_READ				1
#define IO_MMAP				0
#define IO_USERPTR			0

// Typedefs, Structs, & Enums
//
enum io_method 
{
        IO_METHOD_READ,
        IO_METHOD_MMAP,
        IO_METHOD_USERPTR,
};
//
struct buffer 
{
        void   *start;
        size_t  length;
};

// Globals
//
#if 		(IO_READ)
static enum io_method   io = IO_METHOD_READ;
#elif		(IO_MMAP)
static enum io_method   io = IO_METHOD_MMAP;
#elif		(IO_USERPTR)
static enum io_method   io = IO_METHOD_USERPTR;
#else
#error "Must define some IO Method \r\n"
#endif
//	
static struct v4l2_format fmt;
//
static char            *dev_name;
static int              fd = -1;
struct buffer          *buffers;
static unsigned int     n_buffers;
static int              out_buf;
static int              force_format=1;
static int              frame_count = 1;
//
unsigned int framecnt=0;
unsigned char bigbuffer[(1280*960)];
//
static const char short_options[] = "d:hmruofc:";
static const struct option
long_options[] = {
        { "device", required_argument, NULL, 'd' },
        { "help",   no_argument,       NULL, 'h' },
        { "mmap",   no_argument,       NULL, 'm' },
        { "read",   no_argument,       NULL, 'r' },
        { "userp",  no_argument,       NULL, 'u' },
        { "output", no_argument,       NULL, 'o' },
        { "format", no_argument,       NULL, 'f' },
        { "count",  required_argument, NULL, 'c' },
        { 0, 0, 0, 0 }
};


/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
static void errno_exit(const char *s)
{
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}


/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
static int xioctl(int fh, int request, void *arg)
{
        int r;

        do 
        {
            r = ioctl(fh, request, arg);

        } while (-1 == r && EINTR == errno);

        return r;
}


/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
void yuv2rgb_float(float y, float u, float v, 
                   unsigned char *r, unsigned char *g, unsigned char *b)
{
    float r_temp, g_temp, b_temp;

    // R = 1.164(Y-16) + 1.1596(V-128)
    r_temp = 1.164*(y-16.0) + 1.1596*(v-128.0);  
    *r = r_temp > 255.0 ? 255 : (r_temp < 0.0 ? 0 : (unsigned char)r_temp);

    // G = 1.164(Y-16) - 0.813*(V-128) - 0.391*(U-128)
    g_temp = 1.164*(y-16.0) - 0.813*(v-128.0) - 0.391*(u-128.0);
    *g = g_temp > 255.0 ? 255 : (g_temp < 0.0 ? 0 : (unsigned char)g_temp);

    // B = 1.164*(Y-16) + 2.018*(U-128)
    b_temp = 1.164*(y-16.0) + 2.018*(u-128.0);
    *b = b_temp > 255.0 ? 255 : (b_temp < 0.0 ? 0 : (unsigned char)b_temp);
}


/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
void yuv2rgb(int y, int u, int v, unsigned char *r, unsigned char *g, unsigned char *b)
{
   int r1, g1, b1;

   // replaces floating point coefficients
   int c = y-16, d = u - 128, e = v - 128;       

   // Conversion that avoids floating point
   r1 = (298 * c           + 409 * e + 128) >> 8;
   g1 = (298 * c - 100 * d - 208 * e + 128) >> 8;
   b1 = (298 * c + 516 * d           + 128) >> 8;

   // Computed values may need clipping.
   if (r1 > 255) r1 = 255;
   if (g1 > 255) g1 = 255;
   if (b1 > 255) b1 = 255;

   if (r1 < 0) r1 = 0;
   if (g1 < 0) g1 = 0;
   if (b1 < 0) b1 = 0;

   *r = r1 ;
   *g = g1 ;
   *b = b1 ;
}


/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */

static void process_image(const void *p, int size)
{
    int i, newi, newsize=0;
    struct timespec frame_time;
    int y_temp, y2_temp, u_temp, v_temp;
    unsigned char *pptr = (unsigned char *)p;

    // record when process was called
    clock_gettime(CLOCK_REALTIME, &frame_time);    

    framecnt++;
    printf("frame %d: ", framecnt);

    // This just dumps the frame to a file now, but you could replace with whatever image
    // processing you wish.
    //

    if(fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_GREY)
    {
        printf("Dump graymap as-is size %d\n", size);
        //dump_pgm(p, size, framecnt, &frame_time);
    }

    else if(fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
    {

#if defined(COLOR_CONVERT)
        printf("Dump YUYV converted to RGB size %d\n", size);
       
        // Pixels are YU and YV alternating, so YUYV which is 4 bytes
        // We want RGB, so RGBRGB which is 6 bytes
        //
        for(i=0, newi=0; i<size; i=i+4, newi=newi+6)
        {
            y_temp=(int)pptr[i]; u_temp=(int)pptr[i+1]; y2_temp=(int)pptr[i+2]; v_temp=(int)pptr[i+3];
            yuv2rgb(y_temp, u_temp, v_temp, &bigbuffer[newi], &bigbuffer[newi+1], &bigbuffer[newi+2]);
            yuv2rgb(y2_temp, u_temp, v_temp, &bigbuffer[newi+3], &bigbuffer[newi+4], &bigbuffer[newi+5]);
        }

        //dump_ppm(bigbuffer, ((size*6)/4), framecnt, &frame_time); //framecnt == iteration_cnt
#else
        printf("Dump YUYV converted to YY size %d\n", size);
       
        // Pixels are YU and YV alternating, so YUYV which is 4 bytes
        // We want Y, so YY which is 2 bytes
        //
        for(i=0, newi=0; i<size; i=i+4, newi=newi+2)
        {
            // Y1=first byte and Y2=third byte
            bigbuffer[newi]=pptr[i];
            bigbuffer[newi+1]=pptr[i+2];
        }

        //dump_pgm(bigbuffer, (size/2), framecnt, &frame_time);
#endif

    }

    else if(fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB24)
    {
        printf("Dump RGB as-is size %d\n", size);
        //dump_ppm(p, size, framecnt, &frame_time);
    }
    else
    {
        printf("ERROR - unknown dump format\n");
    }

    fflush(stderr);
    //fprintf(stderr, ".");
    fflush(stdout);
}


/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
static int read_frame(void)
{
    struct v4l2_buffer buf;
    unsigned int i;

    switch (io)
    {

        case IO_METHOD_READ:
            if (-1 == read(fd, buffers[0].start, buffers[0].length))
            {
                switch (errno)
                {

                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        errno_exit("read");
                }
            }

            process_image(buffers[0].start, buffers[0].length);
            break;

        case IO_METHOD_MMAP:
            CLEAR(buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
            {
                switch (errno)
                {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, but drivers should only set for serious errors, although some set for
                           non-fatal errors too.
                         */
                        return 0;


                    default:
                        printf("mmap failure\n");
                        errno_exit("VIDIOC_DQBUF");
                }
            }

            assert(buf.index < n_buffers);

            process_image(buffers[buf.index].start, buf.bytesused);

            if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                    errno_exit("VIDIOC_QBUF");
            break;

        case IO_METHOD_USERPTR:
            CLEAR(buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;

            if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
            {
                switch (errno)
                {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        errno_exit("VIDIOC_DQBUF");
                }
            }

            for (i = 0; i < n_buffers; ++i)
                    if (buf.m.userptr == (unsigned long)buffers[i].start
                        && buf.length == buffers[i].length)
                            break;

            assert(i < n_buffers);

            process_image((void *)buf.m.userptr, buf.bytesused);

            if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                    errno_exit("VIDIOC_QBUF");
            break;
    }

    //printf("R");
    return 1;
}




/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
static void stop_capturing(void)
{
        enum v4l2_buf_type type;

        switch (io) {
        case IO_METHOD_READ:
                /* Nothing to do. */
                break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
                        errno_exit("VIDIOC_STREAMOFF");
                break;
        }
}


/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
static void start_capturing(void)
{
        unsigned int i;
        enum v4l2_buf_type type;

        switch (io) 
        {

        case IO_METHOD_READ:
                /* Nothing to do. */
                break;

        case IO_METHOD_MMAP:
                for (i = 0; i < n_buffers; ++i) 
                {
                        printf("allocated buffer %d\n", i);
                        struct v4l2_buffer buf;

                        CLEAR(buf);
                        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                        buf.memory = V4L2_MEMORY_MMAP;
                        buf.index = i;

                        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                                errno_exit("VIDIOC_QBUF");
                }
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
                        errno_exit("VIDIOC_STREAMON");
                break;

        case IO_METHOD_USERPTR:
                for (i = 0; i < n_buffers; ++i) {
                        struct v4l2_buffer buf;

                        CLEAR(buf);
                        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                        buf.memory = V4L2_MEMORY_USERPTR;
                        buf.index = i;
                        buf.m.userptr = (unsigned long)buffers[i].start;
                        buf.length = buffers[i].length;

                        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                                errno_exit("VIDIOC_QBUF");
                }
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
                        errno_exit("VIDIOC_STREAMON");
                break;
        }
}

 
/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
 static void uninit_device(void)
{
        unsigned int i;

        switch (io) {
        case IO_METHOD_READ:
                free(buffers[0].start);
                break;

        case IO_METHOD_MMAP:
                for (i = 0; i < n_buffers; ++i)
                        if (-1 == munmap(buffers[i].start, buffers[i].length))
                                errno_exit("munmap");
                break;

        case IO_METHOD_USERPTR:
                for (i = 0; i < n_buffers; ++i)
                        free(buffers[i].start);
                break;
        }

        free(buffers);
}

 
/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
static void init_read(unsigned int buffer_size)
{
        buffers = calloc(1, sizeof(*buffers));

        if (!buffers) 
        {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        buffers[0].length = buffer_size;
        buffers[0].start = malloc(buffer_size);

        if (!buffers[0].start) 
        {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }
}

 
/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
static void init_mmap(void)
{
        struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count = 6;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) 
        {
                if (EINVAL == errno) 
                {
                        fprintf(stderr, "%s does not support "
                                 "memory mapping\n", dev_name);
                        exit(EXIT_FAILURE);
                } else 
                {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        if (req.count < 2) 
        {
                fprintf(stderr, "Insufficient buffer memory on %s\n", dev_name);
                exit(EXIT_FAILURE);
        }

        buffers = calloc(req.count, sizeof(*buffers));

        if (!buffers) 
        {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
                struct v4l2_buffer buf;

                CLEAR(buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = n_buffers;

                if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
                        errno_exit("VIDIOC_QUERYBUF");

                buffers[n_buffers].length = buf.length;
                buffers[n_buffers].start =
                        mmap(NULL /* start anywhere */,
                              buf.length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED /* recommended */,
                              fd, buf.m.offset);

                if (MAP_FAILED == buffers[n_buffers].start)
                        errno_exit("mmap");
        }
}


/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
static void init_userp(unsigned int buffer_size)
{
        struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count  = 4;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_USERPTR;

        if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support "
                                 "user pointer i/o\n", dev_name);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        buffers = calloc(4, sizeof(*buffers));

        if (!buffers) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
                buffers[n_buffers].length = buffer_size;
                buffers[n_buffers].start = malloc(buffer_size);

                if (!buffers[n_buffers].start) {
                        fprintf(stderr, "Out of memory\n");
                        exit(EXIT_FAILURE);
                }
        }
}


/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
static void init_device(void)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    unsigned int min;

    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap))
    {
        if (EINVAL == errno) {
            fprintf(stderr, "%s is no V4L2 device\n",
                     dev_name);
            exit(EXIT_FAILURE);
        }
        else
        {
                errno_exit("VIDIOC_QUERYCAP");
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        fprintf(stderr, "%s is no video capture device\n",
                 dev_name);
        exit(EXIT_FAILURE);
    }

    switch (io)
    {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE))
            {
                fprintf(stderr, "%s does not support read i/o\n",
                         dev_name);
                exit(EXIT_FAILURE);
            }
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            if (!(cap.capabilities & V4L2_CAP_STREAMING))
            {
                fprintf(stderr, "%s does not support streaming i/o\n",
                         dev_name);
                exit(EXIT_FAILURE);
            }
            break;
    }


    /* Select video input, video standard and tune here. */


    CLEAR(cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap))
    {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop))
        {
            switch (errno)
            {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                        break;
            }
        }

    }
    else
    {
        /* Errors ignored. */
    }


    CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (force_format)
    {
        printf("FORCING FORMAT\n");
        fmt.fmt.pix.width       = HRES_L;
        fmt.fmt.pix.height      = VRES_L;

        // Specify the Pixel Coding Formate here

        // This one work for Logitech C200
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

        //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
        //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_VYUY;

        // Would be nice if camera supported
        //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
        //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;

        //fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
        fmt.fmt.pix.field       = V4L2_FIELD_NONE;

        if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
                errno_exit("VIDIOC_S_FMT");

        /* Note VIDIOC_S_FMT may change width and height. */
    }
    else
    {
        printf("ASSUMING FORMAT\n");
        /* Preserve original settings as set by v4l2-ctl for example */
        if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
                    errno_exit("VIDIOC_G_FMT");
    }

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
            fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
            fmt.fmt.pix.sizeimage = min;

    switch (io)
    {
        case IO_METHOD_READ:
            init_read(fmt.fmt.pix.sizeimage);
            break;

        case IO_METHOD_MMAP:
            init_mmap();
            break;

        case IO_METHOD_USERPTR:
            init_userp(fmt.fmt.pix.sizeimage);
            break;
    }
}


/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
static void close_device(void)
{
        if (-1 == close(fd))
                errno_exit("close");

        fd = -1;
}


/* Func: xxxxx
 * Desc: xxxxx
 * Params:
 * 	void
 * Return:
 * 	void
 */
static void open_device(void)
{
        struct stat st;

        if (-1 == stat(dev_name, &st)) {
                fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                         dev_name, errno, strerror(errno));
                exit(EXIT_FAILURE);
        }

        if (!S_ISCHR(st.st_mode)) {
                fprintf(stderr, "%s is no device\n", dev_name);
                exit(EXIT_FAILURE);
        }

        fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

        if (-1 == fd) {
                fprintf(stderr, "Cannot open '%s': %d, %s\n",
                         dev_name, errno, strerror(errno));
                exit(EXIT_FAILURE);
        }
}


void capture_init(res_t res)
{
    //syslog(LOG_INFO, "IO METHOD = READ:%d MMAP:%d USERPTR:%d \r\n",
	//	IO_READ, IO_MMAP, IO_USERPTR);
	
    printf("IO METHOD = READ:%d MMAP:%d USERPTR:%d \r\n",
		IO_READ, IO_MMAP, IO_USERPTR);
	
    dev_name = "/dev/video0";   //device MMIO loc
                        
    open_device();              //identifies device and opens MMIO
    init_device();              //verifies compatibiliy and sets resolution
    start_capturing();          //sets up buffers to hold images
}


void capture_uninit(void)
{
    stop_capturing();       //turn stream off for userptr method
    uninit_device();        //free the memory used for buffers
    close_device();         //close the file
}   

int capture_photo(void)
{
    struct timespec read_delay;
    struct timespec time_error;
    struct timespec frame_time;

    read_delay.tv_sec=0;
    read_delay.tv_nsec=30000;

    for (;;)
	{
		fd_set fds;
		struct timeval tv;
		int r;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		/* Timeout. */
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(fd + 1, &fds, NULL, NULL, &tv);

		if (-1 == r)
		{
			if (EINTR == errno)
				continue;
			errno_exit("select");
		}

		if (0 == r)
		{
			fprintf(stderr, "select timeout\n");
			exit(EXIT_FAILURE);
		}

		if (capture_frame())
		{
			if(nanosleep(&read_delay, &time_error) != 0)
				perror("nanosleep");
			else
				printf("time_error.tv_sec=%ld, time_error.tv_nsec=%ld\n", time_error.tv_sec, time_error.tv_nsec);

			break;
		}
	}

	return 0;
} 

int capture_frame(void)
{
	if (-1 == read(fd, buffers[0].start, buffers[0].length))
	{
		switch (errno)
		{

			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit("read");
		}
	}

	capture_process(buffers[0].start, buffers[0].length);

	return(1);
}


void capture_process(const void *p, int size)
{
    int i, newi, newsize=0;
    int y_temp, y2_temp, u_temp, v_temp;
    unsigned char *pptr = (unsigned char *)p;  

    framecnt++;
    printf("frame %d: ", framecnt);

	printf("Dump YUYV converted to RGB size %d\n", size);
   
	// Pixels are YU and YV alternating, so YUYV which is 4 bytes
	// We want RGB, so RGBRGB which is 6 bytes
	//
	for(i=0, newi=0; i<size; i=i+4, newi=newi+6)
	{
		y_temp=(int)pptr[i]; u_temp=(int)pptr[i+1]; y2_temp=(int)pptr[i+2]; v_temp=(int)pptr[i+3];
		yuv2rgb(y_temp, u_temp, v_temp, &bigbuffer[newi], &bigbuffer[newi+1], &bigbuffer[newi+2]);
		yuv2rgb(y2_temp, u_temp, v_temp, &bigbuffer[newi+3], &bigbuffer[newi+4], &bigbuffer[newi+5]);
	}

	//dump_ppm(bigbuffer, ((size*6)/4), framecnt, &frame_time); //framecnt == iteration_cnt

}

static int capture_copy(void * dest, int dest_sz, const void * src, int src_sz)
{
	if(dest_sz != src_sz)
	{
		printf("Incompatible buffer sizes \r\n");
		return -1;
	}
	
	memcpy(dest, src, dest_sz);
	
	return 0;
}


int capture_update(void * dest, int dest_sz)
{
	return capture_copy(dest, dest_sz, bigbuffer, (buffers[0].length * 6) / 4);
}
