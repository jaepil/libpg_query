/*
 * Unified Query Algebra
 *
 * Copyright (c) 2023-2026 Cognica, Inc.
 */

#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include "pg_query.h"

static DWORD user_exit_key;
static volatile LONG callbacks;

static void parse_once(void)
{
	PgQueryParseResult result = pg_query_parse("SELECT 1");
	assert(result.error == NULL);
	pg_query_free_parse_result(result);
}

static void CALLBACK parse_from_thread_destructor(void *marker)
{
	assert(marker == &user_exit_key);
	parse_once();
	pg_query_exit();
	InterlockedIncrement(&callbacks);
}

static DWORD WINAPI parse_then_return(void *unused)
{
	assert(unused == NULL);
	parse_once();
	assert(FlsSetValue(user_exit_key, &user_exit_key));
	return 0;
}

int main(void)
{
	/* Initialize parser keys first; a later native destructor uses it again. */
	parse_once();
	pg_query_exit();
	user_exit_key = FlsAlloc(parse_from_thread_destructor);
	assert(user_exit_key != FLS_OUT_OF_INDEXES);
	for (unsigned i = 0; i < 1024; i++)
	{
		HANDLE thread = CreateThread(NULL, 0, parse_then_return, NULL, 0, NULL);
		assert(thread != NULL);
		assert(WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0);
		assert(CloseHandle(thread));
	}
	assert(callbacks == 1024);
	assert(FlsFree(user_exit_key));
	puts("1024 native Windows parser thread lifetimes and destructor reentries passed");
	return 0;
}
