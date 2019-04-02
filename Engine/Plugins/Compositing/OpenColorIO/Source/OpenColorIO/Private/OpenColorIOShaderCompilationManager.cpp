// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOShaderCompilationManager.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "OpenColorIOShared.h"
#include "ShaderCompiler.h"

#if WITH_EDITOR
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#endif


DEFINE_LOG_CATEGORY_STATIC(LogOpenColorIOShaderCompiler, All, All);

static int32 GShowOpenColorIOShaderWarnings = 1;
static FAutoConsoleVariableRef CVarShowOpenColorIOShaderWarnings(
	TEXT("OpenColorIO.ShowShaderCompilerWarnings"),
	GShowOpenColorIOShaderWarnings,
	TEXT("When set to 1, will display all warnings from OpenColorIO shader compiles.")
	);



OPENCOLORIO_API FOpenColorIOShaderCompilationManager GOpenColorIOShaderCompilationManager;

void FOpenColorIOShaderCompilationManager::Tick(float DeltaSeconds)
{
#if WITH_EDITOR
	RunCompileJobs();
#endif
}

FOpenColorIOShaderCompilationManager::FOpenColorIOShaderCompilationManager()
{
	// Ew. Should we just use FShaderCompilingManager's workers instead? Is that safe?
	const int32 NumVirtualCores = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
	const uint32 NumOpenColorIOShaderCompilingThreads = FMath::Min(NumVirtualCores-1, 4);		

	for (uint32 WorkerIndex = 0; WorkerIndex < NumOpenColorIOShaderCompilingThreads; WorkerIndex++)
	{
		WorkerInfos.Add(new FOpenColorIOShaderCompileWorkerInfo());
	}
}


void FOpenColorIOShaderCompilationManager::RunCompileJobs()
{
#if WITH_EDITOR
	// If we aren't compiling through workers, so we can just track the serial time here.
//	COOK_STAT(FScopedDurationTimer CompileTimer(OpenColorIOShaderCookStats::AsyncCompileTimeSec));
	int32 NumActiveThreads = 0;

	for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
	{
		FOpenColorIOShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];

		// If this worker doesn't have any queued jobs, look for more in the input queue
		if (CurrentWorkerInfo.QueuedJobs.Num() == 0)
		{
			check(!CurrentWorkerInfo.bComplete);

			if (JobQueue.Num() > 0)
			{
				bool bAddedLowLatencyTask = false;
				int32 JobIndex = 0;

				// Try to grab up to MaxShaderJobBatchSize jobs
				// Don't put more than one low latency task into a batch
				for (; JobIndex < JobQueue.Num(); JobIndex++)
				{
					CurrentWorkerInfo.QueuedJobs.Add(JobQueue[JobIndex]);
				}

				// Update the worker state as having new tasks that need to be issued					
				// don't reset worker app ID, because the shadercompilerworkers don't shutdown immediately after finishing a single job queue.
				CurrentWorkerInfo.bIssuedTasksToWorker = true;
				CurrentWorkerInfo.bLaunchedWorker = true;
				CurrentWorkerInfo.StartTime = FPlatformTime::Seconds();
				JobQueue.RemoveAt(0, JobIndex);
			}
		}

		if (CurrentWorkerInfo.bIssuedTasksToWorker && CurrentWorkerInfo.bLaunchedWorker)
		{
			NumActiveThreads++;
		}

		if (CurrentWorkerInfo.QueuedJobs.Num() > 0)
		{
			for (int32 JobIndex = 0; JobIndex < CurrentWorkerInfo.QueuedJobs.Num(); JobIndex++)
			{
				FShaderCompileJob& CurrentJob = *((FShaderCompileJob*)(CurrentWorkerInfo.QueuedJobs[JobIndex]));

				check(!CurrentJob.bFinalized);
				CurrentJob.bFinalized = true;

				static ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();

				const FName Format = LegacyShaderPlatformToShaderFormat(EShaderPlatform(CurrentJob.Input.Target.Platform));
				const IShaderFormat* Compiler = TPM.FindShaderFormat(Format);

				if (!Compiler)
				{
					UE_LOG(LogOpenColorIOShaderCompiler, Fatal, TEXT("Can't compile shaders for format %s, couldn't load compiler dll"), *Format.ToString());
				}
				CA_ASSUME(Compiler != nullptr);

				UE_LOG(LogOpenColorIOShaderCompiler, Log, TEXT("Compile Job processing... %s"), *CurrentJob.Input.DebugGroupName);

				FString AbsoluteDebugInfoDirectory = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*(FPaths::ProjectSavedDir() / TEXT("ShaderDebugInfo")));
				FPaths::NormalizeDirectoryName(AbsoluteDebugInfoDirectory);
				CurrentJob.Input.DumpDebugInfoPath = AbsoluteDebugInfoDirectory / Format.ToString() / CurrentJob.Input.DebugGroupName;
				if (!IFileManager::Get().DirectoryExists(*CurrentJob.Input.DumpDebugInfoPath))
				{
					verifyf(IFileManager::Get().MakeDirectory(*CurrentJob.Input.DumpDebugInfoPath, true), TEXT("Failed to create directory for shader debug info '%s'"), *CurrentJob.Input.DumpDebugInfoPath);
				}

				if (IsValidRef(CurrentJob.Input.SharedEnvironment))
				{
					// Merge the shared environment into the per-shader environment before calling into the compile function
					// Normally this happens in the worker
					CurrentJob.Input.Environment.Merge(*CurrentJob.Input.SharedEnvironment);
				}

				// Compile the shader directly through the platform dll (directly from the shader dir as the working directory)
				Compiler->CompileShader(Format, CurrentJob.Input, CurrentJob.Output, FString(FPlatformProcess::ShaderDir()));

				CurrentJob.bSucceeded = CurrentJob.Output.bSucceeded;

				if (CurrentJob.Output.bSucceeded)
				{
					// Generate a hash of the output and cache it
					// The shader processing this output will use it to search for existing FShaderResources
					CurrentJob.Output.GenerateOutputHash();
					UE_LOG(LogOpenColorIOShaderCompiler, Log, TEXT("GPU shader compile succeeded. Id %d"), CurrentJob.Id);
				}
				else
				{
					UE_LOG(LogOpenColorIOShaderCompiler, Log, TEXT("ERROR: GPU shader compile failed! Id %d"), CurrentJob.Id);
				}

				CurrentWorkerInfo.bComplete = true;
			}
		}
	}

	for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
	{
		FOpenColorIOShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];
		if (CurrentWorkerInfo.bComplete)
		{
			for (int32 JobIndex = 0; JobIndex < CurrentWorkerInfo.QueuedJobs.Num(); JobIndex++)
			{
				FOpenColorIOShaderMapCompileResults& ShaderMapResults = OpenColorIOShaderMapJobs.FindChecked(CurrentWorkerInfo.QueuedJobs[JobIndex]->Id);
				ShaderMapResults.FinishedJobs.Add(CurrentWorkerInfo.QueuedJobs[JobIndex]);
				ShaderMapResults.bAllJobsSucceeded = ShaderMapResults.bAllJobsSucceeded && CurrentWorkerInfo.QueuedJobs[JobIndex]->bSucceeded;
			}
		}

		CurrentWorkerInfo.bComplete = false;
		CurrentWorkerInfo.QueuedJobs.Empty();
	}
