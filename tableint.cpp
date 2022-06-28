#include <iostream>
#include <cstring>
#include <vector>
#include <string>
#include "sock.hpp"
#include "tableint.hpp"

#ifdef _WIN32
#include <io.h>
#include <malloc.h>
#elseif __linux__
#include <sys/io.h>
#elseif __APPLE__
#include <uio.h>
#endif /* End of platform specific includes */

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <memory.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <stdlib.h>

//#define _LARGEFILE64_SOURCE     /* See feature_test_macros(7) */

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

extern int errno;

#ifdef _WIN32
#define CREATE_FLAGS _O_CREAT | _O_RDWR | _O_BINARY
#define PERMISSION_FLAGS _S_IREAD | _S_IWRITE
#define OPEN_FLAGS _O_RDWR | _O_BINARY
#else
#define CREATE_FLAGS O_CREAT | O_RDWR
#define PERMISSION_FLAGS 0400 | 0200
#define OPEN_FLAGS O_RDWR
#endif

/* FILE MAGIC - this text is written to the beginning of data file */
static char FILE_SIGNATURE[] = "\033\032DataFile\033\032~~~";

/********************************************************/
/*   table file header structure						*/
/* |-------------|------------|-------|.....|-------|	*/
/*   FILE_SIGNAT    TableInfo    Fld1          FldN		*/
/********************************************************/

typedef struct
{
	char fieldName[MaxFieldNameLen];
	enum FieldType type;
	long len;
	char *pNewValue;
	char *pEditValue;
} FieldStruct;

struct links
{
	long prevOffset;
	long nextOffset;
};

struct TableInfo
{
	long dataOffset;
	long fieldNumber; /* duplicates pFieldStruct->numOfFields */
	long recordSize;
	long totalRecordNumber; /* including deleted records */
	long recordNumber;
	long firstRecordOffset;
	long lastRecordOffset;
	long firstDeletedOffset;
};

struct Table
{
	int fd;
	FieldStruct *pFieldStruct;
	struct TableInfo tableInfo;
	long currentPos;
	Bool editFlag;
	struct links links;
};

/* Возможные сообщения об ошибках */
const char *ErrorText[] =
	{
		"Success.",
		"Can't create table.",
		"Can't open table.",
		"Field not found.",
		"Bad table handle",
		"Wrong arguments",
		"Can't set file position",
		"File write error",
		"File read error",
		"Table data corrupted",
		"Can't create table handle",
		"Can't delete or open read-only file",
		"Illegal file name",
		"Can't delete table",
		"File data corrupted",
		"Bad File Position",
		"Wrong field type",
		"Text value exceeds field length",
		"Current record is not edited",
};

/*  static function prototypes */

enum Direction
{
	LINK_PREV,
	LINK_NEXT
};

static enum Errors GetCurrentRecord(struct Table *tabHandle);
static enum Errors PutCurrentRecord(struct Table *tabHandle);
static enum Errors PutNewRecord(struct Table *tabHandle, long position, struct links *links);
static enum Errors ModifyLinks(struct Table *tabHandle, long position, long value, enum Direction dir);
static enum Errors GetInsertPos(struct Table *tabHandle, long *position);
static enum Errors GetLinks(struct Table *tabHandle, long position, struct links *links);
static enum Errors WriteHeader(struct Table *tabHandle);
static enum Errors ReadHeader(struct Table *tabHandle);
static struct Table *CreateTableHandle(struct TableStruct *tableStruct);
static struct Table *AllocateTableHandle(void);
static void DeleteTableHandle(struct Table *tabHandle);
static enum Errors AllocateBuffers(struct Table *tabHandle);
static void DeallocateBuffers(struct Table *tabHandle);
static enum Errors AddRecToDeleted(struct Table *tabHandle);

/*  library function definitions */

enum Errors createTable(const char *tableName, struct TableStruct *tableStruct)
{
	int fd;
	enum Errors retval;
	struct Table *tabHandle;

	if (!tableName || !tableStruct)
		return BadArgs;

	fd = open(tableName, CREATE_FLAGS, PERMISSION_FLAGS);
	if (fd < 0)
		switch (errno)
		{
		case EACCES:
			return ReadOnlyFile;
		case ENOENT:
			return BadFileName;
		default:
			return CantCreateTable;
		}

	if (tableStruct->numOfFields <= 0 || !tableStruct->fieldsDef)
		return CorruptedData;

	tabHandle = CreateTableHandle(tableStruct);

	if (!tabHandle)
	{
		close(fd);
		return CantCreateHandle;
	}

	tabHandle->fd = fd;
	retval = WriteHeader(tabHandle);

	DeleteTableHandle(tabHandle);

	return retval;
}

enum Errors deleteTable(const char *tableName)
{
	if (!tableName)
		return BadArgs;
	if (!unlink(tableName))
		return OK;
	switch (errno)
	{
	case EACCES:
		return ReadOnlyFile;
	case ENOENT:
		return BadFileName;
	default:
		return CantDeleteTable;
	}
}

enum Errors openTable(const char *tableName, THandle *tableHandle)
{
	int fd;
	enum Errors retval;
	struct Table *tabHandle;

	if (!tableName || !tableHandle)
		return BadArgs;

	*tableHandle = NULL;

	fd = open(tableName, OPEN_FLAGS);
	if (fd < 0)
		switch (errno)
		{
		case EACCES:
			return ReadOnlyFile;
		case ENOENT:
			return BadFileName;
		default:
			return CantOpenTable;
		}
	tabHandle = AllocateTableHandle();
	if (!tabHandle)
	{
		close(fd);
		return CantCreateHandle;
	}

	tabHandle->fd = fd;
	retval = ReadHeader(tabHandle);
	if (retval != OK)
	{
		DeleteTableHandle(tabHandle);
		return retval;
	}

	retval = AllocateBuffers(tabHandle);
	if (retval != OK)
	{
		DeleteTableHandle(tabHandle);
		return retval;
	}

	tabHandle->currentPos = tabHandle->tableInfo.firstRecordOffset;

	if (tabHandle->currentPos >= tabHandle->tableInfo.dataOffset)
	{
		retval = GetCurrentRecord(tabHandle);
		if (retval != OK)
		{
			DeleteTableHandle(tabHandle);
			return retval;
		}
	}

	*tableHandle = tabHandle;
	return OK;
}

enum Errors closeTable(THandle tableHandle)
{
	enum Errors retval = OK;

	if (!tableHandle)
		return BadHandle;
	if (tableHandle->fd >= 0)
	{
		retval = WriteHeader(tableHandle);
	}
	DeleteTableHandle(tableHandle);
	return retval;
}

enum Errors moveFirst(THandle tableHandle)
{
	if (!tableHandle || tableHandle->fd < 0)
		return BadHandle;

