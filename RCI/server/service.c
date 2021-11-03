#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/*
	nc -u tejo.tecnico.ulisboa.pt 59000

	LST
*/


/*

WITHDRAW_START 131;1
WITHDRAW_DS 131;1

WITHDRAW_START 131;2
WITHDRAW_DS 131;2

*/

typedef struct infos{
	int x;
	int id;
	int id_next;
	char ip[128];
	int fd_cs;
	int upt;
	int tpt;
	struct sockaddr_in addr_cs;
	int fd_c;
	struct sockaddr_in addr_c;
	int fd_s_next;
	struct sockaddr_in addr_s_next;
	int fd_s_prev;
	int fd_s_wait;
}infos;

typedef struct states{
	int cs;
	bool leave;
	bool exit;
	bool in_ring;
	bool alone;
	bool dispatch;
	bool boot;
	bool available;
	bool ring_available;
	bool closed;
}states;

#define SET_DS 1
#define WITHDRAW_DS 2
#define SET_START 3
#define WITHDRAW_START 4
#define GET_START 5
#define LEAVE 6

#define TOKEN 1
#define NEW 2
#define NEW_START 3
#define PROPAGATE 4

#define max(A,B) ((A)>=(B)?(A):(B))


void initialize(infos *info, states *state);
void connect_s(int argc, char *argv[], infos *info);
void manage_fd(infos *info, states *state);
void process_commands_k(infos *info, states *state, char *command);
void process_commands_s(infos *info, states *state, char msg[]);
void process_commands_cs(infos *info, states *state, char msg[]);
void process_commands_c(infos *info, states *state);
void send_message_cs(infos *info, states *state, int mode, bool repeat);
void send_message_s(infos *info, int mode, char token, int id2, char ip2[], int tpt2);
int read_message_tcp(infos *info, char str[], bool repeat);



int main(int argc, char *argv[])
{
	infos info;
	states state;

	initialize(&info, &state);
	connect_s(argc, argv, &info);
	manage_fd(&info, &state);
	
	return 0;
}

void initialize(infos *info, states *state)
{
	state->cs = -1;
	state->leave = false;
	state->exit = false;
	state->in_ring = false;
	state->alone = true;
	state->dispatch = false;
	state->boot = false;
	state->available = true;
	state->ring_available = true;
	state->closed = false;

	info->x = -1;
	info->id = -1;
	info->id_next = -1;
	info->ip[0] = '\0';
	info->fd_cs = -1;
	info->upt = -1;
	info->tpt = -1;
	info->fd_c = -1;
	info->fd_s_next = -1;
	info->fd_s_prev = -1;
	info->fd_s_wait = -1;
}

