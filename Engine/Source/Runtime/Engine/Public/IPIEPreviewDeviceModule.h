// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "IDeviceProfileSelectorModule.h"
#include "Widgets/SWindow.h"
#include "PIEPreviewDeviceEnumeration.h"

class IPIEPreviewDeviceModule : public IDeviceProfileSelectorModule
{
	public:
		//~ Begin IDeviceProfileSelectorModule Interface
		virtual const FString GetRuntimeDeviceProfileName() override;
		//~ End IDeviceProfileSelectorModule Interface


		//~ Begin IModuleInterface Interface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
		//~ End IModuleInterface Interface

		/**
		* Virtual destructor.
		*/
		virtual ~IPIEPreviewDeviceModule()
		{
		}

		//~ Begin IPIEPreviewDeviceModule Interface

		/**
		 * Gives the PIEPreviewDeviceModule a chance to modify the command line based on the target device json
		 */
		virtual void ApplyCommandLineOverrides() = 0;

		/**
		* Create PieWindow Ref
		*/
		virtual  TSharedRef<SWindow>   CreatePIEPreviewDeviceWindow(FVector2D ClientSize, FText WindowTitle, EAutoCenter AutoCenterType, FVector2D ScreenPosition, TOptional<float> MaxWindowWidth, TOptional<float> MaxWindowHeight) = 0;

		/**
		 * should be called after the window is created and registered and before scene rendering begins
		*/
		virtual void OnWindowReady(TSharedRef<SWindow> Window) {};

		/**
		* Apply PieWindow device parameters
		*/
		virtual void ApplyPreviewDeviceState() = 0;

		/** we need the game layer manager to control the DPI scaling behavior and this function can be called should be called when the manager is available */
		virtual void SetGameLayerManagerWidget(TSharedPtr<class SGameLayerManager> GameLayerManager)
		{
		}

		//~ End IDeviceProfileSelectorModule Interface
};