	tableHandle->currentPos = tableHandle->tableInfo.firstRecordOffset;
	if (tableHandle->tableInfo.recordNumber == 0)
		return OK;

	return GetCurrentRecord(tableHandle);
}

enum Errors moveLast(THandle tableHandle)
{
	if (!tableHandle || tableHandle->fd < 0)
		return BadHandle;

	tableHandle->currentPos = tableHandle->tableInfo.lastRecordOffset;
	if (tableHandle->tableInfo.recordNumber == 0)
		return OK;

	return GetCurrentRecord(tableHandle);
}

enum Errors moveNext(THandle tableHandle)
{
	if (!tableHandle || tableHandle->fd < 0)
		return BadHandle;
	switch (tableHandle->currentPos)
	{
	case -1: /* beforeFirst */
		tableHandle->currentPos = tableHandle->tableInfo.firstRecordOffset;
		return GetCurrentRecord(tableHandle);
	case 0: /* after last */
		return BadPos;
	default:
		tableHandle->currentPos = tableHandle->links.nextOffset;
		if (tableHandle->currentPos >= tableHandle->tableInfo.dataOffset)
		{
			return GetCurrentRecord(tableHandle);
		}
		else
			return OK;
	}
}

enum Errors movePrevios(THandle tableHandle)
{
	if (!tableHandle || tableHandle->fd < 0)
		return BadHandle;
	switch (tableHandle->currentPos)
	{
	case -1: /* beforeFirst */
		return BadPos;
	case 0: /* after last */
		tableHandle->currentPos = tableHandle->tableInfo.lastRecordOffset;
		return GetCurrentRecord(tableHandle);
	default:
		tableHandle->currentPos = tableHandle->links.prevOffset;
		if (tableHandle->currentPos >= tableHandle->tableInfo.dataOffset)
		{
			return GetCurrentRecord(tableHandle);
		}
		else
			return OK;
	}
}

Bool beforeFirst(THandle tableHandle)
{
	if (!tableHandle || tableHandle->fd < 0)
		return FALSE;
	switch (tableHandle->currentPos)
	{
	case -1: /* beforeFirst */
		return TRUE;
	case 0: /* after last */
		return tableHandle->tableInfo.recordNumber == 0;
	default:
		return FALSE;
	}
}

Bool afterLast(THandle tableHandle)
{
	if (!tableHandle || tableHandle->fd < 0)
		return FALSE;
	switch (tableHandle->currentPos)
	{
	case -1: /* beforeFirst */
		return tableHandle->tableInfo.recordNumber == 0;
	case 0: /* after last */
		return TRUE;
	default:
		return FALSE;
	}
}

enum Errors getText(THandle tableHandle, const char *fieldName, char **pvalue)
{
	int i;

	if (!tableHandle)
		return BadHandle;
	if (!fieldName || !pvalue)
		return BadArgs;

	for (i = 0; i < tableHandle->tableInfo.fieldNumber; i++)
		if (strcmp(fieldName, tableHandle->pFieldStruct[i].fieldName) == 0)
		{
			if (tableHandle->pFieldStruct[i].type != Text)
				return BadFieldType;
			*pvalue = tableHandle->pFieldStruct[i].pEditValue;
			return OK;
		}
	return FieldNotFound;
}

enum Errors getLong(THandle tableHandle, const char *fieldName, long *pvalue)
{
	int i;

	if (!tableHandle)
		return BadHandle;
	if (!fieldName || !pvalue)
		return BadArgs;

	for (i = 0; i < tableHandle->tableInfo.fieldNumber; i++)
		if (strcmp(fieldName, tableHandle->pFieldStruct[i].fieldName) == 0)
		{
			if (tableHandle->pFieldStruct[i].type != Long)
				return BadFieldType;
			memcpy(pvalue, tableHandle->pFieldStruct[i].pEditValue, sizeof(long));
			return OK;
		}
	return FieldNotFound;
}

enum Errors startEdit(THandle tableHandle)
{
	if (!tableHandle)
		return BadHandle;
	if (tableHandle->currentPos < tableHandle->tableInfo.dataOffset)
		return BadPos;
	tableHandle->editFlag = TRUE;
	return OK;
}

enum Errors putText(THandle tableHandle, const char *fieldName, const char *value)
{
	int i;

	if (!tableHandle)
		return BadHandle;
	if (!fieldName || !value)
		return BadArgs;

	for (i = 0; i < tableHandle->tableInfo.fieldNumber; i++)
		if (strcmp(fieldName, tableHandle->pFieldStruct[i].fieldName) == 0)
		{
			if (tableHandle->pFieldStruct[i].type != Text)
				return BadFieldType;
			if (strlen(value) >= (unsigned)tableHandle->pFieldStruct[i].len)
				return BadFieldLen;
			strcpy(tableHandle->pFieldStruct[i].pEditValue, value);
			return OK;
		}
	return FieldNotFound;
}

enum Errors putLong(THandle tableHandle, const char *fieldName, long value)
{
	int i;

	if (!tableHandle)
		return BadHandle;
	if (!fieldName)
		return BadArgs;

	for (i = 0; i < tableHandle->tableInfo.fieldNumber; i++)
		if (strcmp(fieldName, tableHandle->pFieldStruct[i].fieldName) == 0)
		{
			if (tableHandle->pFieldStruct[i].type != Long)
				return BadFieldType;
			memcpy(tableHandle->pFieldStruct[i].pEditValue, &value, sizeof(long));
			return OK;
		}
	return FieldNotFound;
}

enum Errors finishEdit(THandle tableHandle)
{
	if (!tableHandle)
		return BadHandle;
	if (!tableHandle->editFlag)
		return NoEditing;
	tableHandle->editFlag = FALSE;
	return PutCurrentRecord(tableHandle);
}

enum Errors createNew(THandle tableHandle)
{
	int i;
	if (!tableHandle)
		return BadHandle;
	for (i = 0; i < tableHandle->tableInfo.fieldNumber; i++)
	{
		memset(tableHandle->pFieldStruct[i].pNewValue, 0, tableHandle->pFieldStruct[i].len);
	}
	return OK;
}

enum Errors putTextNew(THandle tableHandle, const char *fieldName, const char *value)
{
	int i;

	if (!tableHandle)
		return BadHandle;
	if (!fieldName || !value)
		return BadArgs;

	for (i = 0; i < tableHandle->tableInfo.fieldNumber; i++)
		if (strcmp(fieldName, tableHandle->pFieldStruct[i].fieldName) == 0)
		{
			if (tableHandle->pFieldStruct[i].type != Text)
				return BadFieldType;
			if (strlen(value) >= (unsigned)tableHandle->pFieldStruct[i].len)
				return BadFieldLen;
			strcpy(tableHandle->pFieldStruct[i].pNewValue, value);
			return OK;
		}
	return FieldNotFound;
}

