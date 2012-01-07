#include <fragpool.h>
#include <CUnit/Basic.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#define FRAGMENT_IS_ALLOCATED(_f) (0 > (_f)->length)
#define FRAGMENT_IS_AVAILABLE(_f) (0 < (_f)->length)
#define FRAGMENT_IS_INACTIVE(_f) (0 == (_f)->length)

int init_suite (void) { return 0; }
int clean_suite (void) { return 0; }

#define POOL_SIZE 256
#define POOL_FRAGMENTS 6

static uint8_t data[POOL_SIZE];
FP_DEFINE_POOL(pool, data, POOL_FRAGMENTS);
fp_fragment_t fragment = pool_struct.fixed.fragment;

static void
show_fragments (fp_fragment_t f,
		   fp_fragment_t fe)
{
  do {
    if (FRAGMENT_IS_ALLOCATED(f)) {
      printf(" %u allocated at %p\n", -f->length, f->start);
    } else if (FRAGMENT_IS_AVAILABLE(f)) {
      printf(" %u available at %p\n", f->length, f->start);
    } else {
      printf(" unused fragment\n");
    }
  } while (++f < fe);
}

static void
show_pool (fp_pool_t p)
{
  fp_fragment_t f = p->fragment;
  const fp_fragment_t fe = f + p->fragment_count;
  printf("Pool %p with %u fragments and %u bytes from %p to %p:\n",
	 (void*)p, p->fragment_count, (fp_size_t)(p->pool_end-p->pool_start),
	 p->pool_start, p->pool_end);
  show_fragments(f, fe);
}

static void
show_short_pool (fp_pool_t p)
{
  int fi;
  for (fi = 0; fi < p->fragment_count; fi++) {
    printf(" %d@%u", p->fragment[fi].length, fi);
  }
}
#define SHOW_SHORT_POOL(_p) do { printf(__FILE__ ":%u:", __LINE__); show_short_pool(_p); putchar('\n'); } while (0)

#define CU_ASSERT_POOL_IS_RESET(_p) do {				\
    CU_ASSERT_EQUAL(p->fragment[0].start, p->pool_start);		\
    CU_ASSERT_EQUAL(p->fragment[0].length, (p->pool_end - p->pool_start)); \
    CU_ASSERT_EQUAL(0, fp_validate(_p));				\
  } while (0)
  							  \
static void
config_pool (fp_pool_t p, ...)
{
  fp_fragment_t f = p->fragment;
  const fp_fragment_t fe = f + p->fragment_count;
  va_list ap;
  int len;

  fp_reset(p);
  va_start(ap, p);
  while (f < fe) {
    len = va_arg(ap, int);
    if (FP_MAX_FRAGMENT_SIZE == abs(len)) {
      break;
    }
    f->length = len;
    f[1].start = f[0].start + abs(len);
    ++f;
  }
  if (f < fe) {
    f->length = p->pool_end - f->start;
    if (0 > len) {
      f->length = - f->length;
    }
  }
  va_end(ap);
}

#define RF_DONE_WITH_LEFTOVERS -2
#define RF_DONE -1
static void
release_fragments (fp_pool_t p, ...)
{
  fp_fragment_t f = p->fragment;
  va_list ap;
  int rc;
  int fi;

  va_start(ap, p);
  while (1) {
    fi = va_arg(ap, int);
    if (0 > fi) {
      break;
    }
    rc = fp_release(p, f[fi].start);
    CU_ASSERT_EQUAL(0, rc);
    CU_ASSERT_EQUAL(0, fp_validate(p));
  }
  if (RF_DONE == fi) {
    CU_ASSERT_POOL_IS_RESET(p);
  }
}

