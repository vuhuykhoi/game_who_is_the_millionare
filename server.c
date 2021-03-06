#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h> 
#include <sys/time.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "message.h"
#include "gamehelper.h"
#include "User_ll.h"
#include "database.h"


#define BACKLOG 20   /* Number of allowed connections */
#define BUFF_SIZE 1024 /*Size of buffer message*/


enum num_send{				    // protocol message from server
	BROADCAST_PLAYER_IN_ROOM,   // BROADCAST : send to all client that in room
	SEND_TO_ONE,			    // SEND_TO_ONE: send to an client
	BLOCK,					    // BLOCK : just process data do not send message
};							    


/*------------------------GAME VARIABLES------------------------------*/

extern int num_of_round1_question;			/*amount of questions*/
extern int random_ques;			 			/*for random questions in array questions*/
extern int round2_score;		  			/*count player score*/
extern int round2_ques_number;	  			/*number order of round2 question->to count score*/

extern int help_50_50_used;       			/*check if help_50_50 is used?[used:1/not used:0]*/
extern char main_player[USER_MAX_LENGTH];	/*is username of player who pass round 1*/
extern int *arr_sended_ques;				/*to random questions.
											if randomed question,will be remove from this array*/
extern int arr_size ;						/*arr_size of arr_sended_ques*/


extern int num_of_player;					/*number of player who get in room successful*/
extern int num_players_get_round1_ques;		/*number of in room player get round 1 questions*/	
extern int num_round1_ans;					/*number of in room player anwser round 1 questions*/	

extern linkList *accList;					/*a linklist that save all user accounts*/
extern sqlite3** db;			
extern QUESTION *listQues;					/*an array that save all game questions */
/*--------------------------------------------------------------------*/



/* The processData function copies the input string to output */
int processData(int s ,node** acc_node_pointer, char *in, char *out);

/* The recv() wrapper function*/
int receiveData(int s, char *buff, int size, int flags);

/* The send() wrapper function*/
int sendData(int s, char *buff, int size, int flags);

/*------------------------------------------------------------------*/


/*result contains the scores of users*/
static int on_get_score(void* result, int num_cols, char** row, char** cols) {
	//printf("%s-%s\n", row[0], row[1]);
	char*score =(char*)result;
	strcat(score,row[1]);
	for(int i=0;i<(12-strlen(row[1]));i++)
		strcat(score," ");
	strcat(score,row[2]);
	for(int i=0;i<8-(strlen(row[2]));i++)
		strcat(score," ");
	strcat(score,row[3]);
//	strcat(score,"\n");
	return 0;
}

/*check if user account exist and store user's password to check pass*/
int on_get_user(void* result, int num_cols, char** row, char** cols) {
	char* password = (char*)result;
	//printf("%s\n\n",cols[1]);
	if(strcmp(cols[1], "PASSWORD") == 0) {
		strcpy(password, row[1]);
	} else {
		strcpy(password, "");
	}
	return 0;
}

/*check if username involved in database*/
/*return 1 if username exist*/
/*return 0 if username not exist*/
int on_check_user(void* result, int num_cols, char** row, char** cols) {
	int* check = (int*)result;
	//printf("%s\n\n",cols[1]);
	if(strcmp(cols[0], "USERNAME") == 0) {
		*check=1;
	} else {
		*check=0;
	}
	return 0;
}


