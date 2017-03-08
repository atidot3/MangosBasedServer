#ifndef DBC_FILE_LOADER_H
#define DBC_FILE_LOADER_H

#include "../Define.h"
#include "../Util/ByteConverter.h"
#include <cassert>

enum FieldFormat
{
	FT_NA = 'x',                                            // ignore/ default, 4 byte size, in Source String means field is ignored, in Dest String means field is filled with default value
	FT_NA_BYTE = 'X',                                       // ignore/ default, 1 byte size, see above
	FT_NA_FLOAT = 'F',                                      // ignore/ default,  float size, see above
	FT_NA_POINTER = 'p',                                    // fill default value into dest, pointer size, Use this only with static data (otherwise mem-leak)
	FT_STRING = 's',                                        // char*
	FT_FLOAT = 'f',                                         // float
	FT_INT = 'i',                                           // uint32
	FT_BYTE = 'b',                                          // uint8
	FT_SORT = 'd',                                          // sorted by this field, field is not included
	FT_IND = 'n',                                           // the same,but parsed to data
	FT_LOGIC = 'l'                                          // Logical (boolean)
};
#endif
