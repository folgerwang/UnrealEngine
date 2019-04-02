// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Vulkan : ModuleRules
{
    public Vulkan(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string VulkanSDKPath = Environment.GetEnvironmentVariable("VULKAN_SDK");
        bool bSDKInstalled = !String.IsNullOrEmpty(VulkanSDKPath);
        bool bUseThirdParty = true;
        if (bSDKInstalled)
        {
            // Check if the installed SDK is newer or the same than the provided headers distributed with the Engine
            int ThirdPartyVersion = GetThirdPartyVersion();
            string VulkanSDKHeaderPath = Path.Combine(VulkanSDKPath, "include");

            int SDKVersion = GetSDKVersion(VulkanSDKHeaderPath);
            if (SDKVersion >= ThirdPartyVersion)
            {
                // If the user has an installed SDK, use that instead
                PublicSystemIncludePaths.Add(VulkanSDKHeaderPath);
                // Older SDKs have an extra subfolder
                PublicSystemIncludePaths.Add(VulkanSDKHeaderPath + "/vulkan");

                bUseThirdParty = false;
            }
        }

        if (bUseThirdParty)
        {
            string RootPath = Target.UEThirdPartySourceDirectory + "Vulkan";

            PublicSystemIncludePaths.Add(RootPath + "/Include");
            PublicSystemIncludePaths.Add(RootPath + "/Include/vulkan");
        }

        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
        {
            // Let's always delay load the vulkan dll as not everyone has it installed
            PublicDelayLoadDLLs.Add("vulkan-1.dll");
        }
    }

    internal static int GetVersionFromString(string Text)
    {
        string Token = "#define VK_HEADER_VERSION ";
        Int32 FoundIndex = Text.IndexOf(Token);
        if (FoundIndex > 0)
        {
            string Version = Text.Substring(FoundIndex + Token.Length, 5);
            int Index = 0;
            while (Version[Index] >= '0' && Version[Index] <= '9')
            {
                ++Index;
            }

            Version = Version.Substring(0, Index);

            int VersionNumber = Convert.ToInt32(Version);
            return VersionNumber;
        }

        return -1;
    }

    internal static int GetThirdPartyVersion()
    {
        try
        {
            // Extract current version on ThirdParty
            string Text = File.ReadAllText("ThirdParty/Vulkan/Include/vulkan/vulkan_core.h");
            return GetVersionFromString(Text);
        }
        catch (Exception)
        {
        }

        return -1;
    }

    internal static int GetSDKVersion(string VulkanSDKPath)
    {
        try
        {
            // Extract current version on the SDK folder. Newer SDKs store the version in vulkan_core.h
            string Header = Path.Combine(VulkanSDKPath, "Include/vulkan/vulkan_core.h");
            if (!File.Exists(Header))
            {
                Header = Path.Combine(VulkanSDKPath, "Include/vulkan/vulkan.h");
            }
            string Text = File.ReadAllText(Header);
            return GetVersionFromString(Text);
        }
        catch (Exception)
        {
        }

        return -1;
    }
}
