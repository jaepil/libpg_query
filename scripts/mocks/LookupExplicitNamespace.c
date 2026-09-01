Oid
LookupExplicitNamespace(const char *nspname, bool missing_ok)
{
	//Oid			namespaceId;
	//AclResult	aclresult;

	/* check for pg_temp alias */
	if (strcmp(nspname, "pg_temp") == 0)
	{
		if (OidIsValid(myTempNamespace))
			return myTempNamespace;

		/*
		 * Since this is used only for looking up existing objects, there is
		 * no point in trying to initialize the temp namespace here; and doing
		 * so might create problems for some callers --- just fall through.
		 */
	}

	if (pg_query_plpgsql_catalog_available())
	{
		uint32_t namespace_oid;

		if (pg_query_plpgsql_lookup_namespace(nspname, &namespace_oid))
			return (Oid) namespace_oid;
	}
	else if (strcmp(nspname, "pg_catalog") == 0)
		return PG_CATALOG_NAMESPACE;

	if (missing_ok)
		return InvalidOid;

	ereport(ERROR,
			(errcode(ERRCODE_UNDEFINED_SCHEMA),
			 errmsg("schema \"%s\" does not exist", nspname)));

	/*namespaceId = get_namespace_oid(nspname, missing_ok);
	if (missing_ok && !OidIsValid(namespaceId))
		return InvalidOid;

	aclresult = object_aclcheck(NamespaceRelationId, namespaceId, GetUserId(), ACL_USAGE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, OBJECT_SCHEMA,
					   nspname);*/
	/* Schema search hook for this lookup */
	//InvokeNamespaceSearchHook(namespaceId, true);

	//return namespaceId;
}
