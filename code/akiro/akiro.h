#ifndef _AKIRO_H_
#define _AKIRO_H_

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/unordered_map.h>
#include <EASTL/tuple.h>

struct DefStruct
{
	struct Variable
	{
		eastl::string type;
		eastl::string name;
	};
};

typedef eastl::unordered_map<eastl::string, DefStruct> StructMap;

#endif