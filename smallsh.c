#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

//global variable to check if ^Z has been called 
int canEnterBackground = 1;

/***************************************************
 * Function Name: myExit
 * Description: This function exits the shell and
 * kills all background children
 **************************************************/
void myExit(pid_t childPID[], int numPID){
	if (numPID == 0){ ///originally a debugger, will change at end if time
		//printf("No children. Exiting!\n");
		//fflush(stdout);
	} 
	else {
		pid_t spawnPID = -5;
		int childExitMethod = -5;
		int i; //check the array of background PIDs and close all
		for(i = 0; i < numPID; i++){
			spawnPID = childPID[i];
			waitpid(spawnPID, &childExitMethod, 0);
		}
	}
	exit(0); 
}




/*******************************************************
 * Function Name: myStatus
 * Description: This function shows the status of the
 * last ran process and if it was terminated by 
 * a signal or exited
*****************************************************/
void myStatus(int statNum){
	if (statNum == 0){ //everything exited normally
		printf("exit value 0\n");
		fflush(stdout);
	}
	else{ //else something went wrong
		if (WIFEXITED(statNum) != 0){ //process killed itself
			int exitStatus = WEXITSTATUS(statNum);
			printf("exit value %d\n", exitStatus);
			fflush(stdout);
		}
		else if (WIFSIGNALED(statNum) != 0){ //process was terminated by a signal
			int termSignal = WTERMSIG(statNum);
			printf("terminated by signal %d\n", termSignal);
			fflush(stdout);
		}
	}
}



/******************************************************
 * Function Name: TSTP_handler
 * Description: This function changes the ability to put a process in
 * the background. If called, integer boolean is switched on
 * or off.
********************************************************/
void TSTP_handler(int signalNum){
	if (canEnterBackground == 1) { //need to turn off functionality
		char* foreGOnly = "Entering foreground-only mode (& is now ignored)\n";
		write(1, foreGOnly, 49); //have to use write for reentrancy
		fflush(stdout);
		canEnterBackground = 0; //can't enter background
	}	

	else { //turn back on functionality
		char* fgOnly = "Exiting foreground-only mode\n";
		write(1, fgOnly, 29);
		fflush(stdout);
		canEnterBackground = 1; //can enter background
	}
}
	