int main(int argc,char *argv[]){

	int i, maxi, maxfd, listenfd, connfd, sockfd;
	int nready, client[FD_SETSIZE];
	node *acc_node_pointer[FD_SETSIZE]; //every client has one session so have a node an list account
	ssize_t	ret;
	fd_set	readfds, allset;
	char sendBuff[BUFF_SIZE], rcvBuff[BUFF_SIZE];
	socklen_t clilen;
	struct sockaddr_in cliaddr, servaddr;
	int PORT = 0;

	if(argc != 2){
		printf("Invalid Argument!\n");
		exit(1);
	}
	//GET PORT from ARUGMENT
	PORT = atoi(argv[1]);
	
	
	if(!gameSetup()){
		printf("Error occurred during initialization game variables\n");
		exit(1);
	};

	//Step 1: Construct a TCP socket to listen connection request
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){  /* calls socket() */
		perror("\nError: ");
		return 0;
	}

	//Step 2: Bind address to socket
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(PORT);

	if(bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr))==-1){ /* calls bind() */
		perror("\nError: ");
		return 0;
	} 

	//Step 3: Listen request from client
	if(listen(listenfd, BACKLOG) == -1){  /* calls listen() */
		perror("\nError: ");
		return 0;
	}

	maxfd = listenfd;			/* initialize */
	maxi = -1;				/* index into client[] array */
	for (i = 0; i < FD_SETSIZE; i++){
		client[i] = -1;			/* -1 indicates available entry */
		acc_node_pointer[i] = NULL;
	}

	FD_ZERO(&allset);
	FD_SET(listenfd, &allset);
	
	//Step 4: Communicate with clients
	while (1) {
		readfds = allset;		/* structure assignment */
		nready = select(maxfd+1, &readfds, NULL, NULL, NULL);
		if(nready < 0){
			perror("\nError: ");
			return 0;
		}
		
		if (FD_ISSET(listenfd, &readfds)) {	/* new client connection */
			clilen = sizeof(cliaddr);
			if((connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen)) < 0)
				perror("\nError: ");
			else{
				printf("You got a connection from %s\n", inet_ntoa(cliaddr.sin_addr)); /* prints client's IP */
				for (i = 0; i < FD_SETSIZE; i++)
					if (client[i] < 0) {
						client[i] = connfd;	/* save descriptor */
						break;
					}
				if (i == FD_SETSIZE){
					printf("\nToo many clients");
					close(connfd);
				}

				FD_SET(connfd, &allset);	/* add new descriptor to set */
				if (connfd > maxfd)
					maxfd = connfd;		/* for select */
				if (i > maxi)
					maxi = i;		/* max index in client[] array */

				if (--nready <= 0)
					continue;		/* no more readable descriptors */
			}
		}

		for (i = 0; i <= maxi; i++) {	/* check all clients for data */
			if ( (sockfd = client[i]) < 0)
				continue;
			if (FD_ISSET(sockfd, &readfds)) {
				memset(rcvBuff,'\0',BUFF_SIZE);
				ret = receiveData(sockfd, rcvBuff, BUFF_SIZE, 0);
				if (ret <= 0){
					FD_CLR(sockfd, &allset);
					close(sockfd);
					client[i] = -1;
					acc_node_pointer[i] = NULL;
				}
				
				else {
					int flag = processData(sockfd,&acc_node_pointer[i],rcvBuff, sendBuff);
					ret = strlen(sendBuff);
					
					if (flag == SEND_TO_ONE){
						sendData(sockfd, sendBuff, ret, 0);
						if (ret <= 0){
							FD_CLR(sockfd, &allset);
							close(sockfd);
							client[i] = -1;
							acc_node_pointer[i] = NULL;
						}	

					}else if(flag == BROADCAST_PLAYER_IN_ROOM){
						for(int j = 0; j <= maxi; j++){
							if ( (sockfd = client[j]) < 0) continue;
							sendData(sockfd, sendBuff, ret, 0);
							if (ret <= 0){
								FD_CLR(sockfd, &allset);
								close(sockfd);
								client[i] = -1;
								acc_node_pointer[i] = NULL;
							}	
						}
					}
					
				}

				if (--nready <= 0)
					break;		/* no more readable descriptors */
			}
		}
	}
	
	return 0;
}

