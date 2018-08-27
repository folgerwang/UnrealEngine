// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Package.h"
#include "TickableEditorObject.h"
#include "IPlatformFileSandboxWrapper.h"
#include "INetworkFileSystemModule.h"
#include "CookOnTheFlyServer.generated.h"


class FAssetRegistryGenerator;
class ITargetPlatform;
struct FPropertyChangedEvent;
class IPlugin;
class IAssetRegistry;

struct FPackageNameCache;
struct FPackageTracker;

enum class ECookInitializationFlags
{
	None =										0x00000000, // No flags
	//unused =									0x00000001, 
	Iterative =									0x00000002, // use iterative cooking (previous cooks will not be cleaned unless detected out of date, experimental)
	SkipEditorContent =							0x00000004, // do not cook any content in the content\editor directory
	Unversioned =								0x00000008, // save the cooked packages without a version number
	AutoTick =									0x00000010, // enable ticking (only works in the editor)
	AsyncSave =									0x00000020, // save packages async
	// unused =									0x00000040,
	IncludeServerMaps =							0x00000080, // should we include the server maps when cooking
	UseSerializationForPackageDependencies =	0x00000100, // should we use the serialization code path for generating package dependencies (old method will be deprecated)
	BuildDDCInBackground =						0x00000200, // build ddc content in background while the editor is running (only valid for modes which are in editor IsCookingInEditor())
	GeneratedAssetRegistry =					0x00000400, // have we generated asset registry yet
	OutputVerboseCookerWarnings =				0x00000800, // output additional cooker warnings about content issues
	EnablePartialGC =							0x00001000, // mark up with an object flag objects which are in packages which we are about to use or in the middle of using, this means we can gc more often but only gc stuff which we have finished with
	TestCook =									0x00002000, // test the cooker garbage collection process and cooking (cooker will never end just keep testing).
	//unused =									0x00004000,
	LogDebugInfo =								0x00008000, // enables additional debug log information
	IterateSharedBuild =						0x00010000, // iterate from a build in the SharedIterativeBuild directory 
	IgnoreIniSettingsOutOfDate =				0x00020000, // if the inisettings say the cook is out of date keep using the previously cooked build
	IgnoreScriptPackagesOutOfDate =				0x00040000, // for incremental cooking, ignore script package changes
};
ENUM_CLASS_FLAGS(ECookInitializationFlags);

enum class ECookByTheBookOptions
{
	None =								0x00000000, // no flags
	CookAll	=							0x00000001, // cook all maps and content in the content directory
	MapsOnly =							0x00000002, // cook only maps
	NoDevContent =						0x00000004, // don't include dev content
	ForceDisableCompressed =			0x00000010, // force compression to be disabled even if the cooker was initialized with it enabled
	ForceEnableCompressed =				0x00000020, // force compression to be on even if the cooker was initialized with it disabled
	ForceDisableSaveGlobalShaders =		0x00000040, // force global shaders to not be saved (used if cooking multiple times for the same platform and we know we are up todate)
	NoGameAlwaysCookPackages =			0x00000080, // don't include the packages specified by the game in the cook (this cook will probably be missing content unless you know what you are doing)
	NoAlwaysCookMaps =					0x00000100, // don't include always cook maps (this cook will probably be missing content unless you know what you are doing)
	NoDefaultMaps =						0x00000200, // don't include default cook maps (this cook will probably be missing content unless you know what you are doing)
	NoSlatePackages =					0x00000400, // don't include slate content (this cook will probably be missing content unless you know what you are doing)
	NoInputPackages =					0x00000800, // don't include slate content (this cook will probably be missing content unless you know what you are doing)
	DisableUnsolicitedPackages =		0x00001000, // don't cook any packages which aren't in the files to cook list (this is really dangerious as if you request a file it will not cook all it's dependencies automatically)
	FullLoadAndSave =					0x00002000, // Load all packages into memory and save them all at once in one tick for speed reasons. This requires a lot of RAM for large games.
};
ENUM_CLASS_FLAGS(ECookByTheBookOptions);


UENUM()
namespace ECookMode
{
	enum Type
	{
		/** Default mode, handles requests from network. */
		CookOnTheFly,
		/** Cook on the side. */
		CookOnTheFlyFromTheEditor,
		/** Precook all resources while in the editor. */
		CookByTheBookFromTheEditor,
		/** Cooking by the book (not in the editor). */
		CookByTheBook,
	};
}

