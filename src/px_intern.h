#ifndef __PX_INTERN_H__
#define __PX_INTERN_H__

#define _(a) a

typedef unsigned char byte;
//typedef unsigned short int integer;
typedef struct { char c[2]; } integer;
typedef struct { char c[4]; } longint;
typedef struct { char c[2]; } word;
typedef char *pchar;
typedef struct { char c[4]; } pointer;

#include "fileformat.h"

typedef struct _TFldInfoRec TFldInfoRec;
typedef TFldInfoRec *PFldInfoRec;

typedef struct _TPxHeader TPxHeader;
typedef TPxHeader *PPxHeader;

typedef struct _TPxDataHeader TPxDataHeader;
typedef TPxDataHeader *PPxDataHeader;

typedef struct _TDataBlock TDataBlock;
typedef TDataBlock *PDataBlock;

#endif
