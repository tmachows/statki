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

struct field_struct{

	int my_fields[10][10];
	int enymy_fields [10][10];
};
typedef struct field_struct fields_t;
void print_bord();
void init_fields();
void parse_arguments(int argc, char *argv[]);
void* thread_function(void* arg);
void error(const char *fun_name);
void exit_handler(int signo);
void atexit_function();
void init_client();
void game_function();
void get_history();
int pars_field(char *bufor,int &x,int &y);
int check_field(int x,int y);
int check_fields(int begin_x,int begin_y,int x,int y,int lenght);
void place_vaessel(int begin_x,int begin_y,int x,int y,int lenght);

fields_t field;
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
	if(argc < 4 || argc > 5) {
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
	strcpy(response.name,client_name);
	response.lobby = MENU;
	char bufor[MAX_MSG_LENGHT];

	printf("\n\n#################################################################\n");
	printf("----------------- Witaj w grze sieciowej w statki! --------------\n");
	printf("#################################################################\n\n");

	while(thread_is_alive) {
		printf("\t\t1. Nowa Gra\n");
		printf("\t\t2. Historia\n");
		printf("\t\t3. Wyjscie\n");

		int pick;
		do {
			scanf("%d", &pick);
			if(pick == 1 || pick == 2 || pick == 3)
				break;
			printf("Brak takiej pozycji w menu, sprobuj ponownie.\n");
		} while(1);

		if(pick == 1)
			response.action = START_GAME;
		else if(pick == 2)
			response.action = CHECK_HISTORY;
		else if(pick == 3)
			response.action = DISCONNECT;

		printf("Wysylanie zadania...");
		pthread_mutex_lock(&mutex);
		if(send(socket_fd, (void*) &response, sizeof(response), 0) == -1)
			error("send()");
		printf("\t\t\t\033[32m[ OK ]\033[0m\n");
		pthread_mutex_unlock(&mutex);

		if(pick == 1) {
			printf("Oczekiwanie na gre...");
			game_function();
		}
		else if(pick == 2) 
			get_history();
		

		/*
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
	*/
	kill(getppid(),SIGTSTP);
	return NULL;
}

void game_function(){
	init_fields();

	return ;
}

void init_fields(){
	int x,y,begin_x,begin_y;
	for(int i=0;i<100;i++){
		field.my_fields[i/10][i%10]=0;
		field.enymy_fields[i/10][i%10]=0;
	}
	
	for(int i=1;i<5;i++){
		for(int j=0;j<i;j++){	
			printf("prosze podać poczatek statku nr:%d  o dl %d\n",j+1,5-i);		
			do{		
				char *bufor;
				scanf("%s",bufor);	
			}while(pars_field(bufor,&x,&y)==0&&check_field(x, y)==0);
			begin_x=x;
			begin_y=y;
			printf("prosze podać koniec statku nr:%d  o dl %d\n",j+1,5-i);		
			do{		
				char *bufor;
				scanf("%s",bufor);	
			}while(pars_field(bufor,&x,&y)==0&&check_field(x, y)==0&&check_fields(begin_x,begin_y,x,y,i)==0);
			place_vaessel(begin_x,begin_y,x,y,i);
			print_bord();
		}
		
	}

}

void print_bord(){
	char c='A',b;
	printf("\E[H\E[2J"); \\clear
	printf("\t\t moje statki \t\t\t\t\tstatki preciwnika\n");
	printf("\t  0 1 2 3 4 5 6 7 8 9 \t\t  0 1 2 3 4 5 6 7 8 9\n");
	for(int i=0;i<10;i++){
		printf("\t%c "c+i);
			for(int j=0;j<10;j++){
				if(field.my_fields[i][j]==2)
					b='X';	
				if(field.my_fields[i][j]==1)
					b='O';
				if(field.my_fields[i][j]==0)
					b=' ';
				printf("%c ",b);
			}
		printf("\t\t%c "c+i);
			for(int j=0;j<10;j++){
				if(field.enymy_fields[i][j]==2)
					b='X';	
				if(field.enymy_fields[i][j]==1)
					b='O';
				if(field.enymy_fields[i][j]==0)
					b=' ';
				printf("%c ",b);
			}
		printf("\n");	
	}
	
}

void place_vaessel(int begin_x,int begin_y,int x,int y,int lenght){
	int z=0;
	if(begin_x!=x)
		z=x-begin_x;
		if(z>0)
			z=1;
		else 
			z=-1;
		for(int i=0;i<lenght;i++){
			field.my_fields[begin_x+i*z][begin_y]=1;
		}
	else {
		z=y-begin_y;
		if(z>0)
			z=1;
		else 
			z=-1;
		for(int i=0;i<lenght;i++){
			field.my_fields[begin_x][begin_y+i*z]=1;
		}

	}
	
	
}



int check_fields(int begin_x,int begin_y,int x,int y,int lenght){
	int i=0;
	if(begin_x!=x&&begin_y==y)
		i=abs(begin_x-x);
	else if(begin_x==x&&begin_y!=y){
		i=abs(begin_y-y);
	}
	else{
		printf("podaj inne pole  tak by statek nie zginal sie\n");
		return 0;
	}
	if(i==0){
		printf("podaj pole inne niż początek\n");
		return 0;
	}else if(i!=lenght){
		printf("podaj inne pole  tak by statek miał dł %d\n",lenght);
		return 0;
	}else{
		return 1;
	}
	
}

int check_field(int x,int y){
	if(x==0){
		if(y==0){
			if(field.my_fields[x][y]==0&&field.my_fields[x+1][y]==0&&field.my_fields[x][y+1]==0&&field.my_fields[x+1][y+1]==0)
				return 1;
		}
		else if(y==9){
			if(field.my_fields[x][y]==0&&field.my_fields[x+1][y]==0&&field.my_fields[x][y-1]==0&&field.my_fields[x+1][y-1]==0)
				return 1;
		}else{
			if(field.my_fields[x][y]==0&&field.my_fields[x+1][y-1]==0&&field.my_fields[x+1][y+1]==0&&field.my_fields[x+1][y]==0&&field.my_fields[x][y+1]==0&&field.my_fields[x][y-1]==0)
				return 1;
		}
	}
	else if(x==9){
		if(y==0){
			if(field.my_fields[x][y]==0&&field.my_fields[x-1][y]==0&&field.my_fields[x][y+1]==0&&field.my_fields[x-1][y+1]==0)
				return 1;
		}
		else if(y==9){
			if(field.my_fields[x][y]==0&&field.my_fields[x-1][y]==0&&field.my_fields[x][y-1]==0&&field.my_fields[x-1][y-1]==0)
				return 1;
		}else{
			if(field.my_fields[x][y]==0&&field.my_fields[x-1][y-1]==0&&field.my_fields[x-1][y+1]==0&&field.my_fields[x-1][y]==0&&field.my_fields[x][y+1]==0&&field.my_fields[x][y-1]==0)
				return 1;
		}
	}else{
		if(y==0){
			if(field.my_fields[x][y]==0&&field.my_fields[x+1][y+1]==0&&field.my_fields[x][y+1]==0&&field.my_fields[x-1][y+1]==0&&field.my_fields[x+1][y]==0&&field.my_fields[x-1][y]==0)
				return 1;
		}
		else if(y==9){
			if(field.my_fields[x][y]==0&&field.my_fields[x+1][y-1]==0&&field.my_fields[x][y-1]==0&&field.my_fields[x-1][y-1]==0&&field.my_fields[x+1][y]==0&&field.my_fields[x-1][y]==0)
				return 1;	
		}
		else{
			if(field.my_fields[x][y]==0&&field.my_fields[x+1][y+1]==0&&field.my_fields[x][y+1]==0&&field.my_fields[x-1][y+1]==0&&field.my_fields[x+1][y-1]==0&&field.my_fields[x][y-1]==0&&
			field.my_fields[x-1][y-1]==0&&field.my_fields[x+1][y]==0&&field.my_fields[x-1][y]==0)
				return 1;
			printf("pole jest zajente");	
			return 0;
		}
	}
	
};
int pars_field(char *bufor,int &x,int &y){
		sscanf(bufor,"%d",&x);
		if(x<10&&x>=0){
			
		}
		else if(strstr(bufor,"A")!=NULL){
			y=0;
		}else if(strstr(bufor,"B")!=NULL){
			y=1;
		}else if(strstr(bufor,"C")!=NULL){
			y=2;
		}else if(strstr(bufor,"D")!=NULL){
			y=3;
		}else if(strstr(bufor,"E")!=NULL){
			y=4;
		}else if(strstr(bufor,"F")!=NULL){
			y=5;
		}else if(strstr(bufor,"G")!=NULL){
			y=6;
		}else if(strstr(bufor,"H")!=NULL){
			y=7;
		}else if(strstr(bufor,"I")!=NULL){
			y=8;
		}else if(strstr(bufor,"J")!=NULL){
			y=9;
		}else{
			printf("blendne pole podaj od A0 do J9)
			return 0;
		}
	retutn 1;
	
	
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
