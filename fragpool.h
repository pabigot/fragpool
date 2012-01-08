#ifndef FRAGPOOL_H_
#define FRAGPOOL_H_

#include <stdint.h>

typedef uint16_t fp_size_t;
typedef int16_t fp_ssize_t;

/** The maximum size of a fragment.  This is intentionally a signed
 * value. */
#define FP_MAX_FRAGMENT_SIZE INT16_MAX

#define FP_EINVAL 1

/** Bookkeeping for a fragment within the pool.
 *
 * The fragment state is allocated if its memory has been made
 * available to a caller; available if its memory has been returned to
 * the pool; inactive if the pool partitions do not include this
 * fragment.
 *
 * @warning The only reason you get to see the internals is because
 * this is C and we need to statically allocate pools in user code.
 * You don't get to inspect or mutate the fields of this structure, so
 * any descriptive comments are irrelevant to you. */
typedef struct fp_fragment_t {
  /** Address within the corresponding pool's memory space. */
  uint8_t* start;
  /** Length of the pool.  A negative value indicates an allocated
   * fragment; a positive value indicates an available fragment; a
   * zero value indicates an inactive fragment. */
  fp_ssize_t length;
} *fp_fragment_t;

/** Bookkeeping for a fragment pool.
 * 
 * @warning The only reason you get to see the internals is because
 * this is C and we need to statically allocate pools in user code.
 * You don't get to inspect or mutate the fields of this structure, so
 * any descriptive comments are irrelevant to you. */
typedef struct fp_pool_t {
  /** The address of the start of the pool */
  uint8_t* pool_start;

  /** The address past the end of the pool.  The number of octets in
   * the pool is (pool_end-pool_start). */
  uint8_t* pool_end;

  /** The number of fragments supported by the pool */
  fp_size_t fragment_count;

  /** The fragment array.  In this generic structure this is a
   * flexible array member; it must be in a union with a compatible
   * fixed declaration in which the length of the array is
   * positive.
   *
   * The fragments partition the pool, starting with the first
   * fragment which begins at the pool start.  All inactive fragments
   * occur at the end.  At least one of any two adjacent active
   * fragments must be allocated (if two active available fragments
   * were adjacent, they should have been merged). */
  struct fp_fragment_t fragment[];
} *fp_pool_t;

/** Define storage for a pool.
 *
 * The rationale for the ugliness is the need to ensure alignment is
 * valid when using the generic type with a flexible array member to
 * reference a statically allocated block of data.
 *
 * @param _pool the name to be used for the fp_pool_t declaration.  A
 * static object with this name suffixed by "_union" will be allocated
 * to hold the pool infrastructure.
 *
 * @param _data normally a fixed-size array of uint8_t values.
 *
 * @param _fragments the number of fragments to be supported by the
 * pool.  This should two or larger.
 */
#define FP_DEFINE_POOL(_pool, _data, _fragments)		\
  static union {						\
    struct {							\
      uint8_t* pool_start;					\
      uint8_t* pool_end;					\
      fp_size_t fragment_count;					\
      struct fp_fragment_t fragment[_fragments];		\
    } fixed;							\
    struct fp_pool_t generic;					\
  } _pool##_union = {						\
    .fixed = {							\
      .pool_start = (uint8_t*)_data,				\
      .pool_end = sizeof(data) + (uint8_t*)data,		\
      .fragment_count = _fragments				\
    }								\
  };								\
  fp_pool_t const _pool = &_pool##_union.generic

/**  Reset the pool.  All memory is assigned to a single fragment
 * which is marked unallocated. */
void fp_reset (fp_pool_t pool);

/** Obtain a block of memory from the pool.
 *
 * A block of memory of at least min_size octets is allocated from the
 * pool and returned to the caller.  The value pointed to by
 * fragment_endp is updated to reflect the first byte past the end of
 * the allocated region.  The largest available fragment that does not
 * exceed max_size is returned.  If the requested maximum size is
 * larger than the selected fragment, and there are slots available,
 * the remainder is retained as an available fragment.
 * 
 * @param pool the pool from which memory is obtained
 * 
 * @param min_size the minimum size acceptable fragment
 * 
 * @param max_size the maximum size usable fragment
 * 
 * @param fragment_endp where to store the end of the fragment
 * 
 * @return a pointer to the start of the returned region, or a null
 * pointer if the allocation cannot be satisfied.  */
uint8_t* fp_request (fp_pool_t pool,
		     fp_size_t min_size,
		     fp_size_t max_size,
		     uint8_t** fragment_endp);

/** Attempt to resize a fragment in-place.
 *
 * This operation will release trailing bytes to the pool or attempt
 * to extend the fragment if the following fragment is available.
 *
 * If the new size is smaller, the excess will be returned to the pool
 * if possible.
 *
 * If the new size is larger and the following fragment is available,
 * the fragment will be extended to be no longer than new_size.  It
 * may be extended even if the requested new size cannot be satisfied.
 * 
 * The resize will not move any data.  The caller is responsible for
 * checking *fragment_endp to determine the effect of the resize.
 * 
 * @param pool the pool from which bp was allocated
 * 
 * @param bp the start of an allocated block returned by fp_request,
 * fp_resize, or fp_reallocate.
 * 
 * @param new_size the new desired size for the fragment.
 * 
 * @param fragment_endp where to store the end of the fragment
 *
 * @return fp if the resize succeeded, or a null pointer if an invalid
 * fragment or pool address was provided.  In either case, the
 * new_size octets beginning at fp are unchanged.  *fragment_endp is
 * updated to reflect the end of the fragment, whether the resize
 * succeeded or failed.
 */
uint8_t* fp_resize (fp_pool_t pool,
		    uint8_t* bp,
		    fp_size_t new_size,
		    uint8_t** fragment_endp);

/** Attempt to resize a fragment allowing moves.
 *
 * This operation will place the fragment in the best available
 * location, moving it if necessary to do so.  The expectation is this
 * operation is equivalent to saving the current fragment contents,
 * releasing the fragment, requesting a new fragment with the
 * specified characteristics, and initializing it with the old
 * fragment contents, but without requiring external storage for
 * what's currently in the fragment.
 *
 * If no satisfactory fragment can be found, the function returns a
 * null pointer, but the existing fragment is not affected.
 * 
 * @param pool the pool from which bp was allocated
 * 
 * @param bp the start of an allocated block returned by fp_request,
 * fp_resize, or fp_reallocate.
 * 
 * @param min_size the minimum acceptable size for a new fragment.  Up
 * to this many octets from the existing fragment are copied if the
 * new fragment begins at a different location.  The current fragment
 * may be smaller than this size.
 * 
 * @param new_size the new desired size for the fragment.
 * 
 * @param fragment_endp where to store the end of the fragment
 *
 * @return a pointer to the start of the returned region, or a null
 * pointer if the allocation cannot be satisfied. */
uint8_t* fp_reallocate (fp_pool_t pool,
			uint8_t* bp,
			fp_size_t min_size,
			fp_size_t max_size,
			uint8_t** fragment_endp);

/** Release a block of memory to the pool.
 *
 * @param pool the pool from which bp was allocated
 * 
 * @param bp the start of an allocated block returned by fp_request,
 * fp_resize, or fp_reallocate.
 * 
 * @return zero if the block is released, or an error code if bp is
 * invalid. */
int fp_release (fp_pool_t pool,
		uint8_t* bp);

/** Verify the integrity of the pool.  Return 0 if the pool is valid,
 * or an internal error code if an integrity test fails. */
int fp_validate (const fp_pool_t pool);


#endif /* FRAGPOOL_H_ */

