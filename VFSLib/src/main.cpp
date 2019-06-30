#include "../include/VFSLib.h"
#include "../include/LogHelper.h"
using namespace VFS;

extern "C"
{
	bool InitializeVFS()
	{
		gs_vfsLogHelper = std::make_shared<CLogger>("VFSLogger", "VFSPro.txt");
		if (!gs_vfsLogHelper || !gs_vfsLogHelper.get())
		{
			Logf("VFSPro.txt", "VFSPro initilization fail!");
			return false;
		}

//      gs_vfsLogHelper->sys_log(__FUNCTION__, LL_SYS, "LogHelper Initilization completed!");
		gs_vfsLogHelper->sys_log(__FUNCTION__, LL_SYS, "VFSPro started!");
		return true;
	}
}
