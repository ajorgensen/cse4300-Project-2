#include <stdio.h>
#include <sys/types.h>

int main(void){
	pid_t child_pid; //variable to hold the pid of the child process
	pid_t grandchild_pid;
	printf("\nBegin\n");
	
	child_pid = fork();
	
	if(child_pid >= 0) {
	  //pid >= 0 fork is successful
	  if(child_pid == 0) {
	    //child_pid == 0 we are in the child process
	    grandchild_pid = fork(); //fork another subprocess to create a grandchild
	    if(grandchild_pid >= 0) {
	      //fork successful
	      if(grandchild_pid == 0) {
	        //grandchild_pid == 0 we are in the grandchild process
	        printf("\n...In GrandChild process...\n");
			    printf("My pid: %d\n", getpid()); //print the pid of the grandchild
			    printf("My parent pid: %d\n", getppid()); //print the pid of the grandchild's parent (i.e. the child)
			    printf("My child pid: none\n"); //grandchild has no subprocess
	      }
	      else {
	        //we are in the child process
	        wait(); //wait for the grandchild to finish (for nicer output)
	        printf("\n...In Child process...\n");
			    printf("My pid: %d\n", getpid()); //print pid of the child
			    printf("My parent pid: %d\n", getppid()); //print the pid of the child's parent (i.e. the parent)
			    printf("My child pid: %d\n", grandchild_pid); //print the pid of the child's child (i.e. the grandchild)
	      }
	    }
	  }
	  else {
	    //we are in the parent process
	    wait(); //wait for child/grandchild to finish
	    printf("\n...In Parent process...\n");
			printf("My pid: %d\n", getpid()); //print pid of the parent'
			printf("My parent pid: %d\n", getppid()); //print pid of the parent's parent (i.e. the shell)
			printf("My child pid: %d\n", child_pid); //print the pid of the parent's child (i.e. the child)
			printf("\nEnd\n");
	  }
	}
	else {
	  //pid < 0 is an error
	  printf("Fork failed");
	}
	return 0;
}	
