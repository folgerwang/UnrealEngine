// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PreLoadScreen.h"

// Base implementation of the IPreLoadScreen that handles all the logic for controlling / updating the UI for PreLoadScreens.
// Designed to be overriden by a game specific Plugin that calls FPreloadScreenManager::RegisterPreLoadScreen so that functions are called by the PreLoadScreenManager correctly.
class PRELOADSCREEN_API FPreLoadScreenBase : 
    public IPreLoadScreen 
    //,public TSharedFromThis<FPreLoadScreenBase, ESPMode::ThreadSafe>
{

    /**** IPreLoadScreen implementation ****/
public:
    //We don't use these in the FPreLoadScreenBase, but they are useful for game-specific implementations
    virtual void Tick(float DeltaTime) {}
    virtual void RenderTick(float DeltaTime) {};
    virtual void OnStop() {}

    //Store off TargetWindow
    virtual void OnPlay(TWeakPtr<SWindow> TargetWindow) { OwningWindow = TargetWindow; }

    //By default have a small added tick delay so we don't super spin out while waiting on other threads to load data / etc.
    virtual float GetAddedTickDelay() { return 0.02f; }

    virtual void Init();
    
    virtual TSharedPtr<SWidget> GetWidget() override { return nullptr; }
    virtual const TSharedPtr<const SWidget> GetWidget() const override { return nullptr; }

    //IMPORTANT: This changes a LOT of functionality and implementation details. EarlyStartupScreens happen before the engine is fully initialized and block engine initialization before they finish.
    //           this means they have to forgo even the most basic of engine features like UObject support, as they are displayed before those systems are initialized.
    virtual EPreLoadScreenTypes GetPreLoadScreenType() const { return EPreLoadScreenTypes::EngineLoadingScreen; }

    virtual void SetEngineLoadingFinished(bool IsEngineLoadingFinished) override { bIsEngineLoadingFinished = IsEngineLoadingFinished; }

	// PreLoadScreens not using this functionality should return NAME_None
	virtual FName GetPreLoadScreenTag() const override { return NAME_None; }

    virtual void CleanUp();

    //Default behavior is just to see if we have an active widget. Should really overload with our own behavior to see if we are done displaying
    virtual bool IsDone() const;

public:
    FPreLoadScreenBase()
        : bIsEngineLoadingFinished(false)
    {}

    virtual ~FPreLoadScreenBase() {};

    //Handles constructing a FPreLoadSettingsContainerBase with the 
    virtual void InitSettingsFromConfig(const FString& ConfigFileName);

    //Set what plugin is creating this PreLoadScreenBase. Used to make file paths relative to that plugin as well as
    //determining . Used for converting locations for content to be relative to the plugin calling us
    virtual void SetPluginName(const FString& PluginNameIn) { PluginName = PluginNameIn; }

protected:
    TWeakPtr<SWindow> OwningWindow;

    bool bIsEngineLoadingFinished;
private:
    
    //The name of the Plugin creating this FPreLoadScreenBase.
    //Important: Should be set before Initting settings from Config!
    FString PluginName;
};