#include <stdio.h>
#include <stdlib.h>
#include <time.h>


int main()
{
  srand(time(NULL));
  generate_random_matrix(4, 4);
  return 0;
}
int gen_rand()
{
  return (rand() % 10);
}
int generate_random_matrix(int m, int n)
{
  int array[m][n];
  int i, j;
  for (i=0; i<m; i++)
  {
    for (j=0; j<n; j++)
    {
      array[i][j] = gen_rand();
    }
  }
  print_random_array(&array[0][0], m, n);
  return array[0][0];
}

int print_random_array(int *array, int m, int n)
{
  int i, j;
  for (i=0; i<m; i++)
  {
    for (j=0; j<n; j++)
    {
      int av = array[i*n+j];
      if (j == 0)
      {
        printf("[ %d ", av);
      }
      else if (j == (n-1))
      {
        printf("%d ]\n", av);
      }
      else
      {
        printf("%d ", av);
      }
    }
  }
  return 0;
}
