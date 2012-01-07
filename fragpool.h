#ifndef FRAGPOOL_H_
#define FRAGPOOL_H_

#include <stdint.h>

typedef uint16_t fp_size_t;
typedef int16_t fp_ssize_t;

/** Bookkeeping for a fragment within the pool */
typedef struct fp_fragment_t {
  /** Address within the corresponding pool's memory space. */
  uint8_t* start;
  /** Length of the pool.  A negative value indicates that the start
   * address is valid and begins a sequence of abs(length) octets that
   * are allocated.  A positive value indicates that the start address
   * is valid and begins a sequence of length octets that are not
   * assigned.  A zero value indicates that the fragment is not
   * used. */
  fp_ssize_t length;
} *fp_fragment_t;

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
   * positive. */
  struct fp_fragment_t fragment[];
} *fp_pool_t;

/** Define storage for a pool.
 *
 * The rationale for the ugliness is the need to ensure alignment is
 * valid when using the generic type to reference a statically
 * allocated block of data.
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
  } _pool##_struct = {						\
    .fixed = {							\
      .pool_start = (uint8_t*)_data,				\
      .pool_end = sizeof(data) + (uint8_t*)data + sizeof(data),	\
      .fragment_count = _fragments				\
    }								\
  };								\
  fp_pool_t const _pool = &_pool##_struct.generic

/**  Reset the pool.  All memory is assigned to a single fragment
 * which is marked unallocated. */
void fp_reset (fp_pool_t pool);

uint8_t* fp_request (fp_pool_t pool,
		     fp_size_t min_size,
		     fp_size_t max_size,
		     uint8_t** fp_end);

uint8_t* fp_extend (fp_pool_t pool,
		    uint8_t* fp,
		    fp_size_t new_size,
		    uint8_t** fp_end);

uint8_t* fp_reallocate (fp_pool_t pool,
			uint8_t* fp,
			fp_size_t keep_size,
			fp_size_t new_size,
			uint8_t** fp_end);

int fp_validate (const fp_pool_t pool);

#endif /* FRAGPOOL_H_ */
