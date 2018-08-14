// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IOSPlatformWebBrowser.h"

#if PLATFORM_IOS
#include "IOS/IOSView.h"
#include "IOS/IOSAppDelegate.h"
#include "Widgets/SLeafWidget.h"

#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>
#include <OpenGLES/ES2/glext.h>
#include "ExternalTexture.h"

class SIOSWebBrowserWidget : public SLeafWidget
{
	SLATE_BEGIN_ARGS(SIOSWebBrowserWidget)
		: _InitialURL("about:blank")
		, _UseTransparency(false)
	{ }

	SLATE_ARGUMENT(FString, InitialURL);
	SLATE_ARGUMENT(bool, UseTransparency);
	SLATE_ARGUMENT(TSharedPtr<FWebBrowserWindow>, WebBrowserWindow);

	SLATE_END_ARGS()

		SIOSWebBrowserWidget()
		: WebViewWrapper(nil)
	{}

	void Construct(const FArguments& Args)
	{
		bool bSupportsMetalMRT = false;
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bSupportsMetalMRT, GEngineIni);

		bool bSupportsMetal = false;
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bSupportsMetal, GEngineIni);

		WebViewWrapper = [IOSWebViewWrapper alloc];
		[WebViewWrapper create : TSharedPtr<SIOSWebBrowserWidget>(this) useTransparency : Args._UseTransparency supportsMetal : bSupportsMetal supportsMetalMRT : bSupportsMetalMRT];

		WebBrowserWindowPtr = Args._WebBrowserWindow;
		IsIOS3DBrowser = false;

#if !PLATFORM_TVOS
		TextureSamplePool = new FWebBrowserTextureSamplePool();
		WebBrowserTextureSamplesQueue = MakeShared<FWebBrowserTextureSampleQueue, ESPMode::ThreadSafe>();
		WebBrowserTexture = nullptr;
		WebBrowserMaterial = nullptr;
		WebBrowserBrush = nullptr;

		// create external texture
		WebBrowserTexture = NewObject<UWebBrowserTexture>((UObject*)GetTransientPackage(), NAME_None, RF_Transient | RF_Public);

		if (WebBrowserTexture != nullptr)
		{
			WebBrowserTexture->UpdateResource();
			WebBrowserTexture->AddToRoot();
		}

		// create wrapper material
		UMaterial* Material = LoadObject<UMaterial>(nullptr, TEXT("/WebBrowserWidget/WebTexture_M"), nullptr, LOAD_None, nullptr);
		if (Material)
		{
			// create wrapper material
			WebBrowserMaterial = UMaterialInstanceDynamic::Create(Material, nullptr);

			if (WebBrowserMaterial)
			{
				WebBrowserMaterial->SetTextureParameterValue("SlateUI", WebBrowserTexture);
				WebBrowserMaterial->AddToRoot();

				// create Slate brush
				WebBrowserBrush = MakeShareable(new FSlateBrush());
				{
					WebBrowserBrush->SetResourceObject(WebBrowserMaterial);
				}
			}
		}
