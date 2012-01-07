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
#define POOL_FRAGMENTS 8

static uint8_t data[POOL_SIZE];
FP_DEFINE_POOL(pool, data, POOL_FRAGMENTS);
fp_fragment_t fragment = pool_struct.fixed.fragment;

void
test_check_pool ()
{
  CU_ASSERT_EQUAL(sizeof(data), POOL_SIZE);
  CU_ASSERT_EQUAL(sizeof(data), pool->pool_end - pool->pool_start);
  CU_ASSERT_EQUAL(sizeof(pool_struct.fixed.fragment), POOL_FRAGMENTS*sizeof(struct fp_fragment_t));
}

static void
fp_show_fragments (fp_fragment_t f,
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
fp_show_pool (fp_pool_t p)
{
  fp_fragment_t f = p->fragment;
  const fp_fragment_t fe = f + p->fragment_count;
  printf("Pool %p with %u fragments and %u bytes from %p to %p:\n",
	 (void*)p, p->fragment_count, (fp_size_t)(p->pool_end-p->pool_start),
	 p->pool_start, p->pool_end);
  fp_show_fragments(f, fe);
}

#define CU_ASSERT_POOL_IS_RESET(_p) do {				\
    CU_ASSERT_EQUAL(p->fragment[0].start, p->pool_start);		\
    CU_ASSERT_EQUAL(p->fragment[0].length, (p->pool_end - p->pool_start)); \
    CU_ASSERT_EQUAL(0, fp_validate(_p));					\
  } while (0)
  							  \
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
  uint8_t* b;
  uint8_t* bpe;

  CU_ASSERT_POOL_IS_RESET(p);
  b = fp_request(p, POOL_SIZE, FP_MAX_FRAGMENT_SIZE, &bpe);
  CU_ASSERT_EQUAL(b, p->pool_start);
  CU_ASSERT_EQUAL(bpe, b + POOL_SIZE);
  fp_reset(p);
}

static void
config_pool (fp_pool_t p, ...)
{
  fp_fragment_t f = p->fragment;
  const fp_fragment_t fe = f + p->fragment_count;
  va_list ap;
  int l;

  fp_reset(p);
  va_start(ap, p);
  while (f < fe) {
    l = va_arg(ap, int);
    if (FP_MAX_FRAGMENT_SIZE == abs(l)) {
      break;
    }
    f->length = l;
    f[1].start = f[0].start + abs(l);
    ++f;
  }
  if (f < fe) {
    f->length = p->pool_end - f->start;
    if (0 > l) {
      f->length = - f->length;
    }
  }
  va_end(ap);
}

static void
config_486 (fp_pool_t p)
{
  config_pool(p, 4, 8, 6, FP_MAX_FRAGMENT_SIZE);
}

void
test_fp_merge_adjacent_available ()
{
  fp_pool_t p = pool;
  fp_fragment_t f = p->fragment;
  fp_fragment_t fe = p->fragment + p->fragment_count;
  fp_fragment_t rf;

  config_486(p);
  rf = fp_merge_adjacent_available(f, fe);
  CU_ASSERT_EQUAL(rf, f);
  CU_ASSERT_PTR_EQUAL(f[0].start, data);
  CU_ASSERT_EQUAL(f[0].length, 12);
  CU_ASSERT_PTR_EQUAL(f[1].start, f[0].start+f[0].length);
  CU_ASSERT_EQUAL(f[1].length, 6);
  CU_ASSERT_EQUAL(f[2].length, (p->pool_end - f[2].start));

  config_486(p);
  rf = fp_merge_adjacent_available(f+1, fe);
  CU_ASSERT_EQUAL(rf, f+1);
  CU_ASSERT_PTR_EQUAL(f[0].start, data);
  CU_ASSERT_EQUAL(f[0].length, 4);
  CU_ASSERT_PTR_EQUAL(f[1].start, f[0].start+f[0].length);
  CU_ASSERT_EQUAL(f[1].length, 14);
  CU_ASSERT_EQUAL(f[2].length, (p->pool_end - f[2].start));
}

void
test_fp_get_fragment ()
{
  fp_pool_t p = pool;
  fp_fragment_t f = p->fragment;

  config_486(p);
  CU_ASSERT_PTR_EQUAL(f, fp_get_fragment(p, f->start));
  CU_ASSERT_PTR_EQUAL(f+1, fp_get_fragment(p, f[1].start));
  CU_ASSERT_PTR_EQUAL(f+2, fp_get_fragment(p, f[2].start));
  CU_ASSERT_PTR_NULL(fp_get_fragment(p, f->start+1));
}
  
void
test_fp_release_params ()
{
  fp_pool_t p = pool;
  fp_fragment_t f = p->fragment;

  config_486(p);
  CU_ASSERT_EQUAL(FP_EINVAL, fp_release(p, NULL));
  CU_ASSERT_EQUAL(FP_EINVAL, fp_release(p, f[0].start));
}

void
test_fp_release ()
{
  fp_pool_t p = pool;
  fp_fragment_t f = p->fragment;
  int rv;

  config_pool(p, -4, -8, -6, -9, FP_MAX_FRAGMENT_SIZE);
  CU_ASSERT_EQUAL(0, fp_validate(p));

  CU_ASSERT_EQUAL(-8, f[1].length);
  rv = fp_release(p, f[1].start);
  CU_ASSERT_EQUAL(0, rv);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(8, f[1].length);

  CU_ASSERT_EQUAL(-4, f[0].length);
  rv = fp_release(p, f[0].start);
  CU_ASSERT_EQUAL(0, rv);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(12, f[0].length);

  CU_ASSERT_EQUAL(-6, f[1].length);
  rv = fp_release(p, f[1].start);
  CU_ASSERT_EQUAL(0, rv);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(18, f[0].length);

  CU_ASSERT_EQUAL(-9, f[1].length);
  rv = fp_release(p, f[1].start);
  CU_ASSERT_EQUAL(0, rv);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(POOL_SIZE, f[0].length);
  CU_ASSERT_EQUAL(0, f[1].length);

  config_pool(p, -4, -8, -FP_MAX_FRAGMENT_SIZE);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(-8, f[1].length);
  rv = fp_release(p, f[1].start);
  CU_ASSERT_EQUAL(0, rv);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(-4, f[0].length);
  CU_ASSERT_EQUAL(f[2].length, -(p->pool_end - f[2].start));

  rv = fp_release(p, f[2].start);
  CU_ASSERT_EQUAL(0, rv);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  CU_ASSERT_EQUAL(-4, f[0].length);
  CU_ASSERT_EQUAL(f[1].length, (p->pool_end - f[1].start));
  CU_ASSERT_EQUAL(0, f[2].length);

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
    { "fp_merge_adjacent_available", test_fp_merge_adjacent_available },
    { "fp_get_fragment", test_fp_get_fragment },
    { "fp_release_params", test_fp_release_params },
    { "fp_release", test_fp_release },
  };
  const int ntests = sizeof(tests) / sizeof(*tests);
  int i;
  
  (void)fp_show_pool;

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
