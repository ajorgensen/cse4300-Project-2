#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#define THREAD_COUNT 16
#define MATRIX_ROWS 1000
#define MATRIX_COLS 800

void *Function(void *);

int X;                                  /* global variable for summing */
int pt_index[THREAD_COUNT];             /* local 0-based thread index */
pthread_t thread_id[THREAD_COUNT];      /* to hold "real" thread IDs */
pthread_attr_t attr;                    /* thread attributes, NULL=use default */
pthread_mutex_t my_mutex;               /* MUTEX data structure for locking */

int matrix[MATRIX_ROWS][MATRIX_COLS];
int vvector[MATRIX_ROWS]; //double-v to prevent name-clash
int result_vector[MATRIX_ROWS];


int main()
{
  srand(time(NULL));
  int i, retval, j;
  pthread_t tid;
  time_t  t0, t1; /* time_t is defined on <time.h> and <sys/types.h> as long */
  clock_t c0, c1; /* clock_t is defined on <time.h> and <sys/types.h> as int */

  X = 0;
  pthread_attr_init(&attr);
  pthread_mutex_init(&my_mutex, NULL);
  
  for (i=0; i<MATRIX_ROWS; i++)
  {
    for (j=0; j<MATRIX_COLS; j++)
    {
      matrix[i][j] = gen_rand();
    }
  }
  for (i=0; i<MATRIX_ROWS; i++)
  {
    vvector[i] = gen_rand();
    result_vector[i] = 0;
  }
  printf("Matrix:\n");
/*  print_matrix(matrix);*/
  printf("Vector:\n");
/*  print_vector(vvector);*/
  printf("Main: threads to be created with X=%d\n", X);
  t0 = time(NULL);
  c0 = clock();
  for (i=0; i< THREAD_COUNT; i++)
  {
          pt_index[i] = i;
          retval = pthread_create(&tid, &attr, Function, (void *)pt_index[i]);
          printf("Main: creating i=%d, tid=%d, retval=%d\n", i, tid, retval);
          thread_id[i] = tid;
  }
  printf("\n\n\nJOINING\n\n\n");
  for (i=0; i< THREAD_COUNT; i++)
  {
          printf("Main: waiting for thread=%d to join\n", thread_id[i]);
          retval = pthread_join(thread_id[i], NULL);
          printf("Main: back from join, thread=%d, retval=%d\n", thread_id[i], retval);
  }
  t1 = time(NULL);
  c1 = clock();
  printf("Main: threads completed with X=%d\n", X);
  
  printf("\n\nResult:\n");
/*  print_vector(result_vector);*/

  printf ("elapsed wall clock time: %ld\n", (long) (t1 - t0));
  printf ("elapsed CPU time:        %f\n", (float) (c1-c0));
  return(0);
}

/* start routine for the threads */
void *Function(void *parm)
{
/*    existing code from professor's handout*/
/*        int me, self;*/

/*        me = (int) parm;                /* me = my own assigned thread ordinal */
/*        self = (int) pthread_self();    /* self = the Thread Library thread number */

/*        printf("Function me=%d: self=%d, X=%d\n", me, self, X);*/

/*        pthread_mutex_lock(&my_mutex);*/
/*                X = X + 15;*/
/*        pthread_mutex_unlock(&my_mutex);*/

/*        printf("Function me=%d: done with parm=%d, X=%d\n", me, self, X);*/


  /* call the DOTPRODUCT/SAXPY method from the thread */
  matvec_SAXPY(matrix, vvector, result_vector);
}

int matvec_DOTPRODUCT(int matrix[MATRIX_ROWS][MATRIX_COLS], int vvector[MATRIX_ROWS], int result[MATRIX_ROWS])
{
  int i, j; 

  for (i=0; i<MATRIX_ROWS; i++)
    for (j=0; j<MATRIX_COLS; j++)
    {
      pthread_mutex_lock(&my_mutex);
      result[i] = result[i] + matrix[i][j] * vvector[j];
      pthread_mutex_unlock(&my_mutex);
    }
  return 0;
}
int matvec_SAXPY(int matrix[MATRIX_ROWS][MATRIX_COLS], int vvector[MATRIX_ROWS], int result[MATRIX_ROWS])
{
  int i, j; 

  for (j=0; j<MATRIX_ROWS; j++)
    for (i=0; i<MATRIX_COLS; i++)
    {
      pthread_mutex_lock(&my_mutex);
      result[i] = result[i] + matrix[i][j] * vvector[j];
      pthread_mutex_unlock(&my_mutex);
    }
  return 0;
}
int gen_rand()
{
  return (rand() % 10);
}

int print_matrix(int array[MATRIX_ROWS][MATRIX_COLS])
{
  int i, j;
  for (i=0; i<MATRIX_ROWS; i++)
  {
    for (j=0; j<MATRIX_COLS; j++)
    {
      if (j == 0)
      {
        printf("[");
      }
      printf(" %d ", array[i][j]);
      if (j == (MATRIX_COLS-1))
      {
        printf("]\n");
      }
    }
  }
  return 0;
}
int print_vector(int array[MATRIX_ROWS])
{
  int i;
  for (i=0; i<MATRIX_ROWS; i++)
  {
    if (i == 0)
    {
      printf("[");
    }
    printf(" %d ", array[i]);
    if (i == (MATRIX_ROWS-1))
    {
      printf("]\n");
    }
  }
  return 0;
}