enum Errors putLongNew(THandle tableHandle, const char *fieldName, long value)
{
	int i;

	if (!tableHandle)
		return BadHandle;
	if (!fieldName)
		return BadArgs;

	for (i = 0; i < tableHandle->tableInfo.fieldNumber; i++)
		if (strcmp(fieldName, tableHandle->pFieldStruct[i].fieldName) == 0)
		{
			if (tableHandle->pFieldStruct[i].type != Long)
				return BadFieldType;
			memcpy(tableHandle->pFieldStruct[i].pNewValue, &value, sizeof(long));
			return OK;
		}
	return FieldNotFound;
}

enum Errors insertNew(THandle tableHandle)
{
	long position;
	struct links links;
	enum Errors retval;

	if (!tableHandle || tableHandle->fd < 0)
		return BadHandle;
	retval = GetInsertPos(tableHandle, &position);
	if (retval != OK)
		return retval;
	retval = GetLinks(tableHandle, tableHandle->links.prevOffset, &links);
	if (retval != OK)
		return retval;
	links.nextOffset = tableHandle->currentPos;
	if (links.nextOffset == -1)
		links.nextOffset = 0;
	retval = PutNewRecord(tableHandle, position, &links);
	if (retval != OK)
		return retval;
	retval = ModifyLinks(tableHandle, tableHandle->currentPos, position, LINK_PREV);
	if (retval != OK)
		return retval;
	retval = ModifyLinks(tableHandle, tableHandle->links.prevOffset, position, LINK_NEXT);
	if (retval != OK)
		return retval;
	if (tableHandle->tableInfo.recordNumber == 0)
	{
		tableHandle->tableInfo.firstRecordOffset = position;
		tableHandle->tableInfo.lastRecordOffset = position;
	}
	else if (tableHandle->links.prevOffset == -1)
	{
		tableHandle->tableInfo.firstRecordOffset = position;
	}
	tableHandle->tableInfo.recordNumber++;

	return OK;
}

enum Errors insertaNew(THandle tableHandle)
{
	long position;
	struct links links;
	enum Errors retval;

	if (!tableHandle || tableHandle->fd < 0)
		return BadHandle;
	retval = GetInsertPos(tableHandle, &position);
	if (retval != OK)
		return retval;

	links.nextOffset = tableHandle->tableInfo.firstRecordOffset;
	if (links.nextOffset == -1)
		links.nextOffset = 0;
	links.prevOffset = -1;
	retval = PutNewRecord(tableHandle, position, &links);
	if (retval != OK)
		return retval;
	retval = ModifyLinks(tableHandle, tableHandle->tableInfo.firstRecordOffset, position, LINK_PREV);
	if (retval != OK)
		return retval;
	if (tableHandle->tableInfo.recordNumber == 0)
	{
		tableHandle->tableInfo.lastRecordOffset = position;
	}
	tableHandle->tableInfo.recordNumber++;
	tableHandle->tableInfo.firstRecordOffset = position;

	return OK;
}

enum Errors insertzNew(THandle tableHandle)
{
	long position;
	struct links links;
	enum Errors retval;

	if (!tableHandle || tableHandle->fd < 0)
		return BadHandle;
	retval = GetInsertPos(tableHandle, &position);
	if (retval != OK)
		return retval;
	/* insert at the end */
	links.nextOffset = 0;
	links.prevOffset = tableHandle->tableInfo.lastRecordOffset;
	if (links.prevOffset == 0)
		links.prevOffset = -1;
	retval = PutNewRecord(tableHandle, position, &links);
	if (retval != OK)
		return retval;
	retval = ModifyLinks(tableHandle, tableHandle->tableInfo.lastRecordOffset, position, LINK_NEXT);
	if (retval != OK)
		return retval;
	if (tableHandle->tableInfo.recordNumber == 0)
	{
		tableHandle->tableInfo.firstRecordOffset = position;
	}
	tableHandle->tableInfo.recordNumber++;
	tableHandle->tableInfo.lastRecordOffset = position;
	return OK;
}

const char *getErrorString(enum Errors code)
{
	return ErrorText[code];
}

enum Errors getFieldLen(THandle tableHandle, const char *fieldName, unsigned *plen)
{
	int i;

	if (!tableHandle)
		return BadHandle;
	if (!fieldName || !plen)
		return BadArgs;

	for (i = 0; i < tableHandle->tableInfo.fieldNumber; i++)
		if (strcmp(fieldName, tableHandle->pFieldStruct[i].fieldName) == 0)
		{
			*plen = tableHandle->pFieldStruct[i].len;
			if (tableHandle->pFieldStruct[i].type == Text)
				*plen = *plen - 1;
			return OK;
		}
	return FieldNotFound;
}

enum Errors getFieldType(THandle tableHandle, const char *fieldName, enum FieldType *ptype)
{
	int i;

	if (!tableHandle)
		return BadHandle;
	if (!fieldName || !ptype)
		return BadArgs;

	for (i = 0; i < tableHandle->tableInfo.fieldNumber; i++)
		if (strcmp(fieldName, tableHandle->pFieldStruct[i].fieldName) == 0)
		{
			*ptype = tableHandle->pFieldStruct[i].type;
			return OK;
		}
	return FieldNotFound;
}

enum Errors getFieldsNum(THandle tableHandle, unsigned *pNum)
{
	if (!tableHandle)
		return BadHandle;
	if (!pNum)
		return BadArgs;

	*pNum = tableHandle->tableInfo.fieldNumber;
	return OK;
}

enum Errors getFieldName(THandle tableHandle, unsigned index, char **pFieldName)
{
	if (!tableHandle)
		return BadHandle;
	if (!pFieldName)
		return BadArgs;

	*pFieldName = tableHandle->pFieldStruct[index].fieldName;
	return OK;
}

enum Errors deleteRec(THandle tableHandle)
{
	if (!tableHandle || tableHandle->fd < 0)
		return BadHandle;
	if (tableHandle->currentPos < tableHandle->tableInfo.firstRecordOffset)
		return BadPosition;
	return AddRecToDeleted(tableHandle);
}

/*  internal function definitions */

/* helper macros */
#define MOVE_POS(pos)                                        \
	if (lseek(tabHandle->fd, (pos), SEEK_SET) != (int)(pos)) \
		return CantMoveToPos;

#define WRITE_DATA(buf, size)                               \
	if (write(tabHandle->fd, (buf), (size)) != (int)(size)) \
		return CantWriteData;

#define READ_DATA(buf, size)                               \
	if (read(tabHandle->fd, (buf), (size)) != (int)(size)) \
		return CantReadData;

