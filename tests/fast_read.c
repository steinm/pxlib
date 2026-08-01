#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <paradox.h>

enum { record_count = 400, record_size = 12 };

int main(void) {
	pxdoc_t *writer, *reader, *check;
	pxfield_t *fields;
	pxval_t **values;
	pxval_t *batch;
	FILE *file;
	char expected[record_size], actual[record_size];
	int i, count;

	file = tmpfile();
	assert(file != NULL);
	writer = PX_new();
	assert(writer != NULL);
	fields = writer->malloc(writer, 2*sizeof(*fields), "fast read test fields");
	assert(fields != NULL);
	memset(fields, 0, 2*sizeof(*fields));
	fields[0].px_fname = PX_strdup(writer, "name");
	fields[0].px_ftype = pxfAlpha;
	fields[0].px_flen = 8;
	fields[1].px_fname = PX_strdup(writer, "number");
	fields[1].px_ftype = pxfLong;
	fields[1].px_flen = 4;
	assert(PX_create_fp(writer, fields, 2, file, pxfFileTypNonIndexDB) == 0);
	for(i=0; i<record_count; i++) {
		memset(expected, 0, sizeof(expected));
		snprintf(expected, 8, "r%06d", i);
		PX_put_data_long(writer, expected+8, 4, i);
		assert(PX_put_record(writer, expected) == i+1);
	}
	PX_close(writer);
	PX_delete(writer);
	assert(fflush(file) == 0);
	rewind(file);

	reader = PX_new();
	assert(reader != NULL);
	assert(PX_open_fp(reader, file) == 0);
	assert(PX_get_num_records(reader) == record_count);
	count = PX_retrieve_records(reader, 197, 5, &batch);
	assert(count == 5);
	for(i=0; i<count; i++) {
		assert(batch[i*2].isnull == 0);
		snprintf(expected, 8, "r%06d", 197+i);
		assert(strcmp(batch[i*2].value.str.val, expected) == 0);
		assert(batch[i*2+1].value.lval == 197+i);
	}
	PX_free_records(reader, batch, count);
	count = PX_retrieve_records(reader, record_count-2, 10, &batch);
	assert(count == 2);
	PX_free_records(reader, batch, count);
	assert(PX_retrieve_records(reader, record_count, 10, &batch) == 0);
	assert(batch == NULL);
	assert(PX_retrieve_records(reader, 0, 0, &batch) == 0);
	assert(batch == NULL);
	assert(PX_retrieve_records(reader, -1, 10, &batch) == -1);
	for(i=record_count-1; i>=0; i--) {
		memset(expected, 0, sizeof(expected));
		snprintf(expected, 8, "r%06d", i);
		PX_put_data_long(reader, expected+8, 4, i);
		assert(PX_get_record(reader, i, actual) == actual);
		assert(memcmp(actual, expected, sizeof(actual)) == 0);
	}
	values = PX_retrieve_record(reader, record_count/2);
	assert(values != NULL);
	assert(strcmp(values[0]->value.str.val, "r000200") == 0);
	assert(values[1]->value.lval == record_count/2);
	values[1]->value.lval = 999;
	assert(PX_update_record(reader, values, record_count/2) >= 0);
	PX_free_record(reader, values);
	PX_close(reader);
	PX_delete(reader);
	assert(fflush(file) == 0);
	rewind(file);
	check = PX_new();
	assert(check != NULL);
	assert(PX_open_fp(check, file) == 0);
	values = PX_retrieve_record(check, record_count/2);
	assert(values != NULL);
	assert(values[1]->value.lval == 999);
	PX_free_record(check, values);
	PX_close(check);
	PX_delete(check);
	fclose(file);
	return 0;
}
