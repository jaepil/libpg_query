HeapTuple
SearchSysCache1(int cacheId,
				Datum key1)
{
	/*Assert(cacheId >= 0 && cacheId < SysCacheSize &&
		   PointerIsValid(SysCache[cacheId]));
	Assert(SysCache[cacheId]->cc_nkeys == 1);

	return SearchCatCache1(SysCache[cacheId], key1);*/

	HeapTuple tuple;
	HeapTupleHeader td;
	Form_pg_type t = palloc0(sizeof(FormData_pg_type));
	const PgQueryBuiltinType *bt;
	PgQueryPlpgsqlTypeMetadata type_metadata;
	Oid			type_oid;
	Size		len,
				data_len;
	int			hoff;

    if (cacheId != TYPEOID)
        elog(ERROR, "Not implemented (SearchSysCache1 only supports TYPEOID cache (%d), got cache %d)", TYPEOID, cacheId);

	type_oid = DatumGetObjectId(key1);
    bt = pg_query_builtin_type_by_oid(type_oid);
    if (bt != NULL)
    {
        strlcpy(NameStr(t->typname), bt->typname, NAMEDATALEN);
		t->typnamespace = PG_CATALOG_NAMESPACE;
        t->typlen = bt->typlen;
        t->typbyval = bt->typbyval;
        t->typtype = bt->typtype;
        t->typcategory = bt->typcategory;
        t->typalign = bt->typalign;
        t->typarray = bt->typarray;
        t->typcollation = bt->typcollation;
    }
    else if (pg_query_plpgsql_catalog_available()
             && pg_query_plpgsql_lookup_type_by_oid(type_oid, &type_metadata))
    {
        strlcpy(NameStr(t->typname), type_metadata.name, NAMEDATALEN);
		t->typnamespace = type_metadata.namespace_oid;
        t->typlen = type_metadata.length;
        t->typbyval = type_metadata.by_value;
        t->typtype = type_metadata.type_kind;
        t->typcategory = type_metadata.category;
        t->typalign = type_metadata.alignment;
        t->typstorage = type_metadata.storage;
        t->typarray = type_metadata.array_oid;
        t->typelem = type_metadata.element_oid;
        t->typbasetype = type_metadata.base_type_oid;
        t->typcollation = type_metadata.collation_oid;
        t->typsubscript = type_metadata.subscript_handler_oid;
    }
    else
        elog(ERROR, "SearchSysCache1 could not resolve type OID %u", type_oid);

    t->oid = type_oid;
    t->typisdefined = true;

    /*
     * The generated builtin-type table carries neither typelem nor
     * typsubscript, so derive them for true array types. An array type is
     * the one some base type's typarray points to, and its element type is
     * that base type. Array-aware checks such as get_element_type(),
     * type_is_array(), and the VARIADIC-parameter-must-be-an-array test rely
     * on IsTrueArrayType(), which requires typsubscript to be the array
     * subscript handler. int2vector and oidvector are correctly excluded
     * because no base type's typarray points to them.
     */
    for (size_t i = 0; bt != NULL && i < lengthof(pg_query_builtin_types); i++)
    {
        if (pg_query_builtin_types[i].typarray == t->oid)
        {
            t->typelem = pg_query_builtin_types[i].oid;
            t->typsubscript = F_ARRAY_SUBSCRIPT_HANDLER;
            break;
        }
    }

	// The following logic is copied from heap_form_tuple, but pretends there are no nulls, and copies t_data directly

	/*
	 * Determine total space needed
	 */
	len = offsetof(HeapTupleHeaderData, t_bits);

	//if (hasnull)
	//	len += BITMAPLEN(numberOfAttributes);

	hoff = len = MAXALIGN(len); /* align user data safely */

	//data_len = heap_compute_data_size(tupleDescriptor, values, isnull);
	data_len = MAXALIGN(sizeof(FormData_pg_type));

	len += data_len;

	/*
	 * Allocate and zero the space needed.  Note that the tuple body and
	 * HeapTupleData management structure are allocated in one chunk.
	 */
	tuple = (HeapTuple) palloc0(HEAPTUPLESIZE + len);
	tuple->t_data = td = (HeapTupleHeader) ((char *) tuple + HEAPTUPLESIZE);

	/*
	 * And fill in the information.  Note we fill the Datum fields even though
	 * this tuple may never become a Datum.  This lets HeapTupleHeaderGetDatum
	 * identify the tuple type if needed.
	 */
	tuple->t_len = len;
	ItemPointerSetInvalid(&(tuple->t_self));
	tuple->t_tableOid = InvalidOid;

	HeapTupleHeaderSetDatumLength(td, len);
	//HeapTupleHeaderSetTypeId(td, tupleDescriptor->tdtypeid);
	//HeapTupleHeaderSetTypMod(td, tupleDescriptor->tdtypmod);
	/* We also make sure that t_ctid is invalid unless explicitly set */
	ItemPointerSetInvalid(&(td->t_ctid));

	HeapTupleHeaderSetNatts(td, Natts_pg_type);
	td->t_hoff = hoff;

	/*heap_fill_tuple(tupleDescriptor,
					values,
					isnull,
					(char *) td + hoff,
					data_len,
					&td->t_infomask,
					(hasnull ? td->t_bits : NULL));*/
	memcpy((char *) td + hoff, t, sizeof(FormData_pg_type));

	return tuple;
}
