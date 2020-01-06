#include "udpcontrol.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>

#define BUFLEN 512
#define PORT 5005

char my_ip[20];
char dest_ip[20];
char buf[BUFLEN];
struct sockaddr_in sender_addr;
int sender_sockfd;
socklen_t slen=sizeof(sender_addr);
struct sockaddr_in receiver_addr, incoming_addr;
int receiver_sockfd;
socklen_t srlen=sizeof(incoming_addr);

void udpcontrol_setup(){
	
	if (getenv("GORDON")){
		strcpy(my_ip, "192.168.3.20");
		strcpy(dest_ip, "192.168.3.21");
	}
	else if (getenv("CHELL")){
		strcpy(my_ip, "192.168.3.21");
		strcpy(dest_ip, "192.168.3.20");
	}else{
		printf("Set Env Variable!\n");
		exit(1);
	}
	
	printf("UDP_Control : My IP: %s\n",my_ip);
	printf("UDP_Control : Dest IP: %s\n",dest_ip);
	//sending stuff
	printf("UDP_Control : Binding...\n");	
	if ((sender_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1){
		printf("UDP_Control: Sender bind() failure\n");
		exit(1);
	}
	else
	printf("UDP_Control : Sender Socket() successful\n");
	
	fcntl(sender_sockfd, F_SETFL, FNDELAY); //Set to nonblocking mode so send to wont block on a full UDP buffer both SENDING and REC
	int yes=1;
	//setsockopt(sender_sockfd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
	
	bzero(&sender_addr, sizeof(sender_addr));
	sender_addr.sin_family = AF_INET;
	sender_addr.sin_port = htons(PORT);

	if (inet_aton(dest_ip, &sender_addr.sin_addr)==0)
	{
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	//receive stuff
	if ((receiver_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1){
		printf("UDP_Control: receiver bind() failure\n");
		exit(1);
	}
	else
	printf("UDP_Control : Receiver Socket() successful\n");
	fcntl(receiver_sockfd, F_SETFL, FNDELAY);  //Set to nonblocking mode so send to wont block on a full UDP buffer both SENDING and REC
	bzero(&receiver_addr, sizeof(receiver_addr));
	receiver_addr.sin_family = AF_INET;
	receiver_addr.sin_port = htons(PORT);
	//receiver_addr.sin_addr.s_addr = htonl(dest_ip);
	if (inet_aton(my_ip, &receiver_addr.sin_addr)==0)
	{
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	setsockopt(receiver_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) ;
	
	if (bind(receiver_sockfd, (struct sockaddr* ) &receiver_addr, sizeof(receiver_addr))==-1){
		printf("UDP_Receive : bind() failure\n");
		exit(1);
	}
	else
	printf("UDP_Control : bind() successful\n");

}

int udp_send_state(int state, uint32_t offset){
	int n = sprintf (buf, "%d %d", state, offset);
	//n + 1 to include null terminator!
	return sendto(sender_sockfd, buf, n + 1, MSG_DONTWAIT, (struct sockaddr*)&sender_addr, srlen);
}

int udp_receive_state(int * state, uint32_t * offset){
	int n = recvfrom(receiver_sockfd, buf, BUFLEN, 0, (struct sockaddr*)&incoming_addr, &slen);
	if (n > 0){
		//check source of message
		if ( strcmp (inet_ntoa(incoming_addr.sin_addr) , dest_ip) == 0){
			
			buf[n] = '\0'; //make sure null terminator is installed
			//int ipbytes[4];
			//sscanf( inet_ntoa(incoming_addr.sin_addr), "%3u.%3u.%3u.%3u", &ipbytes[3], &ipbytes[2], &ipbytes[1], &ipbytes[0]);
			//printf("%d %d %d %d\n", ipbytes[0] , ipbytes[1] , ipbytes[2], ipbytes[3]);
			uint32_t temp_offset;
			int temp_state;
			sscanf( buf, "%d %d",&temp_state,&temp_offset);
			if (temp_offset > *offset){
				*offset = temp_offset;
				*state = temp_state;
			}else{
				printf("Going back in time?\n");
				return -1;
			}
		}
	}
	return n;
}