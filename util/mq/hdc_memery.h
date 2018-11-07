#ifndef _SHM_MEMERY_H
#define _SHM_MEMERY_H

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#define H_IPC_IRWUG  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP
#include <string>

inline uint32_t StrToInt32(const char* str){
    if (strlen(str) >= 3){
        if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')){
            const char * p = str;
            p += 2;
            unsigned int  iRet = 0;
            unsigned char iByte;
            for ( ;p != 0; p ++){
                if ( *p >= '0' && *p <= '9'){
                    iByte = (unsigned char)*p - '0';
                }else if(*p >= 'a' && *p <= 'f'){
                    iByte = (unsigned char)*p - 'a' + 10;
                }else if(*p >= 'A' && *p <= 'F'){
                    iByte = (unsigned char)*p - 'A' + 10;
                }else {
                    break;
                }
                iRet = (iRet << 4) | iByte;
            }
            return iRet;
        }
    }
    return (unsigned int)atoi(str);
}

void* ShmPorint(const char* key_id, size_t size);

#endif

