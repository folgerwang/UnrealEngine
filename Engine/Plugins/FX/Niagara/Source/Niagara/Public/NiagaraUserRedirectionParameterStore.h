// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "NiagaraParameterStore.h"
#include "NiagaraUserRedirectionParameterStore.generated.h"

/**
* Extension of the base parameter store to allow the user in the editor to use variable names without 
* the "User." namespace prefix. The names without the prefix just redirect to the original variables, it is just done
* for better usability.
*/
USTRUCT()
struct FNiagaraUserRedirectionParameterStore : public FNiagaraParameterStore
{
	GENERATED_USTRUCT_BODY()
public:
	FNiagaraUserRedirectionParameterStore();
	FNiagaraUserRedirectionParameterStore(const FNiagaraParameterStore& Other);
	FNiagaraUserRedirectionParameterStore& operator=(const FNiagaraParameterStore& Other);

	virtual ~FNiagaraUserRedirectionParameterStore() = default;

	void RecreateRedirections();

	/** Get the list of FNiagaraVariables that are exposed to the user. Note that the values will be stale and are not to be trusted directly. Get the Values using the offset specified by IndexOf or GetParameterValue.*/
	FORCEINLINE void GetUserParameters(TArray<FNiagaraVariable>& OutParameters) const { return UserParameterRedirects.GenerateKeyArray(OutParameters); }

	// ~ Begin FNiagaraParameterStore overrides
	FORCEINLINE_DEBUGGABLE virtual const int32* FindParameterOffset(const FNiagaraVariable& Parameter) const override
	{
		const FNiagaraVariable* Redirection = UserParameterRedirects.Find(Parameter);
		return FNiagaraParameterStore::FindParameterOffset(Redirection ? *Redirection : Parameter);
	}
	virtual int32 IndexOf(const FNiagaraVariable& Parameter) const override;
	virtual bool AddParameter(const FNiagaraVariable& Param, bool bInitialize = true, bool bTriggerRebind = true) override;
	virtual bool RemoveParameter(const FNiagaraVariable& InVar) override;
	virtual void InitFromSource(const FNiagaraParameterStore* SrcStore, bool bNotifyAsDirty) override;
	virtual void Empty(bool bClearBindings = true) override;
	virtual void Reset(bool bClearBindings = true) override;
	// ~ End FNiagaraParameterStore overrides

	/** Used to upgrade a serialized FNiagaraParameterStore property to our own struct */
	bool SerializeFromMismatchedTag(const struct FPropertyTag& Tag, FStructuredArchive::FSlot Slot);

private:

	/** Map from the variables with shortened display names to the original variables with the full namespace */
	UPROPERTY()
	TMap<FNiagaraVariable, FNiagaraVariable> UserParameterRedirects;

	bool IsUserParameter(const FNiagaraVariable& InVar) const;

	FNiagaraVariable GetUserRedirection(const FNiagaraVariable& InVar) const;
};

template<>
struct TStructOpsTypeTraits<FNiagaraUserRedirectionParameterStore> : public TStructOpsTypeTraitsBase2<FNiagaraUserRedirectionParameterStore>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};
