#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <mqueue.h>
#define MAX_PID_LENGTH 10
#define MAXMSG 10
#define MAX_MSG_LENGTH 20
#define NEIGHBOURS_LIMIT 5
#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source),kill(0,SIGKILL),\
		     		     exit(EXIT_FAILURE))

char **neighbours;
int neighboursSize = 0;
int checkIfGivenPidInNeighbours(char* pid);
void prepend(char* s, const char* t);
void closeQueue(mqd_t *queue, char *queueId);
void initializeQueue(mqd_t *queue, char *queueId);
void sethandler( void (*f)(int, siginfo_t*, void*), int sigNo);
int countDigits(long number);
void createCurrentProcessPidString(char **myPid);
void printNeighbours();
void addNeighbour(char *neighbourPid);
void removeNeighbour(char* neighbourPid);
void parseNeighbourPidFromArgs(char **neighbourPid, char **argv);
void subscribeToMessageQueue(mqd_t *queue);
void mq_handler(int sig, siginfo_t *info, void *p);
void intHandler(int sig, siginfo_t *info, void *p);
char* concat(const char *s1, const char *s2);
void sendMessageToOneNeighbour(char* pid, char* s);
void sendMessageToAllNeighbours(char* pid, char* s);
void sendMessageToAllNeighboursWithoutCallingProcess(char* callingProcessPID, char* receivedMessage);
char* getCallingProcessPID(siginfo_t *info);
char* getMyProcessPID();

int main(int argc, char** argv) {
	char *neighbourPid = NULL;
	char *myPid;
	parseNeighbourPidFromArgs(&neighbourPid, argv);
	createCurrentProcessPidString(&myPid);
	printf("PID: [%s]\n\n", myPid);
	mqd_t queue;
	initializeQueue(&queue, myPid);
	sethandler(intHandler, SIGINT);
	sethandler(mq_handler, SIGRTMIN);
	subscribeToMessageQueue(&queue);
	mqd_t neighbourQueue;
	if (neighbourPid != NULL){
		initializeQueue(&neighbourQueue, neighbourPid);	
		if(TEMP_FAILURE_RETRY(mq_send(neighbourQueue,(const char*)myPid,strlen(myPid),3))) ERR("mq_send");
	}
	while(1){
		if(neighboursSize>0){
			char msg[MAX_MSG_LENGTH];
			char pid[MAX_PID_LENGTH];
			if (scanf("%10s %20s", pid, msg) == 2){
				prepend(msg, " ");
				msg[strlen(msg)] = '\0';
				char* s = concat(pid, msg);
				if(strlen(s)>MAX_PID_LENGTH+MAX_MSG_LENGTH+2) {
					printf("Message too long!\n");
					continue;
				}
				if(checkIfGivenPidInNeighbours(pid))
					sendMessageToOneNeighbour(pid, s);
				else if(isdigit(pid[0]))
					sendMessageToAllNeighbours(pid, s);
				else 
					printf("Please enter correct message [PID MSG]\n");
			}
		}
	}
	if (neighbourPid != NULL) closeQueue(&neighbourQueue, neighbourPid);
	closeQueue(&queue, myPid);
	return EXIT_SUCCESS;
}

void parseNeighbourPidFromArgs(char **neighbourPid, char **argv) {
	if (argv[1] != NULL && strlen(argv[1]) > 0) {
		*neighbourPid = (char*) malloc(sizeof(char) * strlen(argv[1]));
		if(!neighbourPid) ERR("malloc");
		strcpy(*neighbourPid, argv[1]);
		addNeighbour(*neighbourPid);
		printNeighbours();
	}
}

void initializeQueue(mqd_t *queue, char *queueId) {
	struct mq_attr attr;
	attr.mq_maxmsg = 10;
	attr.mq_msgsize = MAX_MSG_LENGTH;

	char *tempName = (char*) malloc(sizeof(char) * strlen(queueId));
	strcpy(tempName, queueId);
	prepend(tempName, "/");

	*queue = TEMP_FAILURE_RETRY(mq_open(
		tempName,
		O_RDWR | O_NONBLOCK | O_CREAT,
		0600, &attr
		));

	free(tempName);
	if (*queue == (mqd_t)-1) {
		ERR("Error opening queue");
	}
}

