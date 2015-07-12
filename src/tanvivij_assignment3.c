/**
 * @tanvivij_assignment3
 * @author  Tanvi Vijay Bhavsar <tanvivij@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>


#include "../include/global.h"
#include "../include/logger.h"
#define STDIN 0

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */
int input_timeout,noofpackets;
char topology_file[200];
char myport[20],myip[50];

int ssockfd, lssockfd,nctr=0;
int noofservers,noofneighbours, myid;
//[PA3] Routing Table Start
uint16_t cost[5][5];
//[PA3] Routing Table End
char cmd[100];
char tokens_array[100][100];



//maintains details of all servers
struct node
{
	char port[20] ;
	char ip[50];
	int id;
	int timeout;
	int isneighbour;//tells whether it is neighbour or not
	int sendneighbour;//tells whether its hould send to neighbour or not. Should not be sent if disabled
} myneighbours[5];

//Tells what is nexthop for given iD
int nexthop[15];
struct serverupdate
{
	uint32_t ip1;
	uint16_t port1;
	uint16_t pad1;
	uint16_t id1;
	uint16_t cost1;
};

//[PA3] Update Packet Start
struct routing_packet//For update packet, kept serverupdate maximum which can be 5. While sending send only according to no of servers
{
	uint16_t noofupdates;
	uint16_t port;
	uint32_t ip;
	struct serverupdate neighbours[5];//details of all servers in network
}mypacket;
//[PA3] Update Packet End

//Used to extract details from incoming packet
struct sidcost
{
	uint16_t cost;
	uint16_t id;
}idcost[5];

int ictr=0;
uint16_t packetid;

//string which is sent
char rupdate[100];
int size_rupdate;//size of string
char iupdate[100];

//Stores link cost
uint16_t direct_cost[5],direct_cost2[5];

//displays routing table
void print_routing_table()

{
	int i,j;
	printf("\nRouting table is");
	for(i=0;i<5;i++)
	{
		printf("\n");
		for(j=0;j<5;j++)
			printf("%d  ",cost[i][j]);


	}
}

//Runs bellman ford
void compute_distance()
{

	int j,i;
	for(i=0;i<nctr;i++)

	{
		uint16_t costs, min=65535;
		int destid=myneighbours[i].id;


		for(j=0;j<nctr;j++)


		{
			int nhopid=myneighbours[j].id;

			if(myneighbours[j].isneighbour==1)
			{


				if((direct_cost[nhopid-1]==65535)||(cost[nhopid-1][destid-1]==65535))
					costs=65535;
				else
					costs=direct_cost[nhopid-1]+cost[nhopid-1][destid-1];


				if(min>costs)
				{

					min=costs;
					nexthop[destid-1]=nhopid;
				}

			}
		}


		cost[myid-1][destid-1]=min;
		if(min==65535)
			nexthop[destid-1]=-1;


	}
	nexthop[myid-1]=myid;
	cost[myid-1][myid-1]=0;



}

//Extract details of packet
void extract_string()

