#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <paradox.h>

enum {
	record_count = 400,
	field_count = 9,
	record_size = 42,
	alpha_offset = 0,
	long_offset = 8,
	short_offset = 12,
	number_offset = 14,
	currency_offset = 22,
	logical_offset = 30,
	date_offset = 31,
	time_offset = 35,
	bytes_offset = 39
};

static void
write_test_record(pxdoc_t *pxdoc, int recno, char *data) {
	char payload[3];
	short int small = (short int)(recno % 100 + 1);
	double amount = recno % 2 ? -(recno + 0.25) : recno + 0.25;
	double currency = recno % 2 ? -(recno + 100.5) : recno + 100.5;

	memset(data, 0, record_size);
	snprintf(data + alpha_offset, 8, "r%06d", recno);
	if(recno != 0)
		PX_put_data_long(pxdoc, data + long_offset, 4, recno);
	if(recno % 2)
		small = (short int)-small;
	PX_put_data_short(pxdoc, data + short_offset, 2, small);
	PX_put_data_double(pxdoc, data + number_offset, 8, amount);
	PX_put_data_double(pxdoc, data + currency_offset, 8, currency);
	PX_put_data_byte(pxdoc, data + logical_offset, 1, (char)(recno % 2 + 1));
	PX_put_data_long(pxdoc, data + date_offset, 4, 10000 + recno);
	PX_put_data_long(pxdoc, data + time_offset, 4, 50000 + recno);
	payload[0] = (char)(recno % 127 + 1);
	payload[1] = '\0';
	payload[2] = (char)0xa5;
	memcpy(data + bytes_offset, payload, sizeof(payload));
}

int main(void) {
	pxdoc_t *writer, *reader, *check;
	pxfield_t *fields;
	pxval_t **values;
	pxval_t *batch;
	FILE *file;
	char expected[record_size], actual[record_size];
	char payload[3];
	int i, count, failed_record;
	double expected_amount;
	double expected_currency;

	file = tmpfile();
	assert(file != NULL);
	writer = PX_new();
	assert(writer != NULL);
	fields = writer->malloc(writer, field_count*sizeof(*fields), "fast read test fields");
	assert(fields != NULL);
	memset(fields, 0, field_count*sizeof(*fields));
	fields[0].px_fname = PX_strdup(writer, "name");
	fields[0].px_ftype = pxfAlpha;
	fields[0].px_flen = 8;
	fields[1].px_fname = PX_strdup(writer, "number");
	fields[1].px_ftype = pxfLong;
	fields[1].px_flen = 4;
	fields[2].px_fname = PX_strdup(writer, "small");
	fields[2].px_ftype = pxfShort;
	fields[2].px_flen = 2;
	fields[3].px_fname = PX_strdup(writer, "amount");
	fields[3].px_ftype = pxfNumber;
	fields[3].px_flen = 8;
	fields[4].px_fname = PX_strdup(writer, "currency");
	fields[4].px_ftype = pxfCurrency;
	fields[4].px_flen = 8;
	fields[5].px_fname = PX_strdup(writer, "active");
	fields[5].px_ftype = pxfLogical;
	fields[5].px_flen = 1;
	fields[6].px_fname = PX_strdup(writer, "date");
	fields[6].px_ftype = pxfDate;
	fields[6].px_flen = 4;
	fields[7].px_fname = PX_strdup(writer, "time");
	fields[7].px_ftype = pxfTime;
	fields[7].px_flen = 4;
	fields[8].px_fname = PX_strdup(writer, "payload");
	fields[8].px_ftype = pxfBytes;
	fields[8].px_flen = 3;
	assert(PX_create_fp(writer, fields, field_count, file, pxfFileTypNonIndexDB) == 0);
	for(i=0; i<record_count; i++) {
		write_test_record(writer, i, expected);
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
		int recno = 197+i;
		pxval_t *row = batch + i*field_count;
		assert(row[0].isnull == 0);
		snprintf(expected, 8, "r%06d", recno);
		assert(strcmp(row[0].value.str.val, expected) == 0);
		assert(row[1].isnull == 0 && row[1].value.lval == recno);
		assert(row[2].isnull == 0);
		assert(row[2].value.lval == (recno % 2 ? -(recno % 100 + 1) : recno % 100 + 1));
		expected_amount = recno % 2 ? -(recno + 0.25) : recno + 0.25;
		assert(row[3].isnull == 0);
		assert(row[3].value.dval - expected_amount < 1e-9);
		assert(row[3].value.dval - expected_amount > -1e-9);
		expected_currency = recno % 2 ? -(recno + 100.5) : recno + 100.5;
		assert(row[4].isnull == 0);
		assert(row[4].value.dval - expected_currency < 1e-9);
		assert(row[4].value.dval - expected_currency > -1e-9);
		assert(row[5].isnull == 0 && row[5].value.lval == recno % 2 + 1);
		assert(row[6].isnull == 0 && row[6].value.lval == 10000 + recno);
		assert(row[7].isnull == 0 && row[7].value.lval == 50000 + recno);
		payload[0] = (char)(recno % 127 + 1);
		payload[1] = '\0';
		payload[2] = (char)0xa5;
		assert(row[8].isnull == 0 && row[8].value.str.len == sizeof(payload));
		assert(memcmp(row[8].value.str.val, payload, sizeof(payload)) == 0);
	}
	PX_free_records(reader, batch, count);
	count = PX_retrieve_records(reader, 0, 2, &batch);
	assert(count == 2);
	assert(batch[1].isnull == 1);
	assert(batch[field_count+1].isnull == 0 && batch[field_count+1].value.lval == 1);
	PX_free_records(reader, batch, count);
	failed_record = -2;
	count = PX_retrieve_records_ex(reader, 197, 5, &batch, &failed_record);
	assert(count == 5);
	assert(failed_record == -1);
	PX_free_records(reader, batch, count);
	count = PX_retrieve_records(reader, record_count-2, 10, &batch);
	assert(count == 2);
	PX_free_records(reader, batch, count);
	assert(PX_retrieve_records(reader, record_count, 10, &batch) == 0);
	assert(batch == NULL);
	assert(PX_retrieve_records(reader, 0, 0, &batch) == 0);
	assert(batch == NULL);
	assert(PX_retrieve_records(reader, -1, 10, &batch) == -1);
	assert(PX_retrieve_records(reader, 0, -1, &batch) == -1);
	assert(PX_retrieve_records(reader, INT_MAX, INT_MAX, &batch) == 0);
	assert(PX_retrieve_records(reader, 0, 1, NULL) == -1);
	failed_record = -2;
	assert(PX_retrieve_records_ex(reader, record_count, 1, &batch, &failed_record) == 0);
	assert(batch == NULL);
	assert(failed_record == -1);
	for(i=record_count-1; i>=0; i--) {
		write_test_record(reader, i, expected);
		assert(PX_get_record(reader, i, actual) == actual);
		assert(memcmp(actual, expected, sizeof(actual)) == 0);
	}
	values = PX_retrieve_record(reader, record_count/2);
	assert(values != NULL);
	assert(strcmp(values[0]->value.str.val, "r000200") == 0);
	assert(values[1]->value.lval == record_count/2);
	assert(values[2]->value.lval == (record_count/2 % 2 ?
		-(record_count/2 % 100 + 1) : record_count/2 % 100 + 1));
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
