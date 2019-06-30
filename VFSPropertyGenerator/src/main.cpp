#include <windows.h>
#include <iostream>
#include <vector>

#include "../../VFSLib/include/VFSPropertyManager.h"
#include "../../VFSLib/include/VFSPack.h"

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		printf("Usage: %s <property_folder>\n", argv[0]);
		return EXIT_FAILURE;
	}
	auto target = argv[1];

	auto vfs = new VFS::CVFSPack();
	if (!vfs || vfs->InitializeVFSPack() == false)
	{
		printf("VFS Initilization fail!\n");
		return EXIT_FAILURE;
	}
	vfs->Log(0, "VFS Property generator started! Koray/(c)2019");
	Sleep(2000);

	// ------------------------------------------------------------------------------------------
#if 0
	try
	{
		std::vector <VFS::SPropertyContext> vPropertyList;
		if (VFS::LoadPropertyList("PropertyList.json", vPropertyList) == false)
		{
			printf("Property list can NOT generated!\n");
			return EXIT_FAILURE;
		}
		return 0;
	}
	catch (std::exception& e)
	{
		printf("Exception handled: %s\n", e.what());
		return EXIT_FAILURE;
	}
	catch (...)
	{
		printf("Unhandled exception\n");
		return EXIT_FAILURE;
	}
#endif
	// ------------------------------------------------------------------------------------------

	DeleteFileA("PropertyList.json");

	try
	{
		if (VFS::GeneratePropertyList(target) == false)
		{
			printf("Property list can NOT created!\n");
			return EXIT_FAILURE;
		}
	}
	catch (std::exception & e)
	{
		printf("Exception handled: %s\n", e.what());
		return EXIT_FAILURE;
	}
	catch (...)
	{
		printf("Unhandled exception\n");
		return EXIT_FAILURE;
	}

	printf("Property list succesfully created!\n");

	vfs->FinalizeVFSPack();
	std::system("PAUSE");
	return EXIT_SUCCESS;
};
