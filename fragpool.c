#include <stdlib.h>
#include <string.h>
#include "fragpool.h"

#define FRAGMENT_IS_ALLOCATED(_f) (0 > (_f)->length)
#define FRAGMENT_IS_AVAILABLE(_f) (0 < (_f)->length)
#define FRAGMENT_IS_INACTIVE(_f) (0 == (_f)->length)

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
#endif

static fp_fragment_t
find_best_fragment (fp_pool_t p,
		    fp_size_t min_size,
		    fp_size_t max_size)
{
  fp_fragment_t f = p->fragment;
  const fp_fragment_t fe = f + p->fragment_count;
  fp_fragment_t bf = NULL;

  do {
    /* Candidate must be available with at least the minimum size */
    if (min_size <= f->length) {
      /* Replace if we have no best fragment, or if the new fragment
       * is longer than the current candidate and the candidate isn't
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
  uint8_t* bp;
  fp_fragment_t f = p->fragment;
  const fp_fragment_t fe = f + p->fragment_count;
  fp_fragment_t lf;

  if (p->pool_start >= p->pool_end) {
    return FPVal_PoolBufferInvalid;
  }
  if (0 >= p->fragment_count) {
    return FPVal_FragmentCountInvalid;
  }
  bp = p->pool_start;
  lf = NULL;
  do {
    /* Unused fragments have zero length and must be contiguous at
     * end */
    if (FRAGMENT_IS_INACTIVE(f)) {
      break;
    }
    /* Fragment must start where last one left off */
    if (f->start != bp) {
      return FPVal_FragmentWrongStart;
    }
    if (NULL != lf) {
      /* Adjacent available fragments should have been merged. */
      if (FRAGMENT_IS_AVAILABLE(lf) && FRAGMENT_IS_AVAILABLE(f)) {
	return FPVal_FragmentUnmerged;
      }
      lf = f;
    }
    if (FRAGMENT_IS_AVAILABLE(f)) {
      size += f->length;
      bp += f->length;
    } else {
      size -= f->length;
      bp -= f->length;
    }
  } while (++f < fe);
  /* Trailing (unused) fragments should have zero length */
  while (f < fe) {
    if (! FRAGMENT_IS_INACTIVE(f)) {
      return FPVal_FragmentUsedPastEnd;
    }
    ++f;
  }
  if (size != (p->pool_end - p->pool_start)) {
    return FPVal_FragmentPoolLengthInconsistent;
  }
  return FPVal_OK;
}


uint8_t*
fp_request (fp_pool_t pool,
	    fp_size_t min_size,
	    fp_size_t max_size,
	    uint8_t** buffer_endp)
{
  fp_fragment_t bf;
  const fp_fragment_t fe = pool->fragment + pool->fragment_count;
  fp_fragment_t nbf;

  /* Validate arguments */
  if ((0 >= min_size) || (min_size > max_size) || (NULL == buffer_endp)) {
    return NULL;
  }
  bf = find_best_fragment(pool, min_size, max_size);
  if (NULL == bf) {
    return NULL;
  }
  nbf = bf+1;
  if ((nbf < fe) && (bf->length > max_size)) {
    fp_size_t xl = bf->length - max_size;
    if (FRAGMENT_IS_INACTIVE(nbf)) {
      nbf->start = bf->start + max_size;
      nbf->length = xl;
      bf->length -= xl;
    } else if (FRAGMENT_IS_AVAILABLE(nbf)) {
      nbf->start -= xl;
      nbf->length += xl;
      bf->length -= xl;
    }
  }
  *buffer_endp = bf->start + bf->length;
  bf->length = -bf->length;
  return bf->start;
}