UENUM()
enum class ECookTickFlags : uint8
{
	None =									0x00000000, /* no flags */
	MarkupInUsePackages =					0x00000001, /** Markup packages for partial gc */
	HideProgressDisplay =					0x00000002, /** Hides the progress report */
};
ENUM_CLASS_FLAGS(ECookTickFlags);

UCLASS()
class UNREALED_API UCookOnTheFlyServer : public UObject, public FTickableEditorObject, public FExec
{
	GENERATED_BODY()

	UCookOnTheFlyServer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	/** Current cook mode the cook on the fly server is running in */
	ECookMode::Type CurrentCookMode = ECookMode::CookOnTheFly;
	/** Directory to output to instead of the default should be empty in the case of DLC cooking */ 
	FString OutputDirectoryOverride;

	struct FCookByTheBookOptions;

	FCookByTheBookOptions* CookByTheBookOptions = nullptr;

	//////////////////////////////////////////////////////////////////////////
	// Cook on the fly options

	/** Cook on the fly server uses the NetworkFileServer */
	TArray<class INetworkFileServer*> NetworkFileServers;
	FOnFileModifiedDelegate FileModifiedDelegate;

	//////////////////////////////////////////////////////////////////////////
	// General cook options

	/** Number of packages to load before performing a garbage collect. Set to 0 to never GC based on number of loaded packages */
	uint32 PackagesPerGC;
	/** Amount of time that is allowed to be idle before forcing a garbage collect. Set to 0 to never force GC due to idle time */
	double IdleTimeToGC;
	/** Max memory the cooker should use before forcing a gc */
	uint64 MaxMemoryAllowance;
	/** Min memory before the cooker should partial gc */
	uint64 MinMemoryBeforeGC;
	/** If we have less then this much memory free then finish current task and kick off gc */
	uint64 MinFreeMemory;
	/** Max number of packages to save before we partial gc */
	int32 MaxNumPackagesBeforePartialGC;
	/** Max number of concurrent shader jobs reducing this too low will increase cook time */
	int32 MaxConcurrentShaderJobs;

	ECookInitializationFlags CookFlags = ECookInitializationFlags::None;
	TUniquePtr<class FSandboxPlatformFile> SandboxFile;
	bool bIsInitializingSandbox = false; // stop recursion into callbacks when we are initializing sandbox
	mutable bool bIgnoreMarkupPackageAlreadyLoaded = false; // avoid marking up packages as already loaded (want to put this around some functionality as we want to load packages fully some times)
	bool bIsSavingPackage = false; // used to stop recursive mark package dirty functions


	TMap<FName, int32> MaxAsyncCacheForType; // max number of objects of a specific type which are allowed to async cache at once
	mutable TMap<FName, int32> CurrentAsyncCacheForType; // max number of objects of a specific type which are allowed to async cache at once

	/** List of additional plugin directories to remap into the sandbox as needed */
	TArray<TSharedRef<IPlugin> > PluginsToRemap;

	//////////////////////////////////////////////////////////////////////////
	// precaching system
	//
	// this system precaches materials and textures before we have considered the object 
	// as requiring save so as to utilize the system when it's idle
	
	TArray<FWeakObjectPtr> CachedMaterialsToCacheArray;
	TArray<FWeakObjectPtr> CachedTexturesToCacheArray;
	int32 LastUpdateTick = 0;
	int32 MaxPrecacheShaderJobs = 0;
	void TickPrecacheObjectsForPlatforms(const float TimeSlice, const TArray<const ITargetPlatform*>& TargetPlatform);

	//////////////////////////////////////////////////////////////////////////

	// data about the current packages being processed
	// stores temporal state like finished cache as an optimization so we don't need to 
	struct FReentryData
	{
		FName FileName;
		bool bBeginCacheFinished;
		int BeginCacheCount;
		bool bFinishedCacheFinished;
		bool bIsValid;
		TArray<UObject*> CachedObjectsInOuter;
		TMap<FName, int32> BeginCacheCallCount;

		FReentryData() : FileName(NAME_None), bBeginCacheFinished(false), BeginCacheCount(0), bFinishedCacheFinished(false), bIsValid(false)
		{ }

		void Reset( const FName& InFilename )
		{
			FileName = InFilename;
			bBeginCacheFinished = false;
			BeginCacheCount = 0;
			bIsValid = false;
		}
	};

	mutable TMap<FName, FReentryData> PackageReentryData;