#endif
		LoadURL(Args._InitialURL);
	}


	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		if (WebViewWrapper != nil)
		{
			if (WebBrowserWindowPtr.IsValid() && WebBrowserWindowPtr.Pin()->GetParentWindow().IsValid())
			{
				bool ShouldSet3DBrowser = WebBrowserWindowPtr.Pin()->GetParentWindow().Get()->IsVirtualWindow();
				if (IsIOS3DBrowser != ShouldSet3DBrowser)
				{
					IsIOS3DBrowser = ShouldSet3DBrowser;
					[WebViewWrapper set3D : IsIOS3DBrowser];
				}
			}

			UIView* View = [IOSAppDelegate GetDelegate].IOSView;
			CGFloat contentScaleFactor = View.contentScaleFactor;
			FVector2D Position = AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation() / contentScaleFactor;
			FVector2D Size = TransformVector(AllottedGeometry.GetAccumulatedRenderTransform(), AllottedGeometry.GetLocalSize()) / contentScaleFactor;
			CGRect NewFrame;
			NewFrame.origin.x = FMath::RoundToInt(Position.X);
			NewFrame.origin.y = FMath::RoundToInt(Position.Y);
			NewFrame.size.width = FMath::RoundToInt(Size.X);
			NewFrame.size.height = FMath::RoundToInt(Size.Y);

			[WebViewWrapper updateframe : NewFrame];

#if !PLATFORM_TVOS
			if (IsIOS3DBrowser)
			{
				if (WebBrowserTexture)
				{
					TSharedPtr<FWebBrowserTextureSample, ESPMode::ThreadSafe> WebBrowserTextureSample;
					WebBrowserTextureSamplesQueue->Peek(WebBrowserTextureSample);

					WebBrowserTexture->TickResource(WebBrowserTextureSample);
				}

				if (WebBrowserTexture != nullptr)
				{
					struct FWriteWebBrowserParams
					{
						IOSWebViewWrapper* NativeWebBrowserPtr;
						FGuid PlayerGuid;
						FIntPoint Size;
					};

					FIntPoint viewportSize = WebBrowserWindowPtr.Pin()->GetViewportSize();

					FWriteWebBrowserParams WriteWebBrowserParams = { WebViewWrapper, WebBrowserTexture->GetExternalTextureGuid(), viewportSize };

					ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(WriteWebBrowser, FWriteWebBrowserParams, Params, WriteWebBrowserParams,
					{
						IOSWebViewWrapper* NativeWebBrowser = Params.NativeWebBrowserPtr;

					if (NativeWebBrowser == nil)
					{
						return;
					}

					FTextureRHIRef VideoTexture = [NativeWebBrowser GetVideoTexture];
					if (VideoTexture == nullptr)
					{
						FRHIResourceCreateInfo CreateInfo;
						FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
						FIntPoint Size = Params.Size;
						VideoTexture = RHICmdList.CreateTextureExternal2D(Size.X, Size.Y, PF_R8G8B8A8, 1, 1, 0, CreateInfo);
						[NativeWebBrowser SetVideoTexture : VideoTexture];
						//UE_LOG(LogIOS, Log, TEXT("NativeWebBrowser SetVideoTexture:VideoTexture!"));

						if (VideoTexture == nullptr)
						{
							UE_LOG(LogIOS, Warning, TEXT("CreateTextureExternal2D failed!"));
							return;
						}

						[NativeWebBrowser SetVideoTextureValid : false];

					}

					if ([NativeWebBrowser UpdateVideoFrame : VideoTexture->GetNativeResource()])
					{
						// if region changed, need to reregister UV scale/offset
						//UE_LOG(LogIOS, Log, TEXT("UpdateVideoFrame RT: %s"), *Params.PlayerGuid.ToString());
					}

					if (![NativeWebBrowser IsVideoTextureValid])
					{
						FSamplerStateInitializerRHI SamplerStateInitializer(SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp);
						FSamplerStateRHIRef SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
						FExternalTextureRegistry::Get().RegisterExternalTexture(Params.PlayerGuid, VideoTexture, SamplerStateRHI);
						//UE_LOG(LogIOS, Log, TEXT("Fetch RT: Register Guid: %s"), *Params.PlayerGuid.ToString());

						[NativeWebBrowser SetVideoTextureValid : true];
					}
					});
				}
			}
#endif
		}

	}

	int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
	{
#if !PLATFORM_TVOS
		if (IsIOS3DBrowser && WebBrowserBrush.IsValid())
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), WebBrowserBrush.Get(), ESlateDrawEffect::None);
		}
#endif
		return LayerId;
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(640, 480);
	}

	void LoadURL(const FString& InNewURL)
	{
		if (WebViewWrapper != nil)
		{
			[WebViewWrapper loadurl : [NSURL URLWithString : [NSString stringWithUTF8String : TCHAR_TO_UTF8(*InNewURL)]]];
		}
	}

	void LoadString(const FString& InContents, const FString& InDummyURL)
	{
		if (WebViewWrapper != nil)
		{
			[WebViewWrapper loadstring : [NSString stringWithUTF8String : TCHAR_TO_UTF8(*InContents)] dummyurl : [NSURL URLWithString : [NSString stringWithUTF8String : TCHAR_TO_UTF8(*InDummyURL)]]];
		}
	}

	bool OnBeforeBrowse(const FString& Url, const FWebNavigationRequest& RequestDetails)
	{
		bool Retval = false;
		if (WebBrowserWindowPtr.IsValid())
		{
			TSharedPtr<FWebBrowserWindow> BrowserWindow = WebBrowserWindowPtr.Pin();
			if (BrowserWindow.IsValid())
			{
				if (BrowserWindow->OnBeforeBrowse().IsBound())
				{
					Retval = BrowserWindow->OnBeforeBrowse().Execute(Url, RequestDetails);
				}
			}
		}
		return Retval;
	}

	void ExecuteJavascript(const FString& Script)
	{
		if (WebViewWrapper != nil)
		{
			[WebViewWrapper executejavascript : [NSString stringWithUTF8String : TCHAR_TO_UTF8(*Script)]];
		}
	}

	void Close()
	{
		if (WebViewWrapper != nil)
		{
			[WebViewWrapper close];
			WebViewWrapper = nil;
		}
		WebBrowserWindowPtr.Reset();
	}

	~SIOSWebBrowserWidget()
	{
		Close();
	}

