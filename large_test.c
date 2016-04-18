#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE (4096)

int main()
{
  char *p = malloc(2000 * PAGE_SIZE);

  printf("%p\n", p);

  int i, j;
  char c;

  // touch every page at least once
  for (i = 0; i < 2000 * PAGE_SIZE; i++)
    c = p[i];

  // touch 5 pages many times
  for (i = 0; i < 50000; i++) {
    for (j = 0; j < 5 * PAGE_SIZE; j++) {
      c = p[j];
    }
  }

  printf("%c\n", c);

  return 0;


/*
  size_t bytes_allocated = 0;

  while (1) {
    void *p = malloc(45000000);
    if (p == NULL)
      break;
    bytes_allocated += 45000000;
    printf("%p %u\n", p, bytes_allocated);
  }

  printf("allocated %u total bytes\n", bytes_allocated);
*/
}