static void
execute_pool_ops (fp_pool_t p, ...)
{
  va_list ap;
  int cmd;

  va_start(ap, p);
  printf("\nExecuting commands on pool %p with %u fragments and total size %u:\n",
	 (void*)p, p->fragment_count, (unsigned int)(p->pool_end-p->pool_start));
  printf("initial state:");
  show_short_pool(p);
  putchar('\n');
  while ((cmd = va_arg(ap,int))) {
    switch (cmd) {
    case 'a': {			/* allocate: 'a' min_size max_size */
      int min_size = va_arg(ap, int);
      int max_size = va_arg(ap, int);
      uint8_t* b;
      uint8_t* be;
      
      printf("\tallocate %u..%u ... ", min_size, max_size);
      b = fp_request(p, min_size, max_size, &be);
      if (NULL != b) {
	printf("produced %p len %u\n", b, (unsigned int)(be-b));
      } else {
	printf("failed\n");
      }
      break;
    }
    case 'r': {			/* release: 'r' fragment_index */
      int fi = va_arg(ap, int);
      uint8_t* b = p->fragment[fi].start;
      int rc;
      printf("\trelease fragment %u at %p ... ", fi, b);
      rc = fp_release(p, b);
      printf("returned %d\n", rc);
      break;
    }
    case 'C': {			/* check length: 'C' fragment_index expected_length */
      int fi = va_arg(ap, int);
      int len = p->fragment[fi].length;
      int expected_len = va_arg(ap, int);
      
      printf("\tchecking fragment %u length %d expecting %d\n", fi, len, expected_len);
      CU_ASSERT_EQUAL(expected_len, len);
      break;
    }
    case 'V': {			/* validate: 'V' */
      printf("\tvalidating pool\n");
      CU_ASSERT_EQUAL(0, fp_validate(p));
      break;
    }
    case 'R': {			/* check reset: 'R' */
      printf("\tchecking pool is reset\n");
      CU_ASSERT_POOL_IS_RESET(p);
      break;
    }
    default:
      printf("Unrecognized command character '%c'\n", cmd);
      CU_FAIL("execute_pool_ops");
    }
    printf("\tpool:");
    show_short_pool(p);
    putchar('\n');
  }
  printf("completed execution of pool operations\n");
}

void
test_check_pool ()
{
  CU_ASSERT_EQUAL(sizeof(data), POOL_SIZE);
  CU_ASSERT_EQUAL(sizeof(data), pool->pool_end - pool->pool_start);
  CU_ASSERT_EQUAL(sizeof(pool_struct.fixed.fragment), POOL_FRAGMENTS*sizeof(struct fp_fragment_t));
}

void
test_fp_reset (void)
{
  fp_reset(pool);
  CU_ASSERT_EQUAL(pool->fragment[0].start, pool->pool_start);
  CU_ASSERT_EQUAL(pool->fragment[0].length, pool->pool_end - pool->pool_start);
}

void
test_fp_validate (void)
{
  CU_ASSERT_EQUAL(0, fp_validate(pool));
}

void
test_fp_request_params (void)
{
  fp_pool_t p = pool;
  uint8_t* bpe;
  CU_ASSERT_POOL_IS_RESET(p);
  CU_ASSERT_PTR_NULL(fp_request(p, 0, 0, &bpe));
  CU_ASSERT_PTR_NULL(fp_request(p, 0, FP_MAX_FRAGMENT_SIZE, &bpe));
  CU_ASSERT_PTR_NULL(fp_request(p, 1, 0, &bpe));
  CU_ASSERT_PTR_NULL(fp_request(p, POOL_SIZE, FP_MAX_FRAGMENT_SIZE, NULL));
}

