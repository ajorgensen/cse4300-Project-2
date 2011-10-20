#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#define THREAD_COUNT 16
#define M 1000
#define N 800
#define K 0

void *Function(void *);

int pt_index[THREAD_COUNT];             /* local 0-based thread index */
pthread_t thread_id[THREAD_COUNT];      /* to hold "real" thread IDs */
pthread_attr_t attr;                    /* thread attributes, NULL=use default */
pthread_mutex_t my_mutex;               /* MUTEX data structure for locking */

int matrix[M][N];
int vvector[N]; //double-v to prevent name-clash
int result_vector[N];


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
/*      printf(" %d ", array[i][j]);*/
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
/*    printf(" %d ", array[i]);*/
/*    if (i == (M-1))*/
/*    {*/
/*      printf("]\n");*/
/*    }*/
/*  }*/

/*  printf("\n\nResult:\n");*/

/*  print out the N-by-1 result vector*/
/*  for (i=0; i<N; i++)*/
/*  {*/
/*    if (i == 0)*/
/*    {*/
/*      printf("[");*/
/*    }*/
/*    printf(" %d ", array[i]);*/
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
/*  call the subroutine to perform matrix multiplication*/
  matvec_DOTPRODUCT(matrix, vvector, result_vector);
}

int matvec_DOTPRODUCT(int matrix[M][N], int vvector[N], int result[N])
{
  int i, j; 

  for (i=0; i<M; i++)
    for (j=0; j<N; j++)
    {
      pthread_mutex_lock(&my_mutex);
      result[i] = result[i] + matrix[i][j] * vvector[j];
      pthread_mutex_unlock(&my_mutex);
    }
  return 0;
}

int matvec_SAXPY(int matrix[M][N], int vvector[N], int result[N])
{
  int i, j; 

  for (j=0; j<N; j++)
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
