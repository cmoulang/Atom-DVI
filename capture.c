#include "capture.h"

#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "tusb.h"

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

    FIL file;
    int status = f_open(&file, fname, FA_CREATE_NEW | FA_WRITE);
    if (status == FR_OK) {
        f_close(&file);
    } else {
        printf("cannot open ");
        printf(fname);
        printf("\n");
    }

    ls(DNAME);
    return;
}