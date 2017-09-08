/*
 * =====================================================================================
 *
 *       Filename:  sys_help.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  11/27/2014 11:56:52 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Robber (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __SYS_HELP_H__
#define __SYS_HELP_H__
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

inline int setUser(const char * uid)
{    
    struct passwd *pw;
    if ((pw = getpwnam(uid)) == NULL)
    {
        fprintf(stderr,"unknown user [%s]!", uid);
        return -1;
    }
    else if (setgid(pw->pw_gid) == -1)
    {
        fprintf(stderr,"setgid(%s): error !", uid); 
        return -2;
    }
    else if (setuid(pw->pw_uid) < 0 || seteuid(pw->pw_uid) < 0)
    {
        fprintf(stderr,"setuid(%s): error! ", uid); 
        return -3;
    }
    endpwent(); 
    return 0;
}

inline void initDaemon()
{
    int pid;
    if((pid=fork()))
        exit(0);
    else if(pid< 0)
        exit(1);

    setsid();
    if((pid=fork()))
        exit(0);
    else if(pid< 0)
        exit(1);
    return;
}

inline int enableCoredump()
{
    struct rlimit limit;
    limit.rlim_max = limit.rlim_cur = RLIM_INFINITY;
    if(setrlimit(RLIMIT_CORE, &limit) != 0)
    {
        return -1;
    }
    return 0;
}

inline int setNoFile(unsigned int num)
{
    rlimit limit;
    limit.rlim_max = limit.rlim_cur = num + 1024;
    if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
    {
        return -1;
    }
    return 0;
}

/*int getNoFile(unsigned& rlim_max, unsigned& rlim_cur)
{
    rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
    {
        fprintf(stderr,"set nofile failed!");
        return -1;
    }

    rlim_max =limit.rlim_max;
    rlim_cur = limit.rlim_cur;
    return 0;
}*/

#endif