static enum Errors WriteHeader(struct Table *tabHandle)
{
	MOVE_POS(0);
	WRITE_DATA(FILE_SIGNATURE, sizeof(FILE_SIGNATURE));
	WRITE_DATA(&tabHandle->tableInfo, sizeof(struct TableInfo));
	if (tabHandle->pFieldStruct)
	{
		WRITE_DATA(tabHandle->pFieldStruct, sizeof(FieldStruct) * tabHandle->tableInfo.fieldNumber);
	}
	else
		return CantWriteData;

	return OK;
}

static struct Table *AllocateTableHandle()
{
	struct Table *handle;

	handle = (struct Table *)calloc(1, sizeof(struct Table));
	if (!handle)
		return NULL;

	handle->fd = -1;
	handle->pFieldStruct = NULL;
	handle->currentPos = -1;

	handle->tableInfo.recordNumber = 0;
	handle->tableInfo.totalRecordNumber = 0;
	handle->tableInfo.firstRecordOffset = -1;
	handle->tableInfo.firstDeletedOffset = -1;
	handle->tableInfo.lastRecordOffset = 0;
	handle->tableInfo.fieldNumber = 0;
	handle->tableInfo.recordSize = 0;
	handle->tableInfo.dataOffset = 0;
	handle->links.prevOffset = -1;
	handle->links.nextOffset = 0;
	handle->editFlag = FALSE;
	return handle;
}

static struct Table *CreateTableHandle(struct TableStruct *tableStruct)
{
	struct Table *handle;
	unsigned i;
	long RecSize = 0;

	handle = AllocateTableHandle();
	if (!handle)
		return NULL;

	/* copy structure and count record size */
	handle->tableInfo.fieldNumber = tableStruct->numOfFields;
	handle->pFieldStruct = (FieldStruct *)calloc(tableStruct->numOfFields, sizeof(FieldStruct));
	if (!handle->pFieldStruct)
	{
		DeleteTableHandle(handle);
		return NULL;
	}
	for (i = 0; i < tableStruct->numOfFields; i++)
	{
		strcpy(handle->pFieldStruct[i].fieldName, tableStruct->fieldsDef[i].name);
		handle->pFieldStruct[i].type = tableStruct->fieldsDef[i].type;
		switch (tableStruct->fieldsDef[i].type)
		{
		case Long:
			handle->pFieldStruct[i].len = sizeof(long);
			RecSize += sizeof(long);
			break;
		case Text:
			if (!tableStruct->fieldsDef[i].len)
			{
				DeleteTableHandle(handle);
				return NULL;
			}
			handle->pFieldStruct[i].len = tableStruct->fieldsDef[i].len + 1;
			RecSize += tableStruct->fieldsDef[i].len + 1;
			break;
		default:
			break;
		}
	}

	handle->tableInfo.recordSize = RecSize;
	handle->tableInfo.dataOffset = sizeof(FILE_SIGNATURE) +
								   sizeof(struct TableInfo) + handle->tableInfo.fieldNumber * sizeof(FieldStruct);

	return handle;
}

static void DeleteTableHandle(struct Table *tabHandle)
{
	if (tabHandle->fd >= 0)
		close(tabHandle->fd);
	DeallocateBuffers(tabHandle);
	free(tabHandle->pFieldStruct);
	free(tabHandle);
}

static enum Errors AllocateBuffers(struct Table *tabHandle)
{
	int i;

	if (!tabHandle->pFieldStruct)
		return BadHandle;
	for (i = 0; i < tabHandle->tableInfo.fieldNumber; i++)
	{
		tabHandle->pFieldStruct[i].pNewValue = (char *)malloc(tabHandle->pFieldStruct[i].len);
		tabHandle->pFieldStruct[i].pEditValue = (char *)malloc(tabHandle->pFieldStruct[i].len);
		if (!tabHandle->pFieldStruct[i].pNewValue || !tabHandle->pFieldStruct[i].pEditValue)
			return CantCreateHandle;
	}
	return OK;
}

static void DeallocateBuffers(struct Table *tabHandle)
{
	int i;

	if (!tabHandle->pFieldStruct)
		return;
	for (i = 0; i < tabHandle->tableInfo.fieldNumber; i++)
	{
		if (tabHandle->pFieldStruct[i].pNewValue)
			free(tabHandle->pFieldStruct[i].pNewValue);
		if (tabHandle->pFieldStruct[i].pEditValue)
			free(tabHandle->pFieldStruct[i].pEditValue);
	}
}

static enum Errors ReadHeader(struct Table *tabHandle)
{
	char FileSig[sizeof(FILE_SIGNATURE)];

	MOVE_POS(0);
	READ_DATA(FileSig, sizeof(FILE_SIGNATURE));
	if (strcmp(FileSig, FILE_SIGNATURE))
		return CorruptedFile;
	READ_DATA(&tabHandle->tableInfo, sizeof(struct TableInfo));

	if (tabHandle->tableInfo.fieldNumber <= 0)
		return CorruptedFile;
	tabHandle->pFieldStruct = (FieldStruct *)malloc(tabHandle->tableInfo.fieldNumber * sizeof(FieldStruct));
	if (!tabHandle->pFieldStruct)
	{
		printf("%d %d\n", tabHandle->tableInfo.fieldNumber, sizeof(FieldStruct));
		return CantReadData;
	}

	READ_DATA(tabHandle->pFieldStruct, sizeof(FieldStruct) * tabHandle->tableInfo.fieldNumber);
	return OK;
}

static enum Errors GetCurrentRecord(struct Table *tabHandle)
{
	int i;

	if (tabHandle->currentPos < tabHandle->tableInfo.dataOffset)
		return BadPos;
	MOVE_POS(tabHandle->currentPos);
	READ_DATA(&tabHandle->links, sizeof(tabHandle->links));
	for (i = 0; i < tabHandle->tableInfo.fieldNumber; i++)
	{
		READ_DATA(tabHandle->pFieldStruct[i].pEditValue, tabHandle->pFieldStruct[i].len);
	}
	return OK;
}

static enum Errors GetInsertPos(struct Table *tabHandle, long *position)
{
	struct links links;

	if (tabHandle->tableInfo.firstDeletedOffset >= tabHandle->tableInfo.dataOffset)
	{
		*position = tabHandle->tableInfo.firstDeletedOffset;
		MOVE_POS(*position);
		READ_DATA(&links, sizeof(struct links));
		if (links.prevOffset != -1)
			return CorruptedFile;
		tabHandle->tableInfo.firstDeletedOffset = links.nextOffset;
	}
	else
	{
		/* no deleted records - position at the end of file */
		*position = tabHandle->tableInfo.dataOffset + tabHandle->tableInfo.totalRecordNumber *
														  (tabHandle->tableInfo.recordSize + sizeof(struct links));
		MOVE_POS(*position);
		tabHandle->tableInfo.totalRecordNumber++;
	}
	return OK;
}

