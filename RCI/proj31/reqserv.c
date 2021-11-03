#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>

#define BUFFER_SIZE 100
#define max(A,B) ((A)>=(B)?(A):(B))

void connect_central_server(int argc, char *argv[], struct sockaddr_in *addr);

void get_dispatch_server(struct sockaddr_in addr, struct sockaddr_in *addr_disp, char* buffer, int fd);

void connect_dispatch_server(char* buffer, struct sockaddr_in *addr_disp, int fd);

void sendd(char *buffer, struct sockaddr_in addr, int fd);

void receive(char *buffer, struct sockaddr_in addr, int fd);

void send_receive(struct sockaddr_in addr, int fd);

int main(int argc, char *argv[])
{
	int fd;
	char buffer[BUFFER_SIZE];
	struct sockaddr_in addr;
	struct sockaddr_in addr_disp;
	fd=socket(AF_INET,SOCK_DGRAM,0);
	if(fd==-1)
	{
		printf("Impossible to open socket\n");
		exit(EXIT_FAILURE);
	}
//Comunicação com o servidor central
	connect_central_server(argc, argv, &addr);
	while(1)
	{
		get_dispatch_server(addr, &addr_disp, buffer, fd);
	//Comunicação com o servidor de despacho
		connect_dispatch_server(buffer, &addr_disp, fd);
		send_receive(addr_disp, fd);
		
	}
	close(fd);	
	return 0;
}

/*********************************************************************
++ connect_central_server ++

	Dados os argumentos de entrada, a função abre uma ligação UDP com o
servidor central.
	O endereco IP do servidor central e o porto estão definidos no
argumento argv. Caso um deles, ou os dois, nao esteja especificado,
então é assumido o default
	Os argumentos default são IP correspondente à máquina
tejo.tecnico.ulisboa.pt e porto 59000.

Argumentos:
	argc
	argv
	addr estrutura onde vão ser armazenados os dados do servidor central

*********************************************************************/

void connect_central_server(int argc, char *argv[], struct sockaddr_in *addr)
{
	int i=1;
	char a[]="-i";
	char b[]="-p";
	struct hostent *h;
	struct in_addr *s;

	memset((void*)addr,(int)'\0',sizeof(*addr));

		if((h=gethostbyname("tejo.tecnico.ulisboa.pt"))==NULL)exit(1);//error
		s=(struct in_addr*)h->h_addr_list[0];
		addr->sin_addr= *s;
		addr->sin_port=htons((u_short)59000);
		addr->sin_family=AF_INET;
		//printf("IP: %s (%08lX)\n",inet_ntoa(addr->sin_addr),(long unsigned int)ntohl(addr->sin_addr.s_addr));
		

	for(i=1; i<argc; i++)
	{
		if (strcmp(argv[i],a)==0)
		{
			if(0==inet_aton(argv[i+1],&(addr->sin_addr)))
			{
				printf("Erro na conversão do endereço IP em argumento\n");
			}
			//printf("IP: %s (%08lX)\n",inet_ntoa(addr.sin_addr),(long unsigned int)ntohl(addr.sin_addr.s_addr));
		}

		//Recebe o endereço 
		if (strcmp(argv[i],b)==0)
		{
			addr->sin_port=htons((u_short)atoi(argv[i+1]));	
			//printf("Porto UDP:%d\n",addr.sin_port);
		}
		
	}
	/*
	printf("IP: %s (%08lX)\n",inet_ntoa(addr->sin_addr),(long unsigned int)ntohl(addr->sin_addr.s_addr));
	printf("Porto UDP:%d\n",addr->sin_port);
	*/
}

/*********************************************************************
++ get_dispatch_server ++
	Funcão que 

Argumentos:

Return:

*********************************************************************/

void get_dispatch_server( struct sockaddr_in addr, struct sockaddr_in *addr_disp, char* buffer, int fd)
{	
	int stop=1;
	int service_id=0;

	while(stop)
	{
		printf("Hello, how may i help you?\n");
		fgets(buffer, BUFFER_SIZE, stdin);
			if(1==sscanf(buffer, "rs %d" ,&service_id))
			{
				stop=0;	
			}
			else if(strcmp(buffer,"exit\n")==0)
			{
				close(fd);
				exit(0);
			}
			else
			{
				printf("Invalid arguments:\n");
			}
	}
	//Pedido do servidor de despacho ao servidor central
	sprintf(buffer,"GET_DS_SERVER %d",service_id);
	sendd(buffer, addr, fd);
	//Resposta do servidor central
	receive(buffer, addr, fd);
	//printf("Resposta do Sevidor central:%s\n",buffer);

}

/*********************************************************************
++ connect_dispatch_server ++

	Depois de receber o endereço IP e o porto, esta função estabelece
uma ligação UDP com o servidor de despacho.
	Esta função funciona de forma muito semelhante à função

Argumentos:
	addr_disp,
	fd,
 
*********************************************************************/

