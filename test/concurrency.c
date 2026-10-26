#include <pg_query.h>

#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_tests.c"

#ifdef USE_VALGRIND
#define THREAD_COUNT 50
#else
#define THREAD_COUNT 500
#endif

/* Bound live stacks on 32-bit hosts while retaining every thread lifetime. */
#define ACTIVE_THREAD_LIMIT 16

void* test_runner(void*);
void* parse_once(void*);

static void test_thread_exit_key_is_reused(void) {
  pthread_key_t probe_key;
  void* thread_result;
  int ret;

  for (size_t i = 0; i <= PTHREAD_KEYS_MAX; i += 1) {
    pthread_t thread;
    ret = pthread_create(&thread, NULL, parse_once, NULL);
    assert(ret == 0);
    ret = pthread_join(thread, &thread_result);
    assert(ret == 0);
    assert(thread_result == NULL);
  }

  ret = pthread_key_create(&probe_key, NULL);
  assert(ret == 0);
  pthread_key_delete(probe_key);
}

static void test_explicit_exit_reinitializes_cleanly(void) {
  for (size_t cycle = 0; cycle < 10; cycle++) {
    PgQueryParseResult result = pg_query_parse("SELECT 1");
    assert(result.error == NULL);
    pg_query_free_parse_result(result);
    pg_query_exit();
  }
}

static pthread_key_t parser_user_exit_key;

static void parse_from_thread_destructor(void *marker) {
  assert(marker == &parser_user_exit_key);
  assert(parse_once(NULL) == NULL);
  pg_query_exit();
}

static void *parse_before_thread_destructor(void *unused) {
  assert(unused == NULL);
  assert(parse_once(NULL) == NULL);
  assert(pthread_setspecific(parser_user_exit_key, &parser_user_exit_key) == 0);
  return NULL;
}

static void test_other_thread_destructor_can_parse(void) {
  /* Native keys already exist; this callback may run after their cleanup. */
  assert(pthread_key_create(&parser_user_exit_key, parse_from_thread_destructor) == 0);
  for (size_t cycle = 0; cycle < 16; cycle++) {
    pthread_t thread;
    assert(pthread_create(&thread, NULL, parse_before_thread_destructor, NULL) == 0);
    assert(pthread_join(thread, NULL) == 0);
  }
  assert(pthread_key_delete(parser_user_exit_key) == 0);
}

int main() {
  size_t i;
  int ret;
  pthread_t threads[ACTIVE_THREAD_LIMIT];

  for (size_t batch = 0; batch < THREAD_COUNT; batch += ACTIVE_THREAD_LIMIT) {
    size_t count = THREAD_COUNT - batch;
    if (count > ACTIVE_THREAD_LIMIT) count = ACTIVE_THREAD_LIMIT;
    for (i = 0; i < count; i += 1) {
      ret = pthread_create(&threads[i], NULL, test_runner, NULL);
      assert(ret == 0);
    }
    for (i = 0; i < count; i += 1) {
      ret = pthread_join(threads[i], NULL);
      assert(ret == 0);
    }
  }

  test_thread_exit_key_is_reused();
  test_explicit_exit_reinitializes_cleanly();
  test_other_thread_destructor_can_parse();

  printf("\n");

  return 0;
}

void* parse_once(void* unused_pthread_arg) {
  PgQueryParseResult result;
  bool passed;

  assert(unused_pthread_arg == NULL);
  result = pg_query_parse("SELECT 1");
  passed = result.error == NULL;
  pg_query_free_parse_result(result);

  return passed ? NULL : (void*) 1;
}

void* test_runner(void* unused_pthread_arg) {
  assert(unused_pthread_arg == NULL);
  size_t i;

  for (i = 0; tests[i]; i += 2) {
    PgQueryParseResult result = pg_query_parse(tests[i]);

		if (result.error) {
			printf("%s\n", result.error->message);
		} else if (strcmp(result.parse_tree, tests[i + 1]) == 0) {
      printf(".");
    } else {
      printf("INVALID result for \"%s\"\nexpected: %s\nactual: %s\n", tests[i], tests[i + 1], result.parse_tree);
    }

    pg_query_free_parse_result(result);
  }

  return NULL;
}
