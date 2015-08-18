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
#include <string.h>
#include "config.h"

struct field_struct
{
	int my_fields[10][10];
	int enemy_fields [10][10];
};

#ifdef DEBUG
int debug_fields[10][10] = { {1, 1, 1, 1, 0, 0, 0, 0, 0, 0}, 
							{0, 0, 0, 0, 0, 1, 1, 0, 0, 0},
							{1, 1, 1, 0, 0, 0, 0, 0, 0, 0},
							{0, 0, 0, 0, 0, 1, 0, 0, 0, 0},
							{1, 1, 1, 0, 0, 0, 0, 0, 0, 0},
							{0, 0, 0, 0, 0, 1, 0, 0, 0, 0},
							{1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
							{0, 0, 0, 0, 0, 1, 0, 0, 0, 0},
							{1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
							{0, 0, 0, 0, 0, 1, 0, 0, 0, 0} };
#endif

typedef struct field_struct fields_t;
void print_board();
void init_fields();
void parse_arguments(int argc, char *argv[]);
void* thread_function(void* arg);
void error(const char *fun_name);
void exit_handler(int signo);
void atexit_function();
void init_client();
void game_function();
int parse_field(char *bufor,int *x,int *y);
int check_field(int x,int y);
int check_fields(int begin_x,int begin_y,int x,int y,int length);
void place_vessel(int begin_x,int begin_y,int x,int y,int length);
void aim(int x,int y);
int status();

int wait=0;
int game_state=1;
int my_turn=0;
int opponent_socket;
fields_t field;
int local_flag = 1;
int enemy_ready=0;
int server_port;
int client_port;
char server_path[CLIENT_NAME_LENGTH + 2];
char* ip;
int socket_fd;
pthread_t thread;
int thread_is_alive = 1;
char msg[HISTORY_LENGTH];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t	waiting_cond = PTHREAD_COND_INITIALIZER;
char client_name[CLIENT_NAME_LENGTH];


int main(int argc, char *argv[])
{
	atexit(atexit_function);

	if(signal(SIGTSTP, exit_handler) == SIG_ERR)
		error("signal()");
	if(signal(SIGINT, exit_handler) == SIG_ERR)
		error("signal()");

	printf("\nParsing arguments");
	parse_arguments(argc, argv);
	printf("\t\t\t\t\033[32m[ OK ]\033[0m\n");
	
	printf("Client's name:\t%s\n", client_name);
	init_client();
	// Odpalanie watku gry
	if(pthread_create(&thread, NULL, thread_function, NULL) == -1)
		error("pthread_create()");

	// Rejestracja klienta na serwerze
	request_t request;
	strcpy(request.name, client_name);
	request.lobby=MENU;
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
		
		switch(response.lobby){
		case MENU:
			switch(response.action){
				case HISTORY:
					if(strcmp(response.msg, END_HISTORY) != 0)
						printf("%s\n", response.msg);
					else
						pthread_cond_signal(&waiting_cond);
				break;
			
				default:
					printf("Request received but not recognized: %d\n", response.action);
				break;
			}
			
		break;
		case GAME:
			switch(response.action){
				case FIELD:
					printf("Przeciwnik strzela w: %c %d\n",'A'+response.field.y,response.field.x);
					pthread_mutex_lock(&game_mutex);
					
					aim(response.field.x,response.field.y);
					pthread_mutex_unlock(&game_mutex);

				break;
				case FIELD_STATE:
					if(response.field_state==MISS){
						pthread_mutex_lock(&game_mutex);
						my_turn=0;
						field.enemy_fields[response.field.y][response.field.x]=3;
						print_board();
						printf("Pudlo! Kolej przeciwnika.\n");
						pthread_mutex_unlock(&game_mutex);
					}else{
						pthread_mutex_lock(&game_mutex);
						my_turn=1;
						field.enemy_fields[response.field.y][response.field.x]=2;
						print_board();
						printf("Trafiles! Twoja kolej.\n");
						pthread_cond_signal(&waiting_cond);
						pthread_mutex_unlock(&game_mutex);	
					}
				break;
				case GAME_STATE:
					if(response.game_state == WIN){
						printf("BRAWO! WYGRALES!!!\n");
						request_t response;
						strcpy(response.name,client_name);
						response.lobby = GAME;
						response.action = GAME_STATE;
						response.game_state=WIN;
						pthread_mutex_lock(&mutex);
						if(send(socket_fd, (void*) &response, sizeof(response), 0) == -1)
								error("send() game");
						pthread_mutex_unlock(&mutex);
						pthread_mutex_lock(&game_mutex);
						game_state=0;
						pthread_cond_signal(&waiting_cond);
						pthread_mutex_unlock(&game_mutex);
					}
					else if(response.game_state == DISCON){
						exit(EXIT_SUCCESS);
					}
				break;
		
				case SERVER_INFO:
					if(response.server_info==OPPONENT){
						opponent_socket=response.opponent_socket;
						printf("enemy found \033[32m[ OK ]\033[0m\n");
						pthread_mutex_lock(&game_mutex);
						wait=1;
						pthread_cond_signal(&waiting_cond);
						pthread_mutex_unlock(&game_mutex);
					}
				break;
				case START_GAME:
					if(response.field_state==HIT){
						printf("starting game...\n");
						pthread_mutex_lock(&game_mutex);
						enemy_ready=1;
						my_turn = 1;
						pthread_cond_signal(&waiting_cond);
						pthread_mutex_unlock(&game_mutex);			
					}

				break;
				
				default:
					printf("response received but not recognized: %d\n", response.action);
				break;
			}
		break;
		
		default:
			printf("response received but not recognized: %d\n", response.lobby);
		break;
		
		}
	}
	exit(EXIT_SUCCESS);
}
	



void aim(int x,int y){
	request_t response;
	strcpy(response.name,client_name);
	response.lobby = GAME;
	response.action = FIELD_STATE;
	response.opponent_socket=opponent_socket;	
	response.field.x=x;
	response.field.y=y;


	if(field.my_fields[y][x]==1){
		field.my_fields[y][x]=2;
		print_board();
		printf("Zostalismy trafieni...\n");
		game_state = status();
		if(game_state == 0)
			return;
		my_turn=0;
		response.field_state=HIT;
		
	}else{
		response.field_state=MISS;
		field.my_fields[y][x]=3;
		print_board();
		if(enemy_ready==0)
			enemy_ready=1;
		my_turn=1;
		pthread_cond_signal(&waiting_cond);
	}
	
	pthread_mutex_lock(&mutex);
	if(send(socket_fd, (void*) &response, sizeof(response), 0) == -1)
			error("send() redy");
	pthread_mutex_unlock(&mutex);
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
	if(signal(SIGTSTP, exit_handler) == SIG_ERR)
		error("signal()");
	request_t response;
	strcpy(response.name,client_name);
	response.lobby = MENU;

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

		if(send(socket_fd, (void*) &response, sizeof(response), 0) == -1)
			error("send()");
		printf("\t\t\t\033[32m[ OK ]\033[0m\n");

		if(pick == 1) {
			printf("Oczekiwanie na gre...\n");
			game_function();
		}
		else if(pick == 2) {
			pthread_mutex_lock(&mutex);
			pthread_cond_wait(&waiting_cond, &mutex);
			pthread_mutex_unlock(&mutex);
		}
		else if(pick == 3)
			thread_is_alive = 0;

	}
	kill(getpid(),SIGTSTP);
	return NULL;

}


