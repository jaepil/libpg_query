#include <pg_query.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

typedef struct {
	uint32_t namespace_oid;
	uint32_t type_oid;
	uint32_t shadow_type_oid;
	size_t namespace_lookups;
	size_t catalog_namespace_lookups;
	size_t type_name_lookups;
	size_t type_oid_lookups;
	size_t unqualified_int4_lookups;
	size_t qualified_int4_lookups;
	bool shadow_builtin_type;
	bool fail_type_lookup;
	bool return_invalid_type;
	const char *error;
} TestCatalogContext;

static PgQueryCatalogLookupResult
test_lookup_namespace(void *context, const char *schema_name,
					  uint32_t *namespace_oid)
{
	TestCatalogContext *catalog = context;

	catalog->namespace_lookups++;
	if (strcmp(schema_name, "pg_catalog") == 0)
	{
		catalog->catalog_namespace_lookups++;
		*namespace_oid = 11;
		return PG_QUERY_CATALOG_LOOKUP_FOUND;
	}
	if (strcmp(schema_name, "application_types") == 0)
	{
		*namespace_oid = catalog->namespace_oid;
		return PG_QUERY_CATALOG_LOOKUP_FOUND;
	}
	if (strcmp(schema_name, "public") == 0)
	{
		*namespace_oid = 2200;
		return PG_QUERY_CATALOG_LOOKUP_FOUND;
	}
	return PG_QUERY_CATALOG_LOOKUP_NOT_FOUND;
}

static void
write_test_type(const TestCatalogContext *catalog,
				const char *type_name,
				PgQueryPlpgsqlTypeMetadata *type)
{
	if (strcmp(type_name, "state") == 0)
	{
		*type = (PgQueryPlpgsqlTypeMetadata) {
			.oid = catalog->type_oid,
			.namespace_oid = catalog->namespace_oid,
			.name = "state",
			.length = -1,
			.by_value = false,
			.type_kind = 'e',
			.category = 'E',
			.alignment = 'i',
			.storage = 'x'
		};
		return;
	}
	if (strcmp(type_name, "int4") == 0)
	{
		*type = (PgQueryPlpgsqlTypeMetadata) {
			.oid = catalog->shadow_type_oid,
			.namespace_oid = catalog->namespace_oid,
			.name = "int4",
			.length = -1,
			.by_value = false,
			.type_kind = 'e',
			.category = 'E',
			.alignment = 'i',
			.storage = 'x'
		};
		return;
	}
	*type = (PgQueryPlpgsqlTypeMetadata) {
		.oid = strcmp(type_name, "foo") == 0 ? 900003 : 900004,
		.namespace_oid = 2200,
		.name = type_name,
		.length = -1,
		.by_value = false,
		.type_kind = 'c',
		.category = 'C',
		.alignment = 'd',
		.storage = 'x'
	};
}

static PgQueryCatalogLookupResult
test_lookup_type_by_name(void *context, const char *schema_name,
					 const char *type_name,
					 PgQueryPlpgsqlTypeMetadata *type)
{
	TestCatalogContext *catalog = context;

	catalog->type_name_lookups++;
	if (catalog->fail_type_lookup)
		return PG_QUERY_CATALOG_LOOKUP_ERROR;
	if (strcmp(type_name, "int4") == 0)
	{
		if (schema_name == NULL)
			catalog->unqualified_int4_lookups++;
		else if (strcmp(schema_name, "pg_catalog") == 0)
			catalog->qualified_int4_lookups++;
	}
	if (strcmp(type_name, "state") == 0)
	{
		if (schema_name != NULL
			&& strcmp(schema_name, "application_types") != 0)
			return PG_QUERY_CATALOG_LOOKUP_NOT_FOUND;
	}
	else if (catalog->shadow_builtin_type && strcmp(type_name, "int4") == 0)
	{
		if (schema_name != NULL
			&& strcmp(schema_name, "application_types") != 0)
			return PG_QUERY_CATALOG_LOOKUP_NOT_FOUND;
	}
	else if (strcmp(type_name, "foo") == 0
			 || strcmp(type_name, "dz_sumthing") == 0)
	{
		if (schema_name != NULL && strcmp(schema_name, "public") != 0)
			return PG_QUERY_CATALOG_LOOKUP_NOT_FOUND;
	}
	else
		return PG_QUERY_CATALOG_LOOKUP_NOT_FOUND;
	write_test_type(catalog, type_name, type);
	if (catalog->return_invalid_type)
		type->length = 0;
	return PG_QUERY_CATALOG_LOOKUP_FOUND;
}

static PgQueryCatalogLookupResult
test_lookup_type_by_oid(void *context, uint32_t type_oid,
					PgQueryPlpgsqlTypeMetadata *type)
{
	TestCatalogContext *catalog = context;
	const char *type_name;

	catalog->type_oid_lookups++;
	if (type_oid == catalog->type_oid)
		type_name = "state";
	else if (type_oid == catalog->shadow_type_oid)
		type_name = "int4";
	else if (type_oid == 900003)
		type_name = "foo";
	else if (type_oid == 900004)
		type_name = "dz_sumthing";
	else
		return PG_QUERY_CATALOG_LOOKUP_NOT_FOUND;
	write_test_type(catalog, type_name, type);
	return PG_QUERY_CATALOG_LOOKUP_FOUND;
}

