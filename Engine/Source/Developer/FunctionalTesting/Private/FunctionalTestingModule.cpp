// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingModule.h"
#include "FunctionalTestingManager.h"
#include "Misc/CoreMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "FunctionalTest.h"
#include "EngineGlobals.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"
#include "Misc/CommandLine.h"

#define LOCTEXT_NAMESPACE "FunctionalTesting"

DEFINE_LOG_CATEGORY(LogFunctionalTest);

class FFunctionalTestingModule : public IFunctionalTestingModule
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual void RunAllTestsOnMap(bool bClearLog, bool bRunLooped) override;
	virtual void RunTestOnMap(const FString& TestName, bool bClearLog, bool bRunLooped) override;
	virtual void MarkPendingActivation() override;
	virtual bool IsActivationPending() const override;
	virtual bool IsRunning() const override;
	virtual bool IsFinished() const override;
	virtual void SetManager(class UFunctionalTestingManager* NewManager) override;
	virtual class UFunctionalTestingManager* GetCurrentManager();
	virtual void SetLooping(const bool bLoop) override;
	virtual void GetMapTests(bool bEditorOnlyTests, TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands, TArray<FString>& OutTestMapAssets) const override;

private:
	UWorld* GetTestWorld();
	void OnGetAssetTagsForWorld(const UWorld* World, TArray<UObject::FAssetRegistryTag>& OutTags);

	void BuildTestBlacklistFromConfig();
	bool IsBlacklisted(const FString& MapName, const FString& TestName, FString* OutReason=nullptr, bool* OutWarn=nullptr) const;

	struct FBlacklistEntry
	{
		FBlacklistEntry() :
			bWarn(false) {}

		FString	Map;
		FString Test;
		FString Reason;
		bool bWarn;
	};

	TMap<FString, FBlacklistEntry> TestBlacklist;
	TWeakObjectPtr<class UFunctionalTestingManager> TestManager;
	bool bPendingActivation;
};

void FFunctionalTestingModule::StartupModule() 
{
	bPendingActivation = false;
#if WITH_EDITOR
	FWorldDelegates::GetAssetTags.AddRaw(this, &FFunctionalTestingModule::OnGetAssetTagsForWorld);
#endif
	BuildTestBlacklistFromConfig();
}

void FFunctionalTestingModule::ShutdownModule() 
{
#if WITH_EDITOR
	FWorldDelegates::GetAssetTags.RemoveAll(this);
#endif
}

void FFunctionalTestingModule::OnGetAssetTagsForWorld(const UWorld* World, TArray<UObject::FAssetRegistryTag>& OutTags)
{
#if WITH_EDITOR
	int32 Tests = 0;
	FString TestNames, TestNamesEditor;
	for (TActorIterator<AFunctionalTest> ActorItr(const_cast<UWorld*>(World), AFunctionalTest::StaticClass(), EActorIteratorFlags::AllActors); ActorItr; ++ActorItr)
	{
		AFunctionalTest* FunctionalTest = *ActorItr;

		// Only include enabled tests in the list of functional tests to run.
		if (FunctionalTest->IsEnabled())
		{
			bool bIsEditorOnly = IsEditorOnlyObject(FunctionalTest);

			// Check if this class is editor only
			FString& NamesAppend = bIsEditorOnly ? TestNamesEditor : TestNames;

			Tests++;
			NamesAppend.Append(FunctionalTest->GetActorLabel() + TEXT("|") + FunctionalTest->GetName());
			NamesAppend.Append(TEXT(";"));
		}
	}

	if (Tests > 0)
	{
		OutTags.Add(UObject::FAssetRegistryTag("Tests", FString::FromInt(Tests), UObject::FAssetRegistryTag::TT_Numerical));
	}

	if (!TestNames.IsEmpty())
	{
		OutTags.Add(UObject::FAssetRegistryTag("TestNames", TestNames, UObject::FAssetRegistryTag::TT_Hidden));
	}

	if (!TestNamesEditor.IsEmpty())
	{
		OutTags.Add(UObject::FAssetRegistryTag("TestNamesEditor", TestNamesEditor, UObject::FAssetRegistryTag::TT_Hidden));
	}
#endif
}