void game_function(){
	
/*	request_t response;
	strcpy(response.name,client_name);
	response.lobby = GAME;
	response.action = START_GAME;	
	pthread_mutex_lock(&mutex);
	if(send(socket_fd, (void*) &response, sizeof(response), 0) == -1)
			error("send() redy");
	pthread_mutex_unlock(&mutex);
*/	
	
	pthread_mutex_lock(&game_mutex);
	while(wait==0)
		pthread_cond_wait(&waiting_cond, &game_mutex);
	pthread_mutex_unlock(&game_mutex);
	init_fields();
	char bufor[2];
	
	pthread_mutex_lock(&game_mutex);
	printf("Oczekiwanie na gotowosc przeciwnika..\n");
	while(enemy_ready==0)
		pthread_cond_wait(&waiting_cond, &game_mutex);
	pthread_mutex_unlock(&game_mutex);
	printf("Przeciwnik gotowy - gra rozpoczyna sie!\n");
	int x,y;
	while(!my_turn)
		pthread_cond_wait(&waiting_cond, &game_mutex);
	while(game_state==1&&thread_is_alive==1){
			printf("Podaj pole do zaatakowania: ");
			do{					
				scanf("%s",bufor);	
			}while(parse_field(bufor,&x,&y)==0);
			request_t response;
			strcpy(response.name,client_name);
			response.lobby = GAME;
			response.action = FIELD;
			response.field.x=x;
			response.field.y=y;	
			response.opponent_socket=opponent_socket;
			pthread_mutex_lock(&mutex);
			if(send(socket_fd, (void*) &response, sizeof(response), 0) == -1)
					error("send() game");
			pthread_mutex_unlock(&mutex);
			
		pthread_mutex_lock(&game_mutex);
		do {
			pthread_cond_wait(&waiting_cond, &game_mutex);
		} while(!my_turn);
		pthread_mutex_unlock(&game_mutex);
		if(game_state==1)
			game_state=status();
	}
	printf("Koniec gry\n");

	return ;
}


