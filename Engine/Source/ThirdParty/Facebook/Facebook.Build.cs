// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Facebook : ModuleRules
{
	public Facebook(ReadOnlyTargetRules Target) : base(Target)
    {
		Type = ModuleType.External;

		// Additional Frameworks and Libraries for Android found in OnlineSubsystemFacebook_UPL.xml
        if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicDefinitions.Add("WITH_FACEBOOK=1");
			PublicDefinitions.Add("UE4_FACEBOOK_VER=4.38");

            // These are iOS system libraries that Facebook depends on
            //PublicFrameworks.AddRange(
            //new string[] {
            //    "ImageIO"
            //});

            // More dependencies for Facebook
            //PublicAdditionalLibraries.AddRange(
            //new string[] {
            //    "xml2"
            //});

			//PublicAdditionalFrameworks.Add(
			//	new UEBuildFramework(
			//		"AccountKit",
			//		"IOS/FacebookSDK/AccountKit.embeddedframework.zip",
			//		"AccountKit.framework/AccountKitStrings.bundle"
			//	)
			//);

			PublicAdditionalFrameworks.Add(
				new Framework(
					"Bolts",
					"IOS/FacebookSDK/Bolts.embeddedframework.zip"
				)
			);

			// Access to Facebook core
			PublicAdditionalFrameworks.Add(
				new Framework(
					"FBSDKCoreKit",
					"IOS/FacebookSDK/FBSDKCoreKit.embeddedframework.zip",
					"FBSDKCoreKit.framework/Resources/FacebookSDKStrings.bundle"
				)
			);

			// Access to Facebook login
			PublicAdditionalFrameworks.Add(
				new Framework(
					"FBSDKLoginKit",
					"IOS/FacebookSDK/FBSDKLoginKit.embeddedframework.zip"
				)
			);

			// Access to Facebook marketing 
			//PublicAdditionalFrameworks.Add(
			//	new UEBuildFramework(
			//		"FBSDKMarketingKit",
			//		"IOS/FacebookSDK/FBSDKMarketingKit.embeddedframework.zip"
			//	)
			//);

			// commenting out over if(false) for #jira FORT-77943 per Peter.Sauerbrei prior change with CL 3960071
			//// Access to Facebook places
			//PublicAdditionalFrameworks.Add(
			//	new UEBuildFramework(
			//		"FBSDKPlacesKit",
			//		"IOS/FacebookSDK/FBSDKPlacesKit.embeddedframework.zip"
			//	)
			//);

			// Access to Facebook sharing
			PublicAdditionalFrameworks.Add(
				new Framework(
					"FBSDKShareKit",
					"IOS/FacebookSDK/FBSDKShareKit.embeddedframework.zip"
				)
			);
		}
	}
}

