// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalViewport.cpp: Metal viewport RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#if PLATFORM_MAC
#include "Mac/CocoaWindow.h"
#include "Mac/CocoaThread.h"
#else
#include "IOS/IOSAppDelegate.h"
#endif
#include "RenderCommandFence.h"
#include "Containers/Set.h"
#include "MetalProfiler.h"
#include "RenderUtils.h"

extern int32 GMetalSupportsIntermediateBackBuffer;
extern int32 GMetalSeparatePresentThread;
extern int32 GMetalNonBlockingPresent;
extern float GMetalPresentFramePacing;

#if PLATFORM_IOS
int32 GEnablePresentPacing = 0;
static FAutoConsoleVariableRef CVarMetalEnablePresentPacing(
	   TEXT("ios.PresentPacing"),
	   GEnablePresentPacing,
	   TEXT(""),
		ECVF_Default);
#endif

#if PLATFORM_MAC

// Quick way to disable availability warnings is to duplicate the definitions into a new type - gotta love ObjC dynamic-dispatch!
@interface FCAMetalLayer : CALayer
@property BOOL displaySyncEnabled;
@property BOOL allowsNextDrawableTimeout;
@end

@implementation FMetalView

- (id)initWithFrame:(NSRect)frameRect
{
	self = [super initWithFrame:frameRect];
	if (self)
	{
	}
	return self;
}

- (BOOL)isOpaque
{
	return YES;
}

- (BOOL)mouseDownCanMoveWindow
{
	return YES;
}

@end

#endif

static FCriticalSection ViewportsMutex;
static TSet<FMetalViewport*> Viewports;

