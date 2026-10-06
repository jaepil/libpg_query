/*
 * libpg_query has no call expression from which to resolve polymorphic types.
 * Runtime compilation therefore requires a declaration specialized by the
 * caller; concrete and RECORD argument types already carry the available
 * information. Validator compilation keeps PostgreSQL's representative types.
 */
void
cfunc_resolve_polymorphic_argtypes(int numargs, Oid *argtypes, char *argmodes,
								   Node *call_expr, bool forValidator,
								   const char *proname)
{
	int			i;

	if (!forValidator)
	{
		for (i = 0; i < numargs; i++)
		{
			if (IsPolymorphicType(argtypes[i]))
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("could not determine actual argument "
								"type for polymorphic function \"%s\"",
								proname)));
		}
		return;
	}

	for (i = 0; i < numargs; i++)
	{
		switch (argtypes[i])
		{
			case ANYELEMENTOID:
			case ANYNONARRAYOID:
			case ANYENUMOID:
			case ANYCOMPATIBLEOID:
			case ANYCOMPATIBLENONARRAYOID:
				argtypes[i] = INT4OID;
				break;
			case ANYARRAYOID:
			case ANYCOMPATIBLEARRAYOID:
				argtypes[i] = INT4ARRAYOID;
				break;
			case ANYRANGEOID:
			case ANYCOMPATIBLERANGEOID:
				argtypes[i] = INT4RANGEOID;
				break;
			case ANYMULTIRANGEOID:
				argtypes[i] = INT4MULTIRANGEOID;
				break;
			default:
				break;
		}
	}
}