#endif
}



OPENCOLORIO_API void FOpenColorIOShaderCompilationManager::AddJobs(TArray<FShaderCommonCompileJob*> InNewJobs)
{
#if WITH_EDITOR
	for (FShaderCommonCompileJob *Job : InNewJobs)
	{
		FOpenColorIOShaderMapCompileResults& ShaderMapInfo = OpenColorIOShaderMapJobs.FindOrAdd(Job->Id);
		//@todo : Apply shader map isn't used for now with this compile manager. Should be merged to have a generic shader compiler
		ShaderMapInfo.NumJobsQueued++;
	}

	JobQueue.Append(InNewJobs);
#endif
}


void FOpenColorIOShaderCompilationManager::ProcessAsyncResults()
{
#if WITH_EDITOR
	int32 NumCompilingOpenColorIOShaderMaps = 0;
	TArray<int32> ShaderMapsToRemove;

	// Get all OpenColorIO shader maps to finalize
	//
	for (TMap<int32, FOpenColorIOShaderMapCompileResults>::TIterator It(OpenColorIOShaderMapJobs); It; ++It)
	{
		const FOpenColorIOShaderMapCompileResults& Results = It.Value();

		if (Results.FinishedJobs.Num() == Results.NumJobsQueued)
		{
			ShaderMapsToRemove.Add(It.Key());
			PendingFinalizeOpenColorIOShaderMaps.Add(It.Key(), FOpenColorIOShaderMapFinalizeResults(Results));
		}
	}

	for (int32 RemoveIndex = 0; RemoveIndex < ShaderMapsToRemove.Num(); RemoveIndex++)
	{
		OpenColorIOShaderMapJobs.Remove(ShaderMapsToRemove[RemoveIndex]);
	}

	NumCompilingOpenColorIOShaderMaps = OpenColorIOShaderMapJobs.Num();

	if (PendingFinalizeOpenColorIOShaderMaps.Num() > 0)
	{
		ProcessCompiledOpenColorIOShaderMaps(PendingFinalizeOpenColorIOShaderMaps, 0.1f);
	}
#endif
}


