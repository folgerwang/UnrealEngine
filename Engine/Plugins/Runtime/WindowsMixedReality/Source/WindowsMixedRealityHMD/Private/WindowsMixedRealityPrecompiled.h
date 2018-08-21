// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IHeadMountedDisplayModule.h"
#include "HeadMountedDisplayBase.h"
#include "SceneViewExtension.h"
#include "EngineGlobals.h"
#include "Engine/GameEngine.h"
#include "GameFramework/WorldSettings.h"

#include "ScenePrivate.h"
#include "Slate/SceneViewport.h"
#include "RendererPrivate.h"
#include "PostProcess/PostProcessHMD.h"

#include <memory>
#include <mutex>
#include <concurrent_queue.h>

#include "SceneRendering.h"

#pragma warning(disable:4668)  
#include "D3D11RHIPrivate.h"
#pragma warning(default:4668)
#include "D3D11Util.h"
