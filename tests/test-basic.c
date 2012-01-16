#include <fragpool.h>
#include "fragpool_.h"
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

static uint8_t pool_data[POOL_SIZE];
static union {
  struct {
    FP_POOL_STRUCT_COMMON();
    struct fp_fragment_t fragment[POOL_FRAGMENTS];
  } fixed;
  struct fp_pool_t generic;
} pool_union = {
  .generic = {
    .pool_start = pool_data,
    .pool_end = pool_data + sizeof(pool_data),
    .pool_alignment = 1,
    .fragment_count = POOL_FRAGMENTS
  }
};
fp_pool_t const pool = &pool_union.generic;

static uint8_t apool_data[2+POOL_SIZE];
static union {
  struct {
    FP_POOL_STRUCT_COMMON();
    struct fp_fragment_t fragment[POOL_FRAGMENTS];
  } fixed;
  struct fp_pool_t generic;
} apool_union = {
  .generic = {
    .pool_start = apool_data+1,
    .pool_end = apool_data+1 + POOL_SIZE,
    .pool_alignment = 2,
    .fragment_count = POOL_FRAGMENTS
  }
};
fp_pool_t const apool = &apool_union.generic;

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

#define PO_ALLOCATE 'a'
#define PO_RELEASE 'r'
#define PO_RESET '0'
#define PO_RESIZE 'x'
#define PO_REALLOCATE 'm'
#define PO_FILL_FRAGMENT 'f'
#define PO_DISPLAY_POOL 'D'
#define PO_DISPLAY_FRAGMENT 'd'
#define PO_CHECK_FRAGMENT_LENGTH 'C'
#define PO_CHECK_FRAGMENT_CONTENT 'c'
#define PO_VALIDATE 'V'
#define PO_CHECK_IS_RESET 'R'
#define PO_END_COMMANDS 0
static void
execute_pool_ops (fp_pool_t p, const char* file, int lineno, ...)
{
  va_list ap;
  int cmd;

  va_start(ap, lineno);
  printf("\n%s:%u Executing commands on pool %p with %u fragments and total size %u:\n",
	 file, lineno,
	 (void*)p, p->fragment_count, (unsigned int)(p->pool_end-p->pool_start));
  printf("initial state:");
  show_short_pool(p);
  putchar('\n');
  while (PO_END_COMMANDS != ((cmd = va_arg(ap,int)))) {
    switch (cmd) {
    case PO_RESET: { 		/* reset: PO_RESET */
      printf("\treset pool\n");
      fp_reset(p);
      memset(p->pool_start, '?', p->pool_end - p->pool_start);
      break;
    }
    case PO_ALLOCATE: {	 /* allocate: PO_ALLOCATE min_size max_size */
      int min_size = va_arg(ap, int);
      int max_size = va_arg(ap, int);
      uint8_t* b;
      uint8_t* be;
      
      printf("\tallocate %u..%u ... ", min_size, max_size);
      b = fp_request(p, min_size, max_size, &be);
      if (NULL != b) {
	fp_fragment_t f = fp_get_fragment(p, b);
	printf("produced %p len %u\n", b, (unsigned int)(be-b));
	memset(b, '0' + (f - p->fragment), be-b);
      } else {
	printf("failed\n");
      }
      break;
    }
    case PO_RELEASE: {	      /* release: PO_RELEASE fragment_index */
      int fi = va_arg(ap, int);
      uint8_t* b = p->fragment[fi].start;
      int rc;
      printf("\trelease fragment %u at %p ... ", fi, b);
      rc = fp_release(p, b);
      printf("returned %d\n", rc);
      break;
    }
    case PO_CHECK_FRAGMENT_LENGTH: { /* check length: PO_CHECK_FRAGMENT_LENGTH fragment_index expected_length */
      int fi = va_arg(ap, int);
      int len = p->fragment[fi].length;
      int expected_len = va_arg(ap, int);
      
      printf("\tchecking fragment %u length %d ... ", fi, len);
      if (expected_len == len) {
	printf("as expected\n");
      } else {
	printf("ERROR expected %d\n", expected_len);
	CU_FAIL("exec check fragment length");
      }
      break;
    }
    case PO_CHECK_FRAGMENT_CONTENT: { /* check content: PO_CHECK_FRAGMENT_CONTENT fragment_index char offset length(-1=all) */
      int fi = va_arg(ap, int);
      int fill_char = va_arg(ap, int);
      int offset = va_arg(ap, int);
      int len = va_arg(ap, int);
      fp_fragment_t f = p->fragment + fi;
      uint8_t* b = f->start;
      uint8_t* be = f->start + abs(f->length);
      if ((0 > len) || (len > abs(f->length))) {
	len = abs(f->length);
      }
      b += offset;
      if ((b + len) < be) {
	be = b + len;
      }
      printf("\tcheck %u in %p..%p in %d@%u against '%c' (0x%02x) ... ", (int)(be-b), b, be, f->length, fi, fill_char, fill_char);
      while (b < be) {
	if (fill_char != *b) {
	  break;
	}
	++b;
      }
      if (b < be) {
	printf("FAIL '%c' (%02x) at %p\n", *b, 0xFF & *b, b);
	CU_FAIL("content check");
      } else {
	printf("passed\n");
      }
      break;
    }

    case PO_VALIDATE: {		/* validate: PO_VALIDATE */
      int rc;
      printf("\tvalidating pool ... ");
      rc = fp_validate(p);
      if (0 == rc) {
	printf("succeeded\n");
      } else {
	printf("FAILED\n");
	CU_FAIL("pool validation");
      }
      break;
    }
    case PO_FILL_FRAGMENT: {	/* fill fragment: PO_FILL_FRAGMENT fragment_index char offset length(-1=all) */
      int fi = va_arg(ap, int);
      int fill_char = va_arg(ap, int);
      int offset = va_arg(ap, int);
      int len = va_arg(ap, int);
      fp_fragment_t f = p->fragment + fi;
      uint8_t* b = f->start + offset;
      uint8_t* be = f->start + abs(f->length);
      b += offset;
      if ((0 > len) || (len > abs(f->length))) {
	len = abs(f->length);
      }
      if ((b + len) < be) {
	be = b + len;
      }
      printf("\tfill %p..%p in %d@%u with '%c' (0x%02x)\n", b, be, f->length, fi, fill_char, fill_char);
      memset(b, fill_char, be-b);
      break;
    }
    case PO_DISPLAY_POOL: {	/* display pool: PO_DISPLAY_POOL */
      int fi;
      
      printf("\tPool %p with %u fragments and %u bytes from %p to %p:\n",
	     (void*)p, p->fragment_count, (fp_size_t)(p->pool_end-p->pool_start),
	     p->pool_start, p->pool_end);
      for (fi = 0; fi < p->fragment_count; ++fi) {
	fp_fragment_t f = p->fragment + fi;
	uint8_t* b = f->start;
	uint8_t* be = f->start + abs(f->length);
	
	printf("\t\t%u: ", fi);
	if (FRAGMENT_IS_INACTIVE(f)) {
	  printf("inactive fragment\n");
	  continue;
	}
	if (FRAGMENT_IS_ALLOCATED(f)) {
	  printf("%u allocated at %p: ", -f->length, f->start);
	} else {
	  printf("%u available at %p: ", f->length, f->start);
	}
	while (b < be) {
	  putchar(*b++);
	}
	putchar('\n');
      }
      break;
    }
    case PO_DISPLAY_FRAGMENT: {	/* display fragment: PO_DISPLAY_FRAGMENT fragment_index */
      int fi = va_arg(ap, int);
      fp_fragment_t f = p->fragment + fi;
      uint8_t* b = f->start;
      uint8_t* be = b + abs(f->length);

      printf("\tfragment %d@%u:\n\t\t", f->length, fi);
      while (b < be) {
	putchar(*b++);
      }
      putchar('\n');
      break;
    }
    case PO_CHECK_IS_RESET: {			/* check reset: PO_CHECK_IS_RESET */
      printf("\tchecking pool is reset\n");
      CU_ASSERT_POOL_IS_RESET(p);
      break;
    }
    case PO_RESIZE: {		/* resize fragment: PO_RESIZE fragment_index new_size */
      int fi = va_arg(ap, int);
      int len = va_arg(ap, int);
      fp_fragment_t f = p->fragment + fi;
      uint8_t* b;
      uint8_t* be;

      printf("\tresize fragment %d@%u to %u ... ", f->length, fi, len);
      b = fp_resize(p, f->start, len, &be);
      if (NULL != b) {
	printf("got %u at %p\n", (int)(be-b), b);
      } else {
	printf("failed\n");
      }
      break;
    }
    case PO_REALLOCATE: {	/* reallocate: PO_REALLOCATE fragment_index min_size max_size */
      int fi = va_arg(ap, int);
      int min_size = va_arg(ap, int);
      int max_size = va_arg(ap, int);
      uint8_t* b = p->fragment[fi].start;
      uint8_t* be;

      printf("\treallocate %d@%u %u..%u ... ", p->fragment[fi].length, fi, min_size, max_size);
      b = fp_reallocate(p, b, min_size, max_size, &be);
      if (NULL != b) {
	printf("produced %p len %u\n", b, (unsigned int)(be-b));
      } else {
	printf("failed\n");
      }
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
  CU_ASSERT_EQUAL(sizeof(pool_data), POOL_SIZE);
  CU_ASSERT_EQUAL(sizeof(pool_data), pool->pool_end - pool->pool_start);
  CU_ASSERT_EQUAL(sizeof(pool_union.fixed.fragment), POOL_FRAGMENTS*sizeof(struct fp_fragment_t));
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
  CU_ASSERT_PTR_NULL(fp_request(p, FP_MAX_FRAGMENT_SIZE, FP_MAX_FRAGMENT_SIZE, &bpe));
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

  config_pool(p, 64, 32, 64, FP_MAX_FRAGMENT_SIZE);
  fp_merge_adjacent_available(f, fe);
  CU_ASSERT_PTR_EQUAL(f[0].start, p->pool_start);
  CU_ASSERT_EQUAL(f[0].length, 96);
  CU_ASSERT_PTR_EQUAL(f[1].start, f[0].start+f[0].length);
  CU_ASSERT_EQUAL(f[1].length, 64);
  CU_ASSERT_EQUAL(f[2].length, (p->pool_end - f[2].start));

  config_pool(p, 64, 32, 64, FP_MAX_FRAGMENT_SIZE);
  fp_merge_adjacent_available(f+1, fe);
  CU_ASSERT_PTR_EQUAL(f[0].start, p->pool_start);
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
test_fp_resize_params ()
{
  fp_pool_t p = pool;
  fp_fragment_t f = p->fragment;
  uint8_t* bpe;

  fp_reset(p);
  CU_ASSERT_POOL_IS_RESET(p);
  CU_ASSERT_PTR_NULL(fp_resize(p, NULL, FP_MAX_FRAGMENT_SIZE, &bpe));
  CU_ASSERT_PTR_NULL(fp_resize(p, f->start, FP_MAX_FRAGMENT_SIZE, &bpe));
}

void
test_fp_reallocate_params ()
{
  fp_pool_t p = pool;
  fp_fragment_t f = p->fragment;
  uint8_t* bpe;

  fp_reset(p);
  CU_ASSERT_POOL_IS_RESET(p);
  CU_ASSERT_PTR_NULL(fp_reallocate(p, NULL, 2, 4, &bpe));
  config_pool(p, 32, -32, 64, -64, -FP_MAX_FRAGMENT_SIZE);
  CU_ASSERT_PTR_NULL(fp_reallocate(p, f[1].start, 4, 2, &bpe));
  CU_ASSERT_PTR_NULL(fp_reallocate(p, f[1].start, 2, 4, NULL));
  CU_ASSERT_PTR_NULL(fp_reallocate(p, f[0].start, 2, 4, &bpe));
  CU_ASSERT_PTR_NULL(fp_reallocate(p, f[1].start, 200, FP_MAX_FRAGMENT_SIZE, &bpe));
  CU_ASSERT_PTR_NOT_NULL(fp_reallocate(p, f[1].start, 2, 4, &bpe));
  fp_reset(p);
}

void
test_execute_alloc ()
{
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 16, 64,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -64,
		   PO_CHECK_FRAGMENT_LENGTH, 1, 192,
		   PO_CHECK_FRAGMENT_LENGTH, 2, 0,
		   PO_VALIDATE,
		   PO_RELEASE, 0,
		   PO_CHECK_IS_RESET,
		   PO_END_COMMANDS);

  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 32, 32,
		   PO_ALLOCATE, 32, 32,
		   PO_ALLOCATE, 32, 32,
		   PO_ALLOCATE, 32, 32,
		   PO_ALLOCATE, 32, 32,
		   PO_VALIDATE,
		   PO_CHECK_FRAGMENT_LENGTH, 5, POOL_SIZE-5*32,
		   PO_ALLOCATE, 32, 32,
		   PO_CHECK_FRAGMENT_LENGTH, 5, -(POOL_SIZE-5*32),
		   PO_END_COMMANDS);

  /* Verify preference for larger fragment if acceptable one doesn't
   * reach maximum requested */
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 30, 30,
		   PO_ALLOCATE, 2, 2,
		   PO_ALLOCATE, 62, 62,
		   PO_FILL_FRAGMENT, 2, 'x', 0, -1,
		   PO_ALLOCATE, 2, 2,
		   PO_RELEASE, 0,
		   PO_RELEASE, 2,
		   PO_ALLOCATE, 16, 48,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -48,
		   PO_CHECK_FRAGMENT_LENGTH, 3, 14,
		   PO_CHECK_FRAGMENT_LENGTH, 4, -2,
		   PO_END_COMMANDS);

  /* Verify preference for smaller fragment if still meets maximum
   * requested */
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 62, 62,
		   PO_ALLOCATE, 2, 2,
		   PO_ALLOCATE, 30, 30,
		   PO_FILL_FRAGMENT, 2, 'x', 0, -1,
		   PO_ALLOCATE, 2, 2,
		   PO_RELEASE, 0,
		   PO_RELEASE, 2,
		   PO_ALLOCATE, 16, 24,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -24,
		   PO_CHECK_FRAGMENT_LENGTH, 3, 6,
		   PO_CHECK_FRAGMENT_LENGTH, 4, -2,
		   PO_END_COMMANDS);
}

