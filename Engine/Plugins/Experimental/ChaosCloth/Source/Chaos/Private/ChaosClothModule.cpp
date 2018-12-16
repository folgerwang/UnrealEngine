// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ChaosClothModule.h"
#include "Modules/ModuleManager.h"
#include "ChaosClothPrivate.h"

//////////////////////////////////////////////////////////////////////////
// FChaosClothModule

class FChaosClothModule : public IChaosClothModuleInterface
{
  public:
    virtual void StartupModule() override
    {
        check(GConfig);
    }

    virtual void ShutdownModule() override
    {
    }
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FChaosClothModule, ChaosCloth);
DEFINE_LOG_CATEGORY(LogChaosCloth);