int status(){
	request_t response;
	strcpy(response.name,client_name);
	response.lobby = GAME;
	response.action = GAME_STATE;

	response.opponent_socket=opponent_socket;

	for(int i=0;i<100;i++){
		if(field.my_fields[i/10][i%10]==1){
			return 1;
		}
	}
	printf("Przegrales!\n");
	game_state = 0;
	my_turn = 1;
	pthread_cond_signal(&waiting_cond);
	response.game_state=LOSS;
	pthread_mutex_lock(&mutex);
	if(send(socket_fd, (void*) &response, sizeof(response), 0) == -1)
			error("send() game");
	pthread_mutex_unlock(&mutex);
	return 0;
}


void init_fields(){
	
	for(int i=0;i<100;i++){
		field.my_fields[i/10][i%10]=0;
		field.enemy_fields[i/10][i%10]=0;
	}
	
	#ifdef DEBUG
	for(int i=0;i<10;i++){
		for(int j=0; j<10; j++)
			field.my_fields[i][j] = debug_fields[i][j];
	}
	print_board();
	#endif
	
	#ifndef DEBUG
	int x,y,begin_x,begin_y;
	char bufor[2];
	print_board();
	for(int i=1;i<5;i++){
		for(int j=0;j<i;j++){	
			printf("Podaj poczatek statku nr:%d o dlugosci %d: ",j+1,5-i);		
			do{		
				scanf("%s",bufor);	
				if(parse_field(bufor,&x,&y)!=0){
					if(check_field(y, x)!=0)
						break;
				}
				
				
			}while(1);

			begin_x=x;
			begin_y=y;
			printf("Podaj koniec statku nr:%d o dlugosci %d: ",j+1,5-i);		
			if(5-i!=1){	
				do{		
					scanf("%s",bufor);	
					
					if(parse_field(bufor,&x,&y)!=0){
						if(check_field(y, x)!=0){
							if(check_fields(begin_x,begin_y,x,y,5-i)!=0)
								break;
						}
						
					}	
				}while(1);
			}
			place_vessel(begin_x,begin_y,x,y,5-i);
			print_board();
		}
		
	}
	#endif

	request_t response;
	strcpy(response.name,client_name);
	response.lobby = GAME;
	response.action = PLAYER_READY;
	response.opponent_socket=opponent_socket;	
	pthread_mutex_lock(&mutex);
	if(send(socket_fd, (void*) &response, sizeof(response), 0) == -1)
			error("send() redy");
	pthread_mutex_unlock(&mutex);


}