void
test_fp_request (void)
{
  fp_pool_t p = pool;
  fp_fragment_t f = p->fragment;
  uint8_t* b;
  uint8_t* bpe;

  /* Check basic alloc */
  CU_ASSERT_POOL_IS_RESET(p);
  b = fp_request(p, POOL_SIZE, FP_MAX_FRAGMENT_SIZE, &bpe);
  CU_ASSERT_EQUAL(b, p->pool_start);
  CU_ASSERT_EQUAL(bpe, b + POOL_SIZE);

  /* Check that allocation finds first appropriately-sized block */
  config_pool(p, 32, -32, 64, -64, -FP_MAX_FRAGMENT_SIZE);
  /* 32@0 -32@1 64@2 -64@3 -64@4 0@5 */
  CU_ASSERT_EQUAL(0, fp_validate(p));
  b = fp_request(p, 24, 32, &bpe);
  /* -32@0 -32@1 64@2 -64@3 -64@4 0@5 */
  CU_ASSERT_EQUAL(b, p->pool_start);
  CU_ASSERT_EQUAL(bpe, b + 32);
  CU_ASSERT_EQUAL(p->pool_start+32, f[1].start);
  CU_ASSERT_EQUAL(-32, f[1].length);

  /* Check that allocation finds first appropriately-sized block
     taking maximum request into account: skip 32@0 and use 64@2 */
  config_pool(p, 32, -32, 64, -64, -FP_MAX_FRAGMENT_SIZE);
  /* 32@0 -32@1 64@2 -64@3 -64@4 0@5 */
  CU_ASSERT_EQUAL(0, fp_validate(p));
  b = fp_request(p, 24, 64, &bpe);
  /* 32@0 -32@1 -64@2 -64@3 -64@4 0@5 */
  CU_ASSERT_EQUAL(b, f[2].start);
  CU_ASSERT_EQUAL(bpe, f[2].start-f[2].length);
  CU_ASSERT_EQUAL(0, fp_validate(p));

  /* Check that allocation finds first appropriately-sized block
     taking maximum request into account: use 32@0 */
  config_pool(p, 32, -32, -64, -64, -FP_MAX_FRAGMENT_SIZE);
  /* 32@0 -32@1 -64@2 -64@3 -64@4 0@5 */
  CU_ASSERT_EQUAL(0, fp_validate(p));
  b = fp_request(p, 24, 64, &bpe);
  /* -32@0 -32@1 -64@2 -64@3 -64@4 0@5 */
  CU_ASSERT_EQUAL(b, f[0].start);
  CU_ASSERT_EQUAL(bpe, f[0].start-f[0].length);
  CU_ASSERT_EQUAL(0, fp_validate(p));

  /* Check that allocation reduces first appropriately-sized block, if
     fragments are available.  This one picks 64@2 and changes it to
     48@2 inserting a new fragment 24@3. */
  config_pool(p, 32, -32, 64, -64, -FP_MAX_FRAGMENT_SIZE);
  /* 32@0 -32@1 64@2 -64@3 -64@4 0@5 */
  CU_ASSERT_EQUAL(0, fp_validate(p));
  b = fp_request(p, 24, 48, &bpe);
  /* 32@0 -32@1 -48@2 16@3 -64@4 -64@5 */
  CU_ASSERT_EQUAL(b, p->pool_start+64);
  CU_ASSERT_EQUAL(bpe, b + 48);
  CU_ASSERT_EQUAL(bpe, f[3].start);
  CU_ASSERT_EQUAL(16, f[3].length);
  CU_ASSERT_EQUAL(0, fp_validate(p));

  /* This one picks 32@0, but can't insert an extra fragment because
   * all six slots are filled. */
  /* 32@0 -32@1 -48@2 16@3 -64@4 -64@5 */
  b = fp_request(p, 16, 24, &bpe);
  /* -32@0 -32@1 -48@2 16@3 -64@4 -64@5 */
  CU_ASSERT_EQUAL(b, p->pool_start);
  CU_ASSERT_EQUAL(bpe, b + 32);
  CU_ASSERT_EQUAL(bpe, f[1].start);
  CU_ASSERT_EQUAL(0, fp_validate(p));

  /* -32@1, -64@4, -32@0, -48@2, -64@5 */
  release_fragments(p, 1, 4, 0, 1, 1, RF_DONE);
}

void
test_fp_merge_adjacent_available ()
{
  fp_pool_t p = pool;
  fp_fragment_t f = p->fragment;
  fp_fragment_t fe = p->fragment + p->fragment_count;
  fp_fragment_t rf;

  config_pool(p, 64, 32, 64, FP_MAX_FRAGMENT_SIZE);
  rf = fp_merge_adjacent_available(f, fe);
  CU_ASSERT_EQUAL(rf, f);
  CU_ASSERT_PTR_EQUAL(f[0].start, data);
  CU_ASSERT_EQUAL(f[0].length, 96);
  CU_ASSERT_PTR_EQUAL(f[1].start, f[0].start+f[0].length);
  CU_ASSERT_EQUAL(f[1].length, 64);
  CU_ASSERT_EQUAL(f[2].length, (p->pool_end - f[2].start));

  config_pool(p, 64, 32, 64, FP_MAX_FRAGMENT_SIZE);
  rf = fp_merge_adjacent_available(f+1, fe);
  CU_ASSERT_EQUAL(rf, f+1);
  CU_ASSERT_PTR_EQUAL(f[0].start, data);
  CU_ASSERT_EQUAL(f[0].length, 64);
  CU_ASSERT_PTR_EQUAL(f[1].start, f[0].start+f[0].length);
  CU_ASSERT_EQUAL(f[1].length, 96);
  CU_ASSERT_EQUAL(f[2].length, (p->pool_end - f[2].start));
}

void
test_fp_get_fragment ()
{
  fp_pool_t p = pool;
  fp_fragment_t f = p->fragment;

  config_pool(p, 64, 32, 64, FP_MAX_FRAGMENT_SIZE);
  CU_ASSERT_PTR_EQUAL(f, fp_get_fragment(p, f->start));
  CU_ASSERT_PTR_EQUAL(f+1, fp_get_fragment(p, f[1].start));
  CU_ASSERT_PTR_EQUAL(f+1, fp_get_fragment(p, f[0].start + f[0].length));
  CU_ASSERT_PTR_EQUAL(f+2, fp_get_fragment(p, f[2].start));
  CU_ASSERT_PTR_NULL(fp_get_fragment(p, f->start+32));
}
  
