#pragma once
#ifdef __cplusplus
extern "C" {
#endif 

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>

#define H_IPC_IRWUG  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP


unsigned int StrToInt32( const char* str )
{
    if (strlen(str) >= 3 )
    {
        if( str[0] == '0' && ( str[1] == 'x' || str[1] == 'X' ) )
        {
            const char * p = str;
            p += 2;
            unsigned int  iRet = 0;
            unsigned char iByte;
            for( ;p != 0; p ++)
            {
                if( *p >= '0' && *p <= '9')
                {
                    iByte = (unsigned char)*p - '0';
                }
                else if(*p >= 'a' && *p <= 'f')
                {
                    iByte = (unsigned char)*p - 'a' + 10;
                }
                else if(*p >= 'A' && *p <= 'F')
                {
                    iByte = (unsigned char)*p - 'A' + 10;
                }
                else
                {
                    break;
                }

                iRet = (iRet << 4) | iByte;
            }
            return iRet;
        }
    }
    return (unsigned int)atoi(str);
}

char*  get_share_memery_by_id(unsigned int id, size_t size)
{
    int Memory = shmget ( id, 0, 0 );
    if (Memory < 0)
    {
        if (errno == ENOENT)
        {
            Memory = shmget (id, size, IPC_CREAT|0666);
        }
        if (Memory < 0)
        {
            return 0;
        }
    }
    void* ptr = shmat(Memory,0,0);
    if (ptr == (void*)-1)
    {
        return 0;
    }
    return (char*)ptr;
}

char*  get_share_memery(const char* key_id, size_t size)
{
    unsigned int id = StrToInt32(key_id);
    return get_share_memery_by_id(id, size);
}

#ifdef __cplusplus
}
#endif

