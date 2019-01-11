// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComposurePipelineBaseActor.h"
#include "EditorSupport/CompImageColorPickerInterface.h"
#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h" // for ETextureRenderTargetFormat
#include "Camera/CameraActor.h"
#include "CompositingElements/CompositingTextureLookupTable.h"
#include "EditorSupport/CompFreezeFrameController.h"
#include "CompositingElements/CompositingMaterialPass.h"
#include "CompositingElement.generated.h"

class UComposureCompositingTargetComponent;
class UComposurePostProcessingPassProxy;
class UMaterialInstanceDynamic;
class ACameraActor;
class UTexture;
class UTextureRenderTarget2D;
class FCompElementRenderTargetPool;
class UCompositingElementPass;
class UCompositingElementInput;
class UCompositingElementTransform;
class UCompositingElementOutput;
class UAlphaTransformPass;
struct FInheritedTargetPool;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDynamicOnTransformPassRendered, class ACompositingElement*, CompElement, UTexture*, Texture, FName, PassName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDynamicOnFinalPassRendered, class ACompositingElement*, CompElement, UTexture*, Texture);

UENUM()
enum class ESceneCameraLinkType
{
	Inherited,
	Override,
	Unused, // EDITOR-ONLY value, used to clean up the UI and remove needless params from the details UI on elements that don't need a camera
};

UENUM()
enum class EInheritedSourceType
{
	Inherited,
	Override,
};

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ETargetUsageFlags : uint8
{
	USAGE_None       = 0x00,
	USAGE_Input      = 1<<0,
	USAGE_Transform  = 1<<1,
	USAGE_Output     = 1<<2,
	USAGE_Persistent = 1<<5,

	// If a pass is tagged 'intermediate' it is still available to the pass immediately 
	// after it. So we ping-pong between intermediate tags, clearing the older one.
	USAGE_Intermediate0 = 1<<3 UMETA(Hidden),
	USAGE_Intermediate1 = 1<<4 UMETA(Hidden),
};
ENUM_CLASS_FLAGS(ETargetUsageFlags);

UENUM()
enum class ECompPassConstructionType
{
	Unknown,
	EditorConstructed,
	BlueprintConstructed,
	CodeConstructed,
};

/**
 * 
 */
UCLASS(BlueprintType, meta=(DisplayName="Empty Comp Shot", ShortTooltip="A simple base actor used to composite multiple render layers together."))
class COMPOSURE_API ACompositingElement : public AComposurePipelineBaseActor, public ICompImageColorPickerInterface
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Composure")
	UComposureCompositingTargetComponent* CompositingTarget;

	UPROPERTY(BlueprintReadOnly, Category = "Composure")
	UComposurePostProcessingPassProxy* PostProcessProxy;

protected:
	/*********************************/
	// Pipeline Passes 
	//   - protected to prevent users from directly modifying these lists (use the accessor functions instead)

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, BlueprintGetter = GetInputsList, Category = "Composure|Input", meta=(ShowOnlyInnerProperties))
	TArray<UCompositingElementInput*> Inputs;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, BlueprintGetter = GetTransformsList, Category = "Composure|Transform/Compositing Passes", meta=(DisplayName = "Transform Passes", ShowOnlyInnerProperties, DisplayAfter=Inputs))
	TArray<UCompositingElementTransform*> TransformPasses;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, BlueprintGetter = GetOutputsList, Category = "Composure|Output", meta = (ShowOnlyInnerProperties))
	TArray<UCompositingElementOutput*> Outputs;

public:

	/*********************************/
	// Inputs

	UPROPERTY(EditAnywhere, Category = "Composure|Input")
	ESceneCameraLinkType CameraSource = ESceneCameraLinkType::Unused;

	UPROPERTY(EditInstanceOnly, Category = "Composure|Input")
	TLazyObjectPtr<ACameraActor> TargetCameraActor;

	/*********************************/
	// Outputs

	UPROPERTY(EditAnywhere, Category = "Composure|Output")
	EInheritedSourceType ResolutionSource = EInheritedSourceType::Inherited;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintGetter=GetRenderResolution, Category="Composure|Output")
 	FIntPoint RenderResolution;
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Composure|Output", AdvancedDisplay)
	TEnumAsByte<ETextureRenderTargetFormat> RenderFormat;

	UPROPERTY(EditDefaultsOnly, Category = "Composure|Output", AdvancedDisplay)
	bool bUseSharedTargetPool;

	/** Called when this comp shot element has rendered one of its internal transform passes */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnTransformPassRendered, ACompositingElement*, UTexture*, FName);
	FOnTransformPassRendered OnTransformPassRendered;

	/** Called when this comp shot element has rendered its final output */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFinalPassRendered, ACompositingElement*, UTexture*);
	FOnFinalPassRendered OnFinalPassRendered;

	/*********************************/
	// Editor Only

