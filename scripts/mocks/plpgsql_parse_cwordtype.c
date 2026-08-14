PLpgSQL_type *
plpgsql_parse_cwordtype(List *idents)
{
	PLpgSQL_type *typ;

	typ = (PLpgSQL_type *) palloc0(sizeof(PLpgSQL_type));
	typ->typname = psprintf("%s%%TYPE", NameListToQuotedString(idents));
	typ->origtypname = makeTypeNameFromNameList(idents);
	typ->origtypname->pct_type = true;
	typ->ttype = PLPGSQL_TTYPE_SCALAR;
	return typ;
}
