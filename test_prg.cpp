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
#include "Map.h"
#include "goldchase.h"
#include <time.h>
#include <signal.h>
#include <string>
#include <vector>
#include <mqueue.h>
#include "supportingFunctions.cpp"
#include "fancyRW.h"

using namespace std;

void playGame();
void movePlayers(int no);
void readQueue();
void writeQueue(int plr);
void read_message(int);
void read_message_daemon(int);
void clean_up(int);
void assignQueueName(int pid);
void refreshMap();
void create_server_daemon();
void create_client_daemon(string ip);
void server();
void client(string ip);


mqd_t readqueue_fd; //message queue file descriptor
mqd_t player_other_machine[5];
string mq_name="/AB_player1_mq";

int colCount = 0;
int rowCount = 0;

int randomPlace;
int numberOfGolds;
int foolsNo = 0;
int flagplr2 = 0;
const char* message;
string getmessage;

int sharedRows = 0;
int sharedCols = 0;
unsigned char current_player;
int position;
int debugfd = 0;
int sockfd = 0;
support obj;
char *byte = NULL;
char *client_rows;
char *client_cols;
char *client_n;

struct GameBoard {
  int rows; //4 bytes
  int cols; //4 bytes
  pid_t pid[5];
  int daemonID;
  unsigned char map[0];
};

GameBoard* goldmap;
int shmPtr;
Map *mapPtr = NULL;
int area = 0;
int pid = 0;
unsigned int plrno;
unsigned char* daemon_map;
unsigned char globalSockByte = G_SOCKPLR;
int playerNo = 0;

bool winFlag0 = false;

sem_t *semaphorePtr;
bool interruptFlag = false;

void handle_interrupt(int)
{
	mapPtr->drawMap();
}

void handle_sighup(int){
	interruptFlag = true;
}

void clean_up(int)
{
  interruptFlag = true;
}

void server_cleanup(int sig){
//	int bit = 1;
	unsigned char sockByte = G_SOCKPLR;

	switch(sig){
			
		case SIGHUP:
		//	cerr << "SIGHUP sent\n";
			for(int j=0;j<5;j++){
				if(goldmap->pid[j] != 0){
					if(j==0){sockByte |= G_PLR0;}
					if(j==1){sockByte |= G_PLR1;}
					if(j==2){sockByte |= G_PLR2;}
					if(j==3){sockByte |= G_PLR3;}
					if(j==4){sockByte |= G_PLR4;} 
				}
			}
				globalSockByte = sockByte;
	
				WRITE(sockfd,&sockByte,sizeof(char));
			if(sockByte == G_SOCKPLR){
				cerr << "Memory cleared\n";
				shm_unlink("/ABshm");
				sem_close(semaphorePtr);
				sem_unlink("/ABgoldchase");
				exit(0);
			}
			break;

		case SIGUSR1:
		//	cerr << "sigusr1 send\n";
		{	vector< pair<short,unsigned char> > pvec;
			for(short i=0; i<goldmap->rows*goldmap->cols; ++i)
			{
			  if(goldmap->map[i]!=daemon_map[i])
			  {
			    pair<short,unsigned char> aPair;
			    aPair.first=i;
			    aPair.second=goldmap->map[i];
			    pvec.push_back(aPair);
			    daemon_map[i]=goldmap->map[i];
			  }
			}
			
			if(pvec.size() > 0){
				int dumy = 0;
				for(unsigned short i=0; i<pvec.size(); ++i)
				{
				  WRITE(sockfd,&dumy,sizeof(char));
				  WRITE(sockfd,&pvec[i].first,sizeof(short));
				  WRITE(sockfd,&pvec[i].second,sizeof(char));
				}
			}
			break;
		}
		
	}
}


int main(int argc,char *argv[])
{
	if(argc > 1){
	string ip = argv[1];
		semaphorePtr= sem_open(
	           "/ABgoldchase", 
	           O_CREAT|O_EXCL,S_IROTH| S_IWOTH| S_IRGRP| S_IWGRP| S_IRUSR| S_IWUSR,1); 	
		if(semaphorePtr!=SEM_FAILED){
			create_client_daemon(ip);
		}
		sleep(2);
		playGame();
	}
	else
	{
		playGame();
	} //end else for if (argc > 1)				
	
	return 0;
}