private:
	UPROPERTY(/*BlueprintReadWrite, Category = "Composure",*/ meta = (Bitmask, BitmaskEnum = ETargetUsageFlags))
	int32 FreezeFrameMask = 0x00;

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Composure|Preview")
	EInheritedSourceType PreviewTransformSource = EInheritedSourceType::Inherited;

	UPROPERTY(EditAnywhere, Instanced, Category = "Composure|Preview")
	UCompositingElementTransform* PreviewTransform;

	UPROPERTY(EditDefaultsOnly, Category = "Composure|Editor")
	TSubclassOf<UCompositingElementInput> DefaultInputType;

	UPROPERTY(EditDefaultsOnly, Category = "Composure|Editor")
	TSubclassOf<UCompositingElementTransform> DefaultTransformType;

	UPROPERTY(EditDefaultsOnly, Category = "Composure|Editor")
	TSubclassOf<UCompositingElementOutput> DefaultOutputType;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCompElementConstructed, ACompositingElement*);
	FOnCompElementConstructed OnConstructed;

	FCompFreezeFrameController FreezeFrameController;
#endif // WITH_EDITORONLY_DATA

public:
	void SetCompIdName(const FName NewName);

	bool AttachAsChildLayer(ACompositingElement* Child);
	bool DetatchAsChildLayer(ACompositingElement* Child);

	bool IsSubElement() const;
	ACompositingElement* GetElementParent() const;

	const TArray<ACompositingElement*> GetChildElements() const;

	template<class T>
	T* AddNewPass(FName PassName, ECompPassConstructionType ConstructedBy = ECompPassConstructionType::CodeConstructed)
	{
		return Cast<T>(AddNewPass(PassName, T::StaticClass(), ConstructedBy));
	}
	UCompositingElementPass* AddNewPass(FName PassName, TSubclassOf<UCompositingElementPass> PassType, ECompPassConstructionType ConstructedBy = ECompPassConstructionType::CodeConstructed);
	
	bool RemovePass(UCompositingElementPass* ElementPass);
	int32 RemovePassesOfType(TSubclassOf<UCompositingElementPass> PassType);

	float GetOpacity() const { return OutputOpacity; }
	void SetOpacity(const float NewOpacity);

public:
	//~ ICompImageColorPickerInterface API
	//
	// NOTE: as we cannot make BlueprintCallable functions EditorOnly, we've flagged these as 
	//       "DevelopmentOnly", and made them non-functional outside of the editor
	//

	/* EDITOR ONLY - Specifies which intermediate target to pick colors from (if left unset, we default to the display image) */
	UFUNCTION(BlueprintCallable, Category = "Composure|Editor", meta = (DevelopmentOnly))
	void SetEditorColorPickingTarget(UTextureRenderTarget2D* PickingTarget);

	/* EDITOR ONLY - Specifies an intermediate image to display when picking (if left unset, we default to the final output image) */
	UFUNCTION(BlueprintCallable, Category = "Composure|Editor", meta = (DevelopmentOnly))
	void SetEditorColorPickerDisplayImage(UTexture* PickerDisplayImage);

#if WITH_EDITOR
	virtual void OnBeginPreview() override;
	virtual UTexture* GetEditorPreviewImage() override;
	virtual void OnEndPreview() override;
	virtual bool UseImplicitGammaForPreview() const override;
	virtual UTexture* GetColorPickerDisplayImage() override;
	virtual UTextureRenderTarget2D* GetColorPickerTarget() override;
	virtual FCompFreezeFrameController* GetFreezeFrameController() override;
#endif

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Composure", meta = (CallInEditor = "true"))
	UTexture* RenderCompElement(bool bCameraCutThisFrame);