	FReentryData& GetReentryData(const UPackage* Package) const;

	FString ConvertCookedPathToUncookedPath(const FString& CookedPackageName) const;

	/** Get dependencies for package */
	const TArray<FName>& GetFullPackageDependencies(const FName& PackageName) const;
	mutable TMap<FName, TArray<FName>> CachedFullPackageDependencies;

	/** Cached copy of asset registry */
	IAssetRegistry* AssetRegistry = nullptr;

	/** Map of platform name to asset registry generators, which hold the state of asset registry data for a platform */
	TMap<FName, FAssetRegistryGenerator*> RegistryGenerators;

	/** Map of platform name to scl.csv files we saved out. */
	TMap<FName, TArray<FString>> OutSCLCSVPaths;

	/** List of filenames that may be out of date in the asset registry */
	TSet<FName> ModifiedAssetFilenames;

	//////////////////////////////////////////////////////////////////////////
	// iterative ini settings checking
	// growing list of ini settings which are accessed over the course of the cook

	// tmap of the Config name, Section name, Key name, to the value
	typedef TMap<FName, TMap<FName, TMap<FName, TArray<FString>>>> FIniSettingContainer;

	mutable bool IniSettingRecurse = false;
	mutable FIniSettingContainer AccessedIniStrings;
	TArray<const FConfigFile*> OpenConfigFiles;
	TArray<FString> ConfigSettingBlacklist;
	void OnFConfigDeleted(const FConfigFile* Config);
	void OnFConfigCreated(const FConfigFile* Config);

	void ProcessAccessedIniSettings(const FConfigFile* Config, FIniSettingContainer& AccessedIniStrings) const;

	/**
	* OnTargetPlatformChangedSupportedFormats
	* called when target platform changes the return value of supports shader formats 
	* used to reset the cached cooked shaders
	*/
	void OnTargetPlatformChangedSupportedFormats(const ITargetPlatform* TargetPlatform);

	/**
	 * Returns the current set of cooking targetplatforms
	 *  mostly used for cook on the fly or in situations where the cooker can't figure out what the target platform is
	 * 
	 * @return Array of target platforms which are can be used
	 */
	const TArray<ITargetPlatform*>& GetCookingTargetPlatforms() const;

	/** Cached cooking target platforms from the targetmanager, these are used when we don't know what platforms we should be targeting */
	mutable TArray<ITargetPlatform*> CookingTargetPlatforms;

public:

	enum ECookOnTheSideResult
	{
		COSR_CookedMap				= 0x00000001,
		COSR_CookedPackage			= 0x00000002,
		COSR_ErrorLoadingPackage	= 0x00000004,
		COSR_RequiresGC				= 0x00000008,
		COSR_WaitingOnCache			= 0x00000010,
		COSR_MarkedUpKeepPackages	= 0x00000040
	};


	virtual ~UCookOnTheFlyServer();

	/**
	* FTickableEditorObject interface used by cook on the side
	*/
	TStatId GetStatId() const override;
	void Tick(float DeltaTime) override;
	bool IsTickable() const override;
	ECookMode::Type GetCookMode() const { return CurrentCookMode; }


	/**
	* FExec interface used in the editor
	*/
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/**
	 * Dumps cooking stats to the log
	 *  run from the exec command "Cook stats"
	 */
	void DumpStats();

	/**
	* Initialize the CookServer so that either CookOnTheFly can be called or Cook on the side can be started and ticked
	*/
	void Initialize( ECookMode::Type DesiredCookMode, ECookInitializationFlags InCookInitializationFlags, const FString& OutputDirectoryOverride = FString() );

	/**
	* Cook on the side, cooks while also running the editor...
	*
	* @param  BindAnyPort					Whether to bind on any port or the default port.
	*
	* @return true on success, false otherwise.
	*/
	bool StartNetworkFileServer( bool BindAnyPort );

	/**
	* Broadcast our the fileserver presence on the network
	*/
	bool BroadcastFileserverPresence( const FGuid &InstanceId );
	/** 
	* Stop the network file server
	*
	*/
	void EndNetworkFileServer();