void FFunctionalTestingModule::GetMapTests(bool bEditorOnlyTests, TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands, TArray<FString>& OutTestMapAssets) const
{
	IAssetRegistry& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	if (!AssetRegistry.IsLoadingAssets())
	{
#if WITH_EDITOR
		static bool bDidScan = false;

		if (!GIsEditor && !bDidScan)
		{
			// For editor build -game, we need to do a full scan
			AssetRegistry.SearchAllAssets(true);
			bDidScan = true;
		}
#endif

		TArray<FAssetData> MapList;
		FARFilter Filter;
		Filter.ClassNames.Add(UWorld::StaticClass()->GetFName());
		Filter.bRecursiveClasses = true;
		Filter.bIncludeOnlyOnDiskAssets = true;
		if (AssetRegistry.GetAssets(Filter, /*out*/ MapList))
		{
			for (const FAssetData& MapAsset : MapList)
			{
				FString MapAssetPath = MapAsset.ObjectPath.ToString();

				const FString* Tests = MapAsset.TagsAndValues.Find(TEXT("Tests"));
				const FString* TestNames = MapAsset.TagsAndValues.Find(bEditorOnlyTests ? TEXT("TestNamesEditor") : TEXT("TestNames"));

				if (Tests && TestNames)
				{
					int32 TestCount = FCString::Atoi(**Tests);
					if (TestCount > 0)
					{
						TArray<FString> MapTests;
						(*TestNames).ParseIntoArray(MapTests, TEXT(";"), true);

						for (const FString& MapTest : MapTests)
						{
							FString BeautifulTestName;
							FString RealTestName;
							FString MapPackageName = MapAsset.PackageName.ToString();

							if (MapTest.Split(TEXT("|"), &BeautifulTestName, &RealTestName))
							{
								FString BlacklistReason;
								bool bWarn(false);
								if (!IsBlacklisted(MapPackageName, RealTestName, &BlacklistReason, &bWarn))
								{
									OutBeautifiedNames.Add(MapPackageName + TEXT(".") + *BeautifulTestName);
									OutTestCommands.Add(MapAssetPath + TEXT(";") + MapAsset.PackageName.ToString() + TEXT(";") + *RealTestName);
									OutTestMapAssets.AddUnique(MapAssetPath);
								}
								else
								{
									if (bWarn)
									{
										UE_LOG(LogFunctionalTest, Warning, TEXT("Test '%s' is blacklisted. %s"), *MapTest, *BlacklistReason);
									}
									else
									{
										UE_LOG(LogFunctionalTest, Display, TEXT("Test '%s' is blacklisted. %s"), *MapTest, *BlacklistReason);
									}
								}
							}
						}
					}
				}
				else if (!bEditorOnlyTests && MapAsset.AssetName.ToString().Find(TEXT("FTEST_")) == 0)
				{
					OutBeautifiedNames.Add(MapAsset.AssetName.ToString());
					OutTestCommands.Add(MapAssetPath + TEXT(";") + MapAsset.PackageName.ToString());
					OutTestMapAssets.AddUnique(MapAssetPath);
				}
			}
		}
	}
}

void FFunctionalTestingModule::SetManager(class UFunctionalTestingManager* NewManager)
{
	TestManager = NewManager;
}

UFunctionalTestingManager* FFunctionalTestingModule::GetCurrentManager()
{
	return TestManager.Get();
}

bool FFunctionalTestingModule::IsRunning() const
{
	return TestManager.IsValid() && TestManager->IsRunning();
}

bool FFunctionalTestingModule::IsFinished() const
{
	return (!TestManager.IsValid() || TestManager->IsFinished());
}

void FFunctionalTestingModule::MarkPendingActivation()
{
	bPendingActivation = true;
}

bool FFunctionalTestingModule::IsActivationPending() const
{
	return bPendingActivation;
}

void FFunctionalTestingModule::SetLooping(const bool bLoop)
{
	if (TestManager.IsValid())
	{
		TestManager->SetLooped(bLoop);
	}
}

UWorld* FFunctionalTestingModule::GetTestWorld()
{
#if WITH_EDITOR
	const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
	for (const FWorldContext& Context : WorldContexts)
	{
		if (Context.World() != nullptr)
		{
			if (Context.WorldType == EWorldType::PIE /*&& Context.PIEInstance == 0*/)
			{
				return Context.World();
			}
			
			if (Context.WorldType == EWorldType::Game)
			{
				return Context.World();
			}
		}
	}
#endif

	UWorld* TestWorld = GWorld;
	if (GIsEditor)
	{
		UE_LOG(LogFunctionalTest, Warning, TEXT("Functional Test using GWorld.  Not correct for PIE"));
	}

	return TestWorld;
}

void FFunctionalTestingModule::RunAllTestsOnMap(bool bClearLog, bool bRunLooped)
{
	if (UWorld* TestWorld = GetTestWorld())
	{
		bPendingActivation = false;
		if (UFunctionalTestingManager::RunAllFunctionalTests(TestWorld, bClearLog, bRunLooped) == false)
		{
			UE_LOG(LogFunctionalTest, Error, TEXT("No functional testing script on map."));
		}
	}
}