void print_board(){
	char c='A',b;
	printf("\E[H\E[2J"); //clear
	printf("\t moje statki \t\t\t\tstatki przeciwnika\n");
	printf("\t  0 1 2 3 4 5 6 7 8 9 \t\t\t  0 1 2 3 4 5 6 7 8 9\n");
	for(int i=0;i<10;i++){
		printf("\t%c ",c+i);
			for(int j=0;j<10;j++){
				if(field.my_fields[i][j]==3)
					b='@';	
				if(field.my_fields[i][j]==2)
					b='X';	
				if(field.my_fields[i][j]==1)
					b='O';
				if(field.my_fields[i][j]==0)
					b=' ';
				printf("%c ",b);
			}
		printf("\t\t\t%c ",c+i);
			for(int j=0;j<10;j++){
				if(field.enemy_fields[i][j]==3)
					b='@';	
				if(field.enemy_fields[i][j]==2)
					b='X';	
				if(field.enemy_fields[i][j]==1)
					b='O';
				if(field.enemy_fields[i][j]==0)
					b=' ';
				printf("%c ",b);
			}
		printf("\n");	
	}
	
}

void place_vessel(int begin_x,int begin_y,int x,int y,int length){
	int z=0;
	if(begin_x!=x){
		z=x-begin_x;
		if(z>0)
			z=1;
		else 
			z=-1;
		for(int i=0;i<length;i++){
			field.my_fields[begin_y][begin_x+i*z]=1;
		}
	}else {
		z=y-begin_y;
		if(z>0)
			z=1;
		else 
			z=-1;
		for(int i=0;i<length;i++){
			field.my_fields[begin_y+i*z][begin_x]=1;
		}

	}
	
	
}



int check_fields(int begin_x,int begin_y,int x,int y,int length){
	int i=0;
	if(begin_x!=x&&begin_y==y)
		i=abs(begin_x-x)+1;
	else if(begin_x==x&&begin_y!=y){
		i=abs(begin_y-y)+1;
	}
	else{
		printf("Podaj inne pole tak, by statek nie zginal sie.\n");
		return 0;
	}
	
	if(i==0){
		printf("Podaj pole inne niz poczatek.\n");
		return 0;
	}else if(i!=length){
		printf("Podaj inne pole tak by statek mial dlugosc %d.\n",length);
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
		
		}	
	}
	printf("Pole jest zajete.\n");	
	return 0;
};
int parse_field(char *bufor,int *x,int *y){
		*x=bufor[1]-48;
		if(!(*x<10&&*x>=0)){
			printf("Bledne pole - podaj od A0 do J9.\n");
			return 0;
		}
		
		if(strstr(bufor,"A")!=NULL){
			*y=0;
		}else if(strstr(bufor,"B")!=NULL){
			*y=1;
		}else if(strstr(bufor,"C")!=NULL){
			*y=2;
		}else if(strstr(bufor,"D")!=NULL){
			*y=3;
		}else if(strstr(bufor,"E")!=NULL){
			*y=4;
		}else if(strstr(bufor,"F")!=NULL){
			*y=5;
		}else if(strstr(bufor,"G")!=NULL){
			*y=6;
		}else if(strstr(bufor,"H")!=NULL){
			*y=7;
		}else if(strstr(bufor,"I")!=NULL){
			*y=8;
		}else if(strstr(bufor,"J")!=NULL){
			*y=9;
		}else{
			printf("Bledne pole - podaj od A0 do J9.\n");
			return 0;
		}
	return 1;
	
	
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
	if(signo == SIGTSTP){
	request_t response;
	strcpy(response.name,client_name);
	response.lobby = GAME;
	response.action = GAME_STATE;
	response.game_state=DISCON;
	response.opponent_socket=opponent_socket;	
	
	pthread_mutex_lock(&mutex);
	if(game_state==1)
		if(send(socket_fd, (void*) &response, sizeof(response), 0) == -1)
				error("send() game");
	pthread_mutex_unlock(&mutex);
		
	}
	
		
}

void atexit_function(){

	thread_is_alive = 0;
	//if(thread != 0)
	//	pthread_join(thread, NULL);
	close(socket_fd);
}