void
test_execute_release ()
{
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -64,
		   PO_CHECK_FRAGMENT_LENGTH, 1, -64,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -64,
		   PO_CHECK_FRAGMENT_LENGTH, 3, 64,
		   PO_VALIDATE,
		   PO_RELEASE, 0,
		   PO_RELEASE, 2,
		   PO_VALIDATE,
		   PO_CHECK_FRAGMENT_LENGTH, 0, 64,
		   PO_CHECK_FRAGMENT_LENGTH, 1, -64,
		   PO_CHECK_FRAGMENT_LENGTH, 2, 128,
		   PO_VALIDATE,
		   PO_RELEASE, 1,
		   PO_CHECK_IS_RESET,
		   PO_END_COMMANDS);

  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 32, 32,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 32, 32,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 32, 32,
		   PO_ALLOCATE, 32, 32,
		   PO_VALIDATE,
		   PO_CHECK_FRAGMENT_LENGTH, 5, -32,
		   PO_RELEASE, 5,
		   PO_CHECK_FRAGMENT_LENGTH, 5, 32,
		   PO_RELEASE, 0,
		   PO_CHECK_FRAGMENT_LENGTH, 0, 32,
		   PO_RELEASE, 4,
		   PO_CHECK_FRAGMENT_LENGTH, 4, 64,
		   PO_CHECK_FRAGMENT_LENGTH, 5, 0,
		   PO_VALIDATE,
		   PO_RELEASE, 2,
		   PO_CHECK_FRAGMENT_LENGTH, 2, 32,
		   PO_RELEASE, 3,
		   PO_CHECK_FRAGMENT_LENGTH, 2, 160,
		   PO_CHECK_FRAGMENT_LENGTH, 3, 0,
		   PO_CHECK_FRAGMENT_LENGTH, 4, 0,
		   PO_RELEASE, 1,
		   PO_CHECK_IS_RESET,
		   PO_END_COMMANDS);
}