void subscribeToMessageQueue(mqd_t *queue) {
	static struct sigevent notification;
	notification.sigev_notify = SIGEV_SIGNAL;
	notification.sigev_signo = SIGRTMIN;
	notification.sigev_value.sival_ptr=queue;
	if(mq_notify(*queue, &notification) < 0) {
		ERR("mq_notify");
	}
}

void closeQueue(mqd_t *queue, char *queueId) {
	if (mq_close(*queue)) {
		ERR("Error closing queue");
	}

	char *tempName = (char*) malloc(sizeof(char) * strlen(queueId));
	strcpy(tempName, queueId);
	prepend(tempName, "/");

	if (mq_unlink(tempName))
		ERR("Error unlinkin queue");
}

void printNeighbours() {
	printf("My neighbours:\n");
	int i;
	for (i = 0; i < neighboursSize; i++) {
		printf("%d. [%s]\n",i + 1, neighbours[i]);
	}
	printf("\n");
}

void addNeighbour(char *neighbourPid) {
	neighboursSize++;
	if (neighbours == NULL) {
		neighbours = (char**) malloc(sizeof(char*) * neighboursSize);
	} else {	
		neighbours = (char**) realloc(neighbours, sizeof(char*) * neighboursSize);
	}
	if(!neighbours) ERR("malloc");
	neighbours[neighboursSize - 1] = (char*) malloc(sizeof(char) * strlen(neighbourPid));
	if(!neighbours[neighboursSize - 1]) ERR("malloc");
	strcpy(neighbours[neighboursSize-1], neighbourPid);
}

void removeNeighbour(char* neighbourPid){
	int i;
	for (i = 0; i < neighboursSize; i++){
		if(strcmp(neighbours[i], neighbourPid) == 0){
			int j;
			for(j=i;j<neighboursSize-1;j++){
				neighbours[i] = neighbours[i+1];
			}
			char** tmp = realloc(neighbours, (neighboursSize - 1)*sizeof(char*));
			if(tmp == NULL && neighboursSize > 1)
				ERR("Realloc error");
			neighboursSize = neighboursSize - 1;
			neighbours = tmp;
			printNeighbours();
		}
	}
}

int checkIfGivenPidInNeighbours(char* pid){
	int i;
	for (i = 0; i < neighboursSize; i++)
		if(strcmp(neighbours[i], pid) == 0)
			return 1;
	return 0;
}

void sendMessageToOneNeighbour(char* pid, char* s){
	mqd_t targetQueue;
	prepend(pid, "/");
	targetQueue = TEMP_FAILURE_RETRY(mq_open(pid,O_RDWR | O_NONBLOCK,0600));
	if (targetQueue == (mqd_t)-1) {
		ERR("Error opening queue");
	}
	if(TEMP_FAILURE_RETRY(mq_send(targetQueue, (const char*)s, strlen(s), 2))) ERR("mq_send");
	mq_close(targetQueue);
}

void sendMessageToAllNeighboursWithoutCallingProcess(char* callingProcessPID, char* receivedMessage){
	int i;
	//printf("Received message [%s]\n", receivedMessage);
	for (i = 0; i < neighboursSize; i++) {
		if(strcmp(neighbours[i],callingProcessPID)==0){
			continue;
		}
		char pid[MAX_PID_LENGTH];
		mqd_t targetQueue;
		strcpy(pid, neighbours[i]);
		prepend(pid, "/");
		targetQueue = TEMP_FAILURE_RETRY(mq_open(pid,O_RDWR | O_NONBLOCK,0600));
		if (targetQueue == (mqd_t)-1) {
			ERR("Error opening queue");
		}
		if(TEMP_FAILURE_RETRY(mq_send(targetQueue, (const char*)receivedMessage, strlen(receivedMessage), 2))) ERR("mq_send");
		printf("[%s] send to %s\n", receivedMessage, pid);
		mq_close(targetQueue);
	}
}

