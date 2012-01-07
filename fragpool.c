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

/** Find the fragment that starts at bp.  bp must be a non-null
 * pointer within the pool. */
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
    }
    lf = f;
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
  fp_fragment_t f;
  const fp_fragment_t fe = pool->fragment + pool->fragment_count;

  /* Validate arguments */
  if ((0 >= min_size) || (min_size > max_size) || (NULL == buffer_endp)) {
    return NULL;
  }
  f = find_best_fragment(pool, min_size, max_size);
  if (NULL == f) {
    return NULL;
  }
  if (((f+1) < fe) && (f->length > max_size)) {
    fp_size_t xl = f->length - max_size;
    fp_fragment_t nf = f;
    while ((++nf < fe) && (!FRAGMENT_IS_INACTIVE(nf))) {
      ;
    }
    if (nf < fe) {
      do {
	nf[0] = nf[-1];
      } while (--nf > f);
      f[1].start = f[0].start + max_size;
      f[1].length = xl;
      f[0].length -= xl;
    }
  }
  *buffer_endp = f->start + f->length;
  f->length = -f->length;
  return f->start;
}

static fp_fragment_t
merge_adjacent_available (fp_fragment_t f,
			  fp_fragment_t fe)
{
  fp_fragment_t nf = f+1;

  f->length += nf->length;
  while ((++nf < fe) && (! FRAGMENT_IS_INACTIVE(nf))) {
    nf[-1] = nf[0];
  }
  nf[-1].length = 0;
  return f;
}

int
fp_release (fp_pool_t p,
	    uint8_t* bp)
{
  fp_fragment_t f = get_fragment(p, bp);
  fp_fragment_t nf;
  const fp_fragment_t fe = p->fragment + p->fragment_count;
  
  if ((NULL == f) || (! FRAGMENT_IS_ALLOCATED(f))) {
    return FP_EINVAL;
  }
  f->length = -f->length;
  if ((p->fragment < f) && FRAGMENT_IS_AVAILABLE(f-1)) {
    f = merge_adjacent_available(f-1, fe);
  }
  nf = f+1;
  if ((nf < fe) && FRAGMENT_IS_AVAILABLE(nf)) {
    (void)merge_adjacent_available(f, fe);
  }
  return 0;
}

fp_fragment_t
fp_get_fragment (fp_pool_t p,
		 uint8_t* bp)
{
  return get_fragment(p, bp);
}


fp_fragment_t
fp_find_best_fragment (fp_pool_t p,
		       fp_size_t min_size,
		       fp_size_t max_size)
{
  return find_best_fragment(p, min_size, max_size);
}

fp_fragment_t
fp_merge_adjacent_available (fp_fragment_t f,
			     fp_fragment_t fe)
{
  return merge_adjacent_available (f, fe);
}
