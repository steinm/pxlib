
%.3: %.sgml
	docbook-to-man $< > $@

distclean: clean

clean:
	rm -f ${man_MANS}

SGML_FILES = PX_get_majorversion.sgml \
             PX_get_minorversion.sgml \
						 PX_get_subminorversion.sgml \
						 PX_new2.sgml \
						 PX_new.sgml \
						 PX_open_filename.sgml \
						 PX_open_fp.sgml \
						 PX_create_db.sgml \
						 PX_get_data_short.sgml \
						 PX_get_data_long.sgml \
						 PX_get_data_double.sgml \
						 PX_get_data_alpha.sgml \
						 PX_put_data_short.sgml \
						 PX_put_data_long.sgml \
						 PX_put_data_double.sgml \
						 PX_put_data_alpha.sgml \
						 PX_get_record.sgml \
						 PX_get_record2.sgml \
						 PX_put_record.sgml \
						 PX_get_field.sgml \
						 PX_get_fields.sgml \
						 PX_get_num_fields.sgml \
						 PX_get_num_records.sgml \
						 PX_add_primary_index.sgml \
						 PX_read_primary_index.sgml \
						 PX_set_targetencoding.sgml \
						 PX_close.sgml \
						 PX_delete.sgml

EXTRA_DIST = ${SGML_FILES} pxformat.txt paradox4.txt

man_MANS = $(SGML_FILES:.sgml=.3)