FMetalViewport::FMetalViewport(void* WindowHandle, uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen,EPixelFormat Format)
: DisplayID(0)
, Block(nil)
, bIsFullScreen(bInIsFullscreen)
#if PLATFORM_MAC
, CustomPresent(nullptr)
#endif
{
#if PLATFORM_MAC
	MainThreadCall(^{
		FCocoaWindow* Window = (FCocoaWindow*)WindowHandle;
		const NSRect ContentRect = NSMakeRect(0, 0, InSizeX, InSizeY);
		View = [[FMetalView alloc] initWithFrame:ContentRect];
		[View setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
		[View setWantsLayer:YES];

		CAMetalLayer* Layer = [CAMetalLayer new];

		CGFloat bgColor[] = { 0.0, 0.0, 0.0, 0.0 };
		Layer.edgeAntialiasingMask = 0;
		Layer.masksToBounds = YES;
		Layer.backgroundColor = CGColorCreate(CGColorSpaceCreateDeviceRGB(), bgColor);
		Layer.presentsWithTransaction = NO;
		Layer.anchorPoint = CGPointMake(0.5, 0.5);
		Layer.frame = ContentRect;
		Layer.magnificationFilter = kCAFilterNearest;
		Layer.minificationFilter = kCAFilterNearest;
#if WITH_EDITOR
		if(GIsEditor)
		{
			// Default is YES - Mac Editor only - wait for drawable to be available - nextDrawable can still return nil on error or low memory
			Layer.allowsNextDrawableTimeout = NO;
		}
#endif

		[Layer setDevice:GetMetalDeviceContext().GetDevice()];
		
		[Layer setFramebufferOnly:NO];
		[Layer removeAllAnimations];

		[View setLayer:Layer];

		[Window setContentView:View];
		[[Window standardWindowButton:NSWindowCloseButton] setAction:@selector(performClose:)];
	}, NSDefaultRunLoopMode, true);
#endif
	Resize(InSizeX, InSizeY, bInIsFullscreen, Format);
	
	{
		FScopeLock Lock(&ViewportsMutex);
		Viewports.Add(this);
	}
}

FMetalViewport::~FMetalViewport()
{
	if (Block)
	{
		FScopeLock BlockLock(&Mutex);
		if (GMetalSeparatePresentThread)
		{
			FPlatformRHIFramePacer::RemoveHandler(Block);
		}
		Block_release(Block);
		Block = nil;
	}
	{
		FScopeLock Lock(&ViewportsMutex);
		Viewports.Remove(this);
	}
	
	BackBuffer[0].SafeRelease();	// when the rest of the engine releases it, its framebuffers will be released too (those the engine knows about)
	BackBuffer[1].SafeRelease();
	check(!IsValidRef(BackBuffer[0]));
	check(!IsValidRef(BackBuffer[1]));
}

uint32 FMetalViewport::GetViewportIndex(EMetalViewportAccessFlag Accessor) const
{
	switch(Accessor)
	{
		case EMetalViewportAccessRHI:
			check(IsInRHIThread() || IsInRenderingThread());
			// Deliberate fall-through
		case EMetalViewportAccessDisplayLink: // Displaylink is not an index, merely an alias that avoids the check...
			return (GRHISupportsRHIThread && IsRunningRHIInSeparateThread()) ? EMetalViewportAccessRHI : EMetalViewportAccessRenderer;
		case EMetalViewportAccessRenderer:
			check(IsInRenderingThread());
			return Accessor;
		case EMetalViewportAccessGame:
			check(IsInGameThread());
			return EMetalViewportAccessRenderer;
		default:
			check(false);
			return EMetalViewportAccessRenderer;
	}
}

void FMetalViewport::Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen,EPixelFormat Format)
{
	bIsFullScreen = bInIsFullscreen;
	uint32 Index = GetViewportIndex(EMetalViewportAccessGame);
	
	bool bUseHDR = GRHISupportsHDROutput && Format == GRHIHDRDisplayOutputFormat;
	
	// Format can come in as PF_Unknown in the LDR case or if this RHI doesn't support HDR.
	// So we'll fall back to BGRA8 in those cases.
	if (!bUseHDR)
	{
		Format = PF_B8G8R8A8;
	}
	
	mtlpp::PixelFormat MetalFormat = (mtlpp::PixelFormat)GPixelFormats[Format].PlatformFormat;
	
	if (IsValidRef(BackBuffer[Index]) && Format != BackBuffer[Index]->GetFormat())
	{
		// Really need to flush the RHI thread & GPU here...
		AddRef();
		ENQUEUE_RENDER_COMMAND(FlushPendingRHICommands)(
			[Viewport = this](FRHICommandListImmediate& RHICmdList)
			{
				GRHICommandList.GetImmediateCommandList().BlockUntilGPUIdle();
				Viewport->ReleaseDrawable();
				Viewport->Release();
			});
		
		// Issue a fence command to the rendering thread and wait for it to complete.
		FRenderCommandFence Fence;
		Fence.BeginFence();	
		Fence.Wait();
	}
    
#if PLATFORM_MAC
	MainThreadCall(^
	{
		CAMetalLayer* MetalLayer = (CAMetalLayer*)View.layer;
		
		MetalLayer.drawableSize = CGSizeMake(InSizeX, InSizeY);
		
		if (MetalFormat != (mtlpp::PixelFormat)MetalLayer.pixelFormat)
		{
			MetalLayer.pixelFormat = (MTLPixelFormat)MetalFormat;
		}
		
		if (bUseHDR != MetalLayer.wantsExtendedDynamicRangeContent)
		{
			MetalLayer.wantsExtendedDynamicRangeContent = bUseHDR;
		}
		
	}, NSDefaultRunLoopMode, true);
#else
	// A note on HDR in iOS
	// Setting the pixel format to one of the Apple XR formats is all you need.
	// iOS expects the app to output in sRGB regadless of the display
	// (even though Apple's HDR displays are P3)
	// and its compositor will do the conversion.
	{
		IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
		FIOSView* IOSView = AppDelegate.IOSView;
		CAMetalLayer* MetalLayer = (CAMetalLayer*) IOSView.layer;
		
		if (MetalFormat != (mtlpp::PixelFormat) MetalLayer.pixelFormat)
		{
			MetalLayer.pixelFormat = (MTLPixelFormat) MetalFormat;
		}
		
		[IOSView UpdateRenderWidth:InSizeX andHeight:InSizeY];

		// check the size of the window
		float ScalingFactor = [IOSView contentScaleFactor];
		CGRect ViewFrame = [IOSView frame];
		check(FMath::TruncToInt(ScalingFactor * ViewFrame.size.width) == InSizeX &&
			  FMath::TruncToInt(ScalingFactor * ViewFrame.size.height) == InSizeY);
	}
#endif

    {
        FScopeLock Lock(&Mutex);
        FRHIResourceCreateInfo CreateInfo;
        FTexture2DRHIRef NewBackBuffer;
        FTexture2DRHIRef DoubleBuffer;
        if (GMetalSupportsIntermediateBackBuffer)
        {
            NewBackBuffer = (FMetalTexture2D*)(FTexture2DRHIParamRef)GDynamicRHI->RHICreateTexture2D(InSizeX, InSizeY, Format, 1, 1, TexCreate_RenderTargetable, CreateInfo);
            
            if (GMetalSeparatePresentThread)
            {
                DoubleBuffer = GDynamicRHI->RHICreateTexture2D(InSizeX, InSizeY, Format, 1, 1, TexCreate_RenderTargetable, CreateInfo);
                ((FMetalTexture2D*)DoubleBuffer.GetReference())->Surface.Viewport = this;
            }
        }
        else
        {
            NewBackBuffer = (FMetalTexture2D*)(FTexture2DRHIParamRef)GDynamicRHI->RHICreateTexture2D(InSizeX, InSizeY, Format, 1, 1, TexCreate_RenderTargetable | TexCreate_Presentable, CreateInfo);
        }
        ((FMetalTexture2D*)NewBackBuffer.GetReference())->Surface.Viewport = this;
        
        BackBuffer[Index] = (FMetalTexture2D*)NewBackBuffer.GetReference();
        if (GMetalSeparatePresentThread)
        {
            BackBuffer[EMetalViewportAccessRHI] = (FMetalTexture2D*)DoubleBuffer.GetReference();
        }
        else
        {
            BackBuffer[EMetalViewportAccessRHI] = BackBuffer[Index];
        }
    }
}