void connect_dispatch_server(char* buffer, struct sockaddr_in *addr_disp, int fd)
{
	char ip_dispatch[20];
	int ip1=0, ip2=0, ip3=0, ip4=0;
	int id=0,upt=0;

	sscanf(buffer,"OK %d;%d.%d.%d.%d;%d", &id, &ip1, &ip2, &ip3, &ip4, &upt);
	if (id==0&&ip1==0&&ip2==0)
	{
		printf("Servidores indisponiveis.\n");
		exit(0);
	}

	sprintf(ip_dispatch, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);

	addr_disp->sin_port=htons((u_short)upt);
	inet_aton(ip_dispatch,&(addr_disp->sin_addr));
	addr_disp->sin_family=AF_INET;

	sprintf(buffer,"MY_SERVICE ON");
	sendd(buffer, *addr_disp , fd);
	receive(buffer, *addr_disp, fd);
	printf("***************%s***************\n",buffer);

}

/*********************************************************************
++sendd++
Função que envia uma string para um servidor por uma ligação UDP

Argumentos: 
	buffer, string com a mensagem a enviar.
	addr, estrutura com os dados do servidor para o qual vamos enviar a mensagem.
	fd, socket por onde se vai enviar a mensagem udp


*********************************************************************/

void sendd(char *buffer, struct sockaddr_in addr, int fd)
{
	int n,mlen;
	mlen= strlen(buffer)+1;
	
		if(fd==-1)
		{
			printf("Impossible to open socket\n");
			exit(EXIT_FAILURE);
		}	
	n=sendto(fd,buffer,mlen,0,(struct sockaddr*)&addr,sizeof(addr));
		if(n==-1)
		{
			printf("Impossible to send message\n");
			exit(EXIT_FAILURE);
		}	

}

/*********************************************************************
++receive++
Função que recebe uma string para um servidor por uma ligação UDP

Argumentos: 
	buffer, string onde se vai armazenar a mensagem recebida.
	addr, estrutura com os dados do servidor de onde vem a mensagem que se vai receber.
	fd, socket por onde se vai receber a mensagem udp

*********************************************************************/

void receive(char *buffer, struct sockaddr_in addr, int fd)
{	
	int n;
	socklen_t addrlen;

	addrlen=sizeof(addr);
	n=recvfrom(fd,buffer,100,0,(struct sockaddr*)&addr,&addrlen);
		if(n==-1)
		{
			printf("Impossible to receive message\n");
			exit(EXIT_FAILURE);
		}
	buffer[n]='\0';	
}

/*********************************************************************
++ send_receive ++
	Função que gere o envio e recepção de mensagens para o servidor
de despacho.
	Isto é feito com a função select de modo a que o programa possa
monitorizar a recepção de datagrams e a escrita no teclado em
simultâneo, evitando assim o bloqueio do programa.
	Argumentos:
	addr, estrutura onde estão armazenados os dados referentes ao servidor de despacho.
	fd, socket por onde se vai receber a mensagem udp
*********************************************************************/

void send_receive(struct sockaddr_in addr, int fd)
{

	fd_set rfds;
	int maxfd, counter;
	char buffer[BUFFER_SIZE];
	
	while(1)
	{
		FD_ZERO(&rfds);


		FD_SET(fd,&rfds);
		maxfd=fd;
		
		FD_SET(STDIN_FILENO,&rfds);
		maxfd = STDIN_FILENO;


		counter=select(maxfd+1,&rfds, (fd_set*)NULL,(fd_set*)NULL,(struct timeval *)NULL);
			if(counter<=0)exit(1);//errror

		if(FD_ISSET(fd,&rfds))
		{
			//para já nada, possivél heartbeat
		}
		if(FD_ISSET(STDIN_FILENO,&rfds))
		{
			fgets(buffer, BUFFER_SIZE, stdin);

			if(strcmp(buffer,"ts\n")==0)
			{
				sprintf(buffer,"MY_SERVICE OFF");
				sendd(buffer, addr, fd);
				receive(buffer, addr, fd);
				if (strcmp(buffer,"YOUR_SERVICE OFF")==0)
					break;
				else
				{
					printf("Não foi recebida a mensagem certa de saida do servidor de despacho.");
					break;
				}
			}
			else if(strcmp(buffer,"exit\n")==0)
			{
				printf("Exiting\n");
				sprintf(buffer,"MY_SERVICE OFF");
				sendd(buffer, addr, fd);
				receive(buffer, addr, fd);
				if (strcmp(buffer,"YOUR_SERVICE OFF")==0)
				{
					close(fd);
					exit(0);
				}	
				else
				{
					printf("Erro mensagem de terminação não recebida");
				}
			}
			else
			{
				printf("Invalid arguments:\n");
			}
		}
	}
}


