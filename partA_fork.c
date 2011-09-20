#include <stdio.h>
#include <sys/types.h>

int main(void){
	pid_t child_pid; //variable to hold the pid of the child process
	
	printf("\nBefore fork\n");
	
	/* create a new process */
	child_pid = fork();
	
	printf("\nAfter fork\n");
	
	if(child_pid >= 0){
		if(child_pid == 0){
			printf("\n...In Child process...\n");
			printf("My pid: %d\n", getpid());
			printf("My copied pid: %d\n", child_pid);
		} else {
			printf("\n...Parent process...\n");
			printf("My PID: %d\n", getpid());
			printf("My Childs pid: %d\n", child_pid);
		}
	} else {
		printf("Error");
	}
	
	return 0;
}	