public:
	//~ Blueprint API

	/** Called when a transform pass on this element is rendered */
	UPROPERTY(BlueprintAssignable, BlueprintCallable, DisplayName=OnTransformPassRendered, Category="Composure")
	FDynamicOnTransformPassRendered OnTransformPassRendered_BP;

	/** Called when the final output of this element is rendered */
	UPROPERTY(BlueprintAssignable, BlueprintCallable, DisplayName=OnFinalPassRendered, Category="Composure")
	FDynamicOnFinalPassRendered OnFinalPassRendered_BP;

	UFUNCTION(BlueprintPure, Category = "Composure")
	FName GetCompElementName() const { return CompShotIdName; }

	UFUNCTION(BlueprintCallable, Category = "Composure")
	UTextureRenderTarget2D* RequestNamedRenderTarget(const FName ReferenceName, const float RenderPercentage = 1.f, ETargetUsageFlags UsageTag = ETargetUsageFlags::USAGE_None);

	UFUNCTION(BlueprintCallable, Category = "Composure")
	bool ReleaseOwnedTarget(UTextureRenderTarget2D* OwnedTarget);

	UFUNCTION(BlueprintCallable, Category = "Composure")
	UTexture* RenderCompositingMaterial(UPARAM(ref) FCompositingMaterial& CompMaterial, float RenderScale = 1.f, FName ResultLookupName = NAME_None, ETargetUsageFlags UsageTag = ETargetUsageFlags::USAGE_None);

	UFUNCTION(BlueprintCallable, Category = "Composure")
	UTextureRenderTarget2D* RenderCompositingMaterialToTarget(UPARAM(ref) FCompositingMaterial& CompMaterial, UTextureRenderTarget2D* RenderTarget, FName ResultLookupName = NAME_None);

	UFUNCTION(BlueprintPure, Category = "Composure|Input")
	ACameraActor* FindTargetCamera() const;

	UFUNCTION(BlueprintCallable, Category = "Composure")
	void RegisterPassResult(FName ReferenceName, UTexture* PassResult, bool bSetAsLatestRenderResult = true);
	UFUNCTION(BlueprintCallable, Category = "Composure")
	UTexture* FindNamedRenderResult(FName PassName, bool bSearchSubElements = true); //const;
	UFUNCTION(BlueprintCallable, Category = "Composure")
	UTexture* GetLatestRenderResult() const;

	UFUNCTION(BlueprintGetter)
	FIntPoint GetRenderResolution() const;

	/*********************************/
	// Pass Management 

	UFUNCTION(BlueprintCallable, Category = "Composure", meta = (DeterminesOutputType = "InputType"))
	UCompositingElementInput* FindInputPass(TSubclassOf<UCompositingElementInput> InputType, UTexture*& PassResult, FName OptionalPassName = NAME_None);
	UFUNCTION(BlueprintCallable, Category = "Composure", meta = (DeterminesOutputType = "TransformType"))
	UCompositingElementTransform* FindTransformPass(TSubclassOf<UCompositingElementTransform> TransformType, UTexture*& PassResult, FName OptionalPassName = NAME_None);
	UFUNCTION(BlueprintCallable, Category = "Composure", meta = (DeterminesOutputType = "OutputType"))
	UCompositingElementOutput* FindOutputPass(TSubclassOf<UCompositingElementOutput> OutputType, FName OptionalPassName = NAME_None);
	
	UFUNCTION(BlueprintGetter)
	TArray<UCompositingElementInput*> GetInputsList() const { return GetInternalInputsList(); }
	UFUNCTION(BlueprintGetter)
	TArray<UCompositingElementTransform*> GetTransformsList() const { return GetInternalTransformsList(); }
	UFUNCTION(BlueprintGetter)
	TArray<UCompositingElementOutput*> GetOutputsList() const { return GetInternalOutputsList(); }

protected:
	UFUNCTION(BlueprintCallable, Category = "Composure|Input", meta = (DeterminesOutputType = "InputType", BlueprintProtected))
	UCompositingElementInput* AddNewInputPass(FName PassName, TSubclassOf<UCompositingElementInput> InputType);
	UFUNCTION(BlueprintCallable, Category = "Composure|Input", meta = (DeterminesOutputType = "TransformType", BlueprintProtected))
	UCompositingElementTransform* AddNewTransformPass(FName PassName, TSubclassOf<UCompositingElementTransform> TransformType);
	UFUNCTION(BlueprintCallable, Category = "Composure|Input", meta = (DeterminesOutputType = "OutputType", BlueprintProtected))
	UCompositingElementOutput* AddNewOutputPass(FName PassName, TSubclassOf<UCompositingElementOutput> OutputType);

public: 
	//~ Begin UObject interface
	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#endif
	//~ End UObject interface
	
	//~ Begin AAcotr interface
	virtual void RerunConstructionScripts() override;
#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
#endif
	//~ End AAcotr interface

	//~ Begin AComposurePipelineBaseActor interface
	virtual void SetAutoRun(bool bNewAutoRunVal) override;
	virtual void EnqueueRendering_Implementation(bool bCameraCutThisFrame) override;
	virtual bool IsActivelyRunning_Implementation() const;
	virtual int32 GetRenderPriority() const override;
	//~ End AComposurePipelineBaseActor interface

