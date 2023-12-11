#include "core.h"

RG_BEGIN_RG_NAMESPACE

char* getPrefPath()
{
    return SDL_GetPrefPath("rg", "gamelib");
}

FileData readFile(const char* filepath)
{
	FileData result = {};

	SDL_RWops* fp = SDL_RWFromFile(filepath, "rb");

	if (fp != NULL)
	{
		SDL_RWseek(fp, 0, RW_SEEK_END);
		Sint64 size = (rgSize)SDL_RWtell(fp);

		if (size == -1 || size <= 0)
		{
			SDL_RWclose(fp);
			result.isValid = false;
			return result;
		}
		else
		{
			result.dataSize = (rgSize)size;
		}

		SDL_RWseek(fp, 0, RW_SEEK_SET);

		result.data = (rgU8*)rgMalloc(sizeof(rgU8) * size);
		if (result.data == NULL)
		{
			rgLog("Cannot allocate memory(%dbytes) for reading file %s", size, filepath);
			SDL_RWclose(fp);
			result.isValid = false;
			return result;
		}

		rgSize sizeRead = SDL_RWread(fp, result.data, sizeof(rgU8), size);
		if (sizeRead != size)
		{
			result.isValid = false;
			rgFree(result.data);
		}

		SDL_RWclose(fp);

		result.isValid = true;
	}
	else
	{
		rgLog("Cannot open/read file %s", filepath);
		result.isValid = false;
	}

	return result;
}

void freeFileData(FileData* fd)
{
	rgFree(fd->data);
}

rgBool writeFile(char const* filepath, void* bufferPtr, rgSize bufferSizeInBytes)
{
	// Open file for writing
	SDL_RWops* fp = SDL_RWFromFile(filepath, "wb");

	// If file is opened
	if (fp != 0)
	{
		size_t bytesWritten = SDL_RWwrite(fp, bufferPtr, sizeof(rgU8), bufferSizeInBytes);
		if (bytesWritten != (size_t)bufferSizeInBytes)
		{
			rgLogError("Can't write file %s completely\n", filepath);
			return false;
		}

		SDL_RWclose(fp);
	}
	// If file can't be opened
	else
	{
		rgLogError("Can't open file %s for writing\n", filepath);
		return false;
	}
	return true;
}

RG_END_RG_NAMESPACE