protected:
	mutable __strong IOSWebViewWrapper* WebViewWrapper;
private:
	TWeakPtr<FWebBrowserWindow> WebBrowserWindowPtr;

	/** Enable 3D appearance */
	bool IsIOS3DBrowser;

#if !PLATFORM_TVOS
	/** The external texture to render the webbrowser output. */
	UWebBrowserTexture* WebBrowserTexture;

	/** The material for the external texture. */
	UMaterialInstanceDynamic* WebBrowserMaterial;

	/** The Slate brush that renders the material. */
	TSharedPtr<FSlateBrush> WebBrowserBrush;

	/** The sample queue. */
	TSharedPtr<FWebBrowserTextureSampleQueue, ESPMode::ThreadSafe> WebBrowserTextureSamplesQueue;

	/** Texture sample object pool. */
	FWebBrowserTextureSamplePool* TextureSamplePool;
#endif
};

@implementation IOSWebViewWrapper

#if !PLATFORM_TVOS
@synthesize WebView;
@synthesize WebViewContainer;
#endif
@synthesize NextURL;
@synthesize NextContent;

-(void)create:(TSharedPtr<SIOSWebBrowserWidget>)InWebBrowserWidget useTransparency : (bool)InUseTransparency
supportsMetal : (bool)InSupportsMetal supportsMetalMRT : (bool)InSupportsMetalMRT;
{
	WebBrowserWidget = InWebBrowserWidget;
	NextURL = nil;
	NextContent = nil;
	VideoTexture = nil;
	bNeedsAddToView = true;
	IsIOS3DBrowser = false;
	bVideoTextureValid = false;
	bSupportsMetal = InSupportsMetal;
	bSupportsMetalMRT = InSupportsMetalMRT;

#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		WebViewContainer = [[UIView alloc]initWithFrame:CGRectMake(1, 1, 100, 100)];
		[self.WebViewContainer setOpaque : NO];
		[self.WebViewContainer setBackgroundColor : [UIColor clearColor]];


		WebView = [[WKWebView alloc]initWithFrame:CGRectMake(1, 1, 100, 100)];
		[self.WebViewContainer addSubview : WebView];
		WebView.navigationDelegate = self;
		WebView.UIDelegate = self;

		WebView.scrollView.bounces = NO;

		if (InUseTransparency)
		{
			[self.WebView setOpaque : NO];
			[self.WebView setBackgroundColor : [UIColor clearColor]];
		}
		else
		{
			[self.WebView setOpaque : YES];
		}

		[self setWebViewVisible];
	});
#endif
}

-(void)close;
{
#if !PLATFORM_TVOS
	WebView.navigationDelegate = nil;
	dispatch_async(dispatch_get_main_queue(), ^
	{
		[self.WebViewContainer removeFromSuperview];
		[self.WebView removeFromSuperview];
		WebView = nil;
		WebViewContainer = nil;
	});
#endif
}

-(void)updateframe:(CGRect)InFrame;
{
	self.DesiredFrame = InFrame;

#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (WebView != nil)
		{
			WebViewContainer.frame = self.DesiredFrame;
			WebView.frame = WebViewContainer.bounds;
			if (bNeedsAddToView)
			{
				bNeedsAddToView = false;
				[[IOSAppDelegate GetDelegate].IOSView addSubview : WebViewContainer];
			}
			else
			{
				if (NextContent != nil)
				{
					// Load web content from string
					[self.WebView loadHTMLString : NextContent baseURL : NextURL];
					NextContent = nil;
					NextURL = nil;
				}
				else
					if (NextURL != nil)
					{
						// Load web content from URL
						NSURLRequest *nsrequest = [NSURLRequest requestWithURL : NextURL];
						[self.WebView loadRequest : nsrequest];
						NextURL = nil;
					}
			}
		}
	});
#endif
}

-(void)executejavascript:(NSString*)InJavaScript
{
#if !PLATFORM_TVOS
	dispatch_async(dispatch_get_main_queue(), ^
	{
		[self.WebView evaluateJavaScript : InJavaScript completionHandler : nil];
	});
#endif
}

