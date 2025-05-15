#include "capture.h"

#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "tusb.h"
#include "png.h"

#define DNAME "/atom"
#define FILE_PREFIX "scr"
#define FILE_EXTENSION ".png"

int last_file_number(char* dname) {
    int retval = 0;
    DIR dir;
    if (FR_OK != f_opendir(&dir, dname)) {
        return -1;
    } else {
        FILINFO fno;
        while ((f_readdir(&dir, &fno) == FR_OK) && (fno.fname[0] != 0)) {
            if (fno.fname[0] != '.')  // ignore . and .. entry
            {
                if (fno.fattrib & AM_DIR) {
                    // directory
                } else {
                    int len = strlen(FILE_PREFIX);
                    if (strncmp(FILE_PREFIX, fno.fname, len) == 0) {
                        int file_number = atoi(fno.fname + len);
                        if (file_number > retval) {
                            retval = file_number;
                        }
                    }
                }
            }
        }
        f_closedir(&dir);
        return retval;
    }
}

void setRGB(png_byte *ptr, float val)
{
	int v = (int)(val * 767);
	if (v < 0) v = 0;
	if (v > 767) v = 767;
	int offset = v % 256;

	if (v<256) {
		ptr[0] = 0; ptr[1] = 0; ptr[2] = offset;
	}
	else if (v<512) {
		ptr[0] = 0; ptr[1] = offset; ptr[2] = 255-offset;
	}
	else {
		ptr[0] = offset; ptr[1] = 255-offset; ptr[2] = 0;
	}
}


int writeImage(char* filename, int width, int height, float *buffer, char* title)
{
	int code = 0;
	FILE *fp = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_bytep row = NULL;
	
	// Open file for writing (binary mode)
	fp = fopen(filename, "wb");
	if (fp == NULL) {
		fprintf(stderr, "Could not open file %s for writing\n", filename);
		code = 1;
		goto finalise;
	}

	// Initialize write structure
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		fprintf(stderr, "Could not allocate write struct\n");
		code = 1;
		goto finalise;
	}

	// Initialize info structure
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		fprintf(stderr, "Could not allocate info struct\n");
		code = 1;
		goto finalise;
	}

	// Setup Exception handling
	if (setjmp(png_jmpbuf(png_ptr))) {
		fprintf(stderr, "Error during png creation\n");
		code = 1;
		goto finalise;
	}

	png_init_io(png_ptr, fp);

	// Write header (8 bit colour depth)
	png_set_IHDR(png_ptr, info_ptr, width, height,
			8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	// Set title
	if (title != NULL) {
		png_text title_text;
		title_text.compression = PNG_TEXT_COMPRESSION_NONE;
		title_text.key = "Title";
		title_text.text = title;
		png_set_text(png_ptr, info_ptr, &title_text, 1);
	}

	png_write_info(png_ptr, info_ptr);

	// Allocate memory for one row (3 bytes per pixel - RGB)
	row = (png_bytep) malloc(3 * width * sizeof(png_byte));

	// Write image data
	int x, y;
	for (y=0 ; y<height ; y++) {
		for (x=0 ; x<width ; x++) {
			setRGB(&(row[x*3]), buffer[y*width + x]);
		}
		png_write_row(png_ptr, row);
	}

	// End write
	png_write_end(png_ptr, NULL);

	finalise:
	if (fp != NULL) fclose(fp);
	if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
	if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
	if (row != NULL) free(row);

	return code;
}


bool ls(const char* dpath) {
    DIR dir;
    if (FR_OK != f_opendir(&dir, dpath)) {
        printf("cannot access '%s': No such file or directory\r\n", dpath);
        return false;
    }

    FILINFO fno;
    while ((f_readdir(&dir, &fno) == FR_OK) && (fno.fname[0] != 0)) {
        if (fno.fname[0] != '.')  // ignore . and .. entry
        {
            if (fno.fattrib & AM_DIR) {
                // directory
                printf("/%s\r\n", fno.fname);
            } else {
                printf("%-40s", fno.fname);
                if (fno.fsize < 1024) {
                    printf("%lu B\r\n", fno.fsize);
                } else {
                    printf("%lu KB\r\n", fno.fsize / 1024);
                }
            }
        }
    }

    f_closedir(&dir);
    return true;
}

void capture_task() { tuh_task(); }

void capture_init() {
    int status = tuh_init(BOARD_TUH_RHPORT);
    printf("status = %d\n", status);
}

void capture() {
    char fname[32];
    printf("CAPTURE\n");

    f_mkdir(DNAME);
    int fnum = last_file_number(DNAME);
    if (fnum < 0) {
        printf("cannot open " DNAME "\n");
        return;
    } else {
        fnum += 1;
    }

    sprintf(fname, DNAME "/" FILE_PREFIX "%04d" FILE_EXTENSION, fnum);
    printf("fname = %s\n", fname);

    float buffer[1];
    writeImage(fname, 1,1,buffer,"Hello");


    // FIL file;
    // int status = f_open(&file, fname, FA_CREATE_NEW | FA_WRITE);
    // if (status == FR_OK) {
    //     float buffer[1];
    //     writeImage(fname, 1,1,buffer,"Hello");
    // } else {
    //     printf("cannot open ");
    //     printf(fname);
    //     printf("\n");
    // }

    ls(DNAME);
    return;
}