void
test_execute_resize ()
{
  /* Shrink when following is available */
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 32, 64,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -64,
		   PO_RESIZE, 0, 48,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -48,
		   PO_VALIDATE,
		   PO_END_COMMANDS);

  /* Shrink when following is inactive */
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, POOL_SIZE, POOL_SIZE,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -POOL_SIZE,
		   PO_CHECK_FRAGMENT_LENGTH, 1, 0,
		   PO_RESIZE, 0, 48,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -48,
		   PO_VALIDATE,
		   PO_END_COMMANDS);

  /* Shrink when following is allocated */
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -64,
		   PO_CHECK_FRAGMENT_LENGTH, 1, -64,
		   PO_VALIDATE,
		   PO_RESIZE, 0, 48,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -48,
		   PO_CHECK_FRAGMENT_LENGTH, 1, 16,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -64,
		   PO_VALIDATE,
		   PO_END_COMMANDS);

  /* Expand when following is available and can satisfy request */
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 32, 64,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -64,
		   PO_CHECK_FRAGMENT_LENGTH, 1, 192,
		   PO_RESIZE, 0, 128,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -128,
		   PO_CHECK_FRAGMENT_LENGTH, 1, 128,
		   PO_VALIDATE,
		   PO_END_COMMANDS);

  /* Expand when following is available but cannot satisfy request */
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -64,
		   PO_CHECK_FRAGMENT_LENGTH, 1, -64,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -64,
		   PO_CHECK_FRAGMENT_LENGTH, 3, 64,
		   PO_RELEASE, 1,
		   PO_VALIDATE,
		   PO_RESIZE, 0, 192,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -128,
		   PO_VALIDATE,
		   PO_END_COMMANDS);

  /* Expand when following is active or inactive */
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 128, 128,
		   PO_ALLOCATE, 128, 128,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -128,
		   PO_CHECK_FRAGMENT_LENGTH, 1, -128,
		   PO_CHECK_FRAGMENT_LENGTH, 2, 0,
		   PO_RESIZE, 0, 192,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -128,
		   PO_RESIZE, 1, 192,
		   PO_CHECK_FRAGMENT_LENGTH, 1, -128,
		   PO_VALIDATE,
		   PO_END_COMMANDS);
}

