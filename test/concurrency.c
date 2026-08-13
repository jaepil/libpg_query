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

int main() {
  size_t i;
  int ret;
  pthread_t threads[THREAD_COUNT];

  for (i = 0; i < THREAD_COUNT; i += 1) {
    ret = pthread_create(&threads[i], NULL, test_runner, NULL);
    if (ret) {
      perror("ERROR creating pthread");
      return 1;
    }
  }

  for (i = 0; i < THREAD_COUNT; i += 1) {
    pthread_join(threads[i], NULL);
  }

  test_thread_exit_key_is_reused();

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
