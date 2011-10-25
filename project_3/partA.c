#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#define THREAD_COUNT 2
#define M 4
#define N 4
#define K 0

void *Function(void *);

int pt_index[THREAD_COUNT];             /* local 0-based thread index */
pthread_t thread_id[THREAD_COUNT];      /* to hold "real" thread IDs */
pthread_attr_t attr;                    /* thread attributes, NULL=use default */
pthread_mutex_t my_mutex;               /* MUTEX data structure for locking */

int matrix[M][N];
int vvector[N]; //double-v to prevent name-clash
int result_vector[M];

int main()
{
  srand(time(NULL));
  int i, retval, j;
  pthread_t tid;
  time_t  t0, t1; /* time_t is defined on <time.h> and <sys/types.h> as long */
  clock_t c0, c1; /* clock_t is defined on <time.h> and <sys/types.h> as int */

  pthread_attr_init(&attr);
  pthread_mutex_init(&my_mutex, NULL);
  
/*  generate values for the M-by-N matrix*/
  for (i=0; i<M; i++)
  {
    for (j=0; j<N; j++)
    {
      matrix[i][j] = gen_rand();
    }
  }
  
/*  generate values for the N-by-1 vector and initialize the result vector to <0,0,...,0>*/
  for (i=0; i<N; i++)
  {
    vvector[i] = gen_rand();
    result_vector[i] = 0;
  }
  
/*  start timing, create threads */
  printf("Main: threads to be created.");
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
  
/*  join threads, stop timing*/
  for (i=0; i< THREAD_COUNT; i++)
  {
          printf("Main: waiting for thread=%d to join\n", thread_id[i]);
          retval = pthread_join(thread_id[i], NULL);
          printf("Main: back from join, thread=%d, retval=%d\n", thread_id[i], retval);
  }
  t1 = time(NULL);
  c1 = clock();
  printf("Main: threads completed.");
  
/*  printf("Matrix:\n");*/

/*  print out the M-by-N matrix*/
/*  for (i=0; i<M; i++)*/
/*  {*/
/*    for (j=0; j<N; j++)*/
/*    {*/
/*      if (j == 0)*/
/*      {*/
/*        printf("[");*/
/*      }*/
/*      printf(" %d ", matrix[i][j]);*/
/*      if (j == (N-1))*/
/*      {*/
/*        printf("]\n");*/
/*      }*/
/*    }*/
/*  }*/

/*  printf("Vector:\n");*/

/*  print out the N-by-1 vector*/
/*  for (i=0; i<N; i++)*/
/*  {*/
/*    if (i == 0)*/
/*    {*/
/*      printf("[");*/
/*    }*/
/*    printf(" %d ", vvector[i]);*/
/*    if (i == (N-1))*/
/*    {*/
/*      printf("]\n");*/
/*    }*/
/*  }*/

/*  printf("\n\nResult:\n");*/

/*  print out the M-by-1 result vector*/
/*  for (i=0; i<M; i++)*/
/*  {*/
/*    if (i == 0)*/
/*    {*/
/*      printf("[");*/
/*    }*/
/*    printf(" %d ", result_vector[i]);*/
/*    if (i == (M-1))*/
/*    {*/
/*      printf("]\n");*/
/*    }*/
/*  }*/


/*  display elapsed time*/
  printf ("elapsed wall clock time: %ld\n", (long) (t1 - t0));
  printf ("elapsed CPU time:        %f\n", (float) (c1-c0));
  return(0);
}

/* start routine for the threads */
void *Function(void *parm)
{
  int me, self;

  me = (int) parm;                /* me = my own assigned thread ordinal */
  self = (int) pthread_self();    /* self = the Thread Library thread number */
  int i, j;
  
/* declare a sub-matrix of the original for each thread containing N/THREAD_COUNT columns*/
  int split_matrix[M][N/THREAD_COUNT];
  
  printf("Function me=%d: self=%d", me, self);
  
/* populate the sub-matrix with values of the appropriate columns*/
  for (i=0; i<M; i++)
  {
    for (j=0; j<(N/THREAD_COUNT); j++)
    {
      split_matrix[i][j] = matrix[i][j+(me*THREAD_COUNT)];
    }
  }
/* call the subroutine (DOTPRODUCT or SAXPY) to aggregate results from each thread*/
  matvec_DOTPRODUCT(me, split_matrix, vvector, result_vector);

  printf("Function me=%d: done with parm=%d", me, self);
}

int matvec_DOTPRODUCT(int thread_id, int matrix[M][N/THREAD_COUNT], int vvector[N], int result[M])
{
  int i, j; 

  for (i=0; i<M; i++)
    for (j=0; j<(N/THREAD_COUNT); j++)
    {
      pthread_mutex_lock(&my_mutex);
      result[i] = result[i] + matrix[i][j] * vvector[j+(thread_id*THREAD_COUNT)];
      pthread_mutex_unlock(&my_mutex);
    }
  return 0;
}

int matvec_SAXPY(int thread_id, int matrix[M][N/THREAD_COUNT], int vvector[N], int result[M])
{
  int i, j; 

  for (j=0; j<(N/THREAD_COUNT); j++)
    for (i=0; i<M; i++)
    {
      pthread_mutex_lock(&my_mutex);
      result[i] = result[i] + matrix[i][j] * vvector[j];
      pthread_mutex_unlock(&my_mutex);
    }
  return 0;
}

/*generate random numbers [0, i) _0 inclusive to i exclusive_*/
int gen_rand()
{
  int i = 10;
  return (rand() % i);
}