TRefCountPtr<FMetalTexture2D> FMetalViewport::GetBackBuffer(EMetalViewportAccessFlag Accessor) const
{
	FScopeLock Lock(&Mutex);
	
	uint32 Index = GetViewportIndex(Accessor);
	check(IsValidRef(BackBuffer[Index]));
	return BackBuffer[Index];
}

#if PLATFORM_MAC
@protocol CAMetalLayerSPI <NSObject>
- (BOOL)isDrawableAvailable;
@end
#endif

mtlpp::Drawable FMetalViewport::GetDrawable(EMetalViewportAccessFlag Accessor)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalMakeDrawableTime);
    if (!Drawable
#if !PLATFORM_MAC
        || (((id<CAMetalDrawable>)Drawable.GetPtr()).texture.width != BackBuffer[GetViewportIndex(Accessor)]->GetSizeX() || ((id<CAMetalDrawable>)Drawable.GetPtr()).texture.height != BackBuffer[GetViewportIndex(Accessor)]->GetSizeY())
#endif
        )
	{
		@autoreleasepool
		{
			uint32 IdleStart = FPlatformTime::Cycles();
			
	#if PLATFORM_MAC
			CAMetalLayer* CurrentLayer = (CAMetalLayer*)[View layer];
			if (GMetalNonBlockingPresent == 0 || [((id<CAMetalLayerSPI>)CurrentLayer) isDrawableAvailable])
			{
				Drawable = CurrentLayer ? [CurrentLayer nextDrawable] : nil;
				
		#if WITH_EDITOR
				// Try to confirm UE-72410 out of memory speculation - Mac Editor mode only
				if(!Drawable && GIsEditor && CurrentLayer)
				{
					// Drawable can be nil when:
					// 1) The layer has an invalid combination of drawable properties.
  					// 2) All drawables in the swap queue are in-use and the 1 second timeout has elapsed. (except when `allowsNextDrawableTimeout' is set to NO)
  					// 3) Process is out of memory.
  					UE_LOG(LogMetal, Display, TEXT("FMetalViewport: Missing Drawable: nil returned from [CAMetalLayer nextDrawable] (CAMetalLayer.allowsNextDrawableTimeout=%d)"), CurrentLayer.allowsNextDrawableTimeout ? 1 : 0);
					
					// Blocking wait on next drawable is set - something has gone wrong - report current memory
					if(CurrentLayer.allowsNextDrawableTimeout == NO)
					{
						// System
						{
							FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
							uint64 SysTotalPhysical = MemoryStats.TotalPhysical / 1024 / 1024;
							uint64 SysAvailPhysical = MemoryStats.AvailablePhysical / 1024 / 1024;
							uint64 SysUsedPhysical = MemoryStats.UsedPhysical / 1024 / 1024;
							
							UE_LOG(LogMetal, Display, TEXT("System Memory Stats: TotalPhysical = %lluMB, AvailPhysical = %lluMB, UsedPhysical = %lluMB"), SysTotalPhysical, SysAvailPhysical, SysUsedPhysical);
						}
						
						// GPU
						TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUDescriptors = FMacPlatformMisc::GetGPUDescriptors();
						for(uint32 i = 0;i < GPUDescriptors.Num();++i)
						{
							FMacPlatformMisc::FGPUDescriptor const& Descriptor = GPUDescriptors[i];
							TMap<FString, float> const& GPUStats = Descriptor.GetPerformanceStatistics();
							
							uint32 GPUTotalMemory = Descriptor.GPUMemoryMB;
							int64 vramFree = ((int64)GPUStats[TEXT("vramFreeBytes")]) / 1024 / 1024;
							int64 inUseVidMemory = ((int64)GPUStats[TEXT("inUseVidMemoryBytes")]) / 1024 / 1024;
							
							UE_LOG(LogMetal, Display, TEXT("GPU Memory Stats [Device=%d]: DeviceTotalMemory = %dMB, FreeVRAM = %lldMB, InUseVRAM = %lldMB"), i, GPUTotalMemory , vramFree, inUseVidMemory);
						}
					}
				}
		#endif
			}
			else
			{
				Drawable = nil;
			}
			
#if METAL_DEBUG_OPTIONS
			CGSize Size = ((id<CAMetalDrawable>)Drawable).layer.drawableSize;
			if ((Size.width != BackBuffer[GetViewportIndex(Accessor)]->GetSizeX() || Size.height != BackBuffer[GetViewportIndex(Accessor)]->GetSizeY()))
			{
				UE_LOG(LogMetal, Display, TEXT("Viewport Size Mismatch: Drawable W:%f H:%f, Viewport W:%u H:%u"), Size.width, Size.height, BackBuffer[GetViewportIndex(Accessor)]->GetSizeX(), BackBuffer[GetViewportIndex(Accessor)]->GetSizeY());
			}
#endif

	#else
			CGSize Size;
			IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
			do
			{
				Drawable = [AppDelegate.IOSView MakeDrawable];
				Size.width = ((id<CAMetalDrawable>)Drawable).texture.width;
				Size.height = ((id<CAMetalDrawable>)Drawable).texture.height;
			}
			while (Size.width != BackBuffer[GetViewportIndex(Accessor)]->GetSizeX() || Size.height != BackBuffer[GetViewportIndex(Accessor)]->GetSizeY());
			
	#endif
			
			if (IsInRHIThread())
			{
				GWorkingRHIThreadStallTime += FPlatformTime::Cycles() - IdleStart;
			}
			else
			{
				GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += FPlatformTime::Cycles() - IdleStart;
				GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;
			}
		}
	}
	return Drawable;
}

