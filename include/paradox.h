#ifndef __PARADOX_H__
#define __PARADOX_H__

#ifdef WIN32

#define PXLIB_CALL __cdecl

#ifdef PXLIB_EXPORTS
#define PXLIB_API __declspec(dllexport) /* prepare a DLL (internal use only) */
#elif defined(PXLIB_DLL)
#define PXLIB_API __declspec(dllimport) /* PXlib clients: import PXlib DLL */
#else /* !PXLIB_DLL */
#define PXLIB_API /* */  /* default: generate or use static library */

#endif  /* !PXLIB_DLL */

#endif /* !WIN32 */

#ifndef PXLIB_CALL
#define PXLIB_CALL
#endif
#ifndef PXLIB_API
#define PXLIB_API
#endif

#define px_true 1
#define px_false 0

/* Error codes */
#define PX_MemoryError 1
#define PX_IOError 2
#define PX_RuntimeError 3
#define PX_Warning 100

/* Field types */
#define pxfAlpha        0x01
#define pxfDate         0x02
#define pxfShort        0x03
#define pxfLong         0x04
#define pxfCurrency     0x05
#define pxfNumber       0x06
#define pxfLogical      0x09
#define pxfMemoBLOb     0x0C
#define pxfBLOb         0x0D
#define pxfFmtMemoBLOb  0x0E
#define pxfOLE          0x0F
#define pxfGraphic      0x10
#define pxfTime         0x14
#define pxfTimestamp    0x15
#define pxfAutoInc      0x16
#define pxfBCD          0x17
#define pxfBytes        0x18

/* File types */
#define pxfFileTypIndexDB         0
#define pxfFileTypPrimIndex       1
#define pxfFileTypNonIndexDB      2
#define pxfFileTypNonIncSecIndex  3
#define pxfFileTypSecIndex        4
#define pxfFileTypIncSecIndex     5
#define pxfFileTypNonIncSecIndexG 6
#define pxfFileTypSecIndexG       7
#define pxfFileTypIncSecIndexG    8

struct px_field {
	char *px_fname;
	char px_ftype;
	int px_flen;
	int px_fdc;
};

struct px_head {
	char *px_tablename;
	int px_recordsize;
	char px_filetype;
	int px_fileversion;
	int px_numrecords;
	int px_numfields;
	int px_maxtablesize;
	int px_headersize;
	int px_fileblocks;
	int px_indexfieldnumber;
	int px_writeprotected;
	int px_doscodepage;
	int px_primarykeyfields;
	struct px_field *px_fields;
};

typedef struct px_doc pxdoc_t;
typedef struct px_head pxhead_t;
typedef struct px_field pxfield_t;

struct px_doc {
	FILE *px_fp;
	char *px_name;
	int closefp;

	int in_error;

	pxhead_t *px_head;

	size_t (*writeproc)(pxdoc_t *p, void *data, size_t size);
	void (*errorhandler)(pxdoc_t *p, int level, const char* msg);
	void *(*malloc)(pxdoc_t *p, size_t size, const char *caller);
	void *(*calloc)(pxdoc_t *p, size_t size, const char *caller);
	void *(*realloc)(pxdoc_t *p, void *mem, size_t size, const char *caller);
	void  (*free)(pxdoc_t *p, void *mem);
};

PXLIB_API int PXLIB_CALL
PX_get_majorversion(void);

PXLIB_API int PXLIB_CALL
PX_get_minorversion(void);

PXLIB_API int PXLIB_CALL
PX_get_subminorversion(void);

PXLIB_API pxdoc_t* PXLIB_CALL
PX_new2(void  (*errorhandler)(pxdoc_t *p, int type, const char *msg),
        void* (*allocproc)(pxdoc_t *p, size_t size, const char *caller),
        void* (*reallocproc)(pxdoc_t *p, void *mem, size_t size, const char *caller),
        void  (*freeproc)(pxdoc_t *p, void *mem));

PXLIB_API pxdoc_t* PXLIB_CALL
PX_new(void);

PXLIB_API int PXLIB_CALL
PX_open_fp(pxdoc_t *pxdoc, FILE *fp);

PXLIB_API int PXLIB_CALL
PX_open_file(pxdoc_t *pxdoc, char *filename);

PXLIB_API char * PXLIB_CALL
PX_get_record(pxdoc_t *pxdoc, int recno, char *data);

PXLIB_API void PXLIB_CALL
PX_close(pxdoc_t *pxdoc);
#endif
