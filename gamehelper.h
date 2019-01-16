#define account_file "account.txt"
#define round1_ques_file "round1_ques.txt"
#define user_score_history_file "user_score_history.txt"

#define TIME_ANSWER_ROUND_1_QUES 30
#define TIME_ANSWER_ROUND_2_QUES 30
#define TOTAL_ROUND2_QUES 5


#define USER_MAX_LENGTH 32

#define NUM_FULL_ROOM 2

#define NEW_QUESTION_CHAR '*'
#define QUESTION_CHAR '@'
#define CHOICES_CHAR '#'
#define CHOICE_SEPERATE_CHAR "|"
#define ANSWER_CHAR '$'

#define STOP_GAME "S"
#define REMOVE_WRONG_ANSWER "H"


enum user_status{
	IS_LOG_IN,
	NOT_LOG_IN,
	IN_ROOM,
	IS_ROUND1_PLAYER,
	IS_ROUND2_PLAYER,
	FINISHED_GAME,
};

typedef struct question{
	char *ques;
	char *choices[4];
	char *answ;
}QUESTION;


int gameSetup();
int resetGameVariables();

int getNumOfRound1Questions(FILE *file);
QUESTION* getQuestions(FILE* file);

void printQuestions(QUESTION *listQues,int total);
int countScore(int ques_number);


int convertAnswerToNumber(char c);
char* removeTwoWrongAnswer(QUESTION ques);


int deleteElmentFromArray(int *arr,int *arr_size,int pos);
int randomQuestionNonRepeat(int *arr,int *arr_size);


char *getCurrentTime();
int savePlayerScore(char* username,int score,char *time);

void clearBuffer();