FMetalTexture FMetalViewport::GetDrawableTexture(EMetalViewportAccessFlag Accessor)
{
	id<CAMetalDrawable> CurrentDrawable = (id<CAMetalDrawable>)GetDrawable(Accessor);
#if METAL_DEBUG_OPTIONS
	@autoreleasepool
	{
#if PLATFORM_MAC
		CAMetalLayer* CurrentLayer = (CAMetalLayer*)[View layer];
#else
		CAMetalLayer* CurrentLayer = (CAMetalLayer*)[[IOSAppDelegate GetDelegate].IOSView layer];
#endif
		
		uint32 Index = GetViewportIndex(Accessor);
		CGSize Size = CurrentLayer.drawableSize;
		if (CurrentDrawable.texture.width != BackBuffer[Index]->GetSizeX() || CurrentDrawable.texture.height != BackBuffer[Index]->GetSizeY())
		{
			UE_LOG(LogMetal, Display, TEXT("Viewport Size Mismatch: Drawable W:%f H:%f, Texture W:%llu H:%llu, Viewport W:%u H:%u"), Size.width, Size.height, CurrentDrawable.texture.width, CurrentDrawable.texture.height, BackBuffer[Index]->GetSizeX(), BackBuffer[Index]->GetSizeY());
		}
	}
#endif
	DrawableTextures[Accessor] = CurrentDrawable.texture;
	return CurrentDrawable.texture;
}

