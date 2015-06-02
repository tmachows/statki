#ifndef CONFIG_H
#define CONFIG_H


#define TRUE 1
#define FALSE 0

#define CLIENT_NAME_LENGHT 50
#define HISTORY_LENGHT 1024

typedef enum{
	MENU,
	GAME
} lobby_t


typedef enum {
	START_GAME,	
	CHECK_HISTORY,
	DISCONNECT,
	FIELD,
	FIELD_STATE,
	GAME_STATE, //WIN,LOSS,DISCONNECT
	PLAYER_READY, // READY
	HISTORY	
} action_t;

struct client_struct {
	char name[CLIENT_NAME_LENGTH];
	int socket;
	pthread_t thread;
	struct client_struct* next;
};

typedef struct client_struct client_t;


typedef struct {
	char name[CLIENT_NAME_LENGTH];
	int opponnent_socket;
	lobby_t lobby;
	action_t action;
	field_t field;	
	field_state_t field_state;
	game_state_t game_state;
} request_t;
typedef struct{
	char msg[HISTORY_LENGHT];
} history_request_t

typedef struct{
	int x;
	int y;
}field_t


typedef enum{
	WIN,
	LOSS,
	DISCONNECT
} game_state_t


typedef enum{
	HIT,
	MISS
}field_state_t


#endif //CONFIG_H