void FOpenColorIOShaderCompilationManager::ProcessCompiledOpenColorIOShaderMaps(
	TMap<int32, FOpenColorIOShaderMapFinalizeResults>& CompiledShaderMaps,
	float TimeBudget)
{
#if WITH_EDITOR
	// Keeps shader maps alive as they are passed from the shader compiler and applied to the owning ColorTransform
	TArray<TRefCountPtr<FOpenColorIOShaderMap> > LocalShaderMapReferences;
	TMap<FOpenColorIOTransformResource*, FOpenColorIOShaderMap*> TransformsToUpdate;

	// Process compiled shader maps in FIFO order, in case a shader map has been enqueued multiple times,
	// Which can happen if a ColorTransform is edited while a background compile is going on
	for (TMap<int32, FOpenColorIOShaderMapFinalizeResults>::TIterator ProcessIt(CompiledShaderMaps); ProcessIt; ++ProcessIt)
	{
		TRefCountPtr<FOpenColorIOShaderMap> ShaderMap = nullptr;
		TArray<FOpenColorIOTransformResource*>* ColorTransforms = nullptr;

		for (TMap<TRefCountPtr<FOpenColorIOShaderMap>, TArray<FOpenColorIOTransformResource*> >::TIterator ShaderMapIt(FOpenColorIOShaderMap::GetInFlightShaderMaps()); ShaderMapIt; ++ShaderMapIt)
		{
			if (ShaderMapIt.Key()->GetCompilingId() == ProcessIt.Key())
			{
				ShaderMap = ShaderMapIt.Key();
				ColorTransforms = &ShaderMapIt.Value();
				break;
			}
		}

		if (ShaderMap && ColorTransforms)
		{
			TArray<FString> Errors;
			FOpenColorIOShaderMapFinalizeResults& CompileResults = ProcessIt.Value();
			const TArray<FShaderCommonCompileJob*>& ResultArray = CompileResults.FinishedJobs;

			// Make a copy of the array as this entry of FOpenColorIOShaderMap::ShaderMapsBeingCompiled will be removed below
			TArray<FOpenColorIOTransformResource*> ColorTransformArray = *ColorTransforms;
			bool bSuccess = true;

			for (int32 JobIndex = 0; JobIndex < ResultArray.Num(); JobIndex++)
			{
				FShaderCompileJob& CurrentJob = *((FShaderCompileJob*)(ResultArray[JobIndex]));
				bSuccess = bSuccess && CurrentJob.bSucceeded;

				if (bSuccess)
				{
					check(CurrentJob.Output.ShaderCode.GetShaderCodeSize() > 0);
				}

				if (GShowOpenColorIOShaderWarnings || !CurrentJob.bSucceeded)
				{
					for (int32 ErrorIndex = 0; ErrorIndex < CurrentJob.Output.Errors.Num(); ErrorIndex++)
					{
						Errors.AddUnique(CurrentJob.Output.Errors[ErrorIndex].GetErrorString());
					}

					if (CurrentJob.Output.Errors.Num())
					{
						UE_LOG(LogShaders, Log, TEXT("There were errors for job \"%s\""), *CurrentJob.Input.DebugGroupName)
							for (const FShaderCompilerError& Error : CurrentJob.Output.Errors)
							{
								UE_LOG(LogShaders, Log, TEXT("Error: %s"), *Error.GetErrorString())
							}
					}
				}
				else
				{
					UE_LOG(LogShaders, Log, TEXT("There were NO errors for job \"%s\""), *CurrentJob.Input.DebugGroupName);
				}
			}

			bool bShaderMapComplete = true;

			if (bSuccess)
			{
				bShaderMapComplete = ShaderMap->ProcessCompilationResults(ResultArray, CompileResults.FinalizeJobIndex, TimeBudget);
			}


			if (bShaderMapComplete)
			{
				ShaderMap->SetCompiledSuccessfully(bSuccess);

				// Pass off the reference of the shader map to LocalShaderMapReferences
				LocalShaderMapReferences.Add(ShaderMap);
				FOpenColorIOShaderMap::GetInFlightShaderMaps().Remove(ShaderMap);

				for (FOpenColorIOTransformResource* ColorTransform : ColorTransformArray)
				{
					FOpenColorIOShaderMap* CompletedShaderMap = ShaderMap;

					ColorTransform->RemoveOutstandingCompileId(ShaderMap->GetCompilingId());

					// Only process results that still match the ID which requested a compile
					// This avoids applying shadermaps which are out of date and a newer one is in the async compiling pipeline
					if (ColorTransform->IsSame(CompletedShaderMap->GetShaderMapId()))
					{
						if (Errors.Num() != 0)
						{
							FString SourceCode;
							ColorTransform->GetColorTransformHLSLSource(SourceCode);
							UE_LOG(LogOpenColorIOShaderCompiler, Log, TEXT("Compile output as text:"));
							UE_LOG(LogOpenColorIOShaderCompiler, Log, TEXT("==================================================================================="));
							TArray<FString> OutputByLines;
							SourceCode.ParseIntoArrayLines(OutputByLines, false);
							for (int32 i = 0; i < OutputByLines.Num(); i++)
							{
								UE_LOG(LogOpenColorIOShaderCompiler, Log, TEXT("/*%04d*/\t\t%s"), i + 1, *OutputByLines[i]);
							}
							UE_LOG(LogOpenColorIOShaderCompiler, Log, TEXT("==================================================================================="));
						}

						if (!bSuccess)
						{
							// Propagate error messages
							ColorTransform->SetCompileErrors(Errors);
							TransformsToUpdate.Add(ColorTransform, nullptr);

							for (int32 ErrorIndex = 0; ErrorIndex < Errors.Num(); ErrorIndex++)
							{
								FString ErrorMessage = Errors[ErrorIndex];
								// Work around build machine string matching heuristics that will cause a cook to fail
								ErrorMessage.ReplaceInline(TEXT("error "), TEXT("err0r "), ESearchCase::CaseSensitive);
								UE_LOG(LogOpenColorIOShaderCompiler, Warning, TEXT("	%s"), *ErrorMessage);
							}
						}
						else
						{
							// if we succeeded and our shader map is not complete this could be because the color transform was being edited quicker then the compile could be completed
							// Don't modify color transforms for which the compiled shader map is no longer complete
							// This shouldn't happen since transforms are pretty much baked in the designated config file.
							if (CompletedShaderMap->IsComplete(ColorTransform, true))
							{
								TransformsToUpdate.Add(ColorTransform, CompletedShaderMap);
							}

							if (GShowOpenColorIOShaderWarnings && Errors.Num() > 0)
							{
								UE_LOG(LogOpenColorIOShaderCompiler, Warning, TEXT("Warnings while compiling OpenColorIO ColorTransform %s for platform %s:"),
									*ColorTransform->GetFriendlyName(),
									*LegacyShaderPlatformToShaderFormat(ShaderMap->GetShaderPlatform()).ToString());
								for (int32 ErrorIndex = 0; ErrorIndex < Errors.Num(); ErrorIndex++)
								{
									UE_LOG(LogOpenColorIOShaderCompiler, Warning, TEXT("	%s"), *Errors[ErrorIndex]);
								}
							}
						}
					}
					else
					{
						if (CompletedShaderMap->IsComplete(ColorTransform, true))
						{
							ColorTransform->NotifyCompilationFinished();
						}
					}
				}

				// Cleanup shader jobs and compile tracking structures
				for (int32 JobIndex = 0; JobIndex < ResultArray.Num(); JobIndex++)
				{
					delete ResultArray[JobIndex];
				}

				CompiledShaderMaps.Remove(ShaderMap->GetCompilingId());
			}

			if (TimeBudget < 0)
			{
				break;
			}
		}
	}

	if (TransformsToUpdate.Num() > 0)
	{
		for (TMap<FOpenColorIOTransformResource*, FOpenColorIOShaderMap*>::TConstIterator It(TransformsToUpdate); It; ++It)
		{
			FOpenColorIOTransformResource* ColorTransform = It.Key();
			FOpenColorIOShaderMap* ShaderMap = It.Value();

			ColorTransform->SetGameThreadShaderMap(It.Value());

			ENQUEUE_RENDER_COMMAND(FSetShaderMapOnColorTransformResources)(
				[ColorTransform, ShaderMap](FRHICommandListImmediate& RHICmdList)
				{
					ColorTransform->SetRenderingThreadShaderMap(ShaderMap);
				});


			ColorTransform->NotifyCompilationFinished();
		}
	}
#endif
}


void FOpenColorIOShaderCompilationManager::FinishCompilation(const TCHAR* InTransformName, const TArray<int32>& ShaderMapIdsToFinishCompiling)
{
#if WITH_EDITOR
	check(!FPlatformProperties::RequiresCookedData());

	RunCompileJobs();	// since we don't async compile through another process, this will run all oustanding jobs
	ProcessAsyncResults();	// grab compiled shader maps and assign them to their resources

	check(OpenColorIOShaderMapJobs.Num() == 0);
#endif
}

