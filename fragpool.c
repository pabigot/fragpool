#include <stdlib.h>
#include <string.h>
#include "fragpool.h"

void
fp_reset (fp_pool_t p)
{
  p->fragment[0].start = p->pool_start;
  p->fragment[0].length = p->pool_end - p->pool_start;
  memset(p->fragment+1, 0, (p->fragment_count-1)*sizeof(*p->fragment));
}

#if 0
static fp_fragment_t
get_fragment (fp_pool_t p,
	      uint8_t* bp)
{
  fp_fragment_t f = p->fragment;
  fp_fragment_t fe = f + p->fragment_count;
  do {
    if (f->start == bp) {
      return f;
    }
  } while (++f < fe);
  return NULL;
}

static fp_fragment_t
find_best_fragment (fp_pool_t p,
		    fp_size_t min_size,
		    fp_size_t max_size)
{
  fp_fragment_t f = p->fragment;
  const fp_fragment_t fe = f + p->fragment_count;
  fp_fragment_t bf = NULL;

  do {
    /* Candidate must be at least the minimum size */
    if (min_size <= f->length) {
      /* Replace if we have no best fragment, or if the new fragment
       * is longer than the current candidate if the candidate isn't
       * at least the maximum desired size */
      if ((NULL == bf)
	  || ((f->length > bf->length)
	      && (bf->length < max_size))) {
	bf = f;
      }
    }
  } while (++f < fe);

  return bf;
}
#endif

enum {
  FPVal_OK,
  FPVal_PoolBufferInvalid,
  FPVal_FragmentCountInvalid,
  FPVal_FragmentWrongStart,
  FPVal_FragmentUnmerged,
  FPVal_FragmentUsedPastEnd,
  FPVal_FragmentPoolLengthInconsistent,
};

int
fp_validate (fp_pool_t p)
{
  int size = 0;
  int last_len = 0;
  uint8_t* bp;
  fp_fragment_t f = p->fragment;
  const fp_fragment_t fe = f + p->fragment_count;

  if (p->pool_start >= p->pool_end) {
    return FPVal_PoolBufferInvalid;
  }
  if (0 >= p->fragment_count) {
    return FPVal_FragmentCountInvalid;
  }
  bp = p->pool_start;
  do {
    /* Unused fragments have zero length and must be contiguous at
     * end */
    if (0 == f->length) {
      break;
    }
    /* Fragment must start where last one left off */
    if (f->start != bp) {
      return FPVal_FragmentWrongStart;
    }
    if (0 != last_len) {
      /* Adjacent fragments must differ in whether they are used */
      if ((0 > last_len) == (0 > f->length)) {
	return FPVal_FragmentUnmerged;
      }
      last_len = f->length;
    }
    if (0 < f->length) {
      size += f->length;
      bp += f->length;
    } else {
      size -= f->length;
      bp -= f->length;
    }
  } while (++f < fe);
  /* Trailing (unused) fragments should have zero length */
  while (f < fe) {
    if (0 != f->length) {
      return FPVal_FragmentUsedPastEnd;
    }
    ++f;
  }
  if (size != (p->pool_end - p->pool_start)) {
    return FPVal_FragmentPoolLengthInconsistent;
  }
  return FPVal_OK;
}
