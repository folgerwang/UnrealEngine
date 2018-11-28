// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraCoreModule.h"
#include "Modules/ModuleManager.h"
#include "NiagaraDataInterfaceBase.h"

IMPLEMENT_MODULE(INiagaraCoreModule, NiagaraCore);

// Temporary added for 4.21.1 fix. More permanent fix is in 4.22. Putting this variable here as it is shared between two downstream plugins.
NIAGARACORE_API TMap<FString, UClass*> FNiagaraDataInterfaceParamRefKnownClasses;


UNiagaraDataInterfaceBase::UNiagaraDataInterfaceBase(class FObjectInitializer const & Initializer)
{

}