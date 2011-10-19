#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

//following read/write from/to pipe function concepts taken from GNU's libc manual http://www.gnu.org/s/hello/manual/libc/Creating-a-Pipe.html

//function takes as input the file descriptor int from the pipe[x]
void read_from_pipe (int file) {
  FILE *stream; //declare a FILE pointer for opening
  int c; //declare an integer for characters read from the stream
  stream = fdopen (file, "r"); //open the file descriptor for reading
  while((c = fgetc(stream)) != EOF && c != '\n') //read characters from input file until EOF or the last newline is hit
    putchar(c); //output the character read from input to the screen
  printf("\n"); //terminate the message read from the pipe with a newline
  fclose(stream); //close the file
}

//function takes as input a file descriptor int from pipe[x]
void write_to_pipe (int file, const char* message) {
  FILE *stream; //declare a FILE pointer for opening
  stream = fdopen (file, "w"); //open the file descriptor for writing
  fprintf (stream, "%s\n", message); //output the message to the file and terminate with a newline
  fclose (stream); //close the file
}
int main(void){
  pid_t child_pid; //variable to hold the pid of the child process
  pid_t grandchild_pid; //variable to hold pid of the grandchild process
  printf("\nBegin\n");
  
  int pc_pipe[2]; //pipe from parent-to-child
  int cg_pipe[2]; //pipe from child-to-grandchild
  int gp_pipe[2]; //pipe from grandchild-to-parent
  
  //create the pipes
  pipe(pc_pipe);
  pipe(cg_pipe);
  pipe(gp_pipe);
  
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
          close(gp_pipe[0]); //close the read end before writing to the grandchild-to-parent pipe
          write_to_pipe(gp_pipe[1], "can you hear me, major tom?"); //write a message to the pipe for the parent
          printf("\n...In GrandChild process...\n");
          printf("My pid: %d\n", getpid()); //print the pid of the grandchild
          printf("My parent pid: %d\n", getppid()); //print the pid of the grandchild's parent (i.e. the child)
          printf("My child pid: none\n"); //grandchild has no subprocess
          printf("Message from child: ");
          close(cg_pipe[1]); //close the write end before reading from the child-to-grandchild pipe
          read_from_pipe(cg_pipe[0]); //read the message the child wrote to the grandchild
        }
        else {
          //we are in the child process
          close(cg_pipe[0]); //close the read end before writing to the child-to-grandchild pipe
          write_to_pipe(cg_pipe[1], "your circuit's dead, there's something wrong"); //write a message to the pipe for the grandchild
          printf("\n...In Child process...\n");
          printf("My pid: %d\n", getpid()); //print pid of the child
          printf("My parent pid: %d\n", getppid()); //print the pid of the child's parent (i.e. the parent)
          printf("My child pid: %d\n", grandchild_pid); //print the pid of the child's child (i.e. the grandchild)
          printf("Message from parent: ");
          close(pc_pipe[1]); //close the write end before reading from the parent-to-child pipe
          read_from_pipe(pc_pipe[0]); //read the message the parent wrote to the child
          wait(); //wait for the grandchild process before allowing the parent to coninue
        }
      }
    }
    else {
      //we are in the parent process
      close(pc_pipe[0]); //close the read end before writing to the parent-to-child pipe
      write_to_pipe(pc_pipe[1], "ground control to major tom"); //write a message to the pipe for the child
      wait(); //wait for the child proceess
      printf("\n...In Parent process...\n");
      printf("My pid: %d\n", getpid()); //print pid of the parent'
      printf("My parent pid: %d\n", getppid()); //print pid of the parent's parent (i.e. the shell)
      printf("My child pid: %d\n", child_pid); //print the pid of the parent's child (i.e. the child)
      printf("Message from grandchild: ");
      close(gp_pipe[1]); //close the write end before reading from the grandchild-to-parent pipe
      read_from_pipe(gp_pipe[0]); //read the message that the grandchild wrote to the parent
      printf("\nEnd\n");
    }
  }
  else {
    //pid < 0 is an error
    printf("Fork failed");
  }
  return 0;
}  