void
test_fp_release_params ()
{
  fp_pool_t p = pool;
  fp_fragment_t f = p->fragment;

  config_pool(p, FP_MAX_FRAGMENT_SIZE);
  CU_ASSERT_EQUAL(FP_EINVAL, fp_release(p, NULL));
  CU_ASSERT_EQUAL(FP_EINVAL, fp_release(p, f[0].start));
}

void
test_fp_release ()
{
  fp_pool_t p = pool;
  fp_fragment_t f = p->fragment;
  int rv;

  config_pool(p, -10, -11, -12, -13, FP_MAX_FRAGMENT_SIZE);
  CU_ASSERT_EQUAL(0, fp_validate(p));

  CU_ASSERT_EQUAL(-11, f[1].length);
  rv = fp_release(p, f[1].start);
  CU_ASSERT_EQUAL(0, rv);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(11, f[1].length);

  CU_ASSERT_EQUAL(-10, f[0].length);
  rv = fp_release(p, f[0].start);
  CU_ASSERT_EQUAL(0, rv);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(21, f[0].length);

  CU_ASSERT_EQUAL(-12, f[1].length);
  rv = fp_release(p, f[1].start);
  CU_ASSERT_EQUAL(0, rv);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(33, f[0].length);

  CU_ASSERT_EQUAL(-13, f[1].length);
  rv = fp_release(p, f[1].start);
  CU_ASSERT_EQUAL(0, rv);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(POOL_SIZE, f[0].length);
  CU_ASSERT_EQUAL(0, f[1].length);

  config_pool(p, -10, -11, -FP_MAX_FRAGMENT_SIZE);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(-11, f[1].length);
  rv = fp_release(p, f[1].start);
  CU_ASSERT_EQUAL(0, rv);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(-10, f[0].length);
  CU_ASSERT_EQUAL(f[2].length, -(p->pool_end - f[2].start));

  rv = fp_release(p, f[2].start);
  CU_ASSERT_EQUAL(0, rv);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(-10, f[0].length);
  CU_ASSERT_EQUAL(f[1].length, (p->pool_end - f[1].start));
  CU_ASSERT_EQUAL(0, f[2].length);
}

void
test_execute_alloc ()
{
  fp_reset(pool);
  execute_pool_ops(pool,
		   'a', 16, 64,
		   'C', 0, -64,
		   'C', 1, 192,
		   'C', 2, 0,
		   'r', 0,
		   'R',
		   0);
}

int
main (int argc,
      char* argv[])
{
  CU_ErrorCode rc;
  CU_pSuite suite = NULL;
  typedef struct test_def {
    const char* name;
    void (*fn) (void);
  } test_def;
  const test_def tests[] = {
    { "check pool", test_check_pool },
    { "fp_reset", test_fp_reset },
    { "fp_validate", test_fp_validate },
    { "fp_request_params", test_fp_request_params },
    { "fp_request", test_fp_request },
    { "fp_merge_adjacent_available", test_fp_merge_adjacent_available },
    { "fp_get_fragment", test_fp_get_fragment },
    { "fp_release_params", test_fp_release_params },
    { "fp_release", test_fp_release },
    { "execute_alloc", test_execute_alloc },
  };
  const int ntests = sizeof(tests) / sizeof(*tests);
  int i;
  
  (void)show_pool;
  (void)show_short_pool;
  (void)release_fragments;

  rc = CU_initialize_registry();
  if (CUE_SUCCESS != rc) {
    fprintf(stderr, "CU_initialize_registry %d: %s\n", rc, CU_get_error_msg());
    return CU_get_error();
  }

  suite = CU_add_suite("basic", init_suite, clean_suite);
  if (! suite) {
    fprintf(stderr, "CU_add_suite: %s\n", CU_get_error_msg());
    goto done_registry;
  }

  for (i = 0; i < ntests; ++i) {
    const test_def* td = tests + i;
    if (! (CU_add_test(suite, td->name, td->fn))) {
      fprintf(stderr, "CU_add_test(%s): %s\n", td->name, CU_get_error_msg());
      goto done_registry;
    }
  }
  printf("Running tests\n");
  CU_basic_set_mode(CU_BRM_VERBOSE);
  CU_basic_run_tests();
  
 done_registry:
  CU_cleanup_registry();
  
  return CU_get_error();
}
