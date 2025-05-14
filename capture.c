#include "capture.h"

#include <stdio.h>
#include <string.h>

#include "tusb.h"
#include "ff.h"

#define DNAME "/capture"
#define FILE_PREFIX "capture"
#define FILE_EXTENSION ".png"

int last_file_number(char* dname) {
    int retval = 0;
    DIR dir;
    if ( FR_OK != f_opendir(&dir, dname) )
    {
        return retval;
    }
    else 
    {
        FILINFO fno;
        while( (f_readdir(&dir, &fno) == FR_OK) && (fno.fname[0] != 0) )
        {
            if ( fno.fname[0] != '.' ) // ignore . and .. entry
            {
                if ( fno.fattrib & AM_DIR )
                {
                    // directory
                }else
                {
                    if (strncmp(FILE_PREFIX, fno.fname, 7) == 0) {
                        int file_number = atoi(fno.fname + 7);
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


bool ls(const char* dpath)
{

  DIR dir;
  if ( FR_OK != f_opendir(&dir, dpath) )
  {
    printf("cannot access '%s': No such file or directory\r\n", dpath);
    return false;
  }

  FILINFO fno;
  while( (f_readdir(&dir, &fno) == FR_OK) && (fno.fname[0] != 0) )
  {
    if ( fno.fname[0] != '.' ) // ignore . and .. entry
    {
      if ( fno.fattrib & AM_DIR )
      {
        // directory
        printf("/%s\r\n", fno.fname);
      }else
      {
        printf("%-40s", fno.fname);
        if (fno.fsize < 1024)
        {
          printf("%lu B\r\n", fno.fsize);
        }else
        {
          printf("%lu KB\r\n", fno.fsize / 1024);
        }
      }
    }
  }

  f_closedir(&dir);
  return true;
}

void capture_task() {
    tuh_task();
}

void capture_init() {
    int status = tuh_init(BOARD_TUH_RHPORT);
    printf("status = %d\n", status);
}

void capture() {
    char buffer[100];
    printf("CAPTURE\n");

    f_mkdir(DNAME);
    int fnum = last_file_number(DNAME) + 1;

    sprintf(buffer, DNAME "/" FILE_PREFIX "%04d" FILE_EXTENSION, fnum);
    printf("fname = %s\n", buffer);

    FIL file;
    if ( FR_OK != f_open(&file, buffer, FA_CREATE_NEW | FA_WRITE) )
    {
        f_close(&file);
    }

    ls(DNAME);
    return;

}