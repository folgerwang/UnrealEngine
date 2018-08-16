// UsdWrapperTest.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <tchar.h>

#include "../Source/Public/UnrealUSDWrapper.h"

typedef int int32;

void FindActorsToSpawnRecursive(IUsdStage* UsdStage, IUsdPrim* UsdPrim)
{
	if (UsdPrim->HasGeometryData())
	{
		const FUsdGeomData* UsdGeomData = UsdPrim->GetGeometryData();
	}

	if (UsdPrim->HasTransform())
	{
	
	}

	for (int32 i = 0; i < UsdPrim->GetNumChildren(); ++i)
	{
		FindActorsToSpawnRecursive(UsdStage, UsdPrim->GetChild(i));
	}
}

void FindActorsToSpawn(IUsdStage* UsdStage)
{
	IUsdPrim* RootPrim = UsdStage->GetRootPrim();
	if (!RootPrim)
	{
		return;
	}

	if (RootPrim->HasTransform())
	{
		FindActorsToSpawnRecursive(UsdStage, RootPrim);
	}
	else
	{
		for (int32 i = 0; i < RootPrim->GetNumChildren(); ++i)
		{
			FindActorsToSpawnRecursive(UsdStage, RootPrim->GetChild(i));
		}
	}
}


int main()
{
	const char* Path = "d:\\test\\";
	const char* Filename = "baseLevel.usda";
	std::string BasePluginPath = "..\\..\\";

	std::vector<std::string> PluginDirectories;
	PluginDirectories.push_back(BasePluginPath + "/Resources/UsdResources/Windows/plugins");
	UnrealUSDWrapper::Initialize(PluginDirectories);

	IUsdStage* UsdStage = UnrealUSDWrapper::ImportUSDFile(Path, Filename);
	if (UsdStage)
	{
		FindActorsToSpawn(UsdStage);
	}

	UnrealUSDWrapper::CleanUp();

    return 0;
}

