#include <iostream>
#include <stdio.h>
#include <fstream>
#include <sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<unistd.h> //for read/write
#include<string.h> //for memset
#include<stdio.h> //for fprintf, stderr, etc.
#include<stdlib.h> //for exit
#include<string>
#include "fancyRW.h"
#include <errno.h>

using namespace std;


class support{

	public:
		char* server();
		void client(int sockfd,const char* message);
		int readMap(int&,int&,int&);
		char buffer[100];
		int numberOfGolds;

};

int support::readMap(int& rowCount,int& colCount,int& numberOfGolds)
{
	ifstream fin;
	char ch;
	std::string firstRow;
	int count = 0;
	bool Columnflag = false;

	fin.open("mymap.txt",ios::in);
//	fin.open("map2.txt",ios::in);
	getline(fin,firstRow);

	while(!fin.eof())
	{
		fin.get(ch);
		if(Columnflag==false)
			count++;
		if(ch=='\n'){
			rowCount++;
			colCount = count;
			Columnflag = true;;
		}
	}
	fin.close();
//	numberOfGolds = atoi(firstRow.c_str());
	numberOfGolds = stoi(firstRow);
	rowCount = rowCount - 1;
	colCount = colCount - 1;

	return ((rowCount)*(colCount));
}