static enum Errors GetLinks(struct Table *tabHandle, long position, struct links *links)
{
	if (position == 0 || position == -1)
	{
		links->prevOffset = -1;
		links->nextOffset = 0;
	}
	else
	{
		MOVE_POS(position);
		READ_DATA(links, sizeof(struct links));
	}
	return OK;
}

static enum Errors ModifyLinks(struct Table *tabHandle, long position, long value, enum Direction dir)
{
	long posToWrite = 0;

	if (position != 0 && position != -1)
	{
		switch (dir)
		{
		case LINK_PREV:
			posToWrite = position;
			break;
		case LINK_NEXT:
			posToWrite = position + sizeof(long);
			break;
		}
		MOVE_POS(posToWrite);
		WRITE_DATA(&value, sizeof(value));
	}
	return OK;
}

static enum Errors PutNewRecord(struct Table *tabHandle, long position, struct links *links)
{
	int i;

	MOVE_POS(position);
	WRITE_DATA(links, sizeof(struct links));
	for (i = 0; i < tabHandle->tableInfo.fieldNumber; i++)
	{
		WRITE_DATA(tabHandle->pFieldStruct[i].pNewValue, tabHandle->pFieldStruct[i].len);
	}
	return OK;
}

static enum Errors PutCurrentRecord(struct Table *tabHandle)
{
	int i;

	MOVE_POS(tabHandle->currentPos);
	WRITE_DATA(&tabHandle->links, sizeof(struct links));
	for (i = 0; i < tabHandle->tableInfo.fieldNumber; i++)
	{
		WRITE_DATA(tabHandle->pFieldStruct[i].pEditValue, tabHandle->pFieldStruct[i].len);
	}
	return OK;
}

static enum Errors AddRecToDeleted(struct Table *tabHandle)
{
	static unsigned long DELETE_MARK = ~0;
	// insert record to the deleted list
	MOVE_POS(tabHandle->currentPos);
	WRITE_DATA(&DELETE_MARK, sizeof DELETE_MARK);
	WRITE_DATA(&tabHandle->tableInfo.firstDeletedOffset, sizeof(tabHandle->tableInfo.firstDeletedOffset));
	tabHandle->tableInfo.firstDeletedOffset = tabHandle->currentPos;
	tabHandle->tableInfo.recordNumber--;
	// modify prev and next links
	if (tabHandle->links.prevOffset == -1)
	{ // first record is deleted
		tabHandle->tableInfo.firstRecordOffset = tabHandle->links.nextOffset;
		if (tabHandle->tableInfo.recordNumber == 0)
		{
			tabHandle->tableInfo.firstRecordOffset = -1; // no more records
			tabHandle->tableInfo.lastRecordOffset = 0;	 // no more records
		}
	}
	else
	{
		MOVE_POS(tabHandle->links.prevOffset + sizeof(tabHandle->links.prevOffset));
		WRITE_DATA(&tabHandle->links.nextOffset, sizeof(tabHandle->links.nextOffset));
	}
	if (tabHandle->links.nextOffset == 0)
	{ // last record is deleted
		tabHandle->tableInfo.lastRecordOffset = tabHandle->links.prevOffset;
		if (tabHandle->tableInfo.recordNumber == 0)
		{
			tabHandle->tableInfo.firstRecordOffset = -1; // no more records
			tabHandle->tableInfo.lastRecordOffset = 0;	 // no more records
		}
	}
	else
	{
		MOVE_POS(tabHandle->links.nextOffset);
		WRITE_DATA(&tabHandle->links.prevOffset, sizeof(tabHandle->links.prevOffset));
	}
	return OK;
}

//======================================================
//intermidiary namespace between interpreter and table.h
//======================================================

namespace TableInterface {

    //Exception    
    std::string DataBaseException::GetMessage() {
        return "Data base request failed: " + std::string(getErrorString((Errors)m_ErrCode)); 
    }

    //filters
    std::vector<int> filter_all(std::string &table_name) {
        Errors err;
        THandle td;
        unsigned table_size;
        std::vector<int> ans;

        if ((err = openTable(table_name.c_str(), &td)) != OK) { throw DataBaseException(err); }
        if ((err = getFieldsNum(td, &table_size)) != OK) { throw DataBaseException(err); }
        for (int i = 0; i < table_size; ++i) {
            ans.push_back(i);
        }
        if ((err = closeTable(td)) != OK) { throw DataBaseException(err); }
        return ans;
    }

    // like filter functions:
    void replace_sub(std::string &str, const std::string &prev, const std::string &next)
    {
        size_t cur_pos = 0;
        while ((cur_pos = str.find(prev, cur_pos)) != -1) {
            // Until the end of passing prev
            str.replace(cur_pos, prev.length(), next);
            cur_pos += next.length(); // Next iteration doesnt pick it up as well
        }
    }

    bool compare_like(std::string str, std::string model) {
        replace_sub(model, "'", "");
        replace_sub(model, "\\", "\\\\");
        replace_sub(model, "$", "\\$");
        replace_sub(model, ".", "\\.");
        replace_sub(model, "*", "\\*");
        replace_sub(model, "+", "\\+");
        replace_sub(model, "?", "\\?");
        replace_sub(model, "{", "\\{");
        replace_sub(model, "}", "\\}");
        replace_sub(model, "|", "\\|");
        replace_sub(model, "(", "\\(");
        replace_sub(model, ")", "\\)");
        replace_sub(model, "%", ".*");
        replace_sub(model, "_", ".");
        return std::regex_match(str, std::regex(model));
    }

    // choosing suitable to model-string records, returns array of integer
    std::vector<int> filter_like (std::string &table_name, std::string &field_name, std::string &model, bool &not_flag) {    
        std::vector<int> ans;
        THandle td;
        char * cur_str;
        Errors err;
        int cur_pos = 0;

        if ((err = openTable(table_name.c_str(), &td)) != OK) { throw DataBaseException(err); } 
        if ((err = createNew(td)) != OK) { throw DataBaseException(err); }
        if ((err = moveFirst(td)) != OK) { throw DataBaseException(err); }
        while (!afterLast(td)) {
            if ((err = getText(td, field_name.c_str(), &cur_str))) { throw DataBaseException(err); }             
            std::string tmp = std::string(cur_str); 
            bool like_flag = compare_like(tmp, model);
            if ((!like_flag || !not_flag) && (like_flag || not_flag)) ans.push_back(cur_pos); // Xor for flags (otherwise twice comparison)
            ++cur_pos;
            if ((err = moveNext(td))) { throw DataBaseException(err); }    
        }
        if ((err = closeTable(td)) != OK) { throw DataBaseException(err); }
        return ans;
    }

