/*
 * write template functions that are guaranteed to read and write the 
 * number of bytes desired
 */

#ifndef fancyRW_h
#define fancyRW_h
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

template<typename T>
int READ(int fd, T* obj_ptr, int count)
{
  char* addr=(char*)obj_ptr;

	int err;
	while(true){
		err = read(fd,addr,count);
		
		if(err==-1 && errno==EINTR)
		{				
			continue;
		}
		else if(err==-1)
			return -1;
		break;
	}
	return err;
}

template<typename T>
int WRITE(int fd, T* obj_ptr, int count)
{
  char* addr=(char*)obj_ptr;
  int err,t = 0;

  	while(count>0){
		
		err = write(fd,addr+t,count);
		
		if(err==-1 && errno==EINTR)
		{				
			continue;
		}
		else if(err==-1){
			return -1;
		}
		t = count;
		count = count - err;
	}
	return err;
  //loop. Write repeatedly until count bytes are written out
}
#endif

