#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#include "config.h"

void parse_arguments(int argc, char *argv[]);
void* thread_function(void* arg);
void error(const char *fun_name);
void exit_handler(int signo);
void atexit_function();

int local_flag = 1;
int server_port;
int client_port;
char server_path[CLIENT_NAME_LENGTH + 2];
char* ip;
int socket_fd;
pthread_t thread;
int thread_is_alive = 1;
char msg[MAX_MSG_LENGHT];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
char client_name[CLIENT_NAME_LENGTH];
void init_client();

int main(int argc, char *argv[])
{
	atexit(atexit_function);

	if(signal(SIGTSTP, exit_handler) == SIG_ERR)
		error("signal()");

	printf("\nParsing arguments");
	parse_arguments(argc, argv); //EDIT
	printf("\t\t\t\t\033[32m[ OK ]\033[0m\n");
	
	printf("Client's name:\t%s\n", client_name);
	init_client();
	// Odpalanie watku pisarza
	if(pthread_create(&thread, NULL, thread_function, NULL) == -1)
		error("pthread_create()");

	// Rejestracja klienta na serwerze
	request_t request;
	strcpy(request.name, client_name);
	request.action = ADD_USER;

	printf("Sending registration request");
	pthread_mutex_lock(&mutex);

		if(send(socket_fd, (void*) &request, sizeof(request), 0) == -1)
			error("send()");
		printf("\t\t\t\033[32m[ OK ]\033[0m\n");
	pthread_mutex_unlock(&mutex);
	printf("\nClient registered\n");
	request_t response;
	while(1) {
		//OBSLUGA GRY //EDIT
		if(recv(socket_fd, (void*) &response, sizeof(response), 0) == -1)
			error("recv()");
		if(response.action == MSG_FROM_SERVER)
		printf("\n%s: %s\n",response.name,response.msg);	
	}
	exit(EXIT_SUCCESS);
}

void parse_arguments(int argc, char *argv[]) {
	if(argc < 2) {
		printf("\n1Invalid arguments! Usage:\n\t%s\t [name] [unix|inet]  [[path] | [ip] [port]]\n\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	strcpy(client_name, argv[1]);
	if(strcmp(argv[2], "unix") == 0) {
		if(argc != 4) {
			printf("\n2Invalid arguments! Usage:\n\t%s\t[name] [unix|inet]  [[path] | [ip] [port]]\n\n", argv[0]);
			exit(EXIT_FAILURE);
		}
		local_flag = 1;
		strcpy(server_path, argv[3]);
	}
	else if(strcmp(argv[2], "inet") == 0) {
		if(argc != 5) {
			printf("\n3Invalid arguments! Usage:\n\t%s\t[name] [unix|inet]  [[path] | [ip] [port]]\n\n", argv[0]);
			exit(EXIT_FAILURE);
		}
		local_flag = 0;
		ip= argv[3];
		server_port = atoi(argv[4]);		
	}
	else {
		printf("\nInvalid arguments! Second argument should be 'unix' or 'inet'\n\n");
		exit(EXIT_FAILURE);
	}
}

void* thread_function(void* arg) {
	request_t response;
	response.action = MSG_TO_SERVER;
	strcpy(response.name,client_name);
	char bufor[MAX_MSG_LENGHT];

	while(thread_is_alive) {
		printf("\t Ja:");
		scanf("%s",bufor);
		if(strcmp(bufor,"quit") == 0){
		response.action = UNREGISTER;
		thread_is_alive = 0;
		}else{
			strcpy(response.msg,bufor);
		}
		pthread_mutex_lock(&mutex);
			if(send(socket_fd, (void*) &response, sizeof(response), 0) == -1)
				error("thread_function() --> send()");
		pthread_mutex_unlock(&mutex);
	}
	kill(getpid(),SIGTSTP);
	return NULL;
}


void init_client(){

	printf("Creating socket");
	if(local_flag) {
		if((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
			error("socket()");
	}
	else {
		if((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
			error("socket()");
	}
	printf("\t\t\t\t\t\033[32m[ OK ]\033[0m\n");

	struct sockaddr_un	server_unix_address;
	struct sockaddr_in	server_inet_address;
	struct sockaddr* server_address;

	socklen_t unix_address_size = sizeof(struct sockaddr_un);
	socklen_t inet_address_size = sizeof(struct sockaddr_in);
	socklen_t address_size;

	memset(&server_unix_address, 0, sizeof(server_unix_address));
	memset(&server_inet_address, 0, sizeof(server_inet_address));

	if(local_flag) {
		// Ustawiamy adresy dla połączenia lokalnego
		printf("Setting up server adress");
		server_unix_address.sun_family = AF_UNIX;
		strcpy(server_unix_address.sun_path, server_path);
		server_address = (struct sockaddr*) &server_unix_address;
		printf("\t\t\t\033[32m[ OK ]\033[0m\n");

		address_size = unix_address_size;
	}
	else {
		// Ustawiamy adresy dla połączenia zdalnego
		printf("Setting up server adress");
		server_inet_address.sin_family = AF_INET;
		inet_pton(AF_INET, ip, &(server_inet_address.sin_addr.s_addr));
		server_inet_address.sin_port = htons(server_port);
		server_address = (struct sockaddr*) &server_inet_address;
		printf("\t\t\t\033[32m[ OK ]\033[0m\n");

		address_size = inet_address_size;
	}

	printf("Connecting to server");
	if(connect(socket_fd, server_address, address_size) == -1)
		error("connect()");
	printf("\t\t\t\t\033[32m[ OK ]\033[0m\n");
}
void error(const char *fun_name){
	char info[20];
	int tmp_errno = errno;
	sprintf(info, "error %d in function %s", tmp_errno, fun_name);
	errno = tmp_errno;
	perror(info);
	exit(EXIT_FAILURE);
}

void exit_handler(int signo) {
	if(signo == SIGTSTP)
		exit(EXIT_SUCCESS);
}

void atexit_function(){
	thread_is_alive = 0;
	if(thread != 0)
		pthread_join(thread, NULL);
	close(socket_fd);
}