void connect_s(int argc, char *argv[], infos *info)
{
	bool ib=false, pb=false;
	int i=0;
	int aux;
	struct hostent *hostptr;
	struct sockaddr_in addr;
	struct sockaddr_in addr_cs;

	struct in_addr *a;

	memset((void*)&addr_cs,(int)'\0', sizeof(addr_cs));

	for (i = 1; i < argc; i+=2)
	{
		if(strcmp("-n", argv[i]) == 0)
		{
			sscanf(argv[i+1], "%d", &(info->id));
		}
		else if(strcmp("-j", argv[i]) == 0)
		{
			strcpy(info->ip, argv[i+1]);
		}
		else if(strcmp("-u", argv[i]) == 0)
		{
			sscanf(argv[i+1], "%d", &aux);
			info->upt = aux;

			info->fd_c = socket(AF_INET,SOCK_DGRAM,0);

			memset((void*)&addr,(int)'\0',sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = htonl(INADDR_ANY);
			addr.sin_port = htons((u_short)aux);

			bind(info->fd_c,(struct sockaddr*)&addr, sizeof(addr));
		}
		else if(strcmp("-t", argv[i]) == 0)
		{
			sscanf(argv[i+1], "%d", &aux);
			info->tpt = aux;

			info->fd_s_wait = socket(AF_INET,SOCK_STREAM,0);

			memset((void*)&addr,(int)'\0', sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = htonl(INADDR_ANY);
			addr.sin_port = htons((u_short)aux);
			bind(info->fd_s_wait,(struct sockaddr*)&addr, sizeof(addr));

			listen(info->fd_s_wait, 5);
		}
		else if(strcmp("-i", argv[i]) == 0)
		{
			ib=true;
			inet_aton(argv[i+1], &(info->addr_cs.sin_addr));
		}
		else if(strcmp("-p", argv[i]) == 0)
		{
			pb=true;
			sscanf(argv[i+1], "%d", &aux);
			info->addr_cs.sin_port = htons((u_short)aux);
		}
	}

	if(!ib)
	{
		hostptr=gethostbyname("tejo.tecnico.ulisboa.pt");
		info->addr_cs.sin_addr.s_addr = ((struct in_addr*)(hostptr->h_addr_list[0]))->s_addr;
	}
	if(!pb)
	{
		info->addr_cs.sin_port = htons(59000);
	}

	info->fd_cs = socket(AF_INET,SOCK_DGRAM,0);
	info->addr_cs.sin_family = AF_INET;

	a=(struct in_addr*)hostptr->h_addr_list[0];
	printf("internet address: %s (%08lX)\n",inet_ntoa(*a),(long unsigned int)ntohl(a->s_addr));
}

void manage_fd(infos *info, states *state)
{
	fd_set rfds;
	int maxfd, counter;
	char command[128];
	struct sockaddr_in clientaddr;
	socklen_t addrlen = 0;
	int nread;
	int newfd;
	char msg[128];
	int aux;

	while(1)
	{
		FD_ZERO(&rfds);

		FD_SET(STDIN_FILENO,&rfds);
		maxfd = STDIN_FILENO;

		if(info->fd_s_prev != -1)
		{
			FD_SET(info->fd_s_prev,&rfds);
			maxfd = max(maxfd, info->fd_s_prev);
		}

		FD_SET(info->fd_s_wait,&rfds);
		maxfd = max(maxfd, info->fd_s_wait);

		FD_SET(info->fd_cs,&rfds);
		maxfd = max(maxfd, info->fd_cs);

		FD_SET(info->fd_c,&rfds);
		maxfd = max(maxfd, info->fd_c);

		//printf("Waiting for messages...  next id: %d\n", info->id_next);
		counter=select(maxfd+1,&rfds,(fd_set*)NULL,(fd_set*)NULL,(struct timeval *)NULL);

		if(counter<=0)
		{
			perror("select");
			exit(1);//error
		}
		if(FD_ISSET(STDIN_FILENO,&rfds))
		{
			fgets(command, sizeof(command), stdin);
			//printf("KB: %s", command);
			process_commands_k(info, state, command);
		}
		if(info->fd_s_prev != -1 && FD_ISSET(info->fd_s_prev,&rfds))
		{
			aux = read_message_tcp(info, msg, false);
			printf("RECV_S: %s", msg);
			process_commands_s(info, state, msg);

			while(aux != 0)
			{
				aux = read_message_tcp(info, msg, true);
				printf("RECV_S: %s", msg);
				process_commands_s(info, state, msg);
			}
		}
		if(FD_ISSET(info->fd_s_wait,&rfds))
		{
			printf("Server accepted!\n");
			addrlen=sizeof(clientaddr);
			if((newfd=accept(info->fd_s_wait,(struct sockaddr*)&clientaddr,&addrlen))==-1)
			{
				perror("manage_fd");
				exit(1);
			}
			if (info->fd_s_prev != -1)
			{
				close(info->fd_s_prev);
			}
			info->fd_s_prev = newfd;
		}
		if(FD_ISSET(info->fd_cs,&rfds))
		{
			nread = recvfrom(info->fd_cs, command, sizeof(command),0,(struct sockaddr*)&clientaddr,&addrlen);
			command[nread] = '\0';
			printf("RECV_CS: %s\n", command);
			process_commands_cs(info, state, command);
		}
		if(FD_ISSET(info->fd_c,&rfds))
		{
			process_commands_c(info, state);
		}

		if(state->closed)
		{
			printf("Connections all closed!\n");
			if(state->exit)
			{
				break;
			}
			state->leave = false;
			state->closed = false;
		}
	}
}

void process_commands_k(infos *info, states *state, char *command)
{
	if(command[0]== 'j' && command[1]== 'o' && command[2]== 'i' && command[3]== 'n')
	{
		sscanf(command, "join %d", &(info->x));

		send_message_cs(info, state, GET_START, false);
	}
	else if(strcmp(command, "show_state\n") == 0)
	{
		printf("******************STATE******************\n");
		printf("Service available:%s\n", state->available ? "true" : "false");
		printf("Ring available:%s\n", state->ring_available ? "true" : "false");
		printf("Successor id:%d\n", info->id_next);
		printf("Dispatch server:%s\n", state->dispatch ? "true" : "false");
		printf("Boot server:%s\n", state->boot ? "true" : "false");
		printf("*****************************************\n");
	}
	else if(strcmp(command, "leave\n") == 0)
	{
		state->leave = true;

		if(state->boot)
		{
			send_message_cs(info, state, WITHDRAW_START, false);
		}
		else if(state->dispatch)
		{
			send_message_cs(info, state, WITHDRAW_DS, false);
		}
		else if(state->alone == false)
		{
			send_message_s(info, TOKEN, 'O', info->id_next, inet_ntoa(info->addr_s_next.sin_addr), ntohs(info->addr_s_next.sin_port));
		}
		else
		{
			state->closed = true;
		}
	}
	else if(strcmp(command, "exit\n") == 0)
	{
		state->leave = true;
		state->exit = true;

		if(state->boot)
		{
			send_message_cs(info, state, WITHDRAW_START, false);
		}
		else if(state->dispatch)
		{
			send_message_cs(info, state, WITHDRAW_DS, false);
		}
		else if(state->alone == false)
		{
			send_message_s(info, TOKEN, 'O', info->id_next, inet_ntoa(info->addr_s_next.sin_addr), ntohs(info->addr_s_next.sin_port));
		}
		else
		{
			state->closed = true;
		}
	}
	else
	{
		printf("Wrong command.\n");
	}
}

void process_commands_s(infos *info, states *state, char msg[])
{
	int id, id2, tpt;
	int aux1, aux2, aux3, aux4;
	char ip[128];
	char type;
	int fd;
	int n;

	if(msg[0]=='T'&&msg[1]=='O'&&msg[2]=='K'&&msg[3]=='E'&&msg[4]=='N')
	{
		sscanf(msg, "TOKEN %d;%c;%d;%d.%d.%d.%d;%d", &id, &type, &id2, &aux1, &aux2, &aux3, &aux4, &tpt);
		
		if(type=='S')
		{
			if(id == info->id)
			{
				state->ring_available = false;
				send_message_s(info, TOKEN, 'I', 0, "", 0);
				//send_message_cs(info, state, SET_DS, false);
			}
			else
			{
				if(state->available)
				{
					sprintf(msg, "TOKEN %d;T\n", id);
					send_message_s(info, PROPAGATE, '_', 0, msg, 0);
					send_message_cs(info, state, SET_DS, false);
				}
				else
				{
					send_message_s(info, PROPAGATE, '_', 0, msg, 0);
				}
			}
		}
		else if(type=='T')
		{
			if(id != info->id)
			{
				send_message_s(info, PROPAGATE, '_', 0, msg, 0);
			}
			else if(state->leave)
			{
				send_message_s(info, TOKEN, 'O', info->id_next, inet_ntoa(info->addr_s_next.sin_addr), ntohs(info->addr_s_next.sin_port));
			}
		}
		else if(type=='I')
		{
			if(id != info->id)
			{
				state->ring_available = false;
				send_message_s(info, PROPAGATE, '_', 0, msg, 0);
			}
			else if(state->leave)
			{
				send_message_s(info, TOKEN, 'O', info->id_next, inet_ntoa(info->addr_s_next.sin_addr), ntohs(info->addr_s_next.sin_port));
			}
		}
		else if(type=='D')
		{
			if(id != info->id)
			{
				state->ring_available = true;
				send_message_s(info, PROPAGATE, '_', 0, msg, 0);
			}
		}
		else if(type=='N')
		{
			sprintf(ip, "%d.%d.%d.%d", aux1, aux2, aux3, aux4);
			if(id == info->id_next)
			{
				info->id_next = id2;
				
				fd = socket(AF_INET,SOCK_STREAM,0);

				memset((void*)&(info->addr_s_next),(int)'\0',sizeof(info->addr_s_next));
				info->addr_s_next.sin_family=AF_INET;
				inet_aton(ip, &(info->addr_s_next.sin_addr));
				info->addr_s_next.sin_port=htons((ushort)tpt);

				n=connect(fd, (struct sockaddr*)&(info->addr_s_next),sizeof(info->addr_s_next));
				if(n ==-1)
					perror("connect");

				close(info->fd_s_next);
				info->fd_s_next = fd;

				if(state->ring_available == false)
				{
					state->ring_available = true;
					send_message_s(info, TOKEN, 'S', 0, msg, 0);
					send_message_s(info, TOKEN, 'D', 0, msg, 0);
				}
			}
			else
			{
				send_message_s(info, PROPAGATE, '_', 0, msg, 0);
			}
		}
		else if(type=='O')
		{
			sprintf(ip, "%d.%d.%d.%d", aux1, aux2, aux3, aux4);
			if(id == info->id)
			{
				if(state->exit)
				{
					close(info->fd_s_wait);
					close(info->fd_cs);
					close(info->fd_c);

					info->fd_s_wait = -1;
					info->fd_cs = -1;
					info->fd_c = -1;
				}
				close(info->fd_s_next);
				close(info->fd_s_prev);
				
				info->fd_s_next = -1;
				info->fd_s_prev = -1;
				state->closed = true;
			}
			else if(id2 == info->id && id == info->id_next)
			{
				//there is no successor because there are only two servers
				//and one leaves
				send_message_s(info, PROPAGATE, '_', 0, msg, 0);
				close(info->fd_s_next);
				close(info->fd_s_prev);
				info->fd_s_next = -1;
				info->fd_s_prev = -1;
				state->alone = true;
			}
			else if(id == info->id_next)
			{
				fd = socket(AF_INET,SOCK_STREAM,0);

				memset((void*)&(info->addr_s_next),(int)'\0',sizeof(info->addr_s_next));
				info->addr_s_next.sin_family=AF_INET;
				inet_aton(ip, &(info->addr_s_next.sin_addr));
				info->addr_s_next.sin_port=htons((ushort)tpt);

				n=connect(fd, (struct sockaddr*)&(info->addr_s_next),sizeof(info->addr_s_next));
				if(n ==-1)
					perror("connect:");

				send_message_s(info, PROPAGATE, '_', 0, msg, 0);

				if(close(info->fd_s_next)<0)
				{
					perror("close");
					exit(1);
				}
				info->fd_s_next = fd;

				info->id_next = id2;
			}
			else
			{
				send_message_s(info, PROPAGATE, '_', 0, msg, 0);
			}
		}
	}
	else if(msg[0]=='N'&&msg[1]=='E'&&msg[2]=='W'&&msg[3]=='_'&&msg[4]=='S'&&msg[5]=='T'&&msg[6]=='A'&&msg[7]=='R'&&msg[8]=='T')
	{
		state->boot = true;
		send_message_cs(info, state, SET_START, false);
	}
	else if(msg[0]=='N'&&msg[1]=='E'&&msg[2]=='W')
	{
		sscanf(msg, "NEW %d;%d.%d.%d.%d;%d", &id, &aux1, &aux2, &aux3, &aux4, &tpt);
		sprintf(ip, "%d.%d.%d.%d", aux1, aux2, aux3, aux4);

		if(state->alone)
		{
			state->alone = false;

			info->id_next = id;

			info->fd_s_next = socket(AF_INET, SOCK_STREAM, 0);

			memset((void*)&(info->addr_s_next),(int)'\0',sizeof(info->addr_s_next));
			info->addr_s_next.sin_family=AF_INET;
			inet_aton(ip, &(info->addr_s_next.sin_addr));
			info->addr_s_next.sin_port=htons((ushort)tpt);

			n=connect(info->fd_s_next,(struct sockaddr*)&(info->addr_s_next),sizeof(info->addr_s_next));
			if(n==-1)
			{
				perror("process_commands_s: ");
				exit(1);//error
			}

			if(state->ring_available == false)
			{
				state->ring_available = true;
				send_message_s(info, TOKEN, 'S', 0, msg, 0);
				send_message_s(info, TOKEN, 'D', 0, msg, 0);
			}
		}
		else
		{
			send_message_s(info, TOKEN, 'N', id, ip, tpt);
		}
	}
}

void process_commands_cs(infos *info, states *state, char msg[])
{
	int id, id2, tpt;
	int aux1, aux2, aux3, aux4;
	char ip[128];
	int n;
	int addrlen;
	char str[128];

	addrlen = sizeof(info->addr_c);

	if(state->cs == SET_DS)
	{
		state->dispatch = true;
		state->cs = -1;
		if(state->alone && state->boot == false)
		{
			send_message_cs(info, state, SET_START, false);
		}
		else if(state->alone == false && state->ring_available == false)
		{
			state->ring_available = true;
			send_message_s(info, TOKEN, 'D', 0, "", 0);
		}
	}
	else if(state->cs == WITHDRAW_DS)
	{
		state->dispatch = false;
		state->cs = -1;

		if(state->available == false)
		{
			strcpy(str, "YOUR_SERVICE ON");
			sendto(info->fd_c, str, strlen(str)+1, 0,(struct sockaddr*)&(info->addr_c),addrlen);
			printf("SENT_C: %s\n", msg);
			if(state->alone)
			{
				state->ring_available=false;
			}
		}

		if(state->leave == true)
		{
			if(state->alone==false)
			{
				send_message_s(info, TOKEN, 'S', 0, "", 0);
			}
			else
			{
				state->closed = true;
			}
		}
	}
	else if(state->cs == SET_START)
	{
		state->boot = true;
		state->cs = -1;
	}
	else if(state->cs == WITHDRAW_START)
	{
		state->boot = false;
		state->cs = -1;

		if(state->alone==false)
			send_message_s(info, NEW_START, '_', 0, "", 0);
		
		if(state->leave == true && state->dispatch == true)
		{
			send_message_cs(info, state, WITHDRAW_DS, false);
		}
		else if(state->leave)
		{
			state->closed = true;
		}
	}
	else if(state->cs == GET_START)
	{
		sscanf(msg, "OK %d;%d;%d.%d.%d.%d;%d", &id, &id2, &aux1, &aux2, &aux3, &aux4, &tpt);

		if(id2==0&&aux1==0&&aux2==0&&aux3==0&&aux4==0&&tpt==0)
		{
			state->alone = true;
			send_message_cs(info, state, SET_DS, false);
		}
		else
		{
			state->alone = false;

			sprintf(ip, "%d.%d.%d.%d", aux1, aux2, aux3, aux4);
			info->fd_s_next = socket(AF_INET, SOCK_STREAM, 0);

			info->id_next = id2;

			memset((void*)&(info->addr_s_next),(int)'\0',sizeof(info->addr_s_next));
			info->addr_s_next.sin_family=AF_INET;
			inet_aton(ip, &(info->addr_s_next.sin_addr));
			info->addr_s_next.sin_port=htons((ushort)tpt);

			n=connect(info->fd_s_next,(struct sockaddr*)&(info->addr_s_next),sizeof(info->addr_s_next));
			if(n==-1)
			{
				perror("process_commands_cs: ");
				exit(1);//error
			}
			send_message_s(info, NEW, '_', 0, "", 0);
		}
	}
}

void process_commands_c(infos *info, states *state)
{
	char mode[128];
	socklen_t addrlen;
	char msg[128];
	int len;

	addrlen = sizeof(info->addr_c);

	len = recvfrom(info->fd_c, msg, sizeof(msg),0,(struct sockaddr*)&info->addr_c,&addrlen);
	msg[len] = '\0';
	printf("C:%s\n", msg);
	if(sscanf(msg, "MY_SERVICE %s", mode))
	{
		if(strcmp(mode, "ON") == 0)
		{
			if(state->dispatch)
			{
				state->dispatch=false;
				state->available=false;
				send_message_cs(info, state, WITHDRAW_DS, false);
				if(state->alone==false)
				{
					send_message_s(info, TOKEN, 'S', 0, "", 0);
				}
			}
		}
		else
		{
			state->available=true;
			strcpy(msg, "YOUR_SERVICE OFF");
			sendto(info->fd_c, msg, strlen(msg)+1, 0,(struct sockaddr*)&(info->addr_c),addrlen);
			printf("SENT_C: %s\n", msg);
			if(state->ring_available == false)
			{
				send_message_cs(info, state, SET_DS, false);
			}
		}
	}
}

void send_message_cs(infos *info, states *state, int mode, bool repeat)
{
	static int mode_;
	char msg[128];
	socklen_t addrlen;

	addrlen = sizeof(info->addr_cs);

	if(!repeat)
	{
		mode_ = mode;
	}

	if(mode == SET_DS || (repeat && mode_ == SET_DS))
	{
		state->cs=SET_DS;
		sprintf(msg, "SET_DS %d;%d;%s;%d", info->x, info->id, info->ip, info->upt);
		sendto(info->fd_cs, msg, strlen(msg)+1, 0,(struct sockaddr*)&(info->addr_cs), addrlen);
	}
	else if(mode == WITHDRAW_DS || (repeat && mode_ == WITHDRAW_DS))
	{
		state->cs=WITHDRAW_DS;
		sprintf(msg, "WITHDRAW_DS %d;%d", info->x, info->id);
		sendto(info->fd_cs, msg, strlen(msg)+1, 0,(struct sockaddr*)&(info->addr_cs), addrlen);
	}
	else if(mode == SET_START || (repeat && mode_ == SET_START))
	{
		state->cs=SET_START;
		sprintf(msg, "SET_START %d;%d;%s;%d", info->x, info->id, info->ip, info->tpt);
		sendto(info->fd_cs, msg, strlen(msg)+1, 0,(struct sockaddr*)&(info->addr_cs), addrlen);
	}
	else if(mode == WITHDRAW_START || (repeat && mode_ == WITHDRAW_START))
	{
		state->cs=WITHDRAW_START;
		sprintf(msg, "WITHDRAW_START %d;%d", info->x, info->id);
		sendto(info->fd_cs, msg, strlen(msg)+1, 0,(struct sockaddr*)&(info->addr_cs), addrlen);
	}
	else if(mode == GET_START || (repeat && mode_ == GET_START))
	{
		state->cs=GET_START;
		sprintf(msg, "GET_START %d;%d", info->x, info->id);
		sendto(info->fd_cs, msg, strlen(msg)+1, 0,(struct sockaddr*)&(info->addr_cs), addrlen);
	}

	printf("SENT_CS: %s\n", msg);
}

void send_message_s(infos *info, int mode, char token, int id2, char ip2[], int tpt2)
{
	int nleft, nwritten;
	char *ptr, msg[128];

	if(mode == TOKEN)
	{
		if(token == 'N' || token == 'O')
		{
			//send_message_s(info, TOKEN, 'O', info->id_next, inet_ntoa(info->addr_s_next.sin_addr), ntohs(info->addr_s_next.sin_port));
			sprintf(msg, "TOKEN %d;%c;%d;%s;%d\n",info->id, token, id2, ip2, tpt2);
			ptr = msg;
			nleft=strlen(msg);
			while(nleft>0)
			{
				nwritten=write(info->fd_s_next,ptr,nleft);
				if(nwritten<=0)exit(1);//error
				nleft-=nwritten;
				ptr+=nwritten;
			}
		}
		else
		{
			sprintf(msg, "TOKEN %d;%c\n",info->id, token);
			ptr = msg;
			nleft=strlen(msg);
			while(nleft>0)
			{
				nwritten=write(info->fd_s_next,ptr,nleft);
				if(nwritten<=0)exit(1);//error
				nleft-=nwritten;
				ptr+=nwritten;
			}
		}
	}
	else if(mode == NEW)
	{
		sprintf(msg, "NEW %d;%s;%d\n", info->id, info->ip, info->tpt);
		ptr = msg;
		nleft=strlen(msg);
		while(nleft>0)
		{
			nwritten=write(info->fd_s_next,ptr,nleft);
			if(nwritten<=0)exit(1);//error
			nleft-=nwritten;
			ptr+=nwritten;
		}
	}
	else if(mode == NEW_START)
	{
		sprintf(msg, "NEW_START\n");
		ptr = msg;
		nleft=strlen(msg);
		while(nleft>0)
		{
			nwritten=write(info->fd_s_next,ptr,nleft);
			if(nwritten<=0)exit(1);//error
			nleft-=nwritten;
			ptr+=nwritten;
		}
	}
	else if(mode == PROPAGATE)
	{
		ptr = ip2;
		nleft=strlen(ip2);
		while(nleft>0)
		{
			nwritten=write(info->fd_s_next,ptr,nleft);
		
			if(nwritten<=0)
			{
				perror("send_message_s:PROPAGATE");
				exit(1);
			}
			nleft-=nwritten;
			ptr+=nwritten;
		}
	}


	if (mode==PROPAGATE)
	{
		printf("SENT_S: %s", ip2);
	}
	else
	{
		printf("SENT_S: %s", msg);
	}
}


int read_message_tcp(infos *info, char str[], bool repeat)
{
	static char msg[512] = "";
	static char *ptr=&(msg[0]);
	static int cnt=0;
	int nread=0;

	int i,j;

	while(!repeat)
	{
		nread=read(info->fd_s_prev,ptr,64);
		if(nread==-1)exit(1);//error
		ptr+=nread;
		cnt+=nread;
		if(*(ptr-1) == '\n')
			break;
		if(cnt > 440)
			break;
	}
	for(i = 0; i < cnt; i++)
	{
		str[i] = msg[i];
		if(msg[i] == '\n')
		{
			str[i+1] = '\0';
			break;
		}
	}
	if(i == cnt)
	{
		str[0] = '\n';
	}
	else
	{
		i++;
		for(j=i; j < cnt; j++)
		{
			msg[j-i] = msg[j];
		}
		msg[j] = '\0';
		cnt-=i;
		ptr-=i;
	}
	if(*(ptr-1) == '\n')
		return 1;
	return 0;
}