-(void)loadurl:(NSURL*)InURL;
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		self.NextURL = InURL;
	});
}

-(void)loadstring:(NSString*)InString dummyurl : (NSURL*)InURL;
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		self.NextContent = InString;
	self.NextURL = InURL;
	});
}

-(void)set3D:(bool)InIsIOS3DBrowser;
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		if (IsIOS3DBrowser != InIsIOS3DBrowser)
		{
			//default is 2D
			IsIOS3DBrowser = InIsIOS3DBrowser;
			[self setWebViewVisible];
		}
	});
}

-(void)setWebViewVisible;
{
#if !PLATFORM_TVOS
	if (IsIOS3DBrowser)
	{
		[self.WebViewContainer setHidden : YES];
	}
	else
	{
		[self.WebViewContainer setHidden : NO];
	}
#endif
}

-(FTextureRHIRef)GetVideoTexture;
{
	return VideoTexture;
}

-(void)SetVideoTexture:(FTextureRHIRef)Texture;
{
	VideoTexture = Texture;
}

-(void)SetVideoTextureValid:(bool)Condition;
{
	bVideoTextureValid = Condition;
}

-(bool)IsVideoTextureValid;
{
	return bVideoTextureValid;
}

-(bool)UpdateVideoFrame:(void*)ptr;
{
#if !PLATFORM_TVOS
	@synchronized(self) // Briefly block render thread
		{
		if (bSupportsMetal)
		{
			id<MTLTexture> ptrToMetalTexture = (id<MTLTexture>)ptr;
			NSUInteger width = [ptrToMetalTexture width];
			NSUInteger height = [ptrToMetalTexture height];

			[self updateWebViewMetalTexture : ptrToMetalTexture];
		}
		else
		{
			GLuint glTexture = (GLuint)*reinterpret_cast<int32*>(ptr);
			glBindTexture(GL_TEXTURE_2D, glTexture);
			[self updateWebViewGLESTexture : glTexture];
		}
		}
#endif
	return true;
}

-(void)updateWebViewGLESTexture:(GLuint)gltexture
{
#if !PLATFORM_TVOS
	// create a suitable CoreGraphics context
	CGColorSpaceRef colourSpace = CGColorSpaceCreateDeviceRGB();
	CGContextRef context =
		CGBitmapContextCreate(&gltexture, WebView.bounds.size.width, WebView.bounds.size.height, 8, 4 * WebView.bounds.size.width, colourSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
	CGColorSpaceRelease(colourSpace);
	// draw the view to the buffer
	[WebView.layer renderInContext : context];
	// upload to OpenGL
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WebView.bounds.size.width, WebView.bounds.size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &gltexture);
	// clean up
	CGContextRelease(context);
#endif
}

-(void)updateWebViewMetalTexture:(id<MTLTexture>)texture
{
#if !PLATFORM_TVOS
	@autoreleasepool {
		UIGraphicsBeginImageContextWithOptions(WebView.frame.size, NO, 1.0f);
		[WebView drawViewHierarchyInRect : WebView.bounds afterScreenUpdates : NO];
		UIImage *image = UIGraphicsGetImageFromCurrentImageContext();
		UIGraphicsEndImageContext();
		NSUInteger width = [texture width];
		NSUInteger height = [texture height];
		CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
		CGContextRef context = CGBitmapContextCreate(NULL, width, height, 8, 4 * width, colorSpace, (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
		CGContextDrawImage(context, CGRectMake(0, 0, width, height), image.CGImage);
		[texture replaceRegion : MTLRegionMake2D(0, 0, width, height)
			mipmapLevel : 0
			withBytes : CGBitmapContextGetData(context)
			bytesPerRow : 4 * width];
		CGColorSpaceRelease(colorSpace);
		CGContextRelease(context);
		image = nil;
	}
#endif
}

#if !PLATFORM_TVOS
- (BOOL)webView:(UIWebView*)InWebView
shouldStartLoadWithRequest : (NSURLRequest*)InRequest
	navigationType : (UIWebViewNavigationType)InNavigationType;
{
	FString UrlStr([[InRequest URL]absoluteString]);

	// Notify on the game thread
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		FWebNavigationRequest RequestDetails;
		//@todo sz - not sure how to detect redirects here .. InNavigationType == UIWebViewNavigationTypeOther?
		RequestDetails.bIsRedirect = true;
		RequestDetails.bIsMainFrame = true;
		WebBrowserWidget->OnBeforeBrowse(UrlStr, RequestDetails);
		return true;
	}];

	return YES;
}