void sendMessageToAllNeighbours(char* pid, char* s){
	int i;
	for (i = 0; i < neighboursSize; i++) {
		mqd_t targetQueue;
		strcpy(pid, neighbours[i]);
		prepend(pid, "/");
		targetQueue = TEMP_FAILURE_RETRY(mq_open(pid,O_RDWR | O_NONBLOCK,0600));
		if (targetQueue == (mqd_t)-1) {
			ERR("Error opening queue");
		}
		if(TEMP_FAILURE_RETRY(mq_send(targetQueue, (const char*)s, strlen(s), 2))) ERR("mq_send");
		//printf("%s send to %s\n", s, pid);
		mq_close(targetQueue);
	}
}

void intHandler(int sig, siginfo_t *info, void *p) {
	int i;
	long sPid;
	sPid = (long )info->si_pid;
	char* callingProcessPID;
	callingProcessPID = (char*) malloc(sizeof(char) * countDigits(sPid));
	sprintf(callingProcessPID, "%ld", sPid);
	if (callingProcessPID[0] != '0'){
		removeNeighbour(callingProcessPID);
	}
	for (i = 0; i < neighboursSize; i++) {
		mqd_t queue;
		initializeQueue(&queue, neighbours[i]);
		printf("\nSending terminate singal to [%s]", neighbours[i]);
		long pid = strtol(neighbours[i], NULL, 10);
		if (kill(pid, SIGINT)) {
			ERR("Error sending kill to process\n");
		}
		closeQueue(&queue, neighbours[i]);
	}
	printf("\nTerminating...\n");
	exit(EXIT_SUCCESS);
}

void mq_handler(int sig, siginfo_t *info, void *p) {
	mqd_t *pin;
	char receivedMessage[MAX_MSG_LENGTH];
	pin = (mqd_t *)info->si_value.sival_ptr;
	subscribeToMessageQueue(pin);
	char* callingProcessPID = getCallingProcessPID(info);
	char* myPID = getMyProcessPID();
	for(;;){
		int bytesRead;
		if((bytesRead = mq_receive(*pin, receivedMessage, MAX_MSG_LENGTH, NULL))<1) {
			if(errno==EAGAIN) break;
			else ERR("mq_receive");
		}
		else {
			if(bytesRead<=strlen(myPID)){
				if (neighboursSize >= NEIGHBOURS_LIMIT) {
					printf("Maximal number of clients reached!\n");
					return;
				}
				addNeighbour(receivedMessage);
				printNeighbours();
			} else {
				char* destinationPid = (char*) malloc(sizeof(char) * strlen(myPID));
				strncpy(destinationPid, receivedMessage,strlen(myPID));
				if(strcmp(destinationPid,myPID)==0){
					printf("Received message [%s]\n", receivedMessage);
				} else {
					sendMessageToAllNeighboursWithoutCallingProcess(callingProcessPID, receivedMessage);
				}
			}
		}
	}
}

void sethandler( void (*f)(int, siginfo_t*, void*), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_sigaction = f;
	act.sa_flags=SA_SIGINFO;
	if (-1==sigaction(sigNo, &act, NULL)) ERR("sigaction");
}

char* concat(const char *s1, const char *s2){
    char *result = malloc(strlen(s1)+strlen(s2)+1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

void prepend(char* s, const char* t){
    size_t len = strlen(t);
    size_t i;

    memmove(s + len, s, strlen(s) + 1);

    for (i = 0; i < len; ++i)
    {
        s[i] = t[i];
    }
}

int countDigits(long number) {
	int count = 0;

	while(number != 0)
	{
		number /= 10;
		++count;
	}

    return count;
}

char* getCallingProcessPID(siginfo_t *info){
	long sPid;
	sPid = (long )info->si_pid;
	char* callingProcessPID;
	callingProcessPID = (char*) malloc(sizeof(char) * countDigits(sPid));
	sprintf(callingProcessPID, "%ld", sPid);
	return callingProcessPID;
}

char* getMyProcessPID(){
	long mPid;
	mPid = (long)getpid();
	char* myPID;
	myPID = (char*) malloc(sizeof(char) * countDigits(mPid));
	sprintf(myPID, "%ld", mPid);
	return myPID;
}

void createCurrentProcessPidString(char **myPid) {
	long pid = (long) getpid();
	*myPid = (char*) malloc(sizeof(char) * countDigits(pid));
	sprintf(*myPid, "%ld", pid);
}
