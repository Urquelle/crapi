char *
json_obj(Db_Row *row, Mem_Arena *arena) {
    char *result = "{";

    for ( int j = 0; j < row->num_fields; ++j ) {
        Db_Field *field = row->fields[j];
        result = strf(arena, "%s%s\"%s\": \"%s\"", result, (j == 0) ? "" : ", ",
                field->name, (char *)field->data);
    }

    result = strf(arena, "%s}", result);

    return result;
}

char *
json_array(Db_Result *res, Mem_Arena *arena) {
    char *result = "[";

    for ( int i = 0; i < res->num_rows; ++i ) {
        result = strf(arena, "%s%s%s", result, (i == 0) ? "" : ", ",
                json_obj(res->rows[i], arena));
    }

    result = strf(arena, "%s]", result);

    return result;
}