int processData(int s,node** user, char *in, char *out){
	char scoreinfo[BUFF_SIZE]="";	
	char sql[MAX_STRING_LENGTH_SQL];
	message recv_message;
	recv_message = parseMessage(in);
	char *msg = malloc(sizeof(char)*10);
	time_t t;
	
	switch(recv_message.msg_type){

		case REGISTER_USER:
			//create a new node
			//check if exist accout
			//return INVALID_REGISTER_USER if exist
			//else return VALID_REGISTER_USER]
			strcpy(sql,"SELECT * FROM account WHERE username = '");
			strcat(sql, recv_message.value);
			strcat(sql, "';");
			printf("%s\n",sql);
			int existedUsername=0;
			//select_query(sql, on_get_user, &stored_password);
			//printf("%s\n\n",stored_password);
			select_query(sql,on_check_user,&existedUsername);
			//printf("%d\n",existedUsername);
			if(existedUsername){
				sprintf(out,"%d%s%s",INVALID_REGISTER_USER,SEPARATOR_CHAR,"none");	
				
			}else{
				sprintf(out,"%d%s%s",VALID_REGISTER_USER,SEPARATOR_CHAR,"none");
				(*user) = newNode(newEmptyVal());
				strcpy((*user)->val.acc,recv_message.value);
				
			}		

			return SEND_TO_ONE;
	
		case REGISTER_PASS:
			//save account to list
			if((*user)){
				strcpy((*user)->val.pass,recv_message.value);
				char* sql = (char*)malloc(MAX_STRING_LENGTH_SQL);
				strcpy(sql, "");
				strcat(sql, "INSERT INTO account(username, password) VALUES ('");
				strcat(sql, (*user)->val.acc);
				strcat(sql, "', '");
				strcat(sql, (*user)->val.pass);
				strcat(sql, "');");
				printf("%s\n",sql);	
				insert_query(sql);			
				addToLastOfList(accList,(*user)->val);
				*user = NULL;
				//updateAccountToFile(accList,"account.txt");
				//updateAccountToFile(accList,"account.txt");
				sprintf(out,"%d%s%s",REGISTER_SUCCESS,SEPARATOR_CHAR,"none");	
			}		

			return SEND_TO_ONE;
		
		case LOG_IN_USER:
			strcpy(sql,"SELECT * FROM account WHERE username = '");
			strcat(sql, recv_message.value);
			strcat(sql, "';");
			printf("%s\n",sql);
			char stored_password[20] = "";
			select_query(sql, on_get_user, &stored_password);	
			// if username is not exist
			if(strlen(stored_password) == 0)
			{
				sprintf(out,"%d%s%s",INVALID_LOGIN_USER,SEPARATOR_CHAR,"none");	
			}
			else{
				*user = findNodeByValue(accList,recv_message.value);
				if((*user)->val.sts == IS_LOG_IN){
					sprintf(out,"%d%s%s",USER_ALREADY_LOGIN,SEPARATOR_CHAR,"none");
					*user = NULL;	
				}else
					sprintf(out,"%d%s%s",VALID_LOGIN_USER,SEPARATOR_CHAR,"none");
			}				
	
			return SEND_TO_ONE;
		
		case LOG_IN_PASS:
			if((*user)){
				if(isCorrectPassword((*user)->val,recv_message.value)){
					(*user)->val.sts = IS_LOG_IN;
					sprintf(out,"%d%s%s",LOGIN_SUCCESS,SEPARATOR_CHAR,"none");
				}else{
					sprintf(out,"%d%s%s",INVALID_LOGIN_PASSWORD,SEPARATOR_CHAR,"none");		
				}
			}
		
			return SEND_TO_ONE;
	
		case LOG_OUT_USER:
			if((*user)){
				if((*user)->val.sts != NOT_LOG_IN){
					(*user)->val.sts = NOT_LOG_IN;
					*user = NULL;
					sprintf(out,"%d%s%s",LOGOUT_SUCCESS,SEPARATOR_CHAR,"none");
				}
			}
			
			return SEND_TO_ONE;

		case GET_IN_ROOM:
		if((*user)){
			if((*user)->val.sts == IS_LOG_IN){
				if(num_of_player+1 <= NUM_FULL_ROOM){
					(*user)->val.sts = IS_ROUND1_PLAYER;
					num_of_player++;
					sprintf(out,"%d%s%d",NUM_PLAYER_IN_ROOM,SEPARATOR_CHAR,num_of_player);	
					return BROADCAST_PLAYER_IN_ROOM;
				}else{
					sprintf(out,"%d%s%d",ROOM_IS_FULL,SEPARATOR_CHAR,num_of_player);				
					return SEND_TO_ONE;
				}
			}
		}
		return BLOCK;		
		

		case GET_ROUND1_QUES:
		if((*user)){
			if((*user)->val.sts == IS_ROUND1_PLAYER){
				/*Wait until all player in room request sever send round1 question*/
				num_players_get_round1_ques++;
				if(num_players_get_round1_ques < NUM_FULL_ROOM){
					return BLOCK;
				/*If all player sended request server send question*/
				}else if(num_players_get_round1_ques == NUM_FULL_ROOM){
				
		   			srand((unsigned) time(&t));
		   			random_ques =  randomQuestionNonRepeat(arr_sended_ques,&arr_size);
					sprintf(out,"%d%s%s\n%s\n%s\n%s\n%s",ROUND1_QUES,SEPARATOR_CHAR,
						listQues[random_ques].ques,
						listQues[random_ques].choices[0],
						listQues[random_ques].choices[1],
						listQues[random_ques].choices[2],
						listQues[random_ques].choices[3]
					);
								
					return BROADCAST_PLAYER_IN_ROOM;
				}
			}
		}
		return BLOCK;
			
		case ANSWER_ROUND1_QUES:
		if((*user)){
			if((*user)->val.sts == IS_ROUND1_PLAYER){
				/*set main player
				 *the player answer the questions with correct answer firstly
				 *set that player is main player
				 */
				if(!strcmp(recv_message.value,listQues[random_ques].answ)){
					if(main_player[0] == '\0'){
						strcpy(main_player,(*user)->val.acc);
						(*user)->val.sts = IS_ROUND2_PLAYER;
					}
				}
				/*Wait until all player in room request sever send round1 answer*/
				num_round1_ans++;
				if(num_round1_ans < NUM_FULL_ROOM){
					return BLOCK;
				/*If all player sended request server send answer*/
				}else if(num_round1_ans == NUM_FULL_ROOM){
					sprintf(out,"%d%s%s",ROUND1_ANSW,SEPARATOR_CHAR,listQues[random_ques].answ);		
					return BROADCAST_PLAYER_IN_ROOM;
				}
			}
		}
		return BLOCK;
		
		case GET_ROUND1_RESULT:
		if((*user)){
			if((*user)->val.sts == IS_ROUND1_PLAYER||(*user)->val.sts == IS_ROUND2_PLAYER){
				if(main_player[0] != '\0'){
					/*Send result who is main player to all player in room*/
					sprintf(out,"%d%s%s",ROUND2_PLAYER,SEPARATOR_CHAR,main_player);			
				}else{
					/*If no player pass round1 game over-> reset all game variables */
				resetGameVariables();
				sprintf(out,"%d%s%s",GAME_OVER,SEPARATOR_CHAR,"none");	
				}
				return SEND_TO_ONE;
			}
		}
		return BLOCK;

		case GET_ROUND2_QUES:
		if((*user)->val.sts == FINISHED_GAME){
			sprintf(out,"%d%s%s",ROUND2_FINISH,SEPARATOR_CHAR,"none");
			return BROADCAST_PLAYER_IN_ROOM;
		}else if((*user)->val.sts == IS_ROUND2_PLAYER){
			/* if round2 question number = total round2 question ->
		 	* game continue until reached total round2 questions number
		 	*/
			if(round2_ques_number < TOTAL_ROUND2_QUES ){
			srand((unsigned) time(&t));
			random_ques =  randomQuestionNonRepeat(arr_sended_ques,&arr_size);
			round2_ques_number++;
			sprintf(out,"%d%s%d: %s\n%s\n%s\n%s\n%s",ROUND2_QUES,SEPARATOR_CHAR,
					round2_ques_number,
					listQues[random_ques].ques,
					listQues[random_ques].choices[0],
					listQues[random_ques].choices[1],
					listQues[random_ques].choices[2],
					listQues[random_ques].choices[3]
			);
			/*if round2 question number = total round2 question -> game over*/
			}else if(round2_ques_number == TOTAL_ROUND2_QUES){
				(*user)->val.sts == FINISHED_GAME;
				savePlayerScore((*user)->val.acc,round2_score,getCurrentTime());
				resetGameVariables();
				sprintf(out,"%d%s%s",ROUND2_FINISH,SEPARATOR_CHAR,"none");
			}
			return BROADCAST_PLAYER_IN_ROOM;
		}
		return BLOCK;


		case ANSWER_ROUND2_QUES:
		if((*user)){
			if((*user)->val.sts == IS_ROUND2_PLAYER){
					/*if main player select help stop game:
					 *game over:
					 *reset game variables
					 *save player result
					 */
				if(!strcmp(recv_message.value,STOP_GAME)){
					round2_score = countScore(round2_ques_number-1);
					(*user)->val.sts = FINISHED_GAME;

					savePlayerScore((*user)->val.acc,round2_score,getCurrentTime());
				
					sprintf(out,"%d%s%s",ROUND2_ANSW,SEPARATOR_CHAR,listQues[random_ques].answ);
					resetGameVariables();
				/*if main player select help 50/50 to remove 2 wrong answer:
				 *server random 2 wrong answer and send question + answer to client again
				 *set get_50_50_used = 1 to make sure user just only use 50/50 one time in this current game
				 */		
				}else if(!strcmp(recv_message.value,REMOVE_WRONG_ANSWER)){
					if(help_50_50_used == 0){
						sprintf(out,"%d%s%d: %s",ROUND2_QUES_50_50,SEPARATOR_CHAR,
							round2_ques_number,
							removeTwoWrongAnswer(listQues[random_ques])
						);
						help_50_50_used = 1;
					/*if help 50/50 used send notification that 50/50 had been used to user*/
					}else if(help_50_50_used == 1){
						sprintf(out,"%d%s%s",HELP_50_50_USED,SEPARATOR_CHAR,"none"
						);
					}
				
				/*if is correct answer:
				 *count score
				 *send answer to all player in room
				*/
				}else if(!strcmp(recv_message.value,listQues[random_ques].answ)){
					round2_score = countScore(round2_ques_number);
					sprintf(out,"%d%s%s",ROUND2_ANSW,SEPARATOR_CHAR,listQues[random_ques].answ);		
				
				/* if is wrong answer:
				 * count score
				 * save player score
				 * game over: reset game variables
				 */
				}else{
					//status -> finish game
					if(round2_ques_number == 1){
						round2_score = countScore(0);
					
					}else if(round2_ques_number >1 && round2_ques_number <= 5){
						round2_score = countScore(1);
					
					}else if(round2_ques_number > 5 && round2_ques_number <= 10){
						round2_score = countScore(5);
					
					}else if(round2_ques_number > 10){
						round2_score = countScore(10);
					}

					(*user)->val.sts = FINISHED_GAME;
					savePlayerScore((*user)->val.acc,round2_score,getCurrentTime());
					sprintf(out,"%d%s%s",ROUND2_ANSW,SEPARATOR_CHAR,listQues[random_ques].answ);
					resetGameVariables();		
				}
			return BROADCAST_PLAYER_IN_ROOM;
			}
		}
		return BLOCK;

		case GET_ROUND2_RESULT:
		if((*user)){
			if((*user)->val.sts == IS_ROUND2_PLAYER ||(*user)->val.sts == FINISHED_GAME){
				/*send main player score to all player in room*/
				sprintf(out,"%d%s%d",ROUND2_SCORE,SEPARATOR_CHAR,round2_score);	
				return BROADCAST_PLAYER_IN_ROOM;	
			}
		}
		return BLOCK;
		

		case GET_OUT_ROOM:
		if((*user)){
			if((*user)->val.sts == IS_ROUND1_PLAYER || 
				(*user)->val.sts == IS_ROUND2_PLAYER ||
				(*user)->val.sts == FINISHED_GAME){
				(*user)->val.sts = IS_LOG_IN;
			}
		}	
		return BLOCK;
		
		case GET_HIGH_SCORE:
		if((*user)){
			if((*user)->val.sts == IS_LOG_IN){
				strcpy(sql, "");
				strcpy(sql,"SELECT * FROM userscore ORDER BY score DESC limit 10;");
				printf("%s\n",sql);
				select_query(sql, on_get_score, &scoreinfo);
				printf("%s\n",scoreinfo);
				sprintf(out,"%d%s%s",SCORE_BOARD,SEPARATOR_CHAR,scoreinfo);
				return SEND_TO_ONE;
			}
		}
		return BLOCK;
	}
}

int receiveData(int s, char *buff, int size, int flags){
	int n;
	n = recv(s, buff, size, flags);
	if(n < 0)
		perror("Error: ");
	return n;
}

int sendData(int s, char *buff, int size, int flags){
	int n;
	n = send(s, buff, size, flags);
	if(n < 0)
		perror("Error: ");
	return n;
}
