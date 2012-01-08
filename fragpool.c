#include <stdlib.h>
#include <string.h>
#include "fragpool.h"

#define FRAGMENT_IS_ALLOCATED(_f) (0 > (_f)->length)
#define FRAGMENT_IS_AVAILABLE(_f) (0 < (_f)->length)
#define FRAGMENT_IS_INACTIVE(_f) (0 == (_f)->length)

static inline
uint8_t* align_pointer_up (fp_pool_t p,
			   uint8_t* b)
{
  uintptr_t bi = (uintptr_t)b;
  bi = (bi + p->pool_alignment - 1) & ~(uintptr_t)(p->pool_alignment - 1);
  return (uint8_t*)bi;
}

static inline
uint8_t* align_pointer_down (fp_pool_t p,
			     uint8_t* b)
{
  uintptr_t bi = (uintptr_t)b;
  bi &= ~(uintptr_t)(p->pool_alignment - 1);
  return (uint8_t*)bi;
}

static inline
fp_ssize_t align_size_up (fp_pool_t p,
			  fp_ssize_t s)
{
  fp_size_t us = abs(s);
  us = (us + p->pool_alignment - 1) & ~(fp_size_t)(p->pool_alignment - 1);
  return (0 > s) ? -us : us;
}

static inline
fp_ssize_t align_size_down (fp_pool_t p,
			    fp_ssize_t s)
{
  fp_size_t us = abs(s);
  us &= ~(fp_size_t)(p->pool_alignment - 1);
  return (0 > s) ? -us : us;
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

/* Prefer a new fragment based on size if it's longer than the current
 * candidate and the candidate isn't at least the maximum desired
 * size, or it's shorter than the current candidate while still being
 * at least the maximum desired size. */
#define PREFER_NEW_SIZE(_new_len, _cur_len, _max_size)	\
  ((((_new_len) > (_cur_len))				\
    && ((_cur_len) < (_max_size)))			\
   || (((_new_len) < (_cur_len))			\
       && ((_new_len) >= (_max_size))))
       
/** Locate the best available fragment to use for the given allocation.
 *
 * Satisfactory fragments must be available and have at least min_size octets.
 *
 * The "best" of the satisfactory fragments is selected using the
 * PREFER_NEW_SIZE macro.  The goal is to come as close to the
 * requested maximum as possible with preference to being more than is
 * necessary.
 *
 * @param pool the pool from which memory is obtained
 * 
 * @param min_size the minimum size acceptable fragment
 * 
 * @param max_size the maximum size usable fragment
 *
 * @return the pointer to the best fragment, or a null pointer if no
 * satisfactory fragments are available.
 */
static fp_fragment_t
find_best_fragment (fp_pool_t p,
		    fp_size_t min_size,
		    fp_size_t max_size)
{
  fp_fragment_t f = p->fragment;
  const fp_fragment_t fe = f + p->fragment_count;
  fp_fragment_t bf = NULL;

  do {
    /* Candidate must be available (positive length) with at least the
       minimum size */
    if (min_size <= f->length) {
      /* Replace if we have no best fragment, or we like the new one
       * better. */
      if ((NULL == bf) || PREFER_NEW_SIZE(f->length, bf->length, max_size)) {
	bf = f;
      }
    }
  } while (++f < fe);

  return bf;
}

/** If a fragment slot is available, trim excess octets off the tail
 * of the provided fragment and make it available as a new fragment.
 *
 * @param f an allocated fragment with more space than it needs
 *
 * @param fe the end of the fragment array
 *
 * @param excess the number of trailing octets unneeded by f.  This
 * value must satisfy the pool alignment constraints.
 */
static void
release_suffix (fp_fragment_t f,
		fp_fragment_t fe,
		fp_size_t excess)
{
  fp_fragment_t nf = f+1;

  if (nf >= fe) {
    return;
  }
  if (FRAGMENT_IS_INACTIVE(nf)) {
    nf->length = excess;
    f->length += excess;
    nf->start = f->start - f->length;
  } else if (FRAGMENT_IS_AVAILABLE(nf)) {
    nf->length += excess;
    f->length += excess;
    nf->start -= excess;
  } else {
    while ((++nf < fe) && (!FRAGMENT_IS_INACTIVE(nf))) {
      ;
    }
    if (nf < fe) {
      do {
	nf[0] = nf[-1];
      } while (--nf > f);
      f[0].length += excess;
      f[1].start = f[0].start - f[0].length;
      f[1].length = excess;
    }
  }
}

/** Allocate the fragment.  If the fragment length is more than is
 * needed, attempt to release the suffix for separate allocation.
 *
 * @param p the pool being manipulated
 * 
 * @param f an available fragment, to be switched to allocated mode
 *
 * @param max_size the maximum size usable fragment
 * 
 * @param fragment_endp where to store the end of the fragment
 * 
 * @return a pointer to the start of the returned region, or a null
 * pointer if the allocation cannot be satisfied.  */
static uint8_t*
complete_allocation (fp_pool_t p,
		     fp_fragment_t f,
		     fp_size_t max_size,
		     uint8_t** fragment_endp)
{
  const fp_fragment_t fe = p->fragment + p->fragment_count;
  fp_size_t flen = f->length;
  
  f->length = -f->length;
  if (((f+1) < fe) && (FP_MAX_FRAGMENT_SIZE != max_size)) {
    max_size = align_size_up(p, max_size);
    if (flen > max_size) {
      release_suffix(f, fe, flen - max_size);
    }
  }
  *fragment_endp = f->start - f->length;
  return f->start;
}
		     
/** Extend the space of the provided fragment (allocated or available)
 * by the following fragment, which is then eliminated.
 *
 * @param f is a fragment (either allocated or available), and the
 * next fragment is available.
 * 
 * @param fe is the end of the fragment array
 */
static void
merge_adjacent_available (fp_fragment_t f,
			  fp_fragment_t fe)
{
  fp_fragment_t nf = f+1;

  if (FRAGMENT_IS_ALLOCATED(f)) {
    f->length -= nf->length;
  } else {
    f->length += nf->length;
  }
  while ((++nf < fe) && (! FRAGMENT_IS_INACTIVE(nf))) {
    nf[-1] = nf[0];
  }
  nf[-1].length = 0;
}

void
fp_reset (fp_pool_t p)
{
  p->fragment[0].start = align_pointer_up(p, p->pool_start);
  p->fragment[0].length = align_pointer_down(p, p->pool_end) - p->fragment[0].start;
  memset(p->fragment+1, 0, (p->fragment_count-1)*sizeof(*p->fragment));
}

uint8_t*
fp_request (fp_pool_t p,
	    fp_size_t min_size,
	    fp_size_t max_size,
	    uint8_t** fragment_endp)
{
  fp_fragment_t f;

  /* Validate arguments */
  if ((0 >= min_size) || (min_size > max_size) || (NULL == fragment_endp)) {
    return NULL;
  }
  min_size = align_size_up(p, min_size);
  if (FP_MAX_FRAGMENT_SIZE != max_size) {
    max_size = align_size_up(p, max_size);
  }
  f = find_best_fragment(p, min_size, max_size);
  if (NULL == f) {
    return NULL;
  }
  return complete_allocation(p, f, max_size, fragment_endp);
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
    merge_adjacent_available(--f, fe);
  }
  nf = f+1;
  if ((nf < fe) && FRAGMENT_IS_AVAILABLE(nf)) {
    merge_adjacent_available(f, fe);
  }
  return 0;
}