{
	int noofupdates,ptr=0,i,ictr=0;
	uint16_t temp;
	memcpy(&temp,iupdate,2);
	ptr+=16;
	noofupdates=ntohs(temp);
	printf("\nnoofupdates is %d",noofupdates);
	for(i=0;i<noofupdates;i++)

	{
		memcpy(&temp,iupdate+ptr,2);
		idcost[ictr].id=ntohs(temp);
		ptr+=2;
		memcpy(&temp,iupdate+ptr,2);
		idcost[ictr].cost=ntohs(temp);
		ptr+=10;
		if(idcost[ictr].cost==0)
			packetid=idcost[ictr].id;
		ictr++;
	}
	printf("\n packet id is %d..After string extraction",packetid);
	int k,isdis=0;
	for(k=0;k<nctr;k++)
	{

		if((myneighbours[k].id==packetid))
		{
			if((myneighbours[k].sendneighbour==0)&&(myneighbours[k].isneighbour==1))//neighbour is disabled
				isdis=1;
			else
				myneighbours[k].timeout=0;
		}
	}
	if((isdis==1))
		return;
	cse4589_print_and_log("RECEIVED A MESSAGE FROM SERVER %d\n", packetid);
	//sort according to ID's

	int c,d;

	struct sidcost temp_idcost;
	// for bubble sort used http://www.programmingsimplified.com/c/source-code/c-program-bubble-sort
	for (c = 0 ; c < ( ictr - 1 ); c++)
	{
		for (d = 0 ; d < ictr - c - 1; d++)
		{
			if (idcost[d].id > idcost[d+1].id)
			{
				temp_idcost       = idcost[d];
				idcost[d]   = idcost[d+1];
				idcost[d+1] = temp_idcost;
			}
		}
	}
	for(i=0;i<ictr;i++)
		cse4589_print_and_log("%-15d%-15d\n", idcost[i].id,idcost[i].cost);


	printf("\nRouting table before and after update from routing packet");
	print_routing_table();
	for(i=0;i<nctr;i++)
		cost[packetid-1][idcost[i].id-1]=idcost[i].cost;
	//since received update from neighbour link cost becomes original
	if(direct_cost[packetid-1]==65535)
		direct_cost[packetid-1]=direct_cost2[packetid-1];
	print_routing_table();
	noofpackets++;

}

//Copies packet content to string which is sent
void form_string()
{

	int i,ptr=0;
	memset(rupdate,'\0',100);

	memcpy(rupdate+ptr, &mypacket.noofupdates, 2);
	ptr+=2;

	memcpy(rupdate+ptr, &mypacket.port, 2);
	ptr+=2;
	memcpy(rupdate+ptr, &mypacket.ip, 4);
	ptr+=4;
	for(i=0;i<nctr;i++)
	{

		memcpy(rupdate+ptr, &mypacket.neighbours[i].ip1, 4);
		ptr+=4;

		memcpy(rupdate+ptr, &mypacket.neighbours[i].port1, 2);
		ptr+=2;
		memcpy(rupdate+ptr, &mypacket.neighbours[i].pad1, 2);
		ptr+=2;

		memcpy(rupdate+ptr, &mypacket.neighbours[i].id1, 2);
		ptr+=2;

		memcpy(rupdate+ptr, &mypacket.neighbours[i].cost1, 2);
		ptr+=2;


	}
	size_rupdate=(64+(96*nctr))/8;


}

//Forms packet to be sent
void form_packet()

{


	mypacket.noofupdates=htons(noofservers);
	printf("myport before %s and after atoi is %d",myport, atoi(myport));
	mypacket.port=htons(atoi(myport));
	inet_pton(AF_INET, myip, &(mypacket.ip));
	int i;
	for(i=0;i<nctr;i++)
	{

		inet_pton(AF_INET, myneighbours[nctr-1-i].ip, &(mypacket.neighbours[i].ip1));
		mypacket.neighbours[i].port1=htons(atoi(myneighbours[nctr-1-i].port));
		mypacket.neighbours[i].pad1=htons(0);
		mypacket.neighbours[i].id1=htons(myneighbours[nctr-1-i].id);
		mypacket.neighbours[i].cost1=htons((cost[myid-1][(myneighbours[nctr-1-i].id)-1]));
		printf("\nmypacket.neighbours[i].cost1 is %hd..after ntohs %hd",mypacket.neighbours[i].cost1,ntohs(mypacket.neighbours[i].cost1));

	}


}
int create_socket()