ns::AutoReleased<FMetalTexture> FMetalViewport::GetCurrentTexture(EMetalViewportAccessFlag Accessor)
{
	return DrawableTextures[Accessor];
}

void FMetalViewport::ReleaseDrawable()
{
	if (!GMetalSeparatePresentThread)
	{
		if (Drawable)
		{
			Drawable = nil;
		}
		if (!GMetalSupportsIntermediateBackBuffer && IsValidRef(BackBuffer[GetViewportIndex(EMetalViewportAccessRHI)]))
		{
			BackBuffer[GetViewportIndex(EMetalViewportAccessRHI)]->Surface.Texture = nil;
		}
	}
}

#if PLATFORM_MAC
NSWindow* FMetalViewport::GetWindow() const
{
	return [View window];
}
#endif

void FMetalViewport::Present(FMetalCommandQueue& CommandQueue, bool bLockToVsync)
{
	FScopeLock Lock(&Mutex);
	
	bool bIsLiveResize = false;
#if PLATFORM_MAC
	NSNumber* ScreenId = [View.window.screen.deviceDescription objectForKey:@"NSScreenNumber"];
	DisplayID = ScreenId.unsignedIntValue;
	bIsLiveResize = View.inLiveResize;
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesSupportsVSyncToggle))
	{
		FCAMetalLayer* CurrentLayer = (FCAMetalLayer*)[View layer];
		static bool sVSyncSafe = FPlatformMisc::MacOSXVersionCompare(10,13,4) >= 0;
		CurrentLayer.displaySyncEnabled = bLockToVsync || (!sVSyncSafe && !(IsRunningGame() && bIsFullScreen));
	}