uint8_t*
fp_resize (fp_pool_t p,
	   uint8_t* bp,
	   fp_size_t new_size,
	   uint8_t** fragment_endp)
{
  fp_fragment_t f = get_fragment(p, bp);
  fp_fragment_t fe = p->fragment + p->fragment_count;
  fp_fragment_t nf;
  
  if ((NULL == f) || (!FRAGMENT_IS_ALLOCATED(f))) {
    return NULL;
  }
  nf = f+1;
  if (nf < fe) {
    fp_size_t cur_size = - f->length;
    if (FP_MAX_FRAGMENT_SIZE == new_size) {
      if (FRAGMENT_IS_AVAILABLE(nf)) {
	merge_adjacent_available(f, fe);
      }
    } else {
      new_size = align_size_up(p, new_size);
      if (new_size < cur_size) {
	/* Give back, if possible */
	release_suffix(f, p->fragment + p->fragment_count, cur_size - new_size);
      } else if (new_size > cur_size) {
	/* Extend to following fragment? */
	if (FRAGMENT_IS_AVAILABLE(nf)) {
	  fp_size_t lacking = new_size - cur_size;
	  if (nf->length > lacking) {
	    /* More available than needed; take only what's requested */
	    nf->start += lacking;
	    nf->length -= lacking;
	    f->length -= lacking;
	  } else {
	    merge_adjacent_available(f, fe);
	  }
	}
      }
    }
  }
  *fragment_endp = f->start - f->length;
  return f->start;
}

