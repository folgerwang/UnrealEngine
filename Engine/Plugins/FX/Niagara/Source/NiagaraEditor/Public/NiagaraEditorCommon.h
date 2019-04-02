// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNiagaraEditor, All, All);

/** Information about a Niagara operation. */
class FNiagaraOpInfo
{
public:
	FNiagaraOpInfo()
		: Keywords(FText()), NumericOuputTypeSelectionMode(ENiagaraNumericOutputTypeSelectionMode::Largest), bSupportsAddedInputs(false)
	{}

	FName Name;
	FText Category;
	FText FriendlyName;
	FText Description;
	FText Keywords;
	ENiagaraNumericOutputTypeSelectionMode NumericOuputTypeSelectionMode;
	TArray<FNiagaraOpInOutInfo> Inputs;
	TArray<FNiagaraOpInOutInfo> Outputs;

	/** If true then this operation supports a variable number of inputs */
	bool bSupportsAddedInputs;

	/** 
	* The format that can generate the hlsl for the given number of inputs.
	* Used the placeholder {A} and {B} to chain the inputs together.
	*/
	FString AddedInputFormatting;

	/**
	* If added inputs are enabled then this filters the available pin types shown to the user.
	* If empty then all the default niagara types are shown.
	*/
	TArray<FNiagaraTypeDefinition> AddedInputTypeRestrictions;

	static TMap<FName, int32> OpInfoMap;
	static TArray<FNiagaraOpInfo> OpInfos;

	static void Init();
	static const FNiagaraOpInfo* GetOpInfo(FName OpName);
	static const TArray<FNiagaraOpInfo>& GetOpInfoArray();

	void BuildName(FString InName, FString InCategory);

	bool CreateHlslForAddedInputs(int32 InputCount, FString& HlslResult) const;
};

