#ifndef CONFIG_H
#define CONFIG_H


#define TRUE 1
#define FALSE 0

#define CLIENT_NAME_LENGHT 50
#define HISTORY_LENGHT 1024
#define MAX_CLIENTS 20
#define history "history"
#define END_HISTORY "END"
typedef enum{
	MENU,
	GAME
}lobby_t;


typedef struct{
	int x;
	int y;
}field_t;


typedef enum{
	WIN,
	LOSS,
	DISCON    //uwaga tutaj zmiana komunikatu bo sie powtarzaly i mi bledy lecialy przy kompilacji
}game_state_t;


typedef enum{
	HIT,
	MISS
}field_state_t;


typedef enum{
	ADD_USER, //MENU wysylane przy probie polaczenia z serwerem
	START_GAME,//MENU
	CHECK_HISTORY,//MENU
	DISCONNECT,//MENU,GAME
	FIELD,//GAME
	FIELD_STATE,//GAME
	GAME_STATE, //GAME,WIN,LOSS,DISCONNECT
	PLAYER_READY, //GAME READY
	HISTORY,
	SERVER_INFO
}action_t;

typedef enum{
	CONNECTED,
	ERROR,
	DISCONNECTED,
	OPPONENT
}server_info_t;

struct client_struct {
	char name[CLIENT_NAME_LENGHT];
	int socket;
	pthread_t thread;
	struct client_struct* next;
};

typedef struct client_struct client_t;


typedef struct {
	char name[CLIENT_NAME_LENGHT];
	int opponent_socket;  // GAME.SERVER_INFO.OPPONENT_SOCKET   to w  opponent_socket_socket przeciwnika
	lobby_t lobby;
	action_t action;
	field_t field;	
	field_state_t field_state;
	game_state_t game_state;
	server_info_t server_info;
	char msg[HISTORY_LENGHT];
}request_t;
struct game_struct{
	client_t* player_1;
	client_t* player_2;
	int ready_player_1;
	int ready_player_2;
	struct game_struct* next;
};

typedef struct game_struct game_t;




#endif //CONFIG_H
