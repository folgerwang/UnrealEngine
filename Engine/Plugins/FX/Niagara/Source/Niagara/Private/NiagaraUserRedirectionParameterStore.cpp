// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraUserRedirectionParameterStore.h"
#include "NiagaraStats.h"
#include "NiagaraEmitterInstanceBatcher.h"
#include "NiagaraDataInterface.h"
#include "NiagaraSystemInstance.h"

FNiagaraUserRedirectionParameterStore::FNiagaraUserRedirectionParameterStore() : FNiagaraParameterStore()
{

}

FNiagaraUserRedirectionParameterStore::FNiagaraUserRedirectionParameterStore(const FNiagaraParameterStore& Other)
{
	*this = Other;
}

FNiagaraUserRedirectionParameterStore& FNiagaraUserRedirectionParameterStore::operator=(const FNiagaraParameterStore& Other)
{
	Super::operator=(Other);	
	RecreateRedirections();
	return *this;
}

bool FNiagaraUserRedirectionParameterStore::IsUserParameter(const FNiagaraVariable& InVar) const
{
	return InVar.GetName().ToString().StartsWith(TEXT("User."));
}

FNiagaraVariable FNiagaraUserRedirectionParameterStore::GetUserRedirection(const FNiagaraVariable & InVar) const
{
	if (!IsUserParameter(InVar))
	{
		return InVar;
	}
	FNiagaraVariable SimpleVar = InVar;
	FName DisplayName(*InVar.GetName().ToString().RightChop(5));
	SimpleVar.SetName(DisplayName);
	return SimpleVar;
}

void FNiagaraUserRedirectionParameterStore::RecreateRedirections()
{
	UserParameterRedirects.Reset();
	for (const TPair<FNiagaraVariable, int32>& ParamOffset : GetParameterOffsets())
	{
		const FNiagaraVariable& Var = ParamOffset.Key;
		if (IsUserParameter(Var))
		{
			UserParameterRedirects.Add(GetUserRedirection(Var), Var);
		}
	}
}

int32 FNiagaraUserRedirectionParameterStore::IndexOf(const FNiagaraVariable& Parameter) const
{
	const FNiagaraVariable* Redirection = UserParameterRedirects.Find(Parameter);
	return Super::IndexOf(Redirection ? *Redirection : Parameter);
}

bool FNiagaraUserRedirectionParameterStore::AddParameter(const FNiagaraVariable& Param, bool bInitialize /*= true*/, bool bTriggerRebind /*= true*/)
{
	if (IsUserParameter(Param))
	{
		UserParameterRedirects.Add(GetUserRedirection(Param), Param);
	}
	return Super::AddParameter(Param, bInitialize, bTriggerRebind);
}

bool FNiagaraUserRedirectionParameterStore::RemoveParameter(const FNiagaraVariable& InVar)
{
	const FNiagaraVariable* Redirection = UserParameterRedirects.Find(InVar);
	const FNiagaraVariable& ToRemove = Redirection ? *Redirection : InVar;
	bool Result = Super::RemoveParameter(ToRemove);
	if (Result)
	{
		UserParameterRedirects.Remove(GetUserRedirection(ToRemove));
	}
	return Result;
}

void FNiagaraUserRedirectionParameterStore::InitFromSource(const FNiagaraParameterStore* SrcStore, bool bNotifyAsDirty)
{
	Super::InitFromSource(SrcStore, bNotifyAsDirty);
	RecreateRedirections();
}

void FNiagaraUserRedirectionParameterStore::Empty(bool bClearBindings /*= true*/)
{
	Super::Empty(bClearBindings);
	UserParameterRedirects.Empty();
}

void FNiagaraUserRedirectionParameterStore::Reset(bool bClearBindings /*= true*/)
{
	Super::Reset(bClearBindings);
	UserParameterRedirects.Reset();
}

bool FNiagaraUserRedirectionParameterStore::SerializeFromMismatchedTag(const FPropertyTag & Tag, FStructuredArchive::FSlot Slot)
{
	static FName StoreDataName("NiagaraParameterStore");
	if (Tag.Type == NAME_StructProperty && Tag.StructName == StoreDataName)
	{
		FNiagaraParameterStore OldStore;
		FNiagaraParameterStore::StaticStruct()->SerializeItem(Slot, &OldStore, nullptr);
		*this = OldStore;
		return true;
	}

	return false;
}
