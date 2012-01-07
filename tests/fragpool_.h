#ifndef FRAGPOOL_INTERNAL_H_
#define FRAGPOOL_INTERNAL_H_

fp_fragment_t
fp_get_fragment (fp_pool_t p,
		 uint8_t* bp);

fp_fragment_t
fp_find_best_fragment (fp_pool_t p,
		       fp_size_t min_size,
		       fp_size_t max_size);

void
fp_merge_adjacent_available (fp_fragment_t f,
			     fp_fragment_t fe);

#endif /* FRAGPOOL_INTERNAL_H_ */