	struct FCookByTheBookStartupOptions
	{
	public:
		TArray<ITargetPlatform*> TargetPlatforms;
		TArray<FString> CookMaps;
		TArray<FString> CookDirectories;
		TArray<FString> NeverCookDirectories;
		TArray<FString> CookCultures; 
		TArray<FString> IniMapSections;
		TArray<FString> CookPackages; // list of packages we should cook, used to specify specific packages to cook
		ECookByTheBookOptions CookOptions = ECookByTheBookOptions::None;
		FString DLCName;
		FString CreateReleaseVersion;
		FString BasedOnReleaseVersion;
		bool bGenerateStreamingInstallManifests = false;
		bool bGenerateDependenciesForMaps = false;
		bool bErrorOnEngineContentUse = false; // this is a flag for dlc, will cause the cooker to error if the dlc references engine content
	};

	/**
	* Start a cook by the book session
	* Cook on the fly can't run at the same time as cook by the book
	*/
	void StartCookByTheBook( const FCookByTheBookStartupOptions& CookByTheBookStartupOptions );

	/**
	* Queue a cook by the book cancel (you might want to do this instead of calling cancel directly so that you don't have to be in the game thread when canceling
	*/
	void QueueCancelCookByTheBook();

	/**
	* Cancel the currently running cook by the book (needs to be called from the game thread)
	*/
	void CancelCookByTheBook();

	bool IsCookByTheBookRunning() const;

	/**
	* Get any packages which are in memory, these were probably required to be loaded because of the current package we are cooking, so we should probably cook them also
	*/
	TArray<UPackage*> GetUnsolicitedPackages(const TArray<FName>& TargetPlatformNames) const;

	/**
	* PostLoadPackageFixup
	* after a package is loaded we might want to fix up some stuff before it gets saved
	*/
	void PostLoadPackageFixup(UPackage* Package);

	/**
	* Handles cook package requests until there are no more requests, then returns
	*
	* @param  CookFlags output of the things that might have happened in the cook on the side
	* 
	* @return returns ECookOnTheSideResult
	*/
	uint32 TickCookOnTheSide( const float TimeSlice, uint32 &CookedPackagesCount, ECookTickFlags TickFlags = ECookTickFlags::None );

	/**
	* Clear all the previously cooked data all cook requests from now on will be considered recook requests
	*/
	void ClearAllCookedData();


	/**
	* Clear any cached cooked platform data for a platform
	*  call ClearCachedCookedPlatformData on all Uobjects
	* @param PlatformName platform to clear all the cached data for
	*/
	void ClearCachedCookedPlatformDataForPlatform( const FName& PlatformName );


	/**
	* Clear all the previously cooked data for the platform passed in 
	* 
	* @param name of the platform to clear the cooked packages for
	*/
	void ClearPlatformCookedData( const FName& PlatformName );


	/**
	* Recompile any global shader changes 
	* if any are detected then clear the cooked platform data so that they can be rebuilt
	* 
	* @return return true if shaders were recompiled
	*/
	bool RecompileChangedShaders(const TArray<FName>& TargetPlatforms);


	/**
	* Force stop whatever pending cook requests are going on and clear all the cooked data
	* Note cook on the side / cook on the fly clients may not be able to recover from this if they are waiting on a cook request to complete
	*/
	void StopAndClearCookedData();

	/** 
	* Process any shader recompile requests
	*/
	void TickRecompileShaderRequests();

	bool HasRecompileShaderRequests() const;

	bool HasCookRequests() const;

	uint32 NumConnections() const;

	/**
	* Is this cooker running in the editor
	*
	* @return true if we are running in the editor
	*/
	bool IsCookingInEditor() const;

	/**
	* Is this cooker running in real time mode (where it needs to respect the timeslice) 
	* 
	* @return returns true if this cooker is running in realtime mode like in the editor
	*/
	bool IsRealtimeMode() const;

	/**
	* Helper function returns if we are in any cook by the book mode
	*
	* @return if the cook mode is a cook by the book mode
	*/
	bool IsCookByTheBookMode() const;

	/**
	* Helper function returns if we are in any cook on the fly mode
	*
	* @return if the cook mode is a cook on the fly mode
	*/
	bool IsCookOnTheFlyMode() const;


	virtual void BeginDestroy() override;

	/** Returns the configured number of packages to process before GC */
	uint32 GetPackagesPerGC() const;

	/** Returns the configured number of packages to process before partial GC */
	uint32 GetPackagesPerPartialGC() const;

	/** Returns the configured amount of idle time before forcing a GC */
	double GetIdleTimeToGC() const;

	/** Returns the configured amount of memory allowed before forcing a GC */
	uint64 GetMaxMemoryAllowance() const;