void FFunctionalTestingModule::RunTestOnMap(const FString& TestName, bool bClearLog, bool bRunLooped)
{
	if (UWorld* TestWorld = GetTestWorld())
	{
		bPendingActivation = false;
		if (UFunctionalTestingManager::RunAllFunctionalTests(TestWorld, bClearLog, bRunLooped, TestName) == false)
		{
			UE_LOG(LogFunctionalTest, Error, TEXT("No functional testing script on map."));
		}
	}
}

void FFunctionalTestingModule::BuildTestBlacklistFromConfig() 
{
	TestBlacklist.Empty();
	if (GConfig)
	{
	
		const FString CommandLine = FCommandLine::Get();

		for (const TPair<FString,FConfigFile>& Config : *GConfig)
		{
			FConfigSection* BlacklistSection = GConfig->GetSectionPrivate(TEXT("AutomationTestBlacklist"), false, true, Config.Key);
			if (BlacklistSection)
			{
				// Parse all blacklist definitions of the format "BlacklistTest=(Map=/Game/Tests/MapName, Test=TestName, Reason="Foo")"
				for (FConfigSection::TIterator Section(*BlacklistSection); Section; ++Section)
				{
					if (Section.Key() == TEXT("BlacklistTest"))
					{
						FString BlacklistValue = Section.Value().GetValue();
						FString Map, Test, Reason, Warn;
						bool bSuccess = false;
						
						if (FParse::Value(*BlacklistValue, TEXT("Map="), Map, true) && FParse::Value(*BlacklistValue, TEXT("Test="), Test, true))
						{
							FParse::Value(*BlacklistValue, TEXT("Reason="), Reason);
							FParse::Value(*BlacklistValue, TEXT("Warn="), Warn);

							// These are used as folders so ensure they match the expected layout
							if (Map.StartsWith(TEXT("/")))
							{
								FString ListName = Map + TEXT("/") + Test;
								ListName.RemoveSpacesInline();
								bSuccess = true;

								if (CommandLine.Contains(Map) || CommandLine.Contains(Test))
								{
									UE_LOG(LogFunctionalTest, Warning, TEXT("Test '%s' is blacklisted but allowing due to command line."), *BlacklistValue);
								}
								else
								{
									// convert Pretty.Name into PrettyName as we compare on the latter.
									ListName = ListName.Replace(TEXT("."), TEXT(""));
									FBlacklistEntry& Entry = TestBlacklist.Add(ListName);
									Entry.Map = Map;
									Entry.Test = Test;
									Entry.Reason = Reason;
									Entry.bWarn = Warn.ToBool();
								}
							}
						}
						
						if (!bSuccess)
						{
							UE_LOG(LogFunctionalTest, Error, TEXT("Invalid blacklisted test definition: '%s'"), *BlacklistValue);
						}
					}
				}
			}
		}
	}

	if (TestBlacklist.Num() > 0)
	{
		UE_LOG(LogFunctionalTest, Log, TEXT("Automated Test Blacklist:"));
		for (auto& KV : TestBlacklist)
		{
			UE_LOG(LogFunctionalTest, Log, TEXT("\tTest: %s"), *KV.Key);
		}
	}
}

bool FFunctionalTestingModule::IsBlacklisted(const FString& MapName, const FString& TestName, FString* OutReason, bool *OutWarn) const
{
	FString ListName = MapName + TEXT("/") + TestName;
	ListName.RemoveSpacesInline();

	const FBlacklistEntry* Entry = TestBlacklist.Find(ListName);

	if (Entry)
	{
		if (OutReason != nullptr)
		{
			*OutReason = Entry->Reason;
		}

		if (OutWarn != nullptr)
		{
			*OutWarn = Entry->bWarn;
		}
	}

	return Entry != nullptr;
}

//////////////////////////////////////////////////////////////////////////
// Exec
//////////////////////////////////////////////////////////////////////////

static bool FuncTestExec(UWorld* InWorld, const TCHAR* Command, FOutputDevice& Ar)
{
	if (FParse::Command(&Command, TEXT("ftest")))
	{
		if (FParse::Command(&Command, TEXT("start")))
		{
			const bool bLooped = FParse::Command(&Command, TEXT("loop"));

			//instead of allowing straight use of the functional test framework, this should go through the automation framework and kick off one of the Editor/Client functional tests

			IFunctionalTestingModule& Module = IFunctionalTestingModule::Get();
			if (!Module.IsRunning() && !Module.IsActivationPending())
			{
				Module.RunAllTestsOnMap(/*bClearLog=*/true, bLooped);
			}
		}
		return true;
	}
	return false;
}

FStaticSelfRegisteringExec FuncTestExecRegistration(FuncTestExec);


IMPLEMENT_MODULE( FFunctionalTestingModule, FunctionalTesting );

#undef LOCTEXT_NAMESPACE