    std::vector<int> filter_in(std::string &table_name, std::vector<std::string> &poliz_texts, std::vector<lex_type_t> &poliz_types, std::vector<FieldCont> &const_list, bool &not_flag) {     
        std::vector<int> ans;
        THandle td;
        Errors err;
        int cur_pos = 0;
        FieldCont f_cont;
        bool go;
        bool in_flag;

        if ((err = openTable(table_name.c_str(), &td)) != OK) { throw DataBaseException(err); } 
        if ((err = createNew(td)) != OK) { throw DataBaseException(err); }
        if ((err = moveFirst(td)) != OK) { throw DataBaseException(err); }
        while (!afterLast(td)) {
            f_cont = poliz_count(td, poliz_texts, poliz_types);
            go = true;
            for (int i = 0; (i < const_list.size()) && go; ++i) {
                in_flag = (f_cont.cont == const_list[i].cont) && (f_cont.type == const_list[i].type);
                if ((!not_flag || !in_flag) && (not_flag || in_flag)) { //xor
                    go = false;
                    ans.push_back(cur_pos);
                }
            }
            ++cur_pos;
            if ((err = moveNext(td))) { throw DataBaseException(err); }    
        }
        if ((err = closeTable(td)) != OK) { throw DataBaseException(err); }
        return ans;
    }

    std::vector<int> filter_logic(std::string &table_name,  std::vector<std::string> &poliz_texts, std::vector<lex_type_t> &poliz_types) {    
        std::vector<int> ans;
        THandle td;
        Errors err;
        int cur_pos = 0;
        FieldCont f_cont;

        if ((err = openTable(table_name.c_str(), &td)) != OK) { throw DataBaseException(err); } 
        if ((err = createNew(td)) != OK) { throw DataBaseException(err); }
        if ((err = moveFirst(td)) != OK) { throw DataBaseException(err); }
        while (!afterLast(td)) {
            f_cont = poliz_count(td, poliz_texts, poliz_types);
            if ((f_cont.type == Logic) && std::stoi(f_cont.cont)) {
                ans.push_back(cur_pos);
            }
            ++cur_pos;
            if ((err = moveNext(td))) { throw DataBaseException(err); }    
        }
        if ((err = closeTable(td)) != OK) { throw DataBaseException(err); }
        return ans;
    }




    bool in_list(std::string &str, std::vector<std::string> &list){
        for (int i = 0; i < list.size(); i++) {
            if (str == list[i]) {
                return true;
            } 
        } 
        return false;
    }

    void get_type(std::string &table_name, std::string &field_name, enum FieldType *ptype) {
        Errors err;
        THandle td;
        if ((err = openTable(table_name.data(), &td)) != OK) { throw DataBaseException(err); }      //remove open/close table, mb return not void?
        if ((err = getFieldType(td, field_name.data(), ptype)) != OK) { throw DataBaseException(err); }
        if ((err = closeTable(td)) != OK) { throw DataBaseException(err); }
        return;
    }
   
    std::string print_table(std::string &table_name){
        Errors err;
        THandle td;
        std::string table_str;
        unsigned table_size;
        char *field_name;
        char *text_field_cont;
        long long_field_cont;
        enum FieldType field_type;
        if ((err = openTable(table_name.data() , &td)) != OK) {throw DataBaseException(err);}
        if ((err = getFieldsNum(td, &table_size)) != OK) {throw DataBaseException(err);}
        for (unsigned i = 0; i < table_size; i++) {
            if ((err = getFieldName(td, i, &field_name)) != OK) {throw DataBaseException(err);}
            table_str = table_str + " " + field_name;                                       
            }
        table_str += '\n';
        if ((err = moveFirst(td)) != OK) {throw DataBaseException(err);}
        while (!afterLast(td)){
            for (unsigned j = 0; j < table_size; j++){
                if ((err = getFieldName(td, j, &field_name)) != OK) {throw DataBaseException(err);}
                if ((err = getFieldType(td, field_name, &field_type)) != OK) {throw DataBaseException(err);}
                if (field_type == Text){
                    if ((err = getText(td, field_name, &text_field_cont)) != OK) {throw DataBaseException(err);}
                    table_str =table_str + " " + text_field_cont;                                   //level off
                } else {
                    if ((err = getLong(td, field_name, &long_field_cont)) != OK) {throw DataBaseException(err);}
                    table_str = table_str + " " + std::to_string(long_field_cont);                  //level off
                }
            }
            if ((err = moveNext(td)) != OK) {throw DataBaseException(err);}
            table_str += '\n';
        }
        if ((err = closeTable(td)) != OK) { throw DataBaseException(err); }
        return table_str;
    }

    std::string select(std::string &table_name, std::vector<std::string> &field_list, std::vector<int> &fields_filtered, bool &all_flag){
        Errors err;
        THandle td;
        std::string table_str;
        unsigned table_size;
        unsigned i = 0, j = 0, k;
        char * field_name;
        enum FieldType field_type;
        std::string field_cont;
        std::string field_name_string;
        char * text_field_cont;
        long long_field_cont;

        if ((err = openTable(table_name.c_str(), &td)) != OK) {throw DataBaseException (err);}
        if ((err = getFieldsNum(td, &table_size)) != OK) {throw DataBaseException (err);}
        for (i = 0; i < table_size; i++) {
            if ((err = getFieldName(td, i, &field_name)) != OK) {throw DataBaseException(err);}

            field_name_string = field_name;
            if (!all_flag && in_list(field_name_string, field_list)) {
                table_str = table_str + " " + field_name;
            }
            if (all_flag) {
                table_str = table_str + " " + field_name;
            }
        }
        i = 0;
        table_str += '\n';
        if ((err = moveFirst(td)) != OK) {throw DataBaseException(err);}
        bool stop = afterLast(td);
        while (!stop && j < fields_filtered.size()) {
            if (i == fields_filtered[j]) {
                for (k = 0; k < table_size; k++) {          //goes through all fields, no exception!!!!!!!
                    if ((err = getFieldName(td, k, &field_name)) != OK) {throw DataBaseException(err);}
                    field_name_string = field_name;

                    if (!all_flag && in_list(field_name_string, field_list)) {
                        if ((err = getFieldType(td, field_name, &field_type)) != OK) {throw DataBaseException(err);}
                        if (field_type == Text){
                            if ((err = getText(td, field_name, &text_field_cont)) != OK) {throw DataBaseException(err);}
                            table_str = table_str + " " + text_field_cont;
                        } else {
                            if ((err = getLong(td, field_name, &long_field_cont)) != OK) {throw DataBaseException(err);}
                            table_str = table_str + " " + std::to_string(long_field_cont);
                        }
                    }
                    if (all_flag) {
                        if ((err = getFieldType(td, field_name, &field_type)) != OK) {throw DataBaseException(err);}
                        if (field_type == Text){
                            if ((err = getText(td, field_name, &text_field_cont)) != OK) {throw DataBaseException(err);}
                            table_str = table_str + " " + text_field_cont;
                        } else {
                            if ((err = getLong(td, field_name, &long_field_cont)) != OK) {throw DataBaseException(err);}
                            table_str = table_str + " " + std::to_string(long_field_cont);
                        }
                    }
                }
                i++;
                j++;
                table_str += '\n';
            } else {
                i++;
            }    
            if ((err = moveNext(td)) != OK) {throw DataBaseException(err);}
            stop = afterLast(td);
        }
        return table_str;
    }

