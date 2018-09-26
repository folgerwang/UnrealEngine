// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderingThread.h"

struct FSlateDynamicImageBrush;
struct FCompositeFont;

//This is a helper class that we use to hold values we parse from the .ini. Clean way to access things like dynamic image brushes / fonts / etc used in our UI that
//we want to be somewhat data driven but we can't rely on UObject support to implement(as the PreLoad stuff happens too early for UObject support)
//This lets us set easy to change values in our .ini that are parsed at runtime and stored in this container
class PRELOADSCREEN_API FPreLoadSettingsContainerBase : public FDeferredCleanupInterface
{
public:

    //Helper class to store groups of things we want to display together in the UI so that we can parse it easily in the .ini. 
    //IE: Show this background, with this text at this font size
    class FScreenGroupingBase
    {
    public:
        
        FString ScreenBackgroundIdentifer;
        FString TextIdentifier;
        float FontSize;

        FScreenGroupingBase(const FString& ScreenBackgroundIdentifierIn, const FString& TextIdentifierIn, float FontSizeIn)
            : ScreenBackgroundIdentifer(ScreenBackgroundIdentifierIn)
            , TextIdentifier(TextIdentifierIn)
            , FontSize(FontSizeIn)
        {}
    };

public:

    static FPreLoadSettingsContainerBase& Get()
    {
        if (Instance == nullptr)
        {
            Instance = new FPreLoadSettingsContainerBase();
        }

        return *Instance;
    }

    static void Destroy()
    {
        if (Instance)
        {
            delete Instance;
            Instance = nullptr;
        }
    }

public:

    FPreLoadSettingsContainerBase() 
    { 
    }

    virtual ~FPreLoadSettingsContainerBase();

public:

    virtual const FSlateDynamicImageBrush* GetBrush(const FString& Identifier);
    virtual FText GetLocalizedText(const FString& Identifier);
    virtual TSharedPtr<FCompositeFont> GetFont(const FString& Identifier);
    virtual FScreenGroupingBase* GetScreenGrouping(const FString& Identifier);

    int GetNumScreenGroupings() const { return ScreenGroupings.Num(); }

    virtual const FScreenGroupingBase* GetScreenAtIndex(int index) { return IsValidScreenIndex(index) ? &ScreenGroupings[*ScreenDisplayOrder[index]] : nullptr; }
    virtual bool IsValidScreenIndex(int index) { return ScreenDisplayOrder.IsValidIndex(index); }

    virtual void CreateCustomSlateImageBrush(const FString& Identifier, const FString& TexturePath, const FVector2D& ImageDimensions);
    virtual void AddLocalizedText(const FString& Identifier, FText LocalizedText);
    virtual void AddScreenGrouping(const FString& Identifier, FScreenGroupingBase& ScreenGrouping);
    
    //Maps the given font file to the given language and stores it under the FontIdentifier.
    //Identifier maps the entire CompositeFont, so if you want to add multiple fonts  for multiple languages, just store them all under the same identifer
    virtual void BuildCustomFont(const FString& FontIdentifier, const FString& Language, const FString& FilePath);

    //Helper functions that parse a .ini config entry and call the appropriate create function to 
    virtual void ParseBrushConfigEntry(const FString& BrushConfigEntry);
    virtual void ParseFontConfigEntry(const FString&  SplitConfigEntry);
    virtual void ParseLocalizedTextConfigString(const FString&  SplitConfigEntry);
    virtual void ParseScreenGroupingConfigString(const FString&  SplitConfigEntry);
    
    //Sets the PluginContent dir so that when parsing config entries we can accept plugin relative file paths
    virtual void SetPluginContentDir(const FString& PluginContentDirIn) { PluginContentDir = PluginContentDirIn; }

    float TimeToDisplayEachBackground;
    
    //Screens are displayed in the order of this array
    TArray<FString> ScreenDisplayOrder;

    //Helper function that takes in a file path and tries to reconsile it to be Plugin Specific if applicable.
    //Ensures if file is not found in either Plugin's content dir or the original path
    virtual FString ConvertIfPluginRelativeContentPath(const FString& FilePath);

protected:

    //Helper functions that verify if the supplied .ini config entry is valid to create a resource out of it
    virtual bool IsValidBrushConfig(TArray<FString>& SplitConfigEntry);
    virtual bool IsValidFontConfigString(TArray<FString>& SplitConfigEntry);
    virtual bool IsValidLocalizedTextConfigString(TArray<FString>& SplitConfigEntry);
    virtual bool IsValidScreenGrooupingConfigString(TArray<FString>& SplitConfigEntry);

protected:
    
    /* Property Storage. Ties FName to a particular resource so we can get it by identifier. */
    TMap<FName, const FSlateDynamicImageBrush*> BrushResources;
    TMap<FName, FText> LocalizedTextResources;
    TMap<FName, TSharedPtr<FCompositeFont>> FontResources;
    TMap<FName, FScreenGroupingBase> ScreenGroupings;

    //This string is used to make file paths relative to a particular Plugin's content directory when parsing file paths.
    FString PluginContentDir;

    // Singleton Instance -- This is only not a TSharedPtr as it needs to be cleaned up by a deferredcleanup call which directly
    // nukes the underlying object, causing a SharedPtr crash at shutdown.
    static FPreLoadSettingsContainerBase* Instance;
};