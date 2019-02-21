// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserWidgetModule.h"
#include "Modules/ModuleManager.h"
#include "WebBrowserAssetManager.h"
#include "WebBrowserModule.h"
#include "IWebBrowserSingleton.h"
#include "Materials/Material.h"

//////////////////////////////////////////////////////////////////////////
// FWebBrowserWidgetModule

class FWebBrowserWidgetModule : public IWebBrowserWidgetModule
{
public:
	virtual void StartupModule() override
	{
#if WITH_EDITOR || PLATFORM_ANDROID || PLATFORM_IOS
		if (WebBrowserAssetMgr == nullptr)
		{
			WebBrowserAssetMgr = NewObject<UWebBrowserAssetManager>((UObject*)GetTransientPackage(), NAME_None, RF_Transient | RF_Public);
			WebBrowserAssetMgr->LoadDefaultMaterials();

			IWebBrowserSingleton* WebBrowserSingleton = IWebBrowserModule::Get().GetSingleton();
			if (WebBrowserSingleton)
			{
				WebBrowserSingleton->SetDefaultMaterial(WebBrowserAssetMgr->GetDefaultMaterial());
				WebBrowserSingleton->SetDefaultTranslucentMaterial(WebBrowserAssetMgr->GetDefaultTranslucentMaterial());
			}
		}
#endif
	}

	virtual void ShutdownModule() override
	{
	}
private:
#if WITH_EDITOR || PLATFORM_ANDROID || PLATFORM_IOS
	UWebBrowserAssetManager* WebBrowserAssetMgr;
#endif
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FWebBrowserWidgetModule, WebBrowserWidget);