private:
	void FrameReset();

	void PostSerializeCompatUpgrade(const int32 ComposureVersion);
	void PostLoadCompatUpgrade(const int32 ComposureVersion);

#if WITH_EDITOR
	UCompositingElementTransform* GetPreviewPass() const;
	bool IsPreviewing() const;

	void OnPIEStarted(bool bIsSimulating);
	void SetDebugDisplayImage(UTexture* DebugDisplayImg);
#endif
	void OnDisabled();

	void RefreshAllInternalPassLists();
	void RefreshInternalInputsList();
	void RefreshInternalTransformsList();
	void RefreshInternalOutputsList();

	const TArray<UCompositingElementInput*>& GetInternalInputsList() const;
	const TArray<UCompositingElementTransform*>& GetInternalTransformsList() const;
	const TArray<UCompositingElementOutput*>& GetInternalOutputsList() const;

	void BeginFrameForAllPasses(bool bCameraCutThisFrame);
	void GenerateInputs();
	void ApplyTransforms(FInheritedTargetPool& RenderTargetPool);
	void RelayOutputs(const FInheritedTargetPool& RenderTargetPool);
	void EndFrameForAllPasses();

	void UpdateFinalRenderResult(UTexture* RenderResult);

	typedef TSharedPtr<FCompElementRenderTargetPool> FSharedTargetPoolPtr;
	const FSharedTargetPoolPtr& GetRenderTargetPool();

	void RegisterTaggedPassResult(FName ReferenceName, UTexture* PassResult, ETargetUsageFlags UsageFlags = ETargetUsageFlags::USAGE_None);
	void ResetResultsLookupTable(bool bKeepPassResults = false);

	void IncIntermediateTrackingTag();

private:
	UPROPERTY()
	FName CompShotIdName;

	UPROPERTY()
	ACompositingElement* Parent;
	UPROPERTY()
	TArray<ACompositingElement*> ChildLayers;

	/** EDITOR ONLY - Properties associated with */
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	UTexture* DisabledMsgImage;
	UPROPERTY(Transient)
	UTexture* EmptyWarnImage;
	UPROPERTY(Transient)
	UTexture* SuspendedDbgImage;
	UPROPERTY(Transient)
	UTexture* CompilerErrImage;

	UPROPERTY(Transient, DuplicateTransient)
	bool bUsingDebugDisplayImage = false;

	UPROPERTY(Transient, DuplicateTransient)
	UTexture* ColorPickerDisplayImage;
	UPROPERTY(Transient, DuplicateTransient)
	UTexture* EditorPreviewImage;

	UPROPERTY(Transient, DuplicateTransient)
	UTextureRenderTarget2D* ColorPickerTarget;

	uint32 LastEnqueuedFrameId = (uint32)-1;
	int32  PreviewCount = 0;
#endif // WITH_EDITORONLY_DATA

	ETargetUsageFlags NextIntermediateTrackingTag = ETargetUsageFlags::USAGE_Intermediate0;

	UPROPERTY()
	float OutputOpacity = 1.f;

	/** 
	 * Lists containing passes added programatically (or through Blueprints) via the AddNewPass() functions. 
	 * These need their own separate lists to: 1) hide from the details panel, and 2) clear on 
	 * re-construction, so we don't perpetually grow the lists.
	 */
	UPROPERTY()
	TMap<UCompositingElementInput*, ECompPassConstructionType> UserConstructedInputs;
	UPROPERTY()
	TMap<UCompositingElementTransform*, ECompPassConstructionType> UserConstructedTransforms;
	UPROPERTY()
	TMap<UCompositingElementOutput*, ECompPassConstructionType> UserConstructedOutputs;

	/** 
	 * Authoritative lists that we use to iterate on the passes - conjoined from the public lists and the  
	 * internal 'UserConstructed' ones. Used to: 1) have a single 'goto' list (w/ no nullptrs), and 2)
	 * determine passes that were cleared from the public lists so we can halt their processing (still 
	 * alive via the transaction buffer).
	 */
	UPROPERTY(Instanced, Transient, DuplicateTransient, SkipSerialization)
	TArray<UCompositingElementInput*> InternalInputs;
	UPROPERTY(Instanced, Transient, DuplicateTransient, SkipSerialization)
	TArray<UCompositingElementTransform*> InternalTransformPasses;
	UPROPERTY(Instanced, Transient, DuplicateTransient, SkipSerialization)
	TArray<UCompositingElementOutput*> InternalOutputs;

	UPROPERTY(Transient)
	UAlphaTransformPass* InternalAlphaPass = nullptr;

	/** */
	FCompositingTextureLookupTable PassResultsTable;
	/** */
	FSharedTargetPoolPtr RenderTargetPool;
};