    void insert(std::string &table_name, const std::vector<FieldCont> &field_vect) {
        THandle td;
        unsigned f_num = 0;
        int list_size = field_vect.size();
        FieldType field_type;
        std::string cur_txt;
        unsigned table_size; 
        char** cur_f_name_p = new char*;
        char no_input[4] = "---";
        Errors err;
        
        if ((err = openTable(table_name.data(), &td)) != OK) { delete cur_f_name_p; throw DataBaseException(err); } 
        if ((err = getFieldsNum(td, &table_size)) != OK) { throw DataBaseException(err); }
        if ((err = createNew(td)) != OK) { throw DataBaseException(err); }

        for (auto i = field_vect.begin(); i != field_vect.end(); i++) {                                                                        
            if ((err = getFieldName(td, f_num, cur_f_name_p)) != OK) { throw DataBaseException(err); }
            if ((i -> type) == Text) {
                if ((err = putTextNew(td, *cur_f_name_p, (i -> cont).c_str())) != OK) { throw DataBaseException(err); }
            } else { //Long
                if ((err = putLongNew(td, *cur_f_name_p, std::stol(i -> cont))) != OK) { throw DataBaseException(err); }
            }
            ++f_num; 
        }
        for (f_num = list_size; f_num < table_size; ++f_num) {      
            if ((err = getFieldName(td, f_num, cur_f_name_p)) != OK) { throw DataBaseException(err); }
            if ((err = getFieldType(td, *cur_f_name_p, &field_type)) != OK) { throw DataBaseException(err); }
            if ((field_type) == Text) {
                if ((err = putTextNew(td, *cur_f_name_p, no_input)) != OK) { throw DataBaseException(err); } 
            } else {
                if ((err = putLongNew(td, *cur_f_name_p, 0)) != OK) { throw DataBaseException(err); } 
            }
        }
        if ((err = insertzNew(td)) != OK) { throw DataBaseException(err); }
        if ((err = closeTable(td)) != OK) { throw DataBaseException(err); }
        delete cur_f_name_p;
        return; 
    }

    void update(std::string &table_name, std::string &field_name, std::vector<std::string> &poliz_texts, std::vector<lex_type_t> &poliz_types, std::vector<int> &fields_filtered) {
        Errors err;
        THandle td;
        unsigned table_size;
        unsigned i = 0, j = 0;
        enum FieldType field_type;
        std::string field_cont;
        char * text_field_cont;
        long long_field_cont;
        FieldCont poliz_res; 

        if ((err = openTable(table_name.c_str(), &td)) != OK) {throw DataBaseException (err);}
        if ((err = getFieldsNum(td, &table_size)) != OK) {throw DataBaseException (err);} 
        if ((err = moveFirst(td)) != OK) {throw DataBaseException(err);}
        while (!afterLast(td) && j < fields_filtered.size()) {
            if (i == fields_filtered[j]) {
                poliz_res = poliz_count(td, poliz_texts, poliz_types);
                if ((err = startEdit(td)) != OK) {throw DataBaseException(err);}
                if (poliz_res.type == Text){
                    if ((err = putText(td, field_name.c_str(), poliz_res.cont.c_str())) != OK) {throw DataBaseException(err);}
                    } else {
                        if ((err = putLong(td, field_name.c_str(), std::stol(poliz_res.cont))) != OK) {throw DataBaseException(err);}
                    }
                if ((err = finishEdit(td)) != OK) {throw DataBaseException(err);}
                i++;
                j++;
            } else {
                i++;
            }    
            if ((err = moveNext(td)) != OK) {throw DataBaseException(err);}
        }
        return;
    }

    void delete_fields(std::string &table_name, std::vector<int> &fields_filtered){
        Errors err;
        THandle td;
        unsigned i = 0, j = 0;

        if ((err = openTable(table_name.c_str(), &td)) != OK) {throw DataBaseException(err);}
        if ((err = moveFirst(td)) != OK) {throw DataBaseException(err);}
        while (!afterLast(td) && j < fields_filtered.size()) {
            if (i == fields_filtered[j]) {
                if ((err = deleteRec(td)) != OK) {throw DataBaseException(err);}
                i++; 
                j++;
            } else {
                i++;
            }
            if ((err = moveNext(td)) != OK) {throw DataBaseException(err);}
        }
        if ((err = closeTable(td)) != OK) {throw DataBaseException(err);}
        return;
    }

    void create(std::string &table_name, std::vector<struct FieldDef> &field_vect) {
        Errors err;
        TableStruct tab_struct;
        tab_struct.numOfFields = field_vect.size();
        tab_struct.fieldsDef = &field_vect[0];

        if ((err = createTable(table_name.data(), &tab_struct)) != OK) { throw DataBaseException(err); }
        return;
    }

    void drop(std::string &table_name) {
        Errors err;

        if ((err = deleteTable(table_name.data())) != OK) { throw DataBaseException(err); }
        return;
    }