-(void)webView:(UIWebView*)InWebView didFailLoadWithError : (NSError*)InError;
{

}
#endif
@end

FWebBrowserWindow::FWebBrowserWindow(FString InUrl, TOptional<FString> InContentsToLoad, bool InShowErrorMessage, bool InThumbMouseButtonNavigation, bool InUseTransparency)
	: CurrentUrl(MoveTemp(InUrl))
	, ContentsToLoad(MoveTemp(InContentsToLoad))
	, bUseTransparency(InUseTransparency)
	, IOSWindowSize(FIntPoint(500, 500))
{
}

FWebBrowserWindow::~FWebBrowserWindow()
{
	CloseBrowser(true);
}

void FWebBrowserWindow::LoadURL(FString NewURL)
{
	BrowserWidget->LoadURL(NewURL);
}

void FWebBrowserWindow::LoadString(FString Contents, FString DummyURL)
{
	BrowserWidget->LoadString(Contents, DummyURL);
}

TSharedRef<SWidget> FWebBrowserWindow::CreateWidget()
{
	TSharedRef<SIOSWebBrowserWidget> BrowserWidgetRef =
		SNew(SIOSWebBrowserWidget)
		.UseTransparency(bUseTransparency)
		.InitialURL(CurrentUrl)
		.WebBrowserWindow(SharedThis(this));

	BrowserWidget = BrowserWidgetRef;
	return BrowserWidgetRef;
}

void FWebBrowserWindow::SetViewportSize(FIntPoint WindowSize, FIntPoint WindowPos)
{
	IOSWindowSize = WindowSize;
}

FIntPoint FWebBrowserWindow::GetViewportSize() const
{
	return IOSWindowSize;
}

FSlateShaderResource* FWebBrowserWindow::GetTexture(bool bIsPopup /*= false*/)
{
	return nullptr;
}

bool FWebBrowserWindow::IsValid() const
{
	return false;
}

bool FWebBrowserWindow::IsInitialized() const
{
	return true;
}

bool FWebBrowserWindow::IsClosing() const
{
	return false;
}

EWebBrowserDocumentState FWebBrowserWindow::GetDocumentLoadingState() const
{
	return EWebBrowserDocumentState::Loading;
}

FString FWebBrowserWindow::GetTitle() const
{
	return "";
}

FString FWebBrowserWindow::GetUrl() const
{
	return "";
}

bool FWebBrowserWindow::OnKeyDown(const FKeyEvent& InKeyEvent)
{
	return false;
}

bool FWebBrowserWindow::OnKeyUp(const FKeyEvent& InKeyEvent)
{
	return false;
}

bool FWebBrowserWindow::OnKeyChar(const FCharacterEvent& InCharacterEvent)
{
	return false;
}

FReply FWebBrowserWindow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

void FWebBrowserWindow::OnMouseLeave(const FPointerEvent& MouseEvent)
{
}

void FWebBrowserWindow::SetSupportsMouseWheel(bool bValue)
{

}

bool FWebBrowserWindow::GetSupportsMouseWheel() const
{
	return false;
}

FReply FWebBrowserWindow::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

void FWebBrowserWindow::OnFocus(bool SetFocus, bool bIsPopup)
{
}

void FWebBrowserWindow::OnCaptureLost()
{
}

bool FWebBrowserWindow::CanGoBack() const
{
	return false;
}

void FWebBrowserWindow::GoBack()
{
}

bool FWebBrowserWindow::CanGoForward() const
{
	return false;
}

void FWebBrowserWindow::GoForward()
{
}

bool FWebBrowserWindow::IsLoading() const
{
	return false;
}

void FWebBrowserWindow::Reload()
{
}

void FWebBrowserWindow::StopLoad()
{
}

void FWebBrowserWindow::GetSource(TFunction<void(const FString&)> Callback) const
{
	Callback(FString());
}

int FWebBrowserWindow::GetLoadError()
{
	return 0;
}

void FWebBrowserWindow::SetIsDisabled(bool bValue)
{
}


void FWebBrowserWindow::ExecuteJavascript(const FString& Script)
{
	BrowserWidget->ExecuteJavascript(Script);
}

void FWebBrowserWindow::CloseBrowser(bool bForce)
{
	BrowserWidget->Close();
}

void FWebBrowserWindow::BindUObject(const FString& Name, UObject* Object, bool bIsPermanent /*= true*/)
{
}

void FWebBrowserWindow::UnbindUObject(const FString& Name, UObject* Object /*= nullptr*/, bool bIsPermanent /*= true*/)
{
}

#endif