{
	printf("\n in create_socket");

	//from beej
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr;
	char buf[100];
	socklen_t addr_len;
	char s[100];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP
	printf("\n Calling getaddrinfo to Create listeng socket on port %s", myport);
	if ((rv = getaddrinfo(NULL, myport, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((lssockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}

		if (bind(lssockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(lssockfd);
			perror("listener: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return -1;
	}

	freeaddrinfo(servinfo);

	return 1;
}

//Receives packet at socket
void receivepacket()

{
	struct sockaddr_storage their_addr;
	int numbytes;
	socklen_t addr_len;
	printf("listener: waiting to recvfrom...\n");
	memset(iupdate,'\0',100);
	addr_len = sizeof their_addr;
	//used beej
	if ((numbytes = recvfrom(lssockfd, iupdate, 99 , 0,(struct sockaddr *)&their_addr, &addr_len)) == -1) {
		perror("recvfrom");
		exit(1);
	}

	printf("**********Received data %s***************..numbytes received %d",iupdate,numbytes);

}
int create_send_socket()

{


	//from beej

	printf("\n in send socket");
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	printf("\nCalling  getaddrinfo with ip :%s and port :%s", myip, myport);
	if ((rv = getaddrinfo(myip, myport, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((ssockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("talker: socket");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "talker: failed to bind socket\n");
		return -1;
	}



	freeaddrinfo(servinfo);

	return 1;

}

//send packet to all neighbour
void send_neighbour()

{
	//used http://www.abc.se/~m6695/udp.html 
	//Also beej
	struct sockaddr_in naddr;
	int slen=sizeof(naddr);
	int numbytes,i;


	memset((char *) &naddr, 0, sizeof(naddr));
	naddr.sin_family = AF_INET;

	//send packet to all neighbours
	for(i=0;i<nctr;i++)
	{
		if((myneighbours[i].isneighbour==1)&&(myneighbours[i].sendneighbour==1))
		{
			naddr.sin_port = htons(atoi(myneighbours[i].port));
			if (inet_aton(myneighbours[i].ip, &naddr.sin_addr)==0) {
				printf("Error in ip %s",myneighbours[i].ip);
				fprintf(stderr, "inet_aton() failed\n");
				exit(1);
			}

			printf("\n *****sending rupdate %s  size of rupdate is is %d *****\n",rupdate,size_rupdate);
			if ((numbytes = sendto(ssockfd, rupdate, size_rupdate, 0,
					(struct sockaddr*)&naddr, slen)) == -1) {
				perror("talker: sendto");
				exit(1);
			}
		}
	}

}
//Read from topology file
void read_file()
{

	//initialize cost mayrix to 65535

	int i, j,k;
	for(i=0;i<5;i++)
	{
		for(j=0;j<5;j++)
			cost[i][j]=65535;
	}
	for(i=0;i<5;i++)
	{
		myneighbours[i].isneighbour=0;
		myneighbours[i].timeout=-1;
		myneighbours[i].sendneighbour=1;
	}
	FILE *fp;
	fp = fopen(topology_file, "rb");

	if(!fp)
	{
		printf("\n fopenn error");
		return;
	}

	char line_file[20][50];
	int line_ctr=0,no_read;

	while(fgets ( line_file[line_ctr], 50, fp ) != NULL)

	{



		line_ctr++;
		memset(line_file[line_ctr],'\0',50);



	}
	printf("\nline array is..line_ctr is %d \n",line_ctr);
	for(i=0;i<line_ctr;i++)
		printf("line is %s",line_file[i]);
	noofservers=atoi(line_file[0]);
	noofneighbours=atoi(line_file[1]);
	char *token,buf[100];
	char s[2] = " ";
	strcpy(buf,line_file[line_ctr-1]);
	token=(char *)malloc(100*sizeof(char));

	token = strtok(buf, s);

	myid=atoi(token);
	int id1;
	int ctr=0;
	uint16_t cost1;
	while( token != NULL )
	{

		printf("\n in first while in token");
		fflush(stdout);
		if(ctr==1)
		{
			id1=atoi(token);

		}
		if (ctr==2)
			cost1=atoi(token);
		token = strtok(NULL, s);
		ctr++;

	}

	cost[myid-1][id1-1]=cost1;
	direct_cost[id1-1]=cost1;
	cost[myid-1][myid-1]=0;
	cost[id1-1][id1-1]=0;
	myneighbours[nctr].id=id1;
	myneighbours[nctr].isneighbour=1;
	nctr++;

	for(j=2;j<=noofneighbours;j++)
	{
		strcpy(buf,line_file[line_ctr-j]);
		token = strtok(buf, s);

		ctr=0;
		while( token != NULL )
		{

			fflush(stdout);

			if(ctr==1)
			{
				id1=atoi(token);

			}
			if (ctr==2)
				cost1=atoi(token);

			ctr++;
			token = strtok(NULL, s);

		}
		cost[myid-1][id1-1]=cost1;
		direct_cost[id1-1]=cost1;
		printf("\nid1 is %d",id1);
		cost[id1-1][id1-1]=0;
		myneighbours[nctr].id=id1;
		myneighbours[nctr].isneighbour=1;
		nctr++;
		printf("cost is %d  ",cost[id1-1][id1-1]);
	}

	printf("\n noofservers %d,noofneighbours %d, myid %d",noofservers,noofneighbours, myid);
	print_routing_table();
	char ipn[20];
	char portn[20];
	for(j=0;j<noofservers;j++)
	{

		strcpy(buf,line_file[2+j]);
		token = strtok(buf, s);
		printf("\n buf %s  and token %s after strtok",buf, token);
		id1=atoi(token);
		ctr=0;
		while( token != NULL )
		{

			fflush(stdout);
			if(ctr==1)
			{
				strcpy(ipn,token);

			}
			if (ctr==2)
			{
				strcpy(portn,token);

				int id_f=0;
				if(id1==myid)
				{
					strcpy(myip,ipn);
					strcpy(myport,portn);
					printf("\n ########### myip %s and myport %s",myip, myport);
				}

				for(k=0;k<nctr;k++)
				{

					if(id1==myneighbours[k].id)
					{
						strcpy(myneighbours[k].ip,ipn);
						strcpy(myneighbours[k].port,portn);
						id_f=1;
					}
				}


				if(id_f==0)
				{
					myneighbours[nctr].id=id1;
					strcpy(myneighbours[nctr].ip,ipn);
					strcpy(myneighbours[nctr].port,portn);
					nctr++;

				}
			}
			token = strtok(NULL, s);
			ctr++;

		}

	}
	printf("\nMyneighbours array is");
	//Put initial values to myneighbours
	for(i=0;i<nctr;i++)
	{
		printf("\n id %d ip %s port %s isneighbour %d",myneighbours[i].id,myneighbours[i].ip, myneighbours[i].port, myneighbours[i].isneighbour);
		if(myneighbours[i].isneighbour==1)//initially only neigbours will have nexthop as id
			nexthop[myneighbours[i].id -1]=myneighbours[i].id;
		else
			nexthop[myneighbours[i].id -1]=-1;
	}
	nexthop[myid-1]=myid;
	cost[myid-1][myid-1]=0;
	for(i=0;i<5;i++)
		direct_cost2[i]=direct_cost[i];
}

//step command executed
void input_step()

{

	form_packet(); //forms packet
	form_string(); //puts packet in string
	send_neighbour();//sends to neighbours
	cse4589_print_and_log("%s:SUCCESS\n", tokens_array[0]);
}

//dump command executed
void input_dump()

{



	form_packet();
	form_string();

	cse4589_dump_packet(rupdate, size_rupdate);
	cse4589_print_and_log("%s:SUCCESS\n", tokens_array[0]);


}

//update command executed
void input_update()
{

	int server1=atoi(tokens_array[1]);
	int server2=atoi(tokens_array[2]);
	int costu,nflag=0,i;

	if(server1!=myid)
	{
		cse4589_print_and_log("%s:%s\n", tokens_array[0],"First Id does not belong to this server");
		return;
	}
	for(i=0;i<nctr;i++)
	{

		if(myneighbours[i].id==server2)
		{
			if( (myneighbours[i].isneighbour==1)&&(myneighbours[i].sendneighbour==1))
			{

				nflag=1;
			}
		}

	}
	if(nflag==0)
	{

		cse4589_print_and_log("%s:%s\n", tokens_array[0],"Second Id does not belong to neigbour");
		return;
	}
	if((strcasecmp(tokens_array[3],"inf"))==0)
		costu=65535;
	else
		costu=atoi(tokens_array[3]);
	int cflag=0;
	if (costu>0&&costu<=65535)
		cflag=1;
	if(cflag==0)
	{
		cse4589_print_and_log("%s:%s\n", tokens_array[0],"Invalid cost");
		return;
	}
	direct_cost[server2-1]=costu;
	cse4589_print_and_log("%s:SUCCESS\n", tokens_array[0]);
	compute_distance();
	print_routing_table();

}

//packets command executed
void input_packets()
{
	cse4589_print_and_log("%s:SUCCESS\n", tokens_array[0]);
	cse4589_print_and_log("%d\n",noofpackets);

	noofpackets=0;
}


//disable command executed
void input_disable()
{
	int id=atoi(tokens_array[1]);
	int i,dflag=0;
	for(i=0;i<nctr;i++)
	{
		if(myneighbours[i].id==id)
		{
			myneighbours[i].sendneighbour=0;//should not send updates to neighbour
			direct_cost[id-1]=65535;//set to highest unsingned in value
			dflag=1;
		}

	}
	if(dflag==0)

		cse4589_print_and_log("%s:%s\n", tokens_array[0],"Id does not belong to neighbour");
	else
		cse4589_print_and_log("%s:SUCCESS\n", tokens_array[0]);


	compute_distance();
}

//crash command executed
void input_crash()

{

	while(1)

	{

	}

}

//display command executed

void input_display()

{


	struct node temp;
	int c,d;
	// for bubble sort used http://www.programmingsimplified.com/c/source-code/c-program-bubble-sort
	for (c = 0 ; c < ( nctr - 1 ); c++)
	{
		for (d = 0 ; d < nctr - c - 1; d++)
		{
			if (myneighbours[d].id > myneighbours[d+1].id)
			{
				temp       = myneighbours[d];
				myneighbours[d]   = myneighbours[d+1];
				myneighbours[d+1] = temp;
			}
		}
	}
	cse4589_print_and_log("%s:SUCCESS\n", tokens_array[0]);
	for(c=0;c<nctr;c++)
	{
		int destid=myneighbours[c].id;
		cse4589_print_and_log("%-15d%-15d%-15d\n", destid,nexthop[destid-1],cost[myid-1][destid-1]);

	}

}

//Parse input string and call appropriate function
void check_input()

{
	int argc=0;

	char s[2] = " ";
	char *token;
	token=(char *)malloc(strlen(cmd)*sizeof(char));

	token = strtok(cmd, s);


	while( token != NULL )
	{

		strcpy(tokens_array[argc],token);
		argc+=1;
		token = strtok(NULL, s);
	}
	//Last character of last string has \n
	int c=strlen(tokens_array[argc-1])-1;
	tokens_array[argc-1][c]='\0';

	if((strcasecmp(tokens_array[0],"dump"))==0)
		input_dump();
	else if((strcasecmp(tokens_array[0],"update"))==0)
		input_update();
	else if((strcasecmp(tokens_array[0],"packets"))==0)
		input_packets();

	else if((strcasecmp(tokens_array[0],"step"))==0)
		input_step();
	else if((strcasecmp(tokens_array[0],"disable"))==0)
		input_disable();
	else if((strcasecmp(tokens_array[0],"crash"))==0)
		input_crash();
	else if((strcasecmp(tokens_array[0],"display"))==0)
		input_display();
	else if((strcasecmp(tokens_array[0],"academic_integrity"))==0)
	{
		cse4589_print_and_log("%s:SUCCESS\n", tokens_array[0]);
		cse4589_print_and_log("I have read and understood the course academic integrity policy located at http://www.cse.buffalo.edu/faculty/dimitrio/courses/cse4589_f14/index.html#integrity");
	}
	else
		printf("\n Invalid command");


}
int main(int argc, char **argv)
{
	/*Init. Logger*/
	cse4589_init_log();

	/*Clear LOGFILE and DUMPFILE*/
	fclose(fopen(LOGFILE, "w"));
	fclose(fopen(DUMPFILE, "wb"));

	/*Start Here*/


	/*
	 * Parse the arguments
	 * http://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
also used code from gbn.cpp
	 */
	int opt,res,i,opt1=0,opt2=0;
	while((opt = getopt(argc, argv,"t:i:")) != -1){
		switch (opt){
		case 't':  strcpy(topology_file,optarg);
		printf("\nin t");
		opt1=1;
		break;

		case 'i':
			input_timeout = atoi(optarg);
			printf("\nin i");
			opt2=1;
			break;
		}
	}
	if(opt1==0)
	{
		printf("\n Topology file name was not given by -t");
		exit(1);
	}
	if(opt2==0)
	{
		printf("\n Timeout was not given by -i");
		exit(1);
	}

	read_file();


	fd_set masterlist,watchlist;
	FD_ZERO(&watchlist);

	FD_ZERO(&masterlist);
	int len=strlen(myport);
	if (myport[len-1]=='\n')
	{
		printf("\n newline detected");
		myport[len-1]='\0';
	}

	// To fix getaddrinfo error http://stackoverflow.com/questions/10291923/getaddrinfo-failing-with-error-servname-not-supported-for-ai-socktype-in-c
	int pp = atoi(myport);
	memset(myport,'\0',20);
	sprintf( myport, "%d", pp );
	res=create_socket();
	res=create_send_socket();
	FD_SET(STDIN, &masterlist);
	FD_SET(lssockfd, &masterlist);
	int head_socket = lssockfd,rv,sock_index,k;
	struct timeval sel_timeout_current;
	sel_timeout_current.tv_sec = input_timeout;
	sel_timeout_current.tv_usec = 0;
	while(1)
	{


		fflush(stdout);
		printf("\n [PA3]$ ");
		fflush(stdout);



		watchlist=masterlist;
		rv = select(head_socket+1, &watchlist, NULL, NULL, &sel_timeout_current);

		if (rv == -1) {
			perror("select"); // error occurred in select()
		}
		else if (rv==0)
		{

			printf("\nrv is 0");
			//Timeout so form packet, put in string and send
			form_packet();
			form_string();
			send_neighbour();
			for(k=0;k<nctr;k++)
			{
				if(myneighbours[k].isneighbour==1)
				{

					printf("\nIs neighbour: timeout is %d",myneighbours[k].timeout);
					if((myneighbours[k].timeout!=3) && (myneighbours[k].timeout!=-1))
					{
						myneighbours[k].timeout++;
						printf("\n Timeout incremented");
					}
					if(myneighbours[k].timeout==3)
					{
						if(myneighbours[k].id!=myid)

							direct_cost[myneighbours[k].id-1]=65535;
						printf("\n timeout of neighbour with id %d ",myneighbours[k].id);
						compute_distance();

					}

				}
			}
			sel_timeout_current.tv_sec = input_timeout;
			sel_timeout_current.tv_usec = 0;


		}
		else
		{
			for(sock_index=0; sock_index<=head_socket;sock_index+=1)
			{

				fflush(stdout);
				//  descriptors has data
				if (FD_ISSET(sock_index, &watchlist)) {
					if(sock_index==STDIN)//Command is given as input
					{

						memset(cmd,'\0',100);
						fgets (cmd, 100, stdin);
						fflush(stdout);
						printf("\n $$$ in stdin cmd is %s",cmd);
						check_input();
					}
					if (sock_index==lssockfd)//Received data at socket
					{
						printf("\n receiving packet");
						receivepacket();
						extract_string();
						compute_distance();
					}




				}
			}
		}

	}


	return 0;
}

