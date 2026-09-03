PLpgSQL_type *
plpgsql_parse_wordrowtype(char *ident)
{
	PLpgSQL_type *typ;

	typ = (PLpgSQL_type *) palloc0(sizeof(PLpgSQL_type));
	typ->typname = psprintf("%s%%rowtype", quote_identifier(ident));
	typ->origtypname = makeTypeName(ident);
	typ->ttype = PLPGSQL_TTYPE_SCALAR;
	return typ;
}