int main()
{
	char** myArgs;
	int statNum = 0;
	myArgs = malloc(512 * sizeof(char*));
	pid_t childPID[1000] = {-1};
	int index = 0;
	pid_t newChild = -5;
	int childExitMethod = -5;
	int numCharsEntered = -5;
	int currChar = -5;
	size_t bufferSize = 0;
	char* userLine = NULL;
	int background = 0;
	int numStatus = 0;

	signal(SIGINT, SIG_IGN); //need to ignore ^C in shell
	struct sigaction SIGTSTP_action = {0}; //create sig handler for ^Z
	SIGTSTP_action.sa_handler = TSTP_handler;
	sigfillset(&SIGTSTP_action.sa_mask);
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);
	
	while(1){ //run until exit is called
		background = 0; //reset variable for background use
		while(1){
			printf(": "); //prompt
			fflush(stdout);
			numCharsEntered = getline(&userLine, &bufferSize, stdin); //get user input
			if (numCharsEntered == -1)
			{
				clearerr(stdin);
			}
			else{
				break;
			}
		}
	
		char* token; //put arguments into array with strtok()
		int argNum = 0;
		char* fileFrom = NULL;
		char* fileTo = NULL;
		token = strtok(userLine, " \n"); //parse input string

		while (token != NULL) {
			if (strcmp("&", token) == 0){ //if parsed out ampersand
				background = 1; //go into background
				break;
			}
			else if (strcmp("<", token) == 0){ //if parsed out <
				token = strtok(NULL, " \n");
				fileFrom = token; //save next file into fileFrom
				token = strtok(NULL, " \n"); //for redirecting input
			}
			else if (strcmp(">", token) == 0){
				token = strtok(NULL, " \n"); //if parsed out >
				fileTo = token; //save next file into fileTo
				token = strtok(NULL, " \n"); //for redirecting output
			}
			else {
				myArgs[argNum] = token; //else it is an argument
				argNum += 1;  //save in array
				token = strtok(NULL, " \n");
			}
		}
		
		myArgs[argNum] = NULL; //last character of array needs to be NULL for exec
		int b;
		char* dolla = "$$";
		char newVar[25] = "";
		char* var3;
		//parse for $$ and expand if found
		for (b = 0; b < argNum; b++){
			if(strstr(myArgs[b], dolla) != NULL){ //if $$ found
				strcpy(newVar, myArgs[b]); //copy into temp var
				if (strcmp(dolla, newVar) == 0){ //if $$ is alone
					char pid1[10] = "";
					snprintf(pid1, 10, "%d", (int)getpid()); //cast pid into char
					var3 = pid1;
				}
				else{ //else if $$ was attached to a string
					char* token2;
					token2 = strtok(newVar, "$");//parse arg for $$, save only first part
					while (token2 != NULL) {
						var3 = token2; 
						token2 = strtok(NULL, "$");
					}
					pid_t myPID = getpid();
					char pid2[10] = "";
					snprintf(pid2, 10, "%d", (int)getpid()); //cast pid into char
					strcat(var3, pid2); //cat first part of orig string to pid
				}
				myArgs[b] = var3; //save var into arg array
			}
		}
		//if comment line
		if (strncmp("#", myArgs[0], strlen("#")) == 0){
			continue;
		}
		//if blank line
		else if (myArgs[0] == NULL){
			continue;
		}
		//if trying to get status of exited process
		else if (strcmp("status", myArgs[0]) == 0){
			background = 0;
			myStatus(statNum); //call myStatus with statNum
		}
		//if trying to change directories
		else if (strcmp("cd", myArgs[0]) == 0) {
			char curDir[256];
			if (myArgs[1] == NULL){
				chdir(getenv("HOME")); //PATH to HOME
			}
			else{
				char myDir[32] = "";
				strcpy(myDir, myArgs[1]); //copy directory name into var
				chdir(myDir); //go there
			}
			continue;
		}
		//exit shell
		else if (strcmp("exit", myArgs[0]) == 0){
			myExit(childPID, index); //kill zombies
		}

		//else do system commands
		else{
			newChild = fork(); //start new process
			switch (newChild){
				case -1: //fork failed
					printf("Fork failure!\n");
					fflush(stdout);
					exit(1);
					break;

				case 0:  //fork worked, new process clone
					signal(SIGINT, SIG_DFL); //if ^C, exit
					int redirectFrom = 0; //keep track of if files were used for background
					int redirectTo = 0;
			
					if (fileTo != NULL) { //if a file was parsed from args to put info into
						int target;
						int result;
						target = open(fileTo, O_WRONLY | O_CREAT | O_TRUNC, 0644); //open file
						if (target == -1) {  //if open fails
							printf("Cannot open file\n");
							fflush(stdout);
							exit(1);
						}
						result = dup2(target, 1); //redirect output into file
						redirectTo = 1; //save documentation of file redirect
						if (result == -1){ //if redirect failed
							printf("Could not redirect.\n");
							fflush(stdout);
							exit(1);
						}
						fcntl(target, F_SETFD, FD_CLOEXEC); //close file
					}
					if (fileFrom != NULL) { //if a file was parsed from args to pull info out of
						int source;
						int result2;
						source = open(fileFrom, O_RDONLY); //open file for reading
						if (source == -1){ //cant open file
							printf("Can't open file!\n");
							fflush(stdout);
							exit(1);
						}
						result2 = dup2(source, 0); //redirect iput from file
						redirectFrom = 1;
						if (result2 == -1){
							printf("Could not redirect.\n");
							fflush(stdout);
							exit(1);
						}
						fcntl(source, F_SETFD, FD_CLOEXEC); //close file
					}
					if ((canEnterBackground == 1) && (background == 1)){ //if background is available
						if (redirectFrom == 0){ //if no redirection from anywhere so far
							int result3;
							int source2;
							char* redir = "/dev/null"; //redirect to null
							source2 = open(redir, O_RDONLY);
							if (source2 == -1) {
								printf("Can't open NULL\n");
								fflush(stdout);
								exit(1);
							}
							result3 = dup2(source2, 0);
							if (result3 == -1){
								printf("Can't redirect to NULL\n");
								fflush(stdout);
								exit(1);
							}
							fcntl(source2, F_SETFD, FD_CLOEXEC);
						}
						if (redirectTo == 0){ //if no redirection to anywhere
							int result4;
							int target2;
							char* redir2 = "/dev/null";
							target2 = open(redir2, O_WRONLY); //redirect to null
							if (target2 == -1){
								printf("Can't open NULL\n");
								fflush(stdout);
								exit(1);
							}
							result4 = dup2(target2, 1);
							if (result4 == -1){
								printf("Can't redirect to NULL \n");
								fflush(stdout);
								exit(1);
							}
							fcntl(target2, F_SETFD, FD_CLOEXEC);
						}						
					}
					if (execvp(myArgs[0], myArgs) < 0){ //create completely different process with args given
						printf("Command cannot be executed!\n"); //if bad command
						fflush(stdout);
						exit(1);
					}
					exit(0); //if exited correctly
					break;
			
				default:	
					if (background != 1){ //if not a background command
						pid_t newPID2 = waitpid(newChild, &childExitMethod, 0); //wait for child to finish
						statNum = childExitMethod; //save exit status
						if (WIFSIGNALED(statNum) != 0){ //if terminated by a signal
							myStatus(statNum); //print which signal
						}
					}
					else { //background command
						printf("background pid %d\n", newChild); //tell which background command
						childPID[index] = newChild; //save background commands to later kill on exit
						index++;
					}
					break;
			}
			pid_t childCheck = -5;
			while((childCheck = waitpid(-1, &childExitMethod, WNOHANG)) > 0) { //keep checking for bg commands, but don't wait
				printf("background pid %d is done: ", childCheck); //once finished, let user know
				myStatus(childExitMethod); //show how exited
			}
		}	
	}
	free(myArgs);
	return 0;
}	
		
