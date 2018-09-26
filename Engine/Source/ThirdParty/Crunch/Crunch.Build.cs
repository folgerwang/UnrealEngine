// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Crunch : ModuleRules
{
	public Crunch(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

        string BasePath = Target.UEThirdPartySourceDirectory + "Crunch/";
        PublicSystemIncludePaths.Add(BasePath + "include");

#if false
        string LibPath = BasePath + "Lib/";


        if (Target.Type == TargetType.Editor)
        {
            // link with lib to allow encoding
            if (Target.Platform == UnrealTargetPlatform.Win32 ||
                Target.Platform == UnrealTargetPlatform.Win64)
            {
#if false
                LibPath += (Target.Platform == UnrealTargetPlatform.Win64) ? "Win64/" : "Win32/";
                PublicLibraryPaths.Add(LibPath);
                PublicAdditionalLibraries.Add("crnlib.lib");
#endif
                string Err = "Crunch not setup yet for this platform";
                System.Console.WriteLine(Err);
                throw new BuildException(Err);
            }
            else
            {
                string Err = "Crunch not setup yet for this platform";
                System.Console.WriteLine(Err);
                throw new BuildException(Err);
            }
        }
#endif
    }
}
