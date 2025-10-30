#include "utils.h"

// For getcwd, and chdir
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif


// HELPER FUNCTIONS
// -----------------

eastl::string extractBasePath(eastl::string path)
{
    eastl::string basePath;
    size_t lp = path.find_last_of("/");
    if(lp == eastl::string::npos)
    {
        lp = path.find_last_of("\\");
    }
    if(lp != eastl::string::npos)
    {
        basePath = path.substr(0, lp);
    }
    return basePath;
}

eastl::string getCurrentWorkingDir()
{
    char buf[256];
    return eastl::string(getcwd(buf, 256));
}

void changeWorkingDir(eastl::string dir)
{
    chdir(dir.c_str());
}