void
test_execute_display ()
{
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 64, 64,
		   PO_DISPLAY_FRAGMENT, 0,
		   PO_FILL_FRAGMENT, 0, 'a', 0, -1,
		   PO_DISPLAY_FRAGMENT, 0,
		   PO_CHECK_FRAGMENT_CONTENT, 0, 'a', 0, -1,
		   PO_DISPLAY_POOL,
		   PO_END_COMMANDS);
}

void
test_execute_reallocate ()
{
  uint8_t* b;
  uint8_t* be;
  
  /* Extend into following fragment */
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_RELEASE, 1,
		   PO_DISPLAY_POOL,
		   PO_END_COMMANDS);
  b = fp_reallocate(pool, pool->fragment[0].start, 96, 128, &be);
  CU_ASSERT_PTR_EQUAL(b, pool->fragment[0].start);
  CU_ASSERT_EQUAL(128, (int)(be-b));
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_DISPLAY_POOL,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -128,
		   PO_CHECK_FRAGMENT_LENGTH, 1, -64,
		   PO_CHECK_FRAGMENT_LENGTH, 2, 64,
		   PO_CHECK_FRAGMENT_LENGTH, 3, 0,
		   PO_VALIDATE,
		   PO_END_COMMANDS);

  /* Move to end fragment, full use */
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_DISPLAY_POOL,
		   PO_END_COMMANDS);
  b = fp_reallocate(pool, pool->fragment[0].start, 96, 128, &be);
  CU_ASSERT_PTR_EQUAL(b, pool->fragment[2].start);
  CU_ASSERT_EQUAL(128, (int)(be-b));
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_DISPLAY_POOL,
		   PO_CHECK_FRAGMENT_LENGTH, 0, 64,
		   PO_CHECK_FRAGMENT_LENGTH, 1, -64,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -128,
		   PO_CHECK_FRAGMENT_CONTENT, 2, '0', 0, 64,
		   PO_CHECK_FRAGMENT_CONTENT, 2, '?', 64, -1,
		   PO_CHECK_FRAGMENT_LENGTH, 3, 0,
		   PO_VALIDATE,
		   PO_END_COMMANDS);

  /* Move to end fragment, partial use */
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_DISPLAY_POOL,
		   PO_END_COMMANDS);
  b = fp_reallocate(pool, pool->fragment[0].start, 32, 96, &be);
  CU_ASSERT_PTR_EQUAL(b, pool->fragment[2].start);
  CU_ASSERT_EQUAL(96, (int)(be-b));
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_DISPLAY_POOL,
		   PO_CHECK_FRAGMENT_LENGTH, 0, 64,
		   PO_CHECK_FRAGMENT_LENGTH, 1, -64,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -96,
		   PO_CHECK_FRAGMENT_CONTENT, 2, '0', 0, 32,
		   PO_CHECK_FRAGMENT_CONTENT, 2, '?', 32, -1,
		   PO_CHECK_FRAGMENT_LENGTH, 3, 32,
		   PO_CHECK_FRAGMENT_LENGTH, 4, 0,
		   PO_VALIDATE,
		   PO_END_COMMANDS);

  /* Move to preceding fragment */
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_RELEASE, 1,
		   PO_DISPLAY_POOL,
		   PO_END_COMMANDS);
  b = fp_reallocate(pool, pool->fragment[2].start, 96, 128, &be);
  CU_ASSERT_PTR_EQUAL(b, pool->fragment[1].start);
  CU_ASSERT_EQUAL(128, (int)(be-b));
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_DISPLAY_POOL,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -64,
		   PO_CHECK_FRAGMENT_CONTENT, 0, '0', 0, -1,
		   PO_CHECK_FRAGMENT_LENGTH, 1, -128,
		   PO_CHECK_FRAGMENT_CONTENT, 1, '2', 0, 64,
		   PO_CHECK_FRAGMENT_CONTENT, 1, '2', 64, 64,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -64,
		   PO_CHECK_FRAGMENT_CONTENT, 2, '3', 0, -1,
		   PO_CHECK_FRAGMENT_LENGTH, 3, 0,
		   PO_VALIDATE,
		   PO_END_COMMANDS);

  /* Move to preceding fragment, take part of following */
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 64, 64,
		   PO_ALLOCATE, 32, 32,
		   PO_ALLOCATE, 16, 16,
		   PO_RELEASE, 1,
		   PO_RELEASE, 3,
		   PO_DISPLAY_POOL,
		   PO_END_COMMANDS);
  b = fp_reallocate(pool, pool->fragment[2].start, 32, 160, &be);
  CU_ASSERT_PTR_EQUAL(b, pool->fragment[1].start);
  CU_ASSERT_EQUAL(160, (int)(be-b));
  execute_pool_ops(pool, __FILE__, __LINE__,
		   PO_DISPLAY_POOL,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -64,
		   PO_CHECK_FRAGMENT_CONTENT, 0, '0', 0, -1,
		   PO_CHECK_FRAGMENT_LENGTH, 1, -160,
		   PO_CHECK_FRAGMENT_CONTENT, 1, '2', 0, 32,
		   PO_CHECK_FRAGMENT_CONTENT, 1, '1', 32, 32,
		   PO_CHECK_FRAGMENT_CONTENT, 1, '2', 64, 64,
		   PO_CHECK_FRAGMENT_CONTENT, 1, '3', 128, 32,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -16,
		   PO_CHECK_FRAGMENT_CONTENT, 2, '4', 0, -1,
		   PO_CHECK_FRAGMENT_LENGTH, 3, 16,
		   PO_CHECK_FRAGMENT_LENGTH, 4, 0,
		   PO_VALIDATE,
		   PO_END_COMMANDS);
}