	/** Mark package as keep around for the cooker (don't GC) */
	void MarkGCPackagesToKeepForCooker();

	bool HasExceededMaxMemory() const;

	/**
	* RequestPackage to be cooked
	*
	* @param StandardPackageFName name of the package in standard format as returned by FPaths::MakeStandardFilename
	* @param TargetPlatforms name of the targetplatforms we want this package cooked for
	* @param bForceFrontOfQueue should we put this package in the front of the cook queue (next to be processed) or at the end
	*/
	bool RequestPackage(const FName& StandardPackageFName, const TArray<FName>& TargetPlatforms, const bool bForceFrontOfQueue);

	/**
	* RequestPackage to be cooked
	* This function can only be called while the cooker is in cook by the book mode
	*
	* @param StandardPackageFName name of the package in standard format as returned by FPaths::MakeStandardFilename
	* @param bForceFrontOfQueue should we put this package in the front of the cook queue (next to be processed) or at the end
	*/
	bool RequestPackage(const FName& StandardPackageFName, const bool bForceFrontOfQueue);


	/**
	* Callbacks from editor 
	*/

	void OnObjectModified( UObject *ObjectMoving );
	void OnObjectPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);
	void OnObjectUpdated( UObject *Object );
	void OnObjectSaved( UObject *ObjectSaved );

	/**
	* Marks a package as dirty for cook
	* causes package to be recooked on next request (and all dependent packages which are currently cooked)
	*/
	void MarkPackageDirtyForCooker( UPackage *Package );

	/**
	* MaybeMarkPackageAsAlreadyLoaded
	* Mark the package as already loaded if we have already cooked the package for all requested target platforms
	* this hints to the objects on load that we don't need to load all our bulk data
	* 
	* @param Package to mark as not requiring reload
	*/
	void MaybeMarkPackageAsAlreadyLoaded(UPackage* Package);

	/**
	* Callbacks from UObject globals
	*/
	void PreGarbageCollect();