uint8_t*
fp_reallocate (fp_pool_t p,
	       uint8_t* bp,
	       fp_size_t min_size,
	       fp_size_t max_size,
	       uint8_t** fragment_endp)
{
  fp_fragment_t f = get_fragment(p, bp);
  fp_fragment_t frs;
  fp_fragment_t fre;
  fp_size_t original_min_size;
  fp_size_t frlen;
  fp_fragment_t bf;
  fp_size_t bflen;
  const fp_fragment_t fe = p->fragment + p->fragment_count;
  fp_size_t copy_len;

  /* Validate arguments */
  if ((NULL == f) || (0 >= min_size) || (min_size > max_size) || (NULL == fragment_endp)) {
    return NULL;
  }

  original_min_size = min_size;
  min_size = align_size_up(p, min_size);
  if (FP_MAX_FRAGMENT_SIZE != max_size) {
    max_size = align_size_up(p, max_size);
  }

  /* Create hooks for a pseudo-slot at f0 for flen octets,
   * representing what would happen if this fragment were released. */
  frs = fre = f;
  frlen = -f->length;
  if ((frs > p->fragment) && FRAGMENT_IS_AVAILABLE(frs-1)) {
    frs = f-1;
    frlen += frs->length;
  }
  if (((fre+1) < fe) && FRAGMENT_IS_AVAILABLE(fre+1)) {
    fre = f+1;
    frlen += fre->length;
  }
  bf = NULL;
  bflen = 0;
  {
    fp_fragment_t xf = p->fragment;
    /* Same logic as find_best_fragment, but treat the sequence around
     * the current fragment as a single fragment */
    do {
      fp_ssize_t flen = xf->length;
      
      if (xf == frs) {
	flen = frlen;
      }
      if (min_size <= flen) {
	if ((NULL == bf) || PREFER_NEW_SIZE(flen, bflen, max_size)) {
	  bf = xf;
	  bflen = flen;
	}
      }
      if (xf == frs) {
	xf = fre;
      }
    } while (++xf < fe);
  }
  
  /* If nothing can satisfy the minimum, fail. */
  if (NULL == bf) {
    return NULL;
  }
  /* Save the minimum of the current fragment length and the desired
   * new size */
  copy_len = -f->length;
  if (copy_len > original_min_size) {
    copy_len = original_min_size;
  }
  /* If best is same fragment, just resize */
  if (bf == f) { /* == frs */
    return fp_resize(p, bp, max_size, fragment_endp);
  }
  /* If best is available fragment preceding this fragment, shift the
   * data. */
  if (bf == frs) {
    fp_size_t ffrs_len;
    fp_size_t new_len;

    if (f < fre) {
      merge_adjacent_available(f, fe);
    }
    memmove(frs->start, f->start, copy_len);
    ffrs_len = frs->length - f->length;
    new_len = ffrs_len;
    if (new_len > max_size) {
      new_len = max_size;
    }
    frs->length = -new_len;
    *fragment_endp = frs->start + new_len;
    if (ffrs_len == new_len) {
      while ((++f < fe) && (! FRAGMENT_IS_INACTIVE(f))) {
	f[-1] = f[0];
      }
      f[-1].length = 0;
    } else {
      f->start = *fragment_endp;
      f->length = ffrs_len - new_len;
    }
    return frs->start;
  }
  bp = complete_allocation(p, bf, max_size, fragment_endp);
  memmove(bp, f->start, copy_len);
  fp_release(p, f->start);
  return bp;
}

enum {
  FPVal_OK,
  FPVal_PoolBufferInvalid,
  FPVal_PoolAlignmentInvalid,
  FPVal_FragmentCountInvalid,
  FPVal_FragmentWrongStart,
  FPVal_FragmentLengthUnaligned,
  FPVal_FragmentUnmerged,
  FPVal_FragmentUsedPastEnd,
  FPVal_FragmentPoolLengthInconsistent,
};

int
fp_validate (fp_pool_t p)
{
  int size = 0;
  uint8_t* b;
  fp_fragment_t f = p->fragment;
  const fp_fragment_t fe = f + p->fragment_count;
  uint8_t* aps;
  uint8_t* ape;
  fp_fragment_t lf;

  if (p->pool_start >= p->pool_end) {
    return FPVal_PoolBufferInvalid;
  }
  if ((0 == p->pool_alignment) || (0 != (p->pool_alignment & (p->pool_alignment-1)))) {
    return FPVal_PoolAlignmentInvalid;
  }
  if (0 >= p->fragment_count) {
    return FPVal_FragmentCountInvalid;
  }
  aps = align_pointer_up(p, p->pool_start);
  ape = align_pointer_down(p, p->pool_end);
  b = aps;
  lf = NULL;
  do {
    /* Unused fragments have zero length and must be contiguous at
     * end */
    if (FRAGMENT_IS_INACTIVE(f)) {
      break;
    }
    /* Fragment must start where last one left off */
    if (f->start != b) {
      return FPVal_FragmentWrongStart;
    }
    /* Fragment length must satisfy alignment */
    if ((f->length != align_size_up(p, f->length))
	|| (f->length != align_size_down(p, f->length))) {
      return FPVal_FragmentLengthUnaligned;
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
      b += f->length;
    } else {
      size -= f->length;
      b -= f->length;
    }
  } while (++f < fe);
  /* Trailing (unused) fragments should have zero length */
  while (f < fe) {
    if (! FRAGMENT_IS_INACTIVE(f)) {
      return FPVal_FragmentUsedPastEnd;
    }
    ++f;
  }
  if (ape != b) {
    return FPVal_FragmentPoolLengthInconsistent;
  }
  return FPVal_OK;
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

void
fp_merge_adjacent_available (fp_fragment_t f,
			     fp_fragment_t fe)
{
  merge_adjacent_available (f, fe);
}