    FieldCont poliz_count(THandle td, std::vector<std::string> &poliz_texts, std::vector<lex_type_t> &poliz_types){
        Errors err;
        std::stack<std::string> str_poliz_stack;
        std::stack<FieldType> type_poliz_stack;
        std::string arg1, arg2;
        FieldType arg1_type, arg2_type;
        const char * field_name;
        FieldType field_type;
        std::string field_cont;
        char * text_field_cont;
        long long_field_cont;
        for (int i = 0; i < poliz_texts.size(); i++){
            if (poliz_types[i] == IDEN){
                field_name = poliz_texts[i].c_str();
                if ((err = getFieldType(td, field_name, &field_type)) != OK) {throw DataBaseException(err);}
                if (field_type == Text){
                    if ((err = getText(td, field_name, &text_field_cont)) != OK) {throw DataBaseException(err);}
                    field_cont = text_field_cont;
                    str_poliz_stack.push(field_cont);
                    type_poliz_stack.push(Text);
                } else {
                    if ((err = getLong(td, field_name, &long_field_cont)) != OK) {throw DataBaseException(err);}
                    field_cont = std::to_string(long_field_cont);
                    str_poliz_stack.push(field_cont);
                    type_poliz_stack.push(Long);
                }
            } else if (poliz_types[i] == POSNUM || poliz_types[i] == NEGNUM)  {
                field_cont = poliz_texts[i];
                str_poliz_stack.push(field_cont);
                type_poliz_stack.push(Long);
            } else if (poliz_types[i] == QUOTE){
                field_cont = poliz_texts[i];
                str_poliz_stack.push(field_cont);
                type_poliz_stack.push(Text);
            } else if (poliz_types[i] == PLUS){
                arg2 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                type_poliz_stack.pop();
                type_poliz_stack.pop();
                str_poliz_stack.push(std::to_string(stol(arg1) + stol(arg2)));
                type_poliz_stack.push(Long);
            } else if (poliz_types[i] == MINUS){
                arg2 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                type_poliz_stack.pop();
                type_poliz_stack.pop();
                str_poliz_stack.push(std::to_string(stol(arg1) - stol(arg2)));
                type_poliz_stack.push(Long);
            } else if (poliz_types[i] == MULT){
                arg2 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                type_poliz_stack.pop();
                type_poliz_stack.pop();
                str_poliz_stack.push(std::to_string(stol(arg1) * stol(arg2)));
                type_poliz_stack.push(Long);
            } else if (poliz_types[i] == DIV){
                arg2 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                type_poliz_stack.pop();
                type_poliz_stack.pop();
                str_poliz_stack.push(std::to_string(stol(arg1) / stol(arg2)));
                type_poliz_stack.push(Long);
            } else if (poliz_types[i] == MOD){
                arg2 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                type_poliz_stack.pop();
                type_poliz_stack.pop();
                str_poliz_stack.push(std::to_string(stol(arg1) % stol(arg2)));
                type_poliz_stack.push(Long);
             } else if (poliz_types[i] == EQUAL){
                arg2 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg2_type = type_poliz_stack.top();
                type_poliz_stack.pop();
                arg1_type = type_poliz_stack.top();
                type_poliz_stack.pop();
                if (arg1_type == Text && arg2_type == Text) {
                    if (arg1 == arg2){
                        str_poliz_stack.push("1");
                    } else {
                        str_poliz_stack.push("0");
                    }
                    type_poliz_stack.push(Logic);
                } else {
                    if (stol(arg1) == stol(arg2)){
                        str_poliz_stack.push("1");
                    } else {
                        str_poliz_stack.push("0");
                    }
                    type_poliz_stack.push(Logic);
                }
            } else if (poliz_types[i] == NOTEQUAL){
                arg2 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg2_type = type_poliz_stack.top();
                type_poliz_stack.pop();
                arg1_type = type_poliz_stack.top();
                type_poliz_stack.pop();
                if (arg1_type == Text && arg2_type == Text) {
                    if (arg1 != arg2){
                        str_poliz_stack.push("1");
                    } else {
                        str_poliz_stack.push("0");
                    }
                    type_poliz_stack.push(Logic);
                } else {
                    if (stol(arg1) != stol(arg2)){
                        str_poliz_stack.push("1");
                    } else {
                        str_poliz_stack.push("0");
                    }
                    type_poliz_stack.push(Logic);
                }
            } else if (poliz_types[i] == EQUALLE){
                arg2 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg2_type = type_poliz_stack.top();
                type_poliz_stack.pop();
                arg1_type = type_poliz_stack.top();
                type_poliz_stack.pop();
                if (arg1_type == Text && arg2_type == Text) {
                    if (arg1 <= arg2){
                        str_poliz_stack.push("1");
                    } else {
                        str_poliz_stack.push("0");
                    }
                    type_poliz_stack.push(Logic);
                } else {
                    if (stol(arg1) <= stol(arg2)){
                        str_poliz_stack.push("1");
                    } else {
                        str_poliz_stack.push("0");
                    }
                    type_poliz_stack.push(Logic);
                }
                
            } else if (poliz_types[i] == EQUALGE){
                arg2 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg2_type = type_poliz_stack.top();
                type_poliz_stack.pop();
                arg1_type = type_poliz_stack.top();
                type_poliz_stack.pop();
                if (arg1_type == Text && arg2_type == Text) {
                    if (arg1 >= arg2){
                        str_poliz_stack.push("1");
                    } else {
                        str_poliz_stack.push("0");
                    }
                    type_poliz_stack.push(Logic);
                } else {
                    if (stol(arg1) >= stol(arg2)){
                        str_poliz_stack.push("1");
                    } else {
                        str_poliz_stack.push("0");
                    }
                    type_poliz_stack.push(Logic);
                }
            
            } else if (poliz_types[i] == LESS){
                arg2 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg2_type = type_poliz_stack.top();
                type_poliz_stack.pop();
                arg1_type = type_poliz_stack.top();
                type_poliz_stack.pop();
                if (arg1_type == Text && arg2_type == Text) {
                    if (arg1 < arg2){
                        str_poliz_stack.push("1");
                    } else {
                        str_poliz_stack.push("0");
                    }
                    type_poliz_stack.push(Logic);
                } else {
                    if (stol(arg1) < stol(arg2)){
                        str_poliz_stack.push("1");
                    } else {
                        str_poliz_stack.push("0");
                    }
                    type_poliz_stack.push(Logic);
                }
 
            } else if (poliz_types[i] == GREATER){
                arg2 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg2_type = type_poliz_stack.top();
                type_poliz_stack.pop();
                arg1_type = type_poliz_stack.top();
                type_poliz_stack.pop();
                if (arg1_type == Text && arg2_type == Text) {
                    if (arg1 > arg2){
                        str_poliz_stack.push("1");
                    } else {
                        str_poliz_stack.push("0");
                    }
                    type_poliz_stack.push(Logic);
                } else {
                    if (stol(arg1) > stol(arg2)){
                        str_poliz_stack.push("1");
                    } else {
                        str_poliz_stack.push("0");
                    }
                    type_poliz_stack.push(Logic);
                }
            
            } else if (poliz_types[i] == AND){
                arg2 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                type_poliz_stack.pop();
                type_poliz_stack.pop();
                str_poliz_stack.push(std::to_string(stol(arg1) && stol(arg2)));
                type_poliz_stack.push(Logic);
            } else if (poliz_types[i] == OR){
                arg2 = str_poliz_stack.top();
                str_poliz_stack.pop();
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                type_poliz_stack.pop();
                type_poliz_stack.pop();
                str_poliz_stack.push(std::to_string(stol(arg1) || stol(arg2)));
                type_poliz_stack.push(Logic);
            } else if (poliz_types[i] == NOT){
                arg1 = str_poliz_stack.top();
                str_poliz_stack.pop();
                type_poliz_stack.pop();
                str_poliz_stack.push(std::to_string(!std::stol(arg1)));
                type_poliz_stack.push(Logic);
            }
        }
        FieldCont str;
        str.cont = str_poliz_stack.top();
        str.type = type_poliz_stack.top();
        return str;
    }

}

