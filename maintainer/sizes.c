#include <stdio.h>
#include <stddef.h>
#include "fragpool.h"

static uint8_t data[256];
FP_DEFINE_POOL (pool8, data, 8);

int
main (int argc, char *argv[])
{
  printf ("sizeof(generic pool) = %u\n", (int)sizeof (struct fp_pool_t));
  printf ("sizeof(p.start) = %u\n", (int)sizeof (pool8->pool_start));
  printf ("sizeof(p.end) = %u\n", (int)sizeof (pool8->pool_end));
  printf ("offsetof(fragment) = %u\n", (int)offsetof (struct fp_pool_t,
                                                      fragment));
  printf ("sizeof(p8.fixed) = %u\n", (int)sizeof (pool8_struct.fixed));
  printf ("sizeof(p8.generic) = %u\n", (int)sizeof (pool8_struct.generic));
  printf ("sizeof(fragment) = %u\n", (int)sizeof (struct fp_fragment_t));
}