private:

	//////////////////////////////////////////////////////////////////////////
	// cook by the book specific functions
	/**
	* Collect all the files which need to be cooked for a cook by the book session
	*/
	void CollectFilesToCook(TArray<FName>& FilesInPath, 
		const TArray<FString>& CookMaps, const TArray<FString>& CookDirectories, 
		const TArray<FString>& IniMapSections, ECookByTheBookOptions FilesToCookFlags);

	/**
	* AddFileToCook add file to cook list 
	*/
	void AddFileToCook( TArray<FName>& InOutFilesToCook, const FString &InFilename ) const;

    /**
     * Invokes the necessary FShaderCodeLibrary functions to start cooking the shader code library.
     */
    void InitShaderCodeLibrary(void);
    
	/**
	* Invokes the necessary FShaderCodeLibrary functions to open a named code library.
	*/
	void OpenShaderCodeLibrary(FString const& Name);
    
	/**
	* Invokes the necessary FShaderCodeLibrary functions to save and close a named code library.
	*/
	void SaveShaderCodeLibrary(FString const& Name);

	/**
	* Calls the ShaderPipelineCacheToolsCommandlet to build a upipelinecache file from the .stablepc.csv file, if any
	*/
	void ProcessShaderCodeLibraries(const FString& LibraryName);

    /**
     * Invokes the necessary FShaderCodeLibrary functions to clean out all the temporary files.
     */
    void CleanShaderCodeLibraries();

	/**
	* Call back from the TickCookOnTheSide when a cook by the book finishes (when started form StartCookByTheBook)
	*/
	void CookByTheBookFinished();

	/**
	* Get all the packages which are listed in asset registry passed in.  
	*
	* @param AssetRegistryPath path of the assetregistry.bin file to read
	* @param OutPackageNames out list of uncooked package filenames which were contained in the asset registry file
	* @return true if successfully read false otherwise
	*/
	bool GetAllPackageFilenamesFromAssetRegistry( const FString& AssetRegistryPath, TArray<FName>& OutPackageFilenames ) const;

	/**
	* BuildMapDependencyGraph
	* builds a map of dependencies from maps
	* 
	* @param PlatformName name of the platform we want to build a map for
	*/
	void BuildMapDependencyGraph(const FName& PlatformName);

	/**
	* WriteMapDependencyGraph
	* write a previously built map dependency graph out to the sandbox directory for a platform
	*
	* @param PlatformName name of the platform we want to save out the dependency graph for
	*/
	void WriteMapDependencyGraph(const FName& PlatformName);

	//////////////////////////////////////////////////////////////////////////
	// cook on the fly specific functions

	/**
	 * When we get a new connection from the network make sure the version is compatible 
	 *		Will terminate the connection if return false
	 * 
	 * @return return false if not compatible, true if it is
	 */
	bool HandleNetworkFileServerNewConnection( const FString& VersionInfo, const FString& PlatformName );

	void GetCookOnTheFlyUnsolicitedFiles(const FName& PlatformName, TArray<FString>& UnsolicitedFiles, const FString& Filename);

	/**
	* Cook requests for a package from network
	*  blocks until cook is complete
	* 
	* @param  Filename	requested file to cook
	* @param  PlatformName platform to cook for
	* @param  out UnsolicitedFiles return a list of files which were cooked as a result of cooking the requested package
	*/
	void HandleNetworkFileServerFileRequest( const FString& Filename, const FString& PlatformName, TArray<FString>& UnsolicitedFiles );

	/**
	* Shader recompile request from network
	*  blocks until shader recompile complete
	*
	* @param  RecompileData input params for shader compile and compiled shader output
	*/
	void HandleNetworkFileServerRecompileShaders(const struct FShaderRecompileData& RecompileData);

	/**
	 * Get the sandbox path we want the network file server to use
	 */
	FString HandleNetworkGetSandboxPath();

	void GetCookOnTheFlyUnsolicitedFiles( const FName& PlatformName, TArray<FString>& UnsolicitedFiles );

	/**
	 * HandleNetworkGetPrecookedList 
	 * this is used specifically for cook on the fly with shared cooked builds
	 * returns the list of files which are still valid in the pak file which was initially loaded
	 * 
	 * @param PrecookedFileList all the files which are still valid in the client pak file
	 */
	void HandleNetworkGetPrecookedList( const FString& PlatformName, TMap<FString, FDateTime>& PrecookedFileList );

	//////////////////////////////////////////////////////////////////////////
	// general functions

	/**
	 * Tries to save all the UPackages in the PackagesToSave list
	 *  uses the timer to time slice, any packages not saved are requeued in the CookRequests list
	 *  internal function should not be used externally Call Tick / RequestPackage to initiate
	 * 
	 * @param PackageToSave main cooked package to save
	 * @param TargetPlatformNames list of target platforms names that we want to save this package for
	 * @param TargetPlatformsToCache list of target platforms that we want to cache uobjects for, might not be the same as the list of packages to save
	 * @param Timer FCookerTimer struct which defines the timeslicing behavior 
	 * @param FirstUnsolicitedPackage first package which was not actually requested, unsolicited packages are prioritized differently, they are saved the same way
	 * @param Result (in+out) used to modify the result of the operation and add any relevant flags
	 * @return returns true if we saved all the packages false if we bailed early for any reason
	 */
	void SaveCookedPackages(UPackage* PackageToSave, const TArray<FName>& TargetPlatformNames, const TArray<const ITargetPlatform*>& TargetPlatformsToCache, struct FCookerTimer& Timer, uint32& CookedPackageCount, uint32& Result);

	/** Perform any special processing for freshly loaded packages 
	  */
	void ProcessUnsolicitedPackages();

	/**
	 * Loads a package and prepares it for cooking
	 *  this is the same as a normal load but also ensures that the sublevels are loaded if they are streaming sublevels
	 *
	 * @param BuildFilename long package name of the package to load 
	 * @return UPackage of the package loaded, null if the file didn't exist or other failure
	 */
	UPackage* LoadPackageForCooking(const FString& BuildFilename);

	/**
	* Makes sure a package is fully loaded before we save it out
	* returns true if it succeeded
	*/
	bool MakePackageFullyLoaded(UPackage* Package) const;

	/**
	* Initialize the sandbox 
	*/
	void InitializeSandbox();

	/**
	* Initialize all target platforms
	*/
	void InitializeTargetPlatforms();

	/**
	* Clean up the sandbox
	*/
	void TermSandbox();

	/**
	* GetDependencies
	* 
	* @param Packages List of packages to use as the root set for dependency checking
	* @param Found return value, all objects which package is dependent on
	*/
	void GetDependencies( const TSet<UPackage*>& Packages, TSet<UObject*>& Found);


	/**
	* GetDependencies
	* get package dependencies according to the asset registry
	* 
	* @param Packages List of packages to use as the root set for dependency checking
	* @param Found return value, all objects which package is dependent on
	*/
	void GetDependentPackages( const TSet<UPackage*>& Packages, TSet<FName>& Found);

	/**
	* GetDependencies
	* get package dependencies according to the asset registry
	*
	* @param Root set of packages to use when looking for dependencies
	* @param FoundPackages list of packages which were found
	*/
	void GetDependentPackages(const TSet<FName>& RootPackages, TSet<FName>& FoundPackages);

	/**
	* ContainsWorld
	* use the asset registry to determine if a Package contains a UWorld or ULevel object
	* 
	* @param PackageName to return if it contains the a UWorld object or a ULevel
	* @return true if the Package contains a UWorld or ULevel false otherwise
	*/
	bool ContainsMap(const FName& PackageName) const;


	/** 
	 * Returns true if this package contains a redirector, and fills in paths
	 *
	 * @param PackageName to return if it contains a redirector
	 * @param RedirectedPaths map of original to redirected object paths
	 * @return true if the Package contains a redirector false otherwise
	 */
	bool ContainsRedirector(const FName& PackageName, TMap<FName, FName>& RedirectedPaths) const;
	
	/**
	 * Calls BeginCacheForCookedPlatformData on all UObjects in the package
	 *
	 * @param Package the package used to gather all uobjects from
	 * @param TargetPlatforms target platforms to cache for
	 * @return false if time slice was reached, true if all objects have had BeginCacheForCookedPlatformData called
	 */
	bool BeginPackageCacheForCookedPlatformData(UPackage* Package, const TArray<const ITargetPlatform*>& TargetPlatforms, struct FCookerTimer& Timer) const;
	
	/**
	 * Returns true when all objects in package have all their cooked platform data loaded
	 *	confirms that BeginCacheForCookedPlatformData is called and will return true after all objects return IsCachedCookedPlatformDataLoaded true
	 *
	 * @param Package the package used to gather all uobjects from
	 * @param TargetPlatforms target platforms to cache for
	 * @return false if time slice was reached, true if all return true for IsCachedCookedPlatformDataLoaded 
	 */
	bool FinishPackageCacheForCookedPlatformData(UPackage* Package, const TArray<const ITargetPlatform*>& TargetPlatforms, struct FCookerTimer& Timer) const;

	/**
	* GetCurrentIniVersionStrings gets the current ini version strings for compare against previous cook
	* 
	* @param IniVersionStrings return list of the important current ini version strings
	* @return false if function fails (should assume all platforms are out of date)
	*/
	bool GetCurrentIniVersionStrings( const ITargetPlatform* TargetPlatform, FIniSettingContainer& IniVersionStrings ) const;

	/**
	* GetCookedIniVersionStrings gets the ini version strings used in previous cook for specified target platform
	* 
	* @param IniVersionStrings return list of the previous cooks ini version strings
	* @return false if function fails to find the ini version strings
	*/
	bool GetCookedIniVersionStrings( const ITargetPlatform* TargetPlatform, FIniSettingContainer& IniVersionStrings, TMap<FString, FString>& AdditionalStrings ) const;


	/**
	* Convert a path to a full sandbox path
	* is effected by the cooking dlc settings
	* This function should be used instead of calling the FSandbox Sandbox->ConvertToSandboxPath functions
	*/
	FString ConvertToFullSandboxPath( const FString &FileName, bool bForWrite = false ) const;
	FString ConvertToFullSandboxPath( const FString &FileName, bool bForWrite, const FString& PlatformName ) const;

	/**
	* GetSandboxAssetRegistryFilename
	* 
	* return full path of the asset registry in the sandbox
	*/
	const FString GetSandboxAssetRegistryFilename();

	const FString GetCookedAssetRegistryFilename(const FString& PlatformName);

	/**
	* Get the sandbox root directory for that platform
	* is effected by the CookingDlc settings
	* This should be used instead of calling the Sandbox function
	*/
	FString GetSandboxDirectory( const FString& PlatformName ) const;

	bool IsCookingDLC() const;

	/**
	* GetBaseDirectoryForDLC
	* 
	* @return return the path to the DLC
	*/
	FString GetBaseDirectoryForDLC() const;

	FString GetContentDirecctoryForDLC() const;

	bool IsCreatingReleaseVersion();

	/**
	* Loads the cooked ini version settings maps into the Ini settings cache
	* 
	* @param TargetPlatforms to look for ini settings for
	*/
	//bool CacheIniVersionStringsMap( const ITargetPlatform* TargetPlatform ) const;

	/**
	* Checks if important ini settings have changed since last cook for each target platform 
	* 
	* @param TargetPlatforms to check if out of date
	* @param OutOfDateTargetPlatforms return list of out of date target platforms which should be cleaned
	*/
	bool IniSettingsOutOfDate( const ITargetPlatform* TargetPlatform ) const;

	/**
	* Saves ini settings which are in the memory cache to the hard drive in ini files
	*
	* @param TargetPlatforms to save
	*/
	bool SaveCurrentIniSettings( const ITargetPlatform* TargetPlatform ) const;



	/**
	* IsCookFlagSet
	* 
	* @param InCookFlag used to check against the current cook flags
	* @return true if the cook flag is set false otherwise
	*/
	bool IsCookFlagSet( const ECookInitializationFlags& InCookFlags ) const 
	{
		return (CookFlags & InCookFlags) != ECookInitializationFlags::None;
	}

	/** If true, the maximum file length of a package being saved will be reduced by 32 to compensate for compressed package intermediate files */
	bool ShouldConsiderCompressedPackageFileLengthRequirements() const;

	/**
	*	Cook (save) the given package
	*
	*	@param	Package				The package to cook/save
	*	@param	SaveFlags			The flags to pass to the SavePackage function
	*	@param	bOutWasUpToDate		Upon return, if true then the cooked package was cached (up to date)
	*
	*	@return	ESavePackageResult::Success if packages was cooked
	*/
	void SaveCookedPackage(UPackage* Package, uint32 SaveFlags, TArray<FSavePackageResultStruct>& SavePackageResults);
	/**
	*	Cook (save) the given package
	*
	*	@param	Package				The package to cook/save
	*	@param	SaveFlags			The flags to pass to the SavePackage function
	*	@param	bOutWasUpToDate		Upon return, if true then the cooked package was cached (up to date)
	*	@param  TargetPlatformNames Only cook for target platforms which are included in this array (if empty cook for all target platforms specified on commandline options)
	*									TargetPlatformNames is in and out value returns the platforms which the SaveCookedPackage function saved for
	*
	*	@return	ESavePackageResult::Success if packages was cooked
	*/
	void SaveCookedPackage(UPackage* Package, uint32 SaveFlags, TArray<FName> &TargetPlatformNames, TArray<FSavePackageResultStruct>& SavePackageResults);


	/**
	*  Save the global shader map
	*  
	*  @param	Platforms		List of platforms to make global shader maps for
	*/
	void SaveGlobalShaderMapFiles(const TArray<ITargetPlatform*>& Platforms);


	/** Create sandbox file in directory using current settings supplied */
	void CreateSandboxFile();
	/** Gets the output directory respecting any command line overrides */
	FString GetOutputDirectoryOverride() const;

	/** Cleans sandbox folders for all target platforms */
	void CleanSandbox( const bool bIterative );

	/**
	* Populate the cooked packages list from the on disk content using time stamps and dependencies to figure out if they are ok
	* delete any local content which is out of date
	* 
	* @param Platforms to process
	*/
	void PopulateCookedPackagesFromDisk( const TArray<ITargetPlatform*>& Platforms );

	/**
	* Searches the disk for all the cooked files in the sandbox path provided
	* Returns a map of the uncooked file path matches to the cooked file path for each package which exists
	*
	* @param UncookedpathToCookedPath out Map of the uncooked path matched with the cooked package which exists
	* @param SandboxPath path to search for cooked packages in
	*/
	void GetAllCookedFiles(TMap<FName, FName>& UncookedPathToCookedPath, const FString& SandboxPath);

	/** Generates asset registry */
	void GenerateAssetRegistry();

	/** Generates long package names for all files to be cooked */
	void GenerateLongPackageNames(TArray<FName>& FilesInPath);


	void GetDependencies( UPackage* Package, TArray<UPackage*> Dependencies );

	uint32 FullLoadAndSave(uint32& CookedPackageCount);

	uint32		StatLoadedPackageCount = 0;
	uint32		StatSavedPackageCount = 0;

	FPackageTracker*	PackageTracker;
	FPackageNameCache*	PackageNameCache;

	// temporary -- should eliminate the need for this. Only required right now because FullLoadAndSave 
	// accesses maps directly
	friend FPackageNameCache;
};
