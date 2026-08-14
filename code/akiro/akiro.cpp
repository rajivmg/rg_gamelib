#include "akiro.h"

// EASTL MEMORY OVERLOADS
// ----------------------

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
	//rgLog("new[] size=%ld alignment=%ld alignmentOffset=%ld name=%s flags=%d debugFlags=%d file=%s line=%d\n",
	//    size, alignment, alignmentOffset, pName, flags, debugFlags, file, line);
	return new uint8_t[size];
}

void* __cdecl operator new[](size_t size, const char* name, int flags, unsigned debugFlags, const char* file, int line)
{
	//rgLog("new[] size=%ld name=%s flags=%d debugFlags=%d file=%s line=%d\n",
	//    size, name, flags, debugFlags, file, line);
	return new uint8_t[size];
}


StructMap mapStruct;