void
test_pool_alignment ()
{
  uint8_t alignment;
  fp_pool_t p = apool;
  uintptr_t ps;
  uintptr_t pe;
  fp_fragment_t f = p->fragment;
  uint8_t* b;
  uint8_t* be;

  /* Verify alignment is not universal, and that validation detects
     invalid alignments. */
  alignment = p->pool_alignment;
  CU_ASSERT_EQUAL(2, alignment);
  fp_reset(p);
  CU_ASSERT_EQUAL(0, fp_validate(p));
  p->pool_alignment = 0;
  CU_ASSERT_NOT_EQUAL(0, fp_validate(p));
  p->pool_alignment = 3;
  CU_ASSERT_NOT_EQUAL(0, fp_validate(p));
  p->pool_alignment = alignment;
  CU_ASSERT_EQUAL(0, fp_validate(p));

  /* Verify that pool is not aligned, but first fragment is. */
  ps = (uintptr_t)p->pool_start;
  pe = (uintptr_t)p->pool_end;
  CU_ASSERT_EQUAL(1, ps & 1);
  CU_ASSERT_PTR_NOT_EQUAL(p->pool_start, f->start);
  CU_ASSERT_EQUAL(1, pe & 1);
  CU_ASSERT_PTR_NOT_EQUAL(p->pool_end, f->start + f->length);

  b = fp_request(p, 3, 9, &be);
  CU_ASSERT_EQUAL(b, f->start);
  CU_ASSERT_EQUAL(be, f->start - f->length);
  CU_ASSERT_EQUAL(-10, f->length);
  CU_ASSERT_EQUAL(0, fp_validate(p));

  b = fp_resize(p, b, 15, &be);
  CU_ASSERT_EQUAL(b, f->start);
  CU_ASSERT_EQUAL(be, f->start - f->length);
  CU_ASSERT_EQUAL(-16, f->length);
  CU_ASSERT_EQUAL(0, fp_validate(p));

  b = fp_resize(p, b, FP_MAX_FRAGMENT_SIZE, &be);
  CU_ASSERT_EQUAL(b, f->start);
  CU_ASSERT_EQUAL(be, f->start - f->length);
  CU_ASSERT_EQUAL(-254, f->length);
  CU_ASSERT_EQUAL(0, f[1].length);
  CU_ASSERT_EQUAL(0, fp_validate(p));

  /* Verify validation can find fragment alignment violations */
  fp_reset(pool);
  CU_ASSERT_EQUAL(1, pool->pool_alignment);
  CU_ASSERT_NOT_EQUAL(1, alignment);
  pool->pool_alignment = alignment;
  CU_ASSERT_EQUAL(0, fp_validate(pool));
  pool->pool_alignment = 1;
  b = fp_request(pool, 3, 9, &be);
  show_pool(pool);
  CU_ASSERT_EQUAL(0, fp_validate(pool));
  pool->pool_alignment = alignment;
  CU_ASSERT_NOT_EQUAL(0, fp_validate(pool));
  pool->pool_alignment = 1;

  /* Verify reallocation wholesale move */
  execute_pool_ops(p, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 3, 9,
		   PO_ALLOCATE, 4, 9,
		   PO_ALLOCATE, 5, 9,
		   PO_RELEASE, 1,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -10,
		   PO_CHECK_FRAGMENT_LENGTH, 1, 10,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -10,
		   PO_REALLOCATE, 0, 7, 25,
		   PO_CHECK_FRAGMENT_LENGTH, 0, 20,
		   PO_CHECK_FRAGMENT_LENGTH, 1, -10,
		   PO_CHECK_FRAGMENT_CONTENT, 1, '2', 0, -1,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -26,
		   PO_CHECK_FRAGMENT_CONTENT, 2, '0', 0, 7,
		   PO_CHECK_FRAGMENT_CONTENT, 2, '?', 7, -1,
		   PO_END_COMMANDS);

  /* Verify reallocation resize */
  execute_pool_ops(p, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 3, 9,
		   PO_ALLOCATE, 4, 9,
		   PO_ALLOCATE, 5, 9,
		   PO_RELEASE, 1,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -10,
		   PO_CHECK_FRAGMENT_LENGTH, 1, 10,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -10,
		   PO_REALLOCATE, 0, 7, 17,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -18,
		   PO_CHECK_FRAGMENT_LENGTH, 1, 2,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -10,
		   PO_END_COMMANDS);
  
  /* Verify reallocation shift down */
  execute_pool_ops(p, __FILE__, __LINE__,
		   PO_RESET,
		   PO_ALLOCATE, 3, 9,
		   PO_ALLOCATE, 4, 9,
		   PO_ALLOCATE, 5, 9,
		   PO_ALLOCATE, 6, 9,
		   PO_RELEASE, 1,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -10,
		   PO_CHECK_FRAGMENT_LENGTH, 1, 10,
		   PO_CHECK_FRAGMENT_LENGTH, 2, -10,
		   PO_CHECK_FRAGMENT_LENGTH, 3, -10,
		   PO_REALLOCATE, 2, 7, 17,
		   PO_CHECK_FRAGMENT_LENGTH, 0, -10,
		   PO_CHECK_FRAGMENT_LENGTH, 1, -18,
		   PO_CHECK_FRAGMENT_CONTENT, 1, '2', 0, 7,
		   /* This next check verifies that the unaligned
		    * min_size was used to preserve data.  This is
		    * intentional. */
		   PO_CHECK_FRAGMENT_CONTENT, 1, '1', 7, 3,
		   PO_CHECK_FRAGMENT_CONTENT, 1, '2', 10, -1,
		   PO_CHECK_FRAGMENT_LENGTH, 2, 2,
		   PO_CHECK_FRAGMENT_LENGTH, 3, -10,
		   PO_END_COMMANDS);
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
    { "fp_resize_params", test_fp_resize_params },
    { "fp_reallocate_params", test_fp_reallocate_params },
    { "execute_alloc", test_execute_alloc },
    { "execute_release", test_execute_release },
    { "execute_resize", test_execute_resize },
    { "execute_display", test_execute_display },
    { "execute_reallocate", test_execute_reallocate },
    { "pool_alignment", test_pool_alignment },
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