void playGame(){
	
	ifstream fin;
	char ch;
	string firstLine;
	
	support supportObj;

	struct sigaction action_jackson;
  	action_jackson.sa_handler=handle_interrupt;
  	sigemptyset(&action_jackson.sa_mask);
  	action_jackson.sa_flags=0;
	action_jackson.sa_restorer=NULL;

  	sigaction(SIGUSR1, &action_jackson, NULL);

	
        semaphorePtr= sem_open(
           "/ABgoldchase", 
           O_CREAT|O_EXCL,S_IROTH| S_IWOTH| S_IRGRP| S_IWGRP| S_IRUSR| S_IWUSR,1); 	


	if(semaphorePtr==SEM_FAILED)
       {
         if(errno!=EEXIST)
         {
           perror("Semaphore failed");
           exit(1);
         }
       }


	if(semaphorePtr!=SEM_FAILED)
       	{	
		sem_wait(semaphorePtr);		//sem_wait

		shmPtr = shm_open("/ABshm", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);   //shm_open (creates shared memory)


		if(shmPtr==-1){
			perror("Semaphore failed");
           		exit(1);
		}

		area = supportObj.readMap(rowCount,colCount,numberOfGolds);    		     //open up the map file and determine rows & columns

		if(ftruncate(shmPtr,area+sizeof(GameBoard)) == -1)		     //ftruncate (grows shared memory)
		{
			perror("ftruncate failed");
           		exit(1);
		}


		goldmap=(GameBoard*)mmap(NULL, area,
			    PROT_READ|PROT_WRITE, 
			    MAP_SHARED, 		//mmap so that we can treat shared memory as "normal" memory
			    shmPtr, 0);        

		goldmap->rows=(rowCount);
		goldmap->cols=(colCount);



		for(int j=0;j<5;j++){
			goldmap->pid[j] = 0;		
		}

		         //read in the file again, this time
		         //convert each character into a byte that is stored into
		         //the shared memory

/********************************************************************************************/
		fin.open("mymap.txt",ios::in);
	//	fin.open("map2.txt",ios::in);
		getline(fin,firstLine);
		int i=0;

		while(!fin.eof())
		{
			fin.get(ch);
			if(ch=='\n')
				continue;
		
			if(ch=='*'){
				goldmap->map[i] = G_WALL;
				i++;
			}
			else if(ch==' '){
				goldmap->map[i]=0;
				i++;
			}
		}
			         

	fin.close();
/********************************************************************************************/

	
		int realFlag = 0; 
	
		i=0;
		srand(time(NULL));
		while(numberOfGolds!=0)
		{
			randomPlace = rand() % (area-1) + 1;
			
			if(goldmap->map[randomPlace] == G_WALL)
				continue;
			else if(goldmap->map[randomPlace] == 0 && realFlag == 0){
				goldmap->map[randomPlace]|= G_GOLD;
				realFlag = 1;
			}
			else if(goldmap->map[randomPlace] == 0 && realFlag==1){
				goldmap->map[randomPlace]|= G_FOOL;
			}

			numberOfGolds--;
		}
/********************************************************************************************/
	
	// Now Place the the first player
		realFlag = 0;
		while(realFlag == 0)
		{
			randomPlace = rand() % (area-1) + 1;
			position = randomPlace;
			if(goldmap->map[randomPlace] == G_WALL || goldmap->map[randomPlace] == G_GOLD || goldmap->map[randomPlace] == G_FOOL)
				continue;
			else if(goldmap->map[randomPlace] == 0){
				goldmap->map[randomPlace]|=G_PLR0;
				realFlag = 1;
			}
		}
		goldmap->pid[0] = getpid();
		
/********************************************************************************************/
		if(goldmap->daemonID == 0){
			cerr << "Creating daemon\n";
			create_server_daemon();
			sleep(2);
		}
		if(goldmap->daemonID != 0){
			kill(goldmap->daemonID,SIGHUP);
		}
	
		char key;
		pid = 0;
		current_player = G_PLR0;
		getmessage = "Player# 1 says: ";
		sem_post(semaphorePtr);		//Release semaphore after player1 is placed on board.
		Map goldMine(goldmap->map,(rowCount),(colCount));
		mapPtr =&goldMine;
		refreshMap();
		readQueue();
		
		while((key = goldMine.getKey())!='Q' && interruptFlag!=true){
//			if(interruptFlag == true){break;}			
			if(key=='h')
			{
				if((position)%(colCount)==0){
					if(winFlag0 == true)
						break;
				}
				else if(goldmap->map[position-1] != G_WALL){
					goldmap->map[position] &= ~G_PLR0;
					goldmap->map[position-1]|=G_PLR0;
					position = position-1;
					goldMine.drawMap();
					refreshMap();
				}
				kill(goldmap->daemonID,SIGUSR1);
			} //if key==h

			else if(key=='l')
			{
				if((position+1)%(colCount)==0){
					if(winFlag0 == true)
						break;
				}
				else if(goldmap->map[position+1] != G_WALL){
					goldmap->map[position] &= ~G_PLR0;
					goldmap->map[position+1]|=G_PLR0;
					position = position+1;
					goldMine.drawMap();
					refreshMap();
				}
				kill(goldmap->daemonID,SIGUSR1);
			} //if key==l
		
			else if(key=='j')
			{
				if((position + (colCount)) > ((rowCount)*(colCount))){
					if(winFlag0 == true)
						break;
					
				}
				else if(goldmap->map[position+(colCount)] != G_WALL){
					goldmap->map[position] &= ~G_PLR0;
					goldmap->map[position+(colCount)]|=G_PLR0;
					position = position+(colCount);
					goldMine.drawMap();
					refreshMap();
				}
				kill(goldmap->daemonID,SIGUSR1);
			}
			
			else if(key=='k')
			{
				if(position < (colCount)){
					if(winFlag0 == true)
						break;
					
				}
				else if(goldmap->map[position-(colCount)] != G_WALL){
					goldmap->map[position] &= ~G_PLR0;
					goldmap->map[position-(colCount)]|=G_PLR0;
					position = position-(colCount);
					goldMine.drawMap();
					refreshMap();
				}
				kill(goldmap->daemonID,SIGUSR1);
			}
			else if(key=='m')
			{plrno = 0;int p;
				for(int j=0;j<5;j++){
					if(goldmap->pid[j] != 0){
						if(j==1){plrno |= G_PLR1;}
						if(j==2){plrno |= G_PLR2;}
						if(j==3){plrno |= G_PLR3;}
						if(j==4){plrno |= G_PLR4;}
					}
				}
				p = goldMine.getPlayer(plrno);//playerNo = p;
				if(p==G_PLR1){p=1;playerNo = 1;}
				if(p==G_PLR2){p=2;playerNo = 2;}
				if(p==G_PLR3){p=3;playerNo = 3;}
				if(p==G_PLR4){p=4;playerNo = 4;}
			
				if(plrno!=0){
				  //message = (goldMine.getMessage()).c_str();
				    getmessage = (goldMine.getMessage());
				    message = ("Player# 1 says: "+getmessage).c_str();
					
				    writeQueue(p);
				}

			}
			else if(key=='b')
			{
				getmessage = goldMine.getMessage();
				message = ("Player# 1 says: "+getmessage).c_str();
				for(int j=0;j<5;j++){
					if(goldmap->pid[j] != 0 && j!=0){
						writeQueue(j);	
					}
				}
			}
			if(goldmap->map[position] & G_GOLD)
			{
				goldMine.postNotice("Yeeee ! You found the Gold...Time to make your escape");
				winFlag0 = true;
				goldmap->map[position] &= ~G_GOLD;
				goldMine.drawMap();
			}	
			else
			{	
				   if(goldmap->map[position] & G_FOOL)
				   {		
				       goldMine.postNotice("Fools Gold...Keep Trying");
				
				   }
			}

		}	//end while
		if(winFlag0 == true){
			message = ("Player# 1 wins the game");//.c_str();
				for(int j=0;j<5;j++){
					if(goldmap->pid[j] != 0 && j!=0){
						writeQueue(j);	
					}
				}
		}
		pid = 0;		
		current_player = G_PLR0;
/********************************************************************************************/		

		}	//if(semaphorePtr!=SEM_FAILED) ends	

		else
		{
	
			char ip;
			pid = 0;
			
			bool flag = false;
			semaphorePtr= sem_open(
           		"/ABgoldchase", O_RDWR,S_IROTH| S_IWOTH| S_IRGRP| S_IWGRP | S_IRUSR| S_IWUSR, 1);
			sem_wait(semaphorePtr);
			int fd=shm_open("/ABshm", O_RDWR, S_IRUSR | S_IWUSR);
			if(fd==-1){
				perror("Can't use shared memory");
				exit(1);
			}			
					
			read(fd, &sharedRows, sizeof(int));
			read(fd, &sharedCols, sizeof(int));
//			 readQueue();

			goldmap= (GameBoard*)mmap(NULL, sharedRows*sharedCols, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
/********************************************************************************************/

			if(goldmap==MAP_FAILED)
			{
				cerr<<"\ngoldmap error";
				exit(1);
			}

			if((goldmap->pid[0] == 0)){
				current_player = G_PLR0;
				pid = 0;
				getmessage = "Player# 1 says: ";
			}
			else if((goldmap->pid[1] == 0)){
				pid = 1;
				current_player = G_PLR1;
				getmessage = "Player# 2 says: ";
			}
			else if((goldmap->pid[2] == 0)){
				pid = 2;
				current_player = G_PLR2;
				getmessage = "Player# 3 says: ";
			}			
			else if((goldmap->pid[3] == 0)){
				pid = 3;
				current_player = G_PLR3;
				getmessage = "Player# 4 says: ";
			}
			else if((goldmap->pid[4] == 0)){
				pid = 4;
				current_player = G_PLR4;
				getmessage = "Player# 5 says: ";
			}
			else{
				cerr<<"Not enough space on board"<<endl;
				sem_post(semaphorePtr);
				//return 0;
				return;
			}
/********************************************************************************************/
			goldmap->pid[pid] = getpid();
			assignQueueName(pid);
			srand(time(NULL));
			flagplr2 = 0;
			while(flagplr2 == 0)
			{
				randomPlace = rand() % (sharedRows*sharedCols) + 1;
				position = randomPlace;
				if(goldmap->map[randomPlace] == G_WALL || goldmap->map[randomPlace] == G_GOLD || goldmap->map[randomPlace] == G_FOOL)
					continue;
				else
				{
					goldmap->map[randomPlace] = current_player;					
					flagplr2 = 1;
				}
				
			}
			sem_post(semaphorePtr);
			kill(goldmap->daemonID,SIGHUP);
			kill(goldmap->daemonID,SIGUSR1);
			Map goldMine(goldmap->map,sharedRows,sharedCols);
			mapPtr =&goldMine;
			refreshMap();
			readQueue();

		while((ip = goldMine.getKey())!='Q'){
                         if(interruptFlag == true){break;}
			if(ip=='h')
			{
				if((position)%sharedCols==0){
					if(winFlag0==true)
						{flag = true;break;}
				}
				else if(goldmap->map[position-1] != G_WALL){
					goldmap->map[position] &= ~current_player;
				
					goldmap->map[position-1]|=current_player;
					position = position-1;
					goldMine.drawMap();
				}
				refreshMap();
				kill(goldmap->daemonID,SIGUSR1);
			} //if key==h

			else if(ip=='l')
			{
				if((position+1)%sharedCols==0){
					if(winFlag0==true)
						{flag = true;break;}
				}
				else if(goldmap->map[position+1] != G_WALL){
					goldmap->map[position] &= ~current_player;
				
					goldmap->map[position+1]|=current_player;
					position = position+1;
					goldMine.drawMap();
				}
				refreshMap();
				kill(goldmap->daemonID,SIGUSR1);
			} //if key==l
		
			else if(ip=='j')
			{
				if((position + sharedCols) > (sharedRows*sharedCols)){		
					if(winFlag0==true)
						{flag = true;break;}
				}
				else if(goldmap->map[position+sharedCols] != G_WALL){
					goldmap->map[position] &= ~current_player;
				
					goldmap->map[position+sharedCols]|=current_player;
					position = position+sharedCols;
					goldMine.drawMap();
				}
				refreshMap();
				kill(goldmap->daemonID,SIGUSR1);
			}
			
			else if(ip=='k')
			{
				if(position < sharedCols){
					if(winFlag0==true)
						{flag = true;break;}
					
				}
				else if(goldmap->map[position-sharedCols] != G_WALL){
					goldmap->map[position] &= ~current_player;
					goldmap->map[position-sharedCols]|=current_player;
					position = position-sharedCols;
					goldMine.drawMap();
				}
			refreshMap();
			kill(goldmap->daemonID,SIGUSR1);
			}
			
			else if(ip=='m')
			{plrno = 0;int p;
				for(int j=0;j<5;j++){
					if(goldmap->pid[j] != 0 && pid!=j){
						if(j==0){plrno |= G_PLR0;}
						if(j==1){plrno |= G_PLR1;}
						if(j==2){plrno |= G_PLR2;}
						if(j==3){plrno |= G_PLR3;}
						if(j==4){plrno |= G_PLR4;}
					}
				}
				p = goldMine.getPlayer(plrno);
				if(p==G_PLR0){p=0;}
				if(p==G_PLR1){p=1;}
				if(p==G_PLR2){p=2;}
				if(p==G_PLR3){p=3;}
				if(p==G_PLR4){p=4;}

				if(plrno!=0){
				  string temp = getmessage;
				  temp = getmessage+(goldMine.getMessage());
					
				  message = (temp).c_str();
				  writeQueue(p);
				  temp.clear();
				}

			}
			else if(ip=='b')
			{
				string temp = getmessage;
				temp = getmessage+(goldMine.getMessage());
				message = (temp).c_str();
				for(int j=0;j<5;j++){
					if(goldmap->pid[j] != 0 && j!=pid){
						writeQueue(j);	
					}
				}
				temp.clear();
			}
			
			if(goldmap->map[position] & G_GOLD)
			{
				goldMine.postNotice("Yeeee ! You found the Gold...Time to make your escape");
				winFlag0=true;
				goldmap->map[position] &= ~G_GOLD;
				goldMine.drawMap();
			}	
			else
			{				
				   if(goldmap->map[position] & G_FOOL)
				   {		
				       goldMine.postNotice("Fools Gold...Keep Trying");
			
				   }
			}

		}	//end while

			if(flag==true){
				message = ("Player# "+to_string(pid+1)+" wins the game").c_str();
				for(int j=0;j<5;j++){
					if(goldmap->pid[j] != 0 && j!=pid){
						writeQueue(j);	
					}
				}
			}
		}
		
		assignQueueName(pid);
		mq_close(readqueue_fd);
		mq_unlink(mq_name.c_str());
		goldmap->pid[pid] = 0;
		sem_wait(semaphorePtr);
//		goldmap->players &= ~current_player;
		sem_post(semaphorePtr);
		goldmap->map[position] = 0;
		kill(goldmap->daemonID,SIGHUP);
		delete[]daemon_map;
	refreshMap();
	
//	return 0;
}



void create_client_daemon(string ip)
{
	int pipefd[2];
	unsigned char sockByteCopy;
  pipe(pipefd);
  
  if(fork()>0)//parent;
  {//return;
    close(pipefd[1]); //close write, parent only needs read
    int val;
    read(pipefd[0], &val, sizeof(val));
    if(val==0){
      write(1, "Success!\n", sizeof("Success!\n"));
	  
    }
    else
      write(1, "Failure!\n", sizeof("Failure!\n"));
    return;
  }
  //child
  if(fork()>0)
  {
	exit(0);
  }
  if(setsid()==-1)
    exit(1);
  for(int i=0; i< sysconf(_SC_OPEN_MAX); ++i)
  {
    if(i!=pipefd[1])//close everything, except write
      close(i);
  }
  open("/dev/null", O_RDWR); //fd 0
  open("/dev/null", O_RDWR); //fd 1
  open("/dev/null", O_RDWR); //fd 2
  umask(0);
  chdir("/");

/*	if((debugfd = open("/home/akshay/CSCI611/project3/test_pipe",O_RDWR))==-1){
		cerr << "failed to open pipe\n";
	}*/
client(ip);	
  sleep(5); //here, daemon is setting up shared memory

  int val=0;
  write(pipefd[1], &val, sizeof(val));


	struct sigaction act_sighup;
	act_sighup.sa_handler = server_cleanup;//handle_sighup;	
	sigemptyset(&act_sighup.sa_mask);
	act_sighup.sa_flags=0;
	act_sighup.sa_restorer=NULL;
	sigaction(SIGHUP, &act_sighup, NULL);
	sigaction(SIGUSR1, &act_sighup, NULL);
	sigaction(SIGUSR2, &act_sighup, NULL);

//	obj.client(debugfd,"1");
	while(1){
	
		READ(sockfd, &sockByteCopy, sizeof(char));
//		write(debugfd,&sockByteCopy,sizeof(char));
		if(sockByteCopy==0){
			short loc=0;unsigned char change;
			READ(sockfd, &loc, sizeof(short));
			READ(sockfd, &change, sizeof(unsigned char));
			daemon_map[loc] = change;
			goldmap->map[loc] = change;
			refreshMap();
		}
		else if(sockByteCopy & G_SOCKPLR)
		{
		        unsigned char player_bit[5]={G_PLR0, G_PLR1, G_PLR2, G_PLR3, G_PLR4,};
		          for(int i=0; i<5; ++i) //loop through the player bits
		          {
		            if(sockByteCopy & player_bit[i] && goldmap->pid[i]==0){
		                goldmap->pid[i] = goldmap->daemonID;

					assignQueueName(i);
					//	readQueue();
						 
						struct sigaction action_to_take;
					  	action_to_take.sa_handler=read_message_daemon;
					  //zero out the mask (allow any signal to interrupt)
					  	sigemptyset(&action_to_take.sa_mask); 
					  	action_to_take.sa_flags=0;
					  //tell how to handle SIGINT
					  	sigaction(SIGUSR2, &action_to_take, NULL); 

						struct mq_attr mq_attributes;
					  	mq_attributes.mq_flags=0;
					  	mq_attributes.mq_maxmsg=10;
					  	mq_attributes.mq_msgsize=120;

					  	if((player_other_machine[i] = mq_open(mq_name.c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
						  	S_IRUSR|S_IWUSR, &mq_attributes))==-1)
					  	{
					    		perror("mq_open");
					    		exit(1);
					  	}
						write(debugfd,&player_other_machine[i],sizeof(player_other_machine[i]));
					  //set up message queue to receive signal whenever message comes in
					  	struct sigevent mq_notification_event;
					  	mq_notification_event.sigev_notify=SIGEV_SIGNAL;
					  	mq_notification_event.sigev_signo=SIGUSR2;
					  	mq_notify(player_other_machine[i], &mq_notification_event);
		             
		            }
		            else if(!(sockByteCopy & player_bit[i]) && goldmap->pid[i]!=0){
		                goldmap->pid[i] = 0;
				assignQueueName(i);
			//	mq_close(readqueue_fd);
				mq_close(player_other_machine[i]);
				mq_unlink(mq_name.c_str());
		            }
		        }
		    
		    if(sockByteCopy==G_SOCKPLR){
			kill(goldmap->daemonID,SIGHUP);
		    }
 
                refreshMap();
             }
	}
close(sockfd);
}


void client(string message)
{
//	  int sockfd; //file descriptor for the socket
	  int status; //for error checking

	  //change this # between 2000-65k before using
	//  const char* portno="42424"; 
		const char* portno="42424"; 

	  struct addrinfo hints;
	  memset(&hints, 0, sizeof(hints)); //zero out everything in structure
	  hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
	  hints.ai_socktype=SOCK_STREAM; // TCP stream sockets

	  struct addrinfo *servinfo;
	  //instead of "localhost", it could by any domain name
//	  if((status=getaddrinfo("192.168.194.187", portno, &hints, &servinfo))==-1)
	  if((status=getaddrinfo(message.c_str(), portno, &hints, &servinfo))==-1)
	  {
	    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
	    exit(1);
	  }
	  sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

	  if((status=connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
	  {
	    perror("connect");
	    exit(1);
	  }
	  //release the information allocated by getaddrinfo()
	  freeaddrinfo(servinfo);

//	  const char* message="One small step for (a) man, one large  leap for Mankind";
	  int n;
	  if((n=WRITE(sockfd, "message\n", strlen("message\n")))==-1)
	  {
	    perror("write");
	    exit(1);
	  }
	cerr << "client wrote " << n <<" characters\n";
//	int cnt = 0;
//	unsigned char* input;
	int rows,cols;
	unsigned char sockByteCopy;	

	READ(sockfd, &rows, sizeof(rows));
//	cerr <<"rows: " <<rows<<"\n";
	READ(sockfd, &cols, sizeof(cols));
//	cerr << "cols: "<<cols<<"\n";
	READ(sockfd,&sockByteCopy,sizeof(char));
	write(debugfd,&sockByteCopy,sizeof(char));
	daemon_map=new unsigned char[rows*cols];
	for(int i=0;i<rows*cols;i++){
		READ(sockfd, &daemon_map[i], sizeof(char));
	}

	
//	if(semaphorePtr!=SEM_FAILED)
       	{	
//		cerr <<"\nIn if(semaphorePtr!=SEM_FAILED)";
		sem_wait(semaphorePtr);		//sem_wait

		shmPtr = shm_open("/ABshm", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);   //shm_open (creates shared memory)


		if(shmPtr==-1){
			perror("Semaphore failed");
           		exit(1);
		}

		area = rows*cols;    		     //open up the map file and determine rows & columns

		if(ftruncate(shmPtr,area+sizeof(GameBoard)) == -1)		     //ftruncate (grows shared memory)
		{
			perror("ftruncate failed");
           		exit(1);
		}
		cerr<<"\ntruncated";
		goldmap=(GameBoard*)mmap(NULL, area+sizeof(GameBoard),
			    PROT_READ|PROT_WRITE, 
			    MAP_SHARED, 		
			    shmPtr, 0);        

		goldmap->rows=(rows);
		goldmap->cols=(cols);
	  	goldmap->daemonID=getpid();	
		globalSockByte |= sockByteCopy;
		unsigned char player_bit[5]={G_PLR0, G_PLR1, G_PLR2, G_PLR3, G_PLR4};
		          for(int i=0; i<5; ++i) //loop through the player bits
		          {
		            if(sockByteCopy & player_bit[i] && goldmap->pid[i]==0){
		                goldmap->pid[i] = goldmap->daemonID;
				assignQueueName(i);
			 
				struct sigaction action_to_take;
				action_to_take.sa_handler=read_message_daemon;
				sigemptyset(&action_to_take.sa_mask); 
				action_to_take.sa_flags=0;
				sigaction(SIGUSR2, &action_to_take, NULL); 
				struct mq_attr mq_attributes;
				mq_attributes.mq_flags=0;
				mq_attributes.mq_maxmsg=10;
				mq_attributes.mq_msgsize=120;

				if((player_other_machine[i] = mq_open(mq_name.c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
				 	S_IRUSR|S_IWUSR, &mq_attributes))==-1)
				{
					perror("mq_open");
					exit(1);
				}
				write(debugfd,&player_other_machine[i],sizeof(player_other_machine[i]));
				struct sigevent mq_notification_event;
				mq_notification_event.sigev_notify=SIGEV_SIGNAL;
				mq_notification_event.sigev_signo=SIGUSR2;
				mq_notify(player_other_machine[i], &mq_notification_event);
		             
		            }
		        }
//		cerr <<"\nrows and cols assigned";
		for(int i=0;i<rows*cols;i++)
		{
			goldmap->map[i] = daemon_map[i];
		}
	}
	sem_post(semaphorePtr);

	
     //   close(sockfd);

}
void create_server_daemon()
{
	  if(fork()>0)
	  {
	    //I'm the parent, leave the function
	    return;
	  }

	  if(fork()>0)
	    exit(0);
	  if(setsid()==-1)
	    exit(1);
	  for(int i; i< sysconf(_SC_OPEN_MAX); ++i)
	    close(i);
	  open("/dev/null", O_RDWR); //fd 0
	  open("/dev/null", O_RDWR); //fd 1
	  open("/dev/null", O_RDWR); //fd 2
	  umask(0);
	  chdir("/");

/*	if((debugfd = open("/home/akshay/CSCI_611/project3/test_pipe",O_RDWR))==-1){
		cerr << "failed to open pipe\n";
	}*/
	goldmap=(GameBoard*)mmap(NULL, area,
			    PROT_READ|PROT_WRITE, 
			    MAP_SHARED, 		//mmap so that we can treat shared memory as "normal" memory
			    shmPtr, 0);   
	goldmap->daemonID=getpid();
	daemon_map=new unsigned char[goldmap->rows*goldmap->cols];

	cerr << "deamon created\n";
	for(int i=0;i<(goldmap->rows*goldmap->cols);i++){
		daemon_map[i] = goldmap->map[i];
	}
	

	for(int i=0;i<5;i++)
		player_other_machine[i] = -1;

	struct sigaction act_sighup;
	act_sighup.sa_handler = server_cleanup;//handle_sighup;	
	sigemptyset(&act_sighup.sa_mask);
	act_sighup.sa_flags=0;
	act_sighup.sa_restorer=NULL;
	sigaction(SIGHUP, &act_sighup, NULL);
	sigaction(SIGUSR1, &act_sighup, NULL);
	sigaction(SIGUSR2, &act_sighup, NULL);
	

	server();
		

}


void server()
{
	int old_sockfd; //file descriptor for the socket
	  int status; //for error checking

	unsigned char sockByteCopy;
	  //change this # between 2000-65k before using
	  const char* portno="42424"; 
	  struct addrinfo hints;
	  memset(&hints, 0, sizeof(hints)); //zero out everything in structure
	  hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
	  hints.ai_socktype=SOCK_STREAM; // TCP stream sockets
	  hints.ai_flags=AI_PASSIVE; //file in the IP of the server for me

	  struct addrinfo *servinfo;
	  if((status=getaddrinfo(NULL, portno, &hints, &servinfo))==-1)
	  {
	    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
	    exit(1);
	  }
	  old_sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

	  //avoid "Address already in use" error
	  int yes=1;
	  if(setsockopt(old_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1)
	  {
	    perror("setsockopt");
	    exit(1);
	  }

	  //We need to "bind" the socket to the port number so that the kernel
	  //can match an incoming packet on a port to the proper process
	  if((status=bind(old_sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
	  {
	    perror("bind");
	    exit(1);
	  }
	  //when done, release dynamically allocated memory
	  freeaddrinfo(servinfo);

	  if(listen(old_sockfd,1)==-1)
	  {
	    perror("listen");
	    exit(1);
	  }

//	  printf("Blocking, waiting for client to connect\n");

	  struct sockaddr_in client_addr;
	  socklen_t clientSize=sizeof(client_addr);
	//  int new_sockfd;

	  do{//cerr << "waiting for client to connect\n";
		sockfd=accept(old_sockfd, (struct sockaddr*) &client_addr, &clientSize);
	
	  }while(sockfd==-1 && errno==EINTR);
	  if(sockfd==-1){
		perror("accept");
	   //	exit(1);
	  }
//	cerr << "Connection established\n";
	  WRITE(sockfd,&goldmap->rows,sizeof(goldmap->rows));
	  WRITE(sockfd,&goldmap->cols,sizeof(goldmap->cols));
	  WRITE(sockfd,&globalSockByte,sizeof(char));
//	  WRITE(sockfd,&goldmap->players,sizeof(char));
		  
	  for(int i=0;i<(goldmap->rows*goldmap->cols);i++){
		WRITE(sockfd,&daemon_map[i],sizeof(char));
	  }

	  while(1)
	  {
		READ(sockfd, &sockByteCopy, 1);
		
		if(sockByteCopy & G_SOCKPLR)
		{		
				unsigned char player_bit[5]={G_PLR0, G_PLR1, G_PLR2, G_PLR3, G_PLR4};
  				for(int i=0; i<5; i++) //loop through the player bits
  				{
					if((sockByteCopy & player_bit[i]) && (goldmap->pid[i]==0)){
						goldmap->pid[i] = goldmap->daemonID;
						assignQueueName(i);
						 
						struct sigaction action_to_take;
					  	action_to_take.sa_handler=read_message_daemon;
					  //zero out the mask (allow any signal to interrupt)
					  	sigemptyset(&action_to_take.sa_mask); 
					  	action_to_take.sa_flags=0;
					  //tell how to handle SIGINT
					  	sigaction(SIGUSR2, &action_to_take, NULL); 

						struct mq_attr mq_attributes;
					  	mq_attributes.mq_flags=0;
					  	mq_attributes.mq_maxmsg=10;
					  	mq_attributes.mq_msgsize=120;

					  	if((player_other_machine[i] = mq_open(mq_name.c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
						  	S_IRUSR|S_IWUSR, &mq_attributes))==-1)
					  	{
					    		perror("mq_open");
					    		exit(1);
					  	}
						write(debugfd,&player_other_machine[i],sizeof(player_other_machine[i]));
					  //set up message queue to receive signal whenever message comes in
					  	struct sigevent mq_notification_event;
					  	mq_notification_event.sigev_notify=SIGEV_SIGNAL;
					  	mq_notification_event.sigev_signo=SIGUSR2;
					  	mq_notify(player_other_machine[i], &mq_notification_event);

					}
					else if(!(sockByteCopy & player_bit[i]) && goldmap->pid[i]!=0){

					//	goldmap->players &= ~player_bit[i];
						goldmap->pid[i] = 0;
						assignQueueName(i);
					//	mq_close(readqueue_fd);
						mq_close(player_other_machine[i]);
						mq_unlink(mq_name.c_str());
					
					}
				}
			
			if(sockByteCopy==G_SOCKPLR){
				kill(goldmap->daemonID,SIGHUP);
			}
			
				
 
        	    refreshMap();
             }
	     if(sockByteCopy==0){
			short loc=0;unsigned char change;
			READ(sockfd, &loc, sizeof(short));
			READ(sockfd, &change, sizeof(unsigned char));
			daemon_map[loc] = change;
			goldmap->map[loc] = change;
			refreshMap();
	     }
	  }//end while(1)
	  close(sockfd);
	
}

void assignQueueName(int pid)
{
	if(pid==0){mq_name="/AB_player1_mq";}
	if(pid==1){mq_name="/AB_player2_mq";}
	if(pid==2){mq_name="/AB_player3_mq";}
	if(pid==3){mq_name="/AB_player4_mq";}
	if(pid==4){mq_name="/AB_player5_mq";}

}

void refreshMap()
{
	for(int j=0;j<5;j++){
		if(goldmap->pid[j] != 0){
			kill(goldmap->pid[j],SIGUSR1);
		}
	}
}



void readQueue()
{
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
    		perror("mq_open");//write(debugfd,"problem is here\n",sizeof("problem is here\n"));
    		exit(1);
  	}
  //set up message queue to receive signal whenever message comes in
  	struct sigevent mq_notification_event;
  	mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  	mq_notification_event.sigev_signo=SIGUSR2;
  	mq_notify(readqueue_fd, &mq_notification_event);
}

void read_message(int)
{
  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);

  //read a message
  int err;
  char msg[121];
  memset(msg, 0, 121);//set all characters to '\0'
  while((err=mq_receive(readqueue_fd, msg, 120, NULL))!=-1)
  {
 //   cout << "Message received: " << msg << endl;
	mapPtr->postNotice(msg);
    memset(msg, 0, 121);//set all characters to '\0'
  }
  //we exit while-loop when mq_receive returns -1
  //if errno==EAGAIN that is normal: there is no message waiting
  if(errno!=EAGAIN)
  {
    perror("mq_receive");
    exit(1);
  }
}

void read_message_daemon(int)
{
}



void writeQueue(int plr)
{
	assignQueueName(plr);
  	mqd_t writequeue_fd; //message queue file descriptor
  	if((writequeue_fd=mq_open(mq_name.c_str(), O_WRONLY|O_NONBLOCK))==-1)
  	{
    		perror("mq_open");
    		exit(1);
  	}
//	kill(goldmap->daemonID,SIGUSR2);
  	char message_text[121];
  	memset(message_text, 0, 121);
  	strncpy(message_text, message, 120);
  	if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
  	{
    		perror("mq_send");
    		exit(1);
  	}
  	mq_close(writequeue_fd);
}