#endif
	
	LastCompleteFrame = GetBackBuffer(EMetalViewportAccessRHI);
	FPlatformAtomics::InterlockedExchange(&FrameAvailable, 1);
	
	if (!Block)
	{
#if !PLATFORM_MAC
		uint32 FramePace = FPlatformRHIFramePacer::GetFramePace();
		float MinPresentDuration = FramePace ? (1.0f / (float)FramePace) : 0.0f;
#endif
		Block = Block_copy(^(uint32 InDisplayID, double OutputSeconds, double OutputDuration)
		{
			bool bIsInLiveResize = false;
#if PLATFORM_MAC
			bIsInLiveResize = View.inLiveResize;
#endif
			if (FrameAvailable > 0 && (InDisplayID == 0 || (DisplayID == InDisplayID && !bIsInLiveResize)))
			{
				FPlatformAtomics::InterlockedDecrement(&FrameAvailable);
				id<CAMetalDrawable> LocalDrawable = (id<CAMetalDrawable>)[GetDrawable(EMetalViewportAccessDisplayLink) retain];
				{
					FScopeLock BlockLock(&Mutex);
#if PLATFORM_MAC
					bIsInLiveResize = View.inLiveResize;
#endif
					
					if (LocalDrawable && LocalDrawable.texture && (InDisplayID == 0 || !bIsInLiveResize))
					{
						mtlpp::CommandBuffer CurrentCommandBuffer = CommandQueue.CreateCommandBuffer();
						check(CurrentCommandBuffer);
						
#if ENABLE_METAL_GPUPROFILE
						FMetalProfiler* Profiler = FMetalProfiler::GetProfiler();
						FMetalCommandBufferStats* Stats = Profiler->AllocateCommandBuffer(CurrentCommandBuffer, 0);
#endif
						
						if (GMetalSupportsIntermediateBackBuffer)
						{
							TRefCountPtr<FMetalTexture2D> Texture = LastCompleteFrame;
							check(IsValidRef(Texture));
							
							FMetalTexture Src = Texture->Surface.Texture;
							FMetalTexture Dst = LocalDrawable.texture;
							
							NSUInteger Width = FMath::Min(Src.GetWidth(), Dst.GetWidth());
							NSUInteger Height = FMath::Min(Src.GetHeight(), Dst.GetHeight());
							
							mtlpp::BlitCommandEncoder Encoder = CurrentCommandBuffer.BlitCommandEncoder();
							check(Encoder.GetPtr());
#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS
							FMetalBlitCommandEncoderDebugging Debugging;
							if (SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation)
							{
								FMetalCommandBufferDebugging CmdDebug = FMetalCommandBufferDebugging::Get(CurrentCommandBuffer);
								Debugging = FMetalBlitCommandEncoderDebugging(Encoder, CmdDebug);
							}
#endif
							
							METAL_STATISTIC(Profiler->BeginEncoder(Stats, Encoder));
							METAL_GPUPROFILE(Profiler->EncodeBlit(Stats, __FUNCTION__));

							Encoder.Copy(Src, 0, 0, mtlpp::Origin(0, 0, 0), mtlpp::Size(Width, Height, 1), Dst, 0, 0, mtlpp::Origin(0, 0, 0));
							METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, Debugging.Copy(Src, 0, 0, mtlpp::Origin(0, 0, 0), mtlpp::Size(Width, Height, 1), Dst, 0, 0, mtlpp::Origin(0, 0, 0)));
							
							METAL_STATISTIC(Profiler->EndEncoder(Stats, Encoder));
							Encoder.EndEncoding();
							METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, Debugging.EndEncoder());
							
							mtlpp::CommandBufferHandler H = [Src, Dst](const mtlpp::CommandBuffer &) {
							};
							
							CurrentCommandBuffer.AddCompletedHandler(H);
							
							Drawable = nil;
						}
						
						// This is a bit different than the usual pattern.
						// This command buffer here is committed directly, instead of going through
						// FMetalCommandList::Commit. So long as Present() is called within
						// high level RHI BeginFrame/EndFrame this will not fine.
						// Otherwise the recording of the Present time will be offset by one in the
						// FMetalGPUProfiler frame indices.
      
#if PLATFORM_MAC
						FMetalView* theView = View;
						mtlpp::CommandBufferHandler C = [LocalDrawable, theView](const mtlpp::CommandBuffer & cmd_buf) {
#else
						mtlpp::CommandBufferHandler C = [LocalDrawable](const mtlpp::CommandBuffer & cmd_buf) {
#endif
							FMetalGPUProfiler::RecordPresent(cmd_buf);
							[LocalDrawable release];
#if PLATFORM_MAC
							MainThreadCall(^{
								FCocoaWindow* Window = (FCocoaWindow*)[theView window];
								[Window startRendering];
							}, NSDefaultRunLoopMode, false);
#endif
						};
						
#if WITH_EDITOR			// The Editor needs the older way to present otherwise we end up with bad behaviour of the completion handlers that causes GPU timeouts.
						if (GIsEditor)
						{
#if !PLATFORM_IOS
							mtlpp::CommandBufferHandler H = [LocalDrawable](mtlpp::CommandBuffer const&)
							{
#else
							mtlpp::CommandBufferHandler H = [LocalDrawable, MinPresentDuration, FramePace](mtlpp::CommandBuffer const&)
							{
								if (MinPresentDuration && GEnablePresentPacing)
								{
									[LocalDrawable presentAfterMinimumDuration:1.0f/(float)FramePace];
								}
								else
#endif
								{
									[LocalDrawable present];
								};
							};
								
							CurrentCommandBuffer.AddCompletedHandler(C);
							CurrentCommandBuffer.AddScheduledHandler(H);
						}
						else
#endif
						{
							CurrentCommandBuffer.AddCompletedHandler(C);
#if PLATFORM_IOS
							if (MinPresentDuration && GEnablePresentPacing)
							{
								CurrentCommandBuffer.PresentAfterMinimumDuration(LocalDrawable, 1.0f/(float)FramePace);
							}
							else
#endif
							{
								CurrentCommandBuffer.Present(LocalDrawable);
							}
						}
						
						
						METAL_GPUPROFILE(Stats->End(CurrentCommandBuffer));
						CommandQueue.CommitCommandBuffer(CurrentCommandBuffer);
					}
				}
			}
		});
		
		if (GMetalSeparatePresentThread)
		{
			FPlatformRHIFramePacer::AddHandler(Block);
		}
	}
	
	if (bIsLiveResize || !GMetalSeparatePresentThread)
	{
		Block(0, 0.0, 0.0);
	}
	
	if (!(GRHISupportsRHIThread && IsRunningRHIInSeparateThread()))
	{
		Swap();
	}
}

