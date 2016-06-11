#include <iostream>
#include <stdio.h>
#include <fstream>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include<signal.h>
#include<string>
#include <mqueue.h>


class messageQueue
{
	public:
		void readQueue(int);
		void clean_up(int);


};

void messageQueue::clean_up(int)
{
	interruptFlag = true;
}

void messageQueue::readQueue(int readqueue_fd){

	struct sigaction act_sighup;
	act_sighup.sa_handler = clean_up;//handle_sighup;	
	sigemptyset(&act_sighup.sa_mask);
  	act_sighup.sa_flags=0;
	act_sighup.sa_restorer=NULL;
	sigaction(SIGHUP, &act_sighup, NULL);
	sigaction(SIGINT, &act_sighup, NULL);
	sigaction(SIGTERM, &act_sighup, NULL);

  //Handle the SIGUSR2 when the message queue
  //notification sends the signal
	struct sigaction action_to_take;
  	action_to_take.sa_handler=read_message; 
  //zero out the mask (allow any signal to interrupt)
  	sigemptyset(&action_to_take.sa_mask); 
  	action_to_take.sa_flags=0;
  //tell how to handle SIGINT
  	sigaction(SIGUSR2, &action_to_take, NULL); 

  	struct mq_attr mq_attributes;
  	mq_attributes.mq_flags=0;
  	mq_attributes.mq_maxmsg=10;
  	mq_attributes.mq_msgsize=120;

  	if((readqueue_fd=mq_open(mq_name.c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
          	S_IRUSR|S_IWUSR, &mq_attributes))==-1)
  	{
    		perror("mq_open");
    		exit(1);
  	}
  //set up message queue to receive signal whenever message comes in
  	struct sigevent mq_notification_event;
  	mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  	mq_notification_event.sigev_signo=SIGUSR2;
  	mq_notify(readqueue_fd, &mq_notification_event);

}
