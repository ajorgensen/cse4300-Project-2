#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

//following read/write from/to pipe function concepts taken from GNU's libc manual http://www.gnu.org/s/hello/manual/libc/Creating-a-Pipe.html
void read_from_pipe (int file) {
  FILE *stream;
  int c;
  stream = fdopen (file, "r");
  while( (c = fgetc(stream)) != EOF && c != '\n')
    putchar(c);
  printf("\n");
  fclose(stream);
}

void write_to_pipe (int file, const char* message) {
  FILE *stream;
  stream = fdopen (file, "w");
  fprintf (stream, "%s\n", message);
  fclose (stream);
}
int main(void){
  pid_t child_pid; //variable to hold the pid of the child process
  pid_t grandchild_pid;
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
          close(gp_pipe[0]);
          write_to_pipe(gp_pipe[1], "can you hear me, major tom?");
          //grandchild_pid == 0 we are in the grandchild process
          printf("\n...In GrandChild process...\n");
          printf("My pid: %d\n", getpid()); //print the pid of the grandchild
          printf("My parent pid: %d\n", getppid()); //print the pid of the grandchild's parent (i.e. the child)
          printf("My child pid: none\n"); //grandchild has no subprocess
          printf("Message from child: ");
          close(cg_pipe[1]);
          read_from_pipe(cg_pipe[0]);
        }
        else {
          close(cg_pipe[0]);
          write_to_pipe(cg_pipe[1], "your circuit's dead, there's something wrong");
          //we are in the child process
          printf("\n...In Child process...\n");
          printf("My pid: %d\n", getpid()); //print pid of the child
          printf("My parent pid: %d\n", getppid()); //print the pid of the child's parent (i.e. the parent)
          printf("My child pid: %d\n", grandchild_pid); //print the pid of the child's child (i.e. the grandchild)
          printf("Message from parent: ");
          close(pc_pipe[1]);
          read_from_pipe(pc_pipe[0]);
          wait();
        }
      }
    }
    else {
      close(pc_pipe[0]);
      write_to_pipe(pc_pipe[1], "ground control to major tom");
      wait();
      //we are in the parent process
      printf("\n...In Parent process...\n");
      printf("My pid: %d\n", getpid()); //print pid of the parent'
      printf("My parent pid: %d\n", getppid()); //print pid of the parent's parent (i.e. the shell)
      printf("My child pid: %d\n", child_pid); //print the pid of the parent's child (i.e. the child)
      printf("Message from grandchild: ");
      close(gp_pipe[1]);
      read_from_pipe(gp_pipe[0]);
      printf("\nEnd\n");
    }
  }
  else {
    //pid < 0 is an error
    printf("Fork failed");
  }
  return 0;
}  
