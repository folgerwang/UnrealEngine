// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"


#include "NiagaraEditorDataBase.generated.h"

/** A base class for editor only data which supports post loading from the runtime owner object. */
UCLASS(MinimalAPI)
class UNiagaraEditorDataBase : public UObject
{
	GENERATED_BODY()
public:
#if WITH_EDITORONLY_DATA
	virtual void PostLoadFromOwner(UObject* InOwner) PURE_VIRTUAL(UNiagaraEditorDataBase::PostLoadFromOwner, );
#endif
};