void FMetalViewport::Swap()
{
	if (GMetalSeparatePresentThread)
	{
		FScopeLock Lock(&Mutex);
		
		check(IsValidRef(BackBuffer[0]));
		check(IsValidRef(BackBuffer[1]));
		
		TRefCountPtr<FMetalTexture2D> BB0 = BackBuffer[0];
		TRefCountPtr<FMetalTexture2D> BB1 = BackBuffer[1];
		
		BackBuffer[0] = BB1;
		BackBuffer[1] = BB0;
	}
}

/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FMetalDynamicRHI::RHICreateViewport(void* WindowHandle,uint32 SizeX,uint32 SizeY,bool bIsFullscreen,EPixelFormat PreferredPixelFormat)
{
	check( IsInGameThread() );
	@autoreleasepool {
	return new FMetalViewport(WindowHandle, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
	}
}

void FMetalDynamicRHI::RHIResizeViewport(FViewportRHIParamRef Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen)
{
	RHIResizeViewport(Viewport, SizeX, SizeY, bIsFullscreen, PF_Unknown);
}

void FMetalDynamicRHI::RHIResizeViewport(FViewportRHIParamRef ViewportRHI,uint32 SizeX,uint32 SizeY,bool bIsFullscreen,EPixelFormat Format)
{
	@autoreleasepool {
	check( IsInGameThread() );

	FMetalViewport* Viewport = ResourceCast(ViewportRHI);
	Viewport->Resize(SizeX, SizeY, bIsFullscreen, Format);
	}
}

void FMetalDynamicRHI::RHITick( float DeltaTime )
{
	check( IsInGameThread() );
}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

void FMetalRHICommandContext::RHIBeginDrawingViewport(FViewportRHIParamRef ViewportRHI, FTextureRHIParamRef RenderTargetRHI)
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIBeginDrawingViewport(FViewportRHIParamRef ViewportRHI, FTextureRHIParamRef RenderTargetRHI)
{
	@autoreleasepool {
	FMetalViewport* Viewport = ResourceCast(ViewportRHI);
	check(Viewport);

	((FMetalDeviceContext*)Context)->BeginDrawingViewport(Viewport);

	// Set the render target and viewport.
	if (RenderTargetRHI)
	{
		FRHIRenderTargetView RTV(RenderTargetRHI, GIsEditor ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad);
		RHISetRenderTargets(1, &RTV, nullptr, 0, NULL);
	}
	else
	{
		FRHIRenderTargetView RTV(Viewport->GetBackBuffer(EMetalViewportAccessRHI), GIsEditor ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad);
		RHISetRenderTargets(1, &RTV, nullptr, 0, NULL);
	}
	}
}

void FMetalRHICommandContext::RHIEndDrawingViewport(FViewportRHIParamRef ViewportRHI,bool bPresent,bool bLockToVsync)
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIEndDrawingViewport(FViewportRHIParamRef ViewportRHI,bool bPresent,bool bLockToVsync)
{
	@autoreleasepool {
	FMetalViewport* Viewport = ResourceCast(ViewportRHI);
	((FMetalDeviceContext*)Context)->EndDrawingViewport(Viewport, bPresent, bLockToVsync);
	}
}

FTexture2DRHIRef FMetalDynamicRHI::RHIGetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
	@autoreleasepool {
	FMetalViewport* Viewport = ResourceCast(ViewportRHI);
	return FTexture2DRHIRef(Viewport->GetBackBuffer(EMetalViewportAccessRenderer).GetReference());
	}
}

void FMetalDynamicRHI::RHIAdvanceFrameForGetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
	if (GMetalSeparatePresentThread && (GRHISupportsRHIThread && IsRunningRHIInSeparateThread()))
	{
		FScopeLock Lock(&ViewportsMutex);
		for (FMetalViewport* Viewport : Viewports)
		{
			Viewport->Swap();
		}
	}
}