static const char *
test_catalog_error(void *context)
{
	const TestCatalogContext *catalog = context;

	return catalog->error;
}

static bool
test_catalog_aware_plpgsql_parse(void)
{
	TestCatalogContext context = {
		.namespace_oid = 900001,
		.type_oid = 900002,
		.shadow_type_oid = 900005,
		.error = "test catalog lookup failed"
	};
	const PgQueryPlpgsqlCatalog catalog = {
		.context = &context,
		.lookup_namespace = test_lookup_namespace,
		.lookup_type_by_name = test_lookup_type_by_name,
		.lookup_type_by_oid = test_lookup_type_by_oid,
		.get_error = test_catalog_error
	};
	const char *qualified_sql =
		"CREATE FUNCTION custom_type(input application_types.state) RETURNS "
		"application_types.state LANGUAGE plpgsql AS $$ DECLARE value "
		"application_types.state; BEGIN RETURN input; "
		"END $$";
	PgQueryPlpgsqlParseResult result =
		pg_query_parse_plpgsql_with_catalog(qualified_sql, &catalog);
	bool valid = result.error == NULL && result.plpgsql_funcs != NULL
		&& strstr(result.plpgsql_funcs, "application_types.state") != NULL
		&& context.namespace_lookups > 0 && context.type_name_lookups > 0
		&& context.type_oid_lookups > 0;

	if (!valid && result.error != NULL)
		printf("Catalog-aware PL/pgSQL parse failed: %s\n",
			   result.error->message);
	pg_query_free_plpgsql_parse_result(result);
	if (!valid)
		return false;

	size_t type_oid_lookups = context.type_oid_lookups;
	const char *composite_sql =
		"CREATE FUNCTION custom_composite() RETURNS text LANGUAGE plpgsql AS "
		"$$ DECLARE value public.foo; BEGIN RETURN NULL; END $$";
	result = pg_query_parse_plpgsql_with_catalog(composite_sql, &catalog);
	valid = result.error == NULL && result.plpgsql_funcs != NULL
		&& strstr(result.plpgsql_funcs, "PLpgSQL_rec") != NULL
		&& context.type_oid_lookups > type_oid_lookups;
	if (!valid && result.error != NULL)
		printf("Catalog composite parse failed: %s\n", result.error->message);
	pg_query_free_plpgsql_parse_result(result);
	if (!valid)
		return false;

	size_t unqualified_int4_lookups = context.unqualified_int4_lookups;
	type_oid_lookups = context.type_oid_lookups;
	const char *fallback_builtin_sql =
		"CREATE FUNCTION fallback_builtin() RETURNS text LANGUAGE plpgsql AS "
		"$$ DECLARE value int4; BEGIN RETURN value::text; END $$";
	result = pg_query_parse_plpgsql_with_catalog(fallback_builtin_sql, &catalog);
	valid = result.error == NULL && result.plpgsql_funcs != NULL
		&& context.unqualified_int4_lookups > unqualified_int4_lookups
		&& context.type_oid_lookups == type_oid_lookups;
	if (!valid && result.error != NULL)
		printf("Built-in fallback parse failed: %s\n", result.error->message);
	pg_query_free_plpgsql_parse_result(result);
	if (!valid)
		return false;

	context.shadow_builtin_type = true;
	size_t type_name_lookups = context.type_name_lookups;
	type_oid_lookups = context.type_oid_lookups;
	unqualified_int4_lookups = context.unqualified_int4_lookups;
	const char *shadowed_builtin_sql =
		"CREATE FUNCTION shadowed_builtin() RETURNS text LANGUAGE plpgsql AS "
		"$$ DECLARE value int4; BEGIN RETURN value::text; END $$";
	result = pg_query_parse_plpgsql_with_catalog(shadowed_builtin_sql, &catalog);
	valid = result.error == NULL && result.plpgsql_funcs != NULL
		&& context.type_name_lookups > type_name_lookups
		&& context.unqualified_int4_lookups > unqualified_int4_lookups
		&& context.type_oid_lookups > type_oid_lookups;
	if (!valid && result.error != NULL)
		printf("Shadowed built-in type parse failed: %s\n",
			   result.error->message);
	pg_query_free_plpgsql_parse_result(result);
	if (!valid)
		return false;

	type_name_lookups = context.type_name_lookups;
	type_oid_lookups = context.type_oid_lookups;
	size_t qualified_int4_lookups = context.qualified_int4_lookups;
	size_t catalog_namespace_lookups = context.catalog_namespace_lookups;
	const char *qualified_builtin_sql =
		"CREATE FUNCTION qualified_builtin() RETURNS text LANGUAGE plpgsql AS "
		"$$ DECLARE value pg_catalog.int4; BEGIN RETURN value::text; END $$";
	result = pg_query_parse_plpgsql_with_catalog(qualified_builtin_sql, &catalog);
	valid = result.error == NULL && result.plpgsql_funcs != NULL
		&& context.type_name_lookups > type_name_lookups
		&& context.qualified_int4_lookups > qualified_int4_lookups
		&& context.catalog_namespace_lookups > catalog_namespace_lookups
		&& context.type_oid_lookups == type_oid_lookups;
	if (!valid && result.error != NULL)
		printf("Qualified built-in type parse failed: %s\n",
			   result.error->message);
	pg_query_free_plpgsql_parse_result(result);
	if (!valid)
		return false;

	context.fail_type_lookup = true;
	result = pg_query_parse_plpgsql_with_catalog(qualified_sql, &catalog);
	valid = result.error != NULL
		&& strstr(result.error->message, context.error) != NULL;
	pg_query_free_plpgsql_parse_result(result);
	context.fail_type_lookup = false;
	if (!valid)
		return false;

	context.return_invalid_type = true;
	result = pg_query_parse_plpgsql_with_catalog(qualified_sql, &catalog);
	valid = result.error != NULL
		&& strstr(result.error->message, "invalid type metadata") != NULL;
	pg_query_free_plpgsql_parse_result(result);
	context.return_invalid_type = false;
	if (!valid)
		return false;

	PgQueryPlpgsqlCatalog incomplete_catalog = catalog;
	incomplete_catalog.lookup_type_by_oid = NULL;
	result = pg_query_parse_plpgsql_with_catalog(qualified_sql,
											  &incomplete_catalog);
	valid = result.error != NULL
		&& strstr(result.error->message, "resolver is incomplete") != NULL;
	pg_query_free_plpgsql_parse_result(result);
	if (!valid)
		return false;

	const char *unqualified_sql =
		"CREATE FUNCTION custom_type_search_path() RETURNS text LANGUAGE "
		"plpgsql AS $$ DECLARE value state; BEGIN RETURN value::text; END $$";
	result = pg_query_parse_plpgsql_with_catalog(unqualified_sql, &catalog);
	valid = result.error == NULL && result.plpgsql_funcs != NULL;
	if (!valid && result.error != NULL)
		printf("Unqualified catalog type parse failed: %s\n",
			   result.error->message);
	pg_query_free_plpgsql_parse_result(result);
	if (!valid)
		return false;

	const char *missing_schema_sql =
		"CREATE FUNCTION missing_type() RETURNS text LANGUAGE plpgsql AS $$ "
		"DECLARE value missing_schema.state; BEGIN RETURN value::text; END $$";
	result = pg_query_parse_plpgsql_with_catalog(missing_schema_sql, &catalog);
	valid = result.error != NULL
		&& strstr(result.error->message, "missing_schema") != NULL;
	pg_query_free_plpgsql_parse_result(result);
	return valid;
}

