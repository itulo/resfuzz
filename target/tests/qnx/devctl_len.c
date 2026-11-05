/* SPDX-License-Identifier: Apache-2.0 */
/* TODO-QNX D6: devctl payload length vs. receive buffer. Run with buggy_rm
 * started as BUGGY_TRACE=1 buggy_rm /dev/buggy and read its stderr.
 * qcc -Vgcc_ntox86_64 -Wall -Ibuggy_rm -o build-qnx/devctl_len tests/qnx/devctl_len.c */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <devctl.h>
#include <sys/neutrino.h>
#include <sys/iomsg.h>
#include "buggy_rm.h"
int main(void){
  int fd=open("/dev/buggy",O_RDWR); if(fd<0){perror("open");return 1;}
  char name[32]={0}; strcpy(name,"hello-d6");
  int e=devctl(fd,DCMD_BUGGY_SET_NAME,name,20,NULL); printf("SET_NAME 20 -> %d\n",e);
  char out[32]={0}; e=devctl(fd,DCMD_BUGGY_GET_NAME,out,sizeof out,NULL); printf("GET_NAME -> %d '%s'\n",e,out);
  static char big[4096]; memset(big,'A',sizeof big);
  e=devctl(fd,DCMD_BUGGY_SET_NAME,big,sizeof big,NULL); printf("SET_NAME 4096 -> %d\n",e);
  memset(out,0,sizeof out); e=devctl(fd,DCMD_BUGGY_GET_NAME,out,sizeof out,NULL); printf("GET_NAME -> %d len=%zu\n",e,strnlen(out,32));
  return 0;}
