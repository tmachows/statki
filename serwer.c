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

void* server_thread_function(void* arg);
void error(const char *fun_name);
void exit_handler(int signo);
int clients_count();
void atexit_function();
int get_client_number(client_t* client);
// Gawel
void check_enter(int argc, char** argv);
void init_server();
void listen_function();
void send_to_all(request_t request, client_t * client);
void unregister_client(client_t * client);
int port;
char* path;

client_t* head_client = NULL;// zrobic tablice [10] klientow4
pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;

int unix_socket;
int inet_socket;

struct sockaddr_un	server_unix_address;
struct sockaddr_un	client_unix_address;
struct sockaddr_in	server_inet_address;
struct sockaddr_in	client_inet_address;

socklen_t unix_address_size = sizeof(struct sockaddr_un);
socklen_t inet_address_size = sizeof(struct sockaddr_in);

int main(int argc, char **argv)
{
	atexit(atexit_function);
	check_enter(argc, argv);
	init_server();
	listen_function();
	
	exit(EXIT_SUCCESS);
}

void* server_thread_function(void* tmp_client) {
	printf("\nNew thread created\n");
	client_t client;
	client.socket = ((client_t*) tmp_client)->socket;
	client.thread = ((client_t*) tmp_client)->thread;

	if(clients_count() >= MAX_CLIENTS) {
		printf("Too much clients canceling thread\n");
		pthread_mutex_unlock(&client_list_mutex);
		if(pthread_cancel(client.thread) == -1)
			error("server_thread_function() --> pthread_cancel()");

	}

	client.next = head_client;
	head_client = &client;


	request_t request;

	while(1) {

		if(recv(client.socket, (void*) &request, sizeof(request), 0) == -1)
			error("recv()");

		printf("\nReceived request no. %d from Client #%d\n", request.action, get_client_number(&client));
	
		switch(request.action){
		case ADD_USER:
			printf("Registering client ");
				strcpy(client.name, request.name);
			printf("\t\t\t\t\t\033[32m[ OK ]\033[0m\n");
			pthread_mutex_unlock(&client_list_mutex);
			break;
		case UNREGISTER:
				printf("Unregistering client: %s\n", client.name);
			pthread_mutex_lock(&client_list_mutex);
			unregister_client(&client);
			pthread_mutex_unlock(&client_list_mutex);
			if(pthread_cancel(client.thread) == -1)
				error("server_thread_function() --> pthread_cancel()");

			break;
		case MSG_TO_SERVER:
			printf("Sending msg from: %s\n", request.name);

			pthread_mutex_lock(&client_list_mutex);
				send_to_all(request,&client);
			pthread_mutex_unlock(&client_list_mutex);
			break;
		default:
			printf("Request received but not recognized: %d\n", request.action);
		}
	}

	exit(EXIT_SUCCESS);
}

int get_client_number(client_t* client) {
	client_t* tmp = head_client;
	int i = 0;
	while(tmp != client && tmp != NULL) {
		tmp = tmp->next;
		i++;
	}

	return (tmp == NULL) ? -1 : i;
}

int clients_count() {
	int result = 0;

		client_t* tmp = head_client;
		while(tmp != NULL) {
			tmp = tmp->next;
			result++;
		}

	return result;
}


void check_enter(int argc, char** argv){
	if(argc != 3) {
		printf("\nInvalid arguments! Usage:\n\t%s\t<port> <path>\n\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[1]);
	path = argv[2];

	if(signal(SIGTSTP, exit_handler) == SIG_ERR)
		error("signal()");
}

void init_server(){

	printf("\nCreating server socket for local communication");
	if((unix_socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
		error("socket(server unix)");
	printf("\t\t\033[32m[ OK ]\033[0m\n");

	printf("Creating server socket for internet communication");
	if((inet_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
		error("socket(server inet)");
	printf("\t\033[32m[ OK ]\033[0m\n");

	memset(&server_unix_address, 0, sizeof(server_unix_address));
	memset(&server_inet_address, 0, sizeof(server_inet_address));
	memset(&client_unix_address, 0, sizeof(client_unix_address));
	memset(&client_inet_address, 0, sizeof(client_inet_address));

	// Ustawiamy adres lokalny
	server_unix_address.sun_family = AF_UNIX;
	strcpy(server_unix_address.sun_path, path);

	// Ustawiamy adres inet
	server_inet_address.sin_family = AF_INET;
	server_inet_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_inet_address.sin_port = htons(port);

	printf("Binding sockets");
	
	if(bind(unix_socket, (struct sockaddr*) &server_unix_address, unix_address_size) == -1)
		error("bind(unix)");
	
	if(bind(inet_socket, (struct sockaddr*) &server_inet_address, inet_address_size) == -1)
		error("bind(inet)");
	printf("\t\t\t\t\t\t\033[32m[ OK ]\033[0m\n");

	if(listen(unix_socket, 10) == -1)
		error("listen(unix)");
	if(listen(inet_socket, 10) == -1)
		error("listen(inet)");
}

void listen_function(){
printf("Waiting for clients\n");
	client_t tmp_client;
	while(1) {
		pthread_mutex_lock(&client_list_mutex);
		tmp_client.socket = accept(unix_socket, NULL, NULL);
		if(tmp_client.socket == -1) {
			if(tmp_client.socket == EAGAIN || tmp_client.socket == EWOULDBLOCK)
				error("accept(unix)");
			pthread_mutex_unlock(&client_list_mutex);
		}
		else {
			if(pthread_create(&tmp_client.thread, NULL, server_thread_function, &tmp_client) == -1)
				error("pthread_create()");
		}

		pthread_mutex_lock(&client_list_mutex);
		tmp_client.socket = accept(inet_socket, NULL, NULL);
		if(tmp_client.socket == -1) {
			if(tmp_client.socket == EAGAIN || tmp_client.socket == EWOULDBLOCK)
				error("accept(inet)");
			pthread_mutex_unlock(&client_list_mutex);
		}
		else {
			if(pthread_create(&tmp_client.thread, NULL, server_thread_function, &tmp_client) == -1)
				error("pthread_create()");
		}
	}
}


void send_to_all(request_t request, client_t *client){
	

	request_t answer;
	client_t * tmp = head_client;
	strcpy(answer.msg,request.msg);
	strcpy(answer.name,request.name);
	answer.action = MSG_FROM_SERVER;
	while(tmp!= NULL){
		if(tmp->socket!=client->socket){
			if(send(tmp->socket, (void *) &answer, sizeof(answer),0) == -1)
			error("Send_to_all()");
		}	
		tmp=tmp->next;
	}
	

}

void unregister_client(client_t* client){
		client_t *tmp  = head_client;
		while(tmp!=NULL){
			printf("%d:%s",tmp->socket,tmp->name);
			tmp=tmp->next;
		}
				if(head_client == client)
						head_client = client->next;
				else {
					client_t* prev = head_client;
					while(prev->next != client && prev->next != NULL)
						prev = prev->next;
					if(prev->next != NULL)
						prev->next = client->next;
				}
			
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
	//int i;
	/*for(i=0;i<client_counter;i++){
		shutdown(clients[i],SHUT_RDWR);
	}*/
	close(unix_socket);
	close(inet_socket);
	remove(path);
}