int main() {
	bool ret_code = EXIT_SUCCESS;
	char *sample_buffer;
	struct stat sample_stat;
	int fd;
	FILE* f_out;
	PgQueryPlpgsqlParseResult result;
	TestCatalogContext catalog_context = {
		.namespace_oid = 900001,
		.type_oid = 900002,
		.shadow_type_oid = 900005,
		.error = "sample catalog lookup failed"
	};
	const PgQueryPlpgsqlCatalog catalog = {
		.context = &catalog_context,
		.lookup_namespace = test_lookup_namespace,
		.lookup_type_by_name = test_lookup_type_by_name,
		.lookup_type_by_oid = test_lookup_type_by_oid,
		.get_error = test_catalog_error
	};

	fd = open("test/plpgsql_samples.sql", O_RDONLY);
	if (fd < 0) {
		printf("Could not read samples file\n");
		return EXIT_FAILURE;
    }

	fstat(fd, &sample_stat);

	sample_buffer = malloc(sample_stat.st_size + 1);
	read(fd, sample_buffer, sample_stat.st_size);
	sample_buffer[sample_stat.st_size] = 0;

	if (sample_buffer != (void *) - 1)
	{
		result = pg_query_parse_plpgsql_with_catalog(sample_buffer, &catalog);
		free(sample_buffer);
		close(fd);
	} else {
		printf("Could not read samples file\n");
		close(fd);
		return EXIT_FAILURE;
	}

	if (result.error) {
		printf("ERROR: %s\n", result.error->message);
		printf("CONTEXT: %s\n", result.error->context);
		printf("LOCATION: %s, %s:%d\n\n", result.error->funcname, result.error->filename, result.error->lineno);

		pg_query_free_plpgsql_parse_result(result);
		return EXIT_FAILURE;
	}

	f_out = fopen("test/plpgsql_samples.actual.json", "w");
	fprintf(f_out, "%s\n", result.plpgsql_funcs);
	fclose(f_out);

	pg_query_free_plpgsql_parse_result(result);

	if (!test_catalog_aware_plpgsql_parse())
		return EXIT_FAILURE;

	pg_query_exit();

	return ret_code;
}
