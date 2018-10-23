// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameterMacros.h: Macros to builds shader parameter structures and
		their metadata.
=============================================================================*/

#pragma once

#include "ShaderParameterMetadata.h"


class FRDGTexture;
class FRDGTextureSRV;
class FRDGTextureUAV;
class FRDGBuffer;
class FRDGBufferSRV;
class FRDGBufferUAV;


// RHICreateUniformBuffer assumes C++ constant layout matches the shader layout when extracting float constants, yet the C++ struct contains pointers.  
// Enforce a min size of 64 bits on pointer types in uniform buffer structs to guarantee layout matching between languages.
#define SHADER_PARAMETER_POINTER_ALIGNMENT sizeof(uint64)
static_assert(sizeof(void*) <= SHADER_PARAMETER_POINTER_ALIGNMENT, "The alignment of pointer needs to match the largest pointer.");


/** Alignements tools because alignas() does not work on type in clang. */
template<typename T, int32 Alignment>
class TAlignedTypedef;

#define IMPLEMENT_ALIGNED_TYPE(Alignment) \
	template<typename T> \
	class TAlignedTypedef<T,Alignment> \
	{ \
	public: \
		typedef MS_ALIGN(Alignment) T Type GCC_ALIGN(Alignment); \
	};

IMPLEMENT_ALIGNED_TYPE(1);
IMPLEMENT_ALIGNED_TYPE(2);
IMPLEMENT_ALIGNED_TYPE(4);
IMPLEMENT_ALIGNED_TYPE(8);
IMPLEMENT_ALIGNED_TYPE(16);
#undef IMPLEMENT_ALIGNED_TYPE


#if PLATFORM_64BITS

/** Fixed 8bytes sized and aligned pointer for shader parameters. */
template<typename PtrType>
using TAlignedShaderParameterPtr = typename TAlignedTypedef<PtrType, SHADER_PARAMETER_POINTER_ALIGNMENT>::Type;

static_assert(sizeof(void*) == 8, "Wrong PLATFORM_64BITS settings.");

#else //!PLATFORM_64BITS

/** Fixed 8bytes sized pointer for shader parameters. */
template<typename PtrType>
class alignas(SHADER_PARAMETER_POINTER_ALIGNMENT) TAlignedShaderParameterPtr
{
public:
	TAlignedShaderParameterPtr()
	{ }

	TAlignedShaderParameterPtr(const PtrType& Other)
		: Ref(Other)
	{ }

	TAlignedShaderParameterPtr(const TAlignedShaderParameterPtr<PtrType>& Other)
		: Ref(Other.Ref)
	{ }

	FORCEINLINE void operator=(const PtrType& Other)
	{
		Ref = Other;
	}

	FORCEINLINE operator PtrType&()
	{
		return Ref;
	}

	FORCEINLINE operator const PtrType&() const
	{
		return Ref;
	}

	FORCEINLINE const PtrType& operator->() const
	{
		return Ref;
	}

private:
	PtrType Ref;
	#if !PLATFORM_64BITS
		uint32 _Padding;
		static_assert(sizeof(void*) == 4, "Wrong PLATFORM_64BITS settings.");
	#endif

	static_assert(sizeof(PtrType) == sizeof(void*), "T should be a pointer.");
};

#endif // !PLATFORM_64BITS


/** A reference to a uniform buffer RHI resource with a specific structure. */
template<typename TBufferStruct>
class TUniformBufferRef : public FUniformBufferRHIRef
{
public:

	/** Initializes the reference to null. */
	TUniformBufferRef()
	{}

	/** Creates a uniform buffer with the given value, and returns a structured reference to it. */
	static TUniformBufferRef<TBufferStruct> CreateUniformBufferImmediate(const TBufferStruct& Value, EUniformBufferUsage Usage)
	{
		check(IsInRenderingThread() || IsInRHIThread());
		return TUniformBufferRef<TBufferStruct>(RHICreateUniformBuffer(&Value,TBufferStruct::StaticStructMetadata.GetLayout(),Usage));
	}
	/** Creates a uniform buffer with the given value, and returns a structured reference to it. */
	static FLocalUniformBuffer CreateLocalUniformBuffer(FRHICommandList& RHICmdList, const TBufferStruct& Value, EUniformBufferUsage Usage)
	{
		return RHICmdList.BuildLocalUniformBuffer(&Value, sizeof(TBufferStruct), TBufferStruct::StaticStructMetadata.GetLayout());
	}

private:

	/** A private constructor used to coerce an arbitrary RHI uniform buffer reference to a structured reference. */
	TUniformBufferRef(FUniformBufferRHIParamRef InRHIRef)
	: FUniformBufferRHIRef(InRHIRef)
	{}

	template<typename TBufferStruct2>
	friend class TUniformBuffer;
};


/** Render graph information about how to bind a render target. */
struct alignas(SHADER_PARAMETER_STRUCT_ALIGNMENT) FRenderTargetBinding
{
	FRenderTargetBinding()
		: Texture(nullptr)
	{ }
	FRenderTargetBinding(const FRenderTargetBinding&) = default;

	/** Creates a render target binding informations.
	 *
	 * Notes: Load and store action are on purpose without default values, to force the user to not forget one of these.
	 */
	FRenderTargetBinding(const FRDGTexture* InTexture, ERenderTargetLoadAction InLoadAction, ERenderTargetStoreAction InStoreAction, uint8 InMipIndex = 0)
		: Texture(InTexture)
		, LoadAction(InLoadAction)
		, StoreAction(InStoreAction)
		, MipIndex(InMipIndex)
	{}

	inline const FRDGTexture* GetTexture() const {
		return Texture;
	};
	inline ERenderTargetLoadAction GetLoadAction() const {
		return LoadAction;
	};
	inline ERenderTargetStoreAction GetStoreAction() const {
		return StoreAction;
	};
	inline uint8 GetMipIndex() const {
		return MipIndex;
	};

private:
	/** All parameters required to bind a render target deferred. This are purposefully private to
	 * force the user to call FRenderTargetBinding() constructor, forcing him to specify the load and store action.
	 */
	TAlignedShaderParameterPtr<const FRDGTexture*> Texture;
	ERenderTargetLoadAction		LoadAction		= ERenderTargetLoadAction::ENoAction;
	ERenderTargetStoreAction	StoreAction		= ERenderTargetStoreAction::ENoAction;
	uint8						MipIndex		= 0;
};


/** Render graph information about how to bind a depth-stencil render target. */
struct alignas(SHADER_PARAMETER_STRUCT_ALIGNMENT) FDepthStencilBinding
{
	FDepthStencilBinding()
		: Texture(nullptr)
	{ }
	FDepthStencilBinding(const FDepthStencilBinding&) = default;

	TAlignedShaderParameterPtr<const FRDGTexture*> Texture;
	ERenderTargetLoadAction		DepthLoadAction		= ERenderTargetLoadAction::ENoAction;
	ERenderTargetStoreAction	DepthStoreAction	= ERenderTargetStoreAction::ENoAction;
	ERenderTargetLoadAction		StencilLoadAction	= ERenderTargetLoadAction::ENoAction;
	ERenderTargetStoreAction	StencilStoreAction	= ERenderTargetStoreAction::ENoAction;
	FExclusiveDepthStencil		DepthStencilAccess	= FExclusiveDepthStencil::DepthNop_StencilNop;
};


/** Special shader parameters type for a pass parameter to setup render targets. */
struct alignas(SHADER_PARAMETER_STRUCT_ALIGNMENT) FRenderTargetBindingSlots
{
	TStaticArray<FRenderTargetBinding, MaxSimultaneousRenderTargets> Output;
	FDepthStencilBinding DepthStencil;

	/** Accessors for regular output to simplify the syntax to:
	 *
	 *	FRenderTargetParameters PassParameters;
	 *	PassParameters.RenderTargets.DepthStencil = ... ;
	 *	PassParameters.RenderTargets[0] = ... ;
	 */
	FRenderTargetBinding& operator[](uint32 Index)
	{
		return Output[Index];
	}

	const FRenderTargetBinding& operator[](uint32 Index) const
	{
		return Output[Index];
	}

	struct FTypeInfo {
		static constexpr int32 NumRows = 1;
		static constexpr int32 NumColumns = 1;
		static constexpr int32 NumElements = 0;
		static constexpr int32 Alignment = SHADER_PARAMETER_STRUCT_ALIGNMENT;
		static constexpr bool bIsStoredInConstantBuffer = false;

		using TAlignedType = FRenderTargetBindingSlots;

		static inline const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
	};
};

static_assert(sizeof(FRenderTargetBindingSlots) == 144, "FRenderTargetBindingSlots needs to be same size on all platforms.");


/** Template to transcode some meta data information for a type <TypeParameter> not specific to shader parameters API. */
template<typename TypeParameter>
struct TShaderParameterTypeInfo
{
	/** Defines what the type actually is. */
	static constexpr EUniformBufferBaseType BaseType = UBMT_INVALID;

	/** Defines the number rows and columns for vector or matrix based data typed. */
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;

	/** Defines the number of elements in an array fashion. 0 means this is not a TStaticArray,
	 * which therefor means there is 1 element.
	 */
	static constexpr int32 NumElements = 0;

	/** Defines the alignment of the elements in bytes. */
	static constexpr int32 Alignment = alignof(TypeParameter);

	/** Defines whether this element is stored in constant buffer or not.
	 * This informations is usefull to ensure at compile time everything in the
	 * structure get defined at the end of the structure, to reduce as much as possible
	 * the size of the constant buffer.
	 */
	static constexpr bool bIsStoredInConstantBuffer = true;

	/** Type that is actually alligned. */
	using TAlignedType = TypeParameter;

	static const FShaderParametersMetadata* GetStructMetadata() { return &TypeParameter::StaticStructMetadata; }
};

template<>
struct TShaderParameterTypeInfo<bool>
{
	static constexpr EUniformBufferBaseType BaseType = UBMT_BOOL;
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = 4;
	static constexpr bool bIsStoredInConstantBuffer = true;
	
	using TAlignedType = TAlignedTypedef<bool, Alignment>::Type;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};

template<>
struct TShaderParameterTypeInfo<uint32>
{
	static constexpr EUniformBufferBaseType BaseType = UBMT_UINT32;
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = 4;
	static constexpr bool bIsStoredInConstantBuffer = true;

	using TAlignedType = TAlignedTypedef<uint32, Alignment>::Type;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};
	

template<>
struct TShaderParameterTypeInfo<int32>
{
	static constexpr EUniformBufferBaseType BaseType = UBMT_INT32;
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = 4;
	static constexpr bool bIsStoredInConstantBuffer = true;

	using TAlignedType = TAlignedTypedef<int32, Alignment>::Type;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};

template<>
struct TShaderParameterTypeInfo<float>
{
	static constexpr EUniformBufferBaseType BaseType = UBMT_FLOAT32;
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = 4;
	static constexpr bool bIsStoredInConstantBuffer = true;

	using TAlignedType = TAlignedTypedef<float, Alignment>::Type;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};

template<>
struct TShaderParameterTypeInfo<FVector2D>
{
	static constexpr EUniformBufferBaseType BaseType = UBMT_FLOAT32;
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 2;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = 8;
	static constexpr bool bIsStoredInConstantBuffer = true;

	using TAlignedType = TAlignedTypedef<FVector2D, Alignment>::Type;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};

template<>
struct TShaderParameterTypeInfo<FVector>
{
	static constexpr EUniformBufferBaseType BaseType = UBMT_FLOAT32;
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 3;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = 16;
	static constexpr bool bIsStoredInConstantBuffer = true;

	using TAlignedType = TAlignedTypedef<FVector, Alignment>::Type;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};

template<>
struct TShaderParameterTypeInfo<FVector4>
{
	static constexpr EUniformBufferBaseType BaseType = UBMT_FLOAT32;
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 4;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = 16;
	static constexpr bool bIsStoredInConstantBuffer = true;

	using TAlignedType = TAlignedTypedef<FVector4, Alignment>::Type;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};
	
template<>
struct TShaderParameterTypeInfo<FLinearColor>
{
	static constexpr EUniformBufferBaseType BaseType = UBMT_FLOAT32;
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 4;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = 16;
	static constexpr bool bIsStoredInConstantBuffer = true;

	using TAlignedType = TAlignedTypedef<FLinearColor, Alignment>::Type;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};
	
template<>
struct TShaderParameterTypeInfo<FIntPoint>
{
	static constexpr EUniformBufferBaseType BaseType = UBMT_INT32;
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 2;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = 8;
	static constexpr bool bIsStoredInConstantBuffer = true;

	using TAlignedType = TAlignedTypedef<FIntPoint, Alignment>::Type;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};

template<>
struct TShaderParameterTypeInfo<FIntVector>
{
	static constexpr EUniformBufferBaseType BaseType = UBMT_INT32;
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 3;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = 16;
	static constexpr bool bIsStoredInConstantBuffer = true;

	using TAlignedType = TAlignedTypedef<FIntVector, Alignment>::Type;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};

template<>
struct TShaderParameterTypeInfo<FIntRect>
{
	static constexpr EUniformBufferBaseType BaseType = UBMT_INT32;
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 4;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = 16;
	static constexpr bool bIsStoredInConstantBuffer = true;

	using TAlignedType = TAlignedTypedef<FIntRect, Alignment>::Type;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};

template<>
struct TShaderParameterTypeInfo<FMatrix>
{
	static constexpr EUniformBufferBaseType BaseType = UBMT_FLOAT32;
	static constexpr int32 NumRows = 4;
	static constexpr int32 NumColumns = 4;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = 16;
	static constexpr bool bIsStoredInConstantBuffer = true;

	using TAlignedType = TAlignedTypedef<FMatrix, Alignment>::Type;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};

template<typename T, size_t InNumElements>
struct TShaderParameterTypeInfo<T[InNumElements]>
{
	static constexpr EUniformBufferBaseType BaseType = TShaderParameterTypeInfo<T>::BaseType;
	static constexpr int32 NumRows = TShaderParameterTypeInfo<T>::NumRows;
	static constexpr int32 NumColumns = TShaderParameterTypeInfo<T>::NumColumns;
	static constexpr int32 NumElements = InNumElements;
	static constexpr int32 Alignment = SHADER_PARAMETER_ARRAY_ELEMENT_ALIGNMENT;
	static constexpr bool bIsStoredInConstantBuffer = TShaderParameterTypeInfo<T>::bIsStoredInConstantBuffer;

	using TAlignedType = TStaticArray<T, InNumElements, Alignment>;

	static const FShaderParametersMetadata* GetStructMetadata() { return TShaderParameterTypeInfo<T>::GetStructMetadata(); }
};
	
template<typename T,size_t InNumElements,uint32 IgnoredAlignment>
struct TShaderParameterTypeInfo<TStaticArray<T,InNumElements,IgnoredAlignment>>
{
	static constexpr EUniformBufferBaseType BaseType = TShaderParameterTypeInfo<T>::BaseType;
	static constexpr int32 NumRows = TShaderParameterTypeInfo<T>::NumRows;
	static constexpr int32 NumColumns = TShaderParameterTypeInfo<T>::NumColumns;
	static constexpr int32 NumElements = InNumElements;
	static constexpr int32 Alignment = SHADER_PARAMETER_ARRAY_ELEMENT_ALIGNMENT;
	static constexpr bool bIsStoredInConstantBuffer = TShaderParameterTypeInfo<T>::bIsStoredInConstantBuffer;

	using TAlignedType = TStaticArray<T, InNumElements, Alignment>;

	static const FShaderParametersMetadata* GetStructMetadata() { return TShaderParameterTypeInfo<T>::GetStructMetadata(); }
};

template<>
struct TShaderParameterTypeInfo<FShaderResourceViewRHIParamRef>
{
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = SHADER_PARAMETER_POINTER_ALIGNMENT;
	static constexpr bool bIsStoredInConstantBuffer = false;

	using TAlignedType = TAlignedShaderParameterPtr<FShaderResourceViewRHIParamRef>;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};
static_assert(sizeof(TShaderParameterTypeInfo<FShaderResourceViewRHIParamRef>::TAlignedType) == SHADER_PARAMETER_POINTER_ALIGNMENT, "Uniform buffer layout must not be platform dependent.");

template<>
struct TShaderParameterTypeInfo<FSamplerStateRHIParamRef>
{
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = SHADER_PARAMETER_POINTER_ALIGNMENT;
	static constexpr bool bIsStoredInConstantBuffer = false;

	using TAlignedType = TAlignedShaderParameterPtr<FSamplerStateRHIParamRef>;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};
static_assert(sizeof(TShaderParameterTypeInfo<FSamplerStateRHIParamRef>::TAlignedType) == SHADER_PARAMETER_POINTER_ALIGNMENT, "Uniform buffer layout must not be platform dependent.");

template<>
struct TShaderParameterTypeInfo<FTextureRHIParamRef>
{
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = SHADER_PARAMETER_POINTER_ALIGNMENT;
	static constexpr bool bIsStoredInConstantBuffer = false;

	using TAlignedType = TAlignedShaderParameterPtr<FTextureRHIParamRef>;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};
static_assert(sizeof(TShaderParameterTypeInfo<FTextureRHIParamRef>::TAlignedType) == SHADER_PARAMETER_POINTER_ALIGNMENT, "Uniform buffer layout must not be platform dependent.");

template<>
struct TShaderParameterTypeInfo<const FRDGTexture*>
{
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = SHADER_PARAMETER_POINTER_ALIGNMENT;
	static constexpr bool bIsStoredInConstantBuffer = false;

	using TAlignedType = TAlignedShaderParameterPtr<const FRDGTexture*>;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};
static_assert(sizeof(TShaderParameterTypeInfo<const FRDGTexture*>::TAlignedType) == SHADER_PARAMETER_POINTER_ALIGNMENT, "Uniform buffer layout must not be platform dependent.");

template<>
struct TShaderParameterTypeInfo<const FRDGTextureSRV*>
{
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = SHADER_PARAMETER_POINTER_ALIGNMENT;
	static constexpr bool bIsStoredInConstantBuffer = false;
	
	using TAlignedType = TAlignedShaderParameterPtr<const FRDGTextureSRV*>;
	
	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};
static_assert(sizeof(TShaderParameterTypeInfo<const FRDGTextureSRV*>::TAlignedType) == SHADER_PARAMETER_POINTER_ALIGNMENT, "Uniform buffer layout must not be platform dependent.");

template<>
struct TShaderParameterTypeInfo<const FRDGTextureUAV*>
{
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = SHADER_PARAMETER_POINTER_ALIGNMENT;
	static constexpr bool bIsStoredInConstantBuffer = false;
	
	using TAlignedType = TAlignedShaderParameterPtr<const FRDGTextureUAV*>;
	
	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};
static_assert(sizeof(TShaderParameterTypeInfo<const FRDGTextureUAV*>::TAlignedType) == SHADER_PARAMETER_POINTER_ALIGNMENT, "Uniform buffer layout must not be platform dependent.");

template<>
struct TShaderParameterTypeInfo<const FRDGBuffer*>
{
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = SHADER_PARAMETER_POINTER_ALIGNMENT;
	static constexpr bool bIsStoredInConstantBuffer = false;

	using TAlignedType = TAlignedShaderParameterPtr<const FRDGBuffer*>;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};
static_assert(sizeof(TShaderParameterTypeInfo<const FRDGBuffer*>::TAlignedType) == SHADER_PARAMETER_POINTER_ALIGNMENT, "Uniform buffer layout must not be platform dependent.");

template<>
struct TShaderParameterTypeInfo<const FRDGBufferSRV*>
{
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = SHADER_PARAMETER_POINTER_ALIGNMENT;
	static constexpr bool bIsStoredInConstantBuffer = false;

	using TAlignedType = TAlignedShaderParameterPtr<const FRDGBufferSRV*>;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};
static_assert(sizeof(TShaderParameterTypeInfo<const FRDGBufferSRV*>::TAlignedType) == SHADER_PARAMETER_POINTER_ALIGNMENT, "Uniform buffer layout must not be platform dependent.");

template<>
struct TShaderParameterTypeInfo<const FRDGBufferUAV*>
{
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = SHADER_PARAMETER_POINTER_ALIGNMENT;
	static constexpr bool bIsStoredInConstantBuffer = false;

	using TAlignedType = TAlignedShaderParameterPtr<const FRDGBufferUAV*>;

	static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};
static_assert(sizeof(TShaderParameterTypeInfo<const FRDGBufferUAV*>::TAlignedType) == SHADER_PARAMETER_POINTER_ALIGNMENT, "Uniform buffer layout must not be platform dependent.");

template<class UniformBufferStructType>
struct TShaderParameterTypeInfo<TUniformBufferRef<UniformBufferStructType>>
{
	static constexpr int32 NumRows = 1;
	static constexpr int32 NumColumns = 1;
	static constexpr int32 NumElements = 0;
	static constexpr int32 Alignment = SHADER_PARAMETER_POINTER_ALIGNMENT;
	static constexpr bool bIsStoredInConstantBuffer = false;
	
	using TAlignedType = TAlignedShaderParameterPtr<TUniformBufferRef<UniformBufferStructType>>;
	
	static const FShaderParametersMetadata* GetStructMetadata() { return &UniformBufferStructType::StaticStructMetadata; }
};


#define INTERNAL_BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT \
	static FShaderParametersMetadata StaticStructMetadata; \

#define INTERNAL_GLOBAL_SHADER_PARAMETER_GET_STRUCT_METADATA(StructTypeName) \
	return &StructTypeName::StaticStructMetadata;

#define INTERNAL_LOCAL_SHADER_PARAMETER_GET_STRUCT_METADATA(StructTypeName) \
	static FShaderParametersMetadata StaticStructMetadata(\
		FShaderParametersMetadata::EUseCase::ShaderParameterStruct, \
		FName(TEXT(#StructTypeName)), \
		nullptr, \
		nullptr, \
		sizeof(StructTypeName), \
		StructTypeName::zzGetMembers()); \
	return &StaticStructMetadata;


/** Begins a uniform buffer struct declaration. */
#define INTERNAL_SHADER_PARAMETER_STRUCT_BEGIN(StructTypeName,PrefixKeywords,ConstructorSuffix,GetStructMetadataScope) \
	MS_ALIGN(SHADER_PARAMETER_STRUCT_ALIGNMENT) class PrefixKeywords StructTypeName \
	{ \
	public: \
		StructTypeName () ConstructorSuffix \
		struct FTypeInfo { \
			static constexpr int32 NumRows = 1; \
			static constexpr int32 NumColumns = 1; \
			static constexpr int32 NumElements = 0; \
			static constexpr int32 Alignment = SHADER_PARAMETER_STRUCT_ALIGNMENT; \
			static constexpr bool bIsStoredInConstantBuffer = true; \
			using TAlignedType = StructTypeName; \
			static inline const FShaderParametersMetadata* GetStructMetadata() { GetStructMetadataScope } \
		}; \
	private: \
		typedef StructTypeName zzTThisStruct; \
		struct zzFirstMemberId { enum { HasDeclaredResource = 0 }; }; \
		static TArray<FShaderParametersMetadata::FMember> zzGetMembersBefore(zzFirstMemberId) \
		{ \
			return TArray<FShaderParametersMetadata::FMember>(); \
		} \
		typedef zzFirstMemberId

/** Declares a member of a uniform buffer struct. */
#define DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(BaseType,TypeInfo,MemberType,MemberName,ArrayDecl,Precision,OptionalShaderType,IsMemberStruct) \
		zzMemberId##MemberName; \
	public: \
		typedef MemberType zzA##MemberName ArrayDecl; \
		typedef TypeInfo::TAlignedType zzT##MemberName; \
		zzT##MemberName MemberName; \
		static_assert(BaseType != UBMT_INVALID, "Invalid type " #MemberType " of member " #MemberName "."); \
	private: \
		struct zzNextMemberId##MemberName { enum { HasDeclaredResource = zzMemberId##MemberName::HasDeclaredResource || !TypeInfo::bIsStoredInConstantBuffer }; }; \
		static TArray<FShaderParametersMetadata::FMember> zzGetMembersBefore(zzNextMemberId##MemberName) \
		{ \
			static_assert(!TypeInfo::bIsStoredInConstantBuffer || zzMemberId##MemberName::HasDeclaredResource == 0, "Shader resources must be declared after " #MemberName "."); \
			static_assert(TypeInfo::bIsStoredInConstantBuffer || TIsArrayOrRefOfType<decltype(OptionalShaderType), TCHAR>::Value, "No shader type for " #MemberName "."); \
			/* Route the member enumeration on to the function for the member following this. */ \
			TArray<FShaderParametersMetadata::FMember> OutMembers = zzGetMembersBefore(zzMemberId##MemberName()); \
			/* Add this member. */ \
			OutMembers.Add(FShaderParametersMetadata::FMember( \
				TEXT(#MemberName), \
				OptionalShaderType, \
				STRUCT_OFFSET(zzTThisStruct,MemberName), \
				EUniformBufferBaseType(BaseType), \
				Precision, \
				TypeInfo::NumRows, \
				TypeInfo::NumColumns, \
				TypeInfo::NumElements, \
				TypeInfo::GetStructMetadata() \
				)); \
			static_assert( \
				(STRUCT_OFFSET(zzTThisStruct,MemberName) & (TypeInfo::Alignment - 1)) == 0, \
				"Misaligned uniform buffer struct member " #MemberName "."); \
			return OutMembers; \
		} \
		typedef zzNextMemberId##MemberName

/** Finds the FShaderParametersMetadata corresponding to the given name, or NULL if not found. */
extern RENDERCORE_API FShaderParametersMetadata* FindUniformBufferStructByName(const TCHAR* StructName);
extern RENDERCORE_API FShaderParametersMetadata* FindUniformBufferStructByFName(FName StructName);


/** Begins & ends a shader parameter structure.
 *
 * Example:
 *	BEGIN_SHADER_PARAMETER_STRUCT(FMyParameterStruct, RENDERER_API)
 *	END_SHADER_PARAMETER_STRUCT()
 */
#define BEGIN_SHADER_PARAMETER_STRUCT(StructTypeName, PrefixKeywords) \
	INTERNAL_SHADER_PARAMETER_STRUCT_BEGIN(StructTypeName, PrefixKeywords, {}, INTERNAL_LOCAL_SHADER_PARAMETER_GET_STRUCT_METADATA(StructTypeName))

#define END_SHADER_PARAMETER_STRUCT() \
		zzLastMemberId; \
		static TArray<FShaderParametersMetadata::FMember> zzGetMembers() { return zzGetMembersBefore(zzLastMemberId()); } \
	} GCC_ALIGN(SHADER_PARAMETER_STRUCT_ALIGNMENT);

/** Begins & ends a shader global parameter structure.
 *
 * Example:
 *	// header
 *	BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FMyParameterStruct, RENDERER_API)
 *	END_GLOBAL_SHADER_PARAMETER_STRUCT()
 *
 *	// C++ file
 *	IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FMyParameterStruct, "MyShaderBindingName");
 */
#define BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(StructTypeName, PrefixKeywords) \
	INTERNAL_SHADER_PARAMETER_STRUCT_BEGIN(StructTypeName,PrefixKeywords,{} INTERNAL_BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT, INTERNAL_GLOBAL_SHADER_PARAMETER_GET_STRUCT_METADATA(StructTypeName))

#define BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT_WITH_CONSTRUCTOR(StructTypeName, PrefixKeywords) \
	INTERNAL_SHADER_PARAMETER_STRUCT_BEGIN(StructTypeName,PrefixKeywords,; INTERNAL_BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT, INTERNAL_GLOBAL_SHADER_PARAMETER_GET_STRUCT_METADATA(StructTypeName))

#define END_GLOBAL_SHADER_PARAMETER_STRUCT() \
	END_SHADER_PARAMETER_STRUCT()

#define IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(StructTypeName,ShaderVariableName) \
	FShaderParametersMetadata StructTypeName::StaticStructMetadata( \
	FShaderParametersMetadata::EUseCase::GlobalShaderParameterStruct, \
	FName(TEXT(#StructTypeName)), \
	TEXT(#StructTypeName), \
	TEXT(ShaderVariableName), \
	sizeof(StructTypeName), \
	StructTypeName::zzGetMembers())


/** Adds a constant-buffer stored value.
 *
 * Example:
 *	SHADER_PARAMETER(float, MyScalar)
 *	SHADER_PARAMETER(FMatrix, MyMatrix)
 */
#define SHADER_PARAMETER(MemberType,MemberName) \
	SHADER_PARAMETER_EX(MemberType,MemberName,EShaderPrecisionModifier::Float)

#define SHADER_PARAMETER_EX(MemberType,MemberName,Precision) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(TShaderParameterTypeInfo<MemberType>::BaseType, TShaderParameterTypeInfo<MemberType>, MemberType,MemberName,,Precision,TEXT(""),false)

/** Adds a constant-buffer stored array of values.
 *
 * Example:
 *	SHADER_PARAMETER_ARRAY(float, MyScalarArray, [8])
 *	SHADER_PARAMETER_ARRAY(FMatrix, MyMatrixArray)
 */
#define SHADER_PARAMETER_ARRAY(MemberType,MemberName,ArrayDecl) \
	SHADER_PARAMETER_ARRAY_EX(MemberType,MemberName,ArrayDecl,EShaderPrecisionModifier::Float)

#define SHADER_PARAMETER_ARRAY_EX(MemberType,MemberName,ArrayDecl,Precision) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(TShaderParameterTypeInfo<MemberType ArrayDecl>::BaseType, TShaderParameterTypeInfo<MemberType ArrayDecl>, MemberType,MemberName,ArrayDecl,Precision,TEXT(""),false)

/** Adds a texture.
 *
 * Example:
 *	SHADER_PARAMETER_TEXTURE(Texture2D, MyTexture)
 */
#define SHADER_PARAMETER_TEXTURE(ShaderType,MemberName) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(UBMT_TEXTURE, TShaderParameterTypeInfo<FTextureRHIParamRef>, FTextureRHIParamRef,MemberName,,EShaderPrecisionModifier::Float,TEXT(#ShaderType),false)

/** Adds a shader resource view.
 *
 * Example:
 *	SHADER_PARAMETER_SRV(Texture2D, MySRV)
 */
#define SHADER_PARAMETER_SRV(ShaderType,MemberName) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(UBMT_SRV, TShaderParameterTypeInfo<FShaderResourceViewRHIParamRef>, FShaderResourceViewRHIParamRef,MemberName,,EShaderPrecisionModifier::Float,TEXT(#ShaderType),false)

/** Adds a sampler.
 *
 * Example:
 *	SHADER_PARAMETER_SAMPLER(SamplerState, MySampler)
 */
#define SHADER_PARAMETER_SAMPLER(ShaderType,MemberName) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(UBMT_SAMPLER, TShaderParameterTypeInfo<FSamplerStateRHIParamRef>, FSamplerStateRHIParamRef,MemberName,,EShaderPrecisionModifier::Float,TEXT(#ShaderType),false)

/** Adds a render graph tracked texture.
 *
 * Example:
 *	SHADER_PARAMETER_GRAPH_TEXTURE(Texture2D, MyTexture)
 */
#define SHADER_PARAMETER_GRAPH_TEXTURE(ShaderType,MemberName) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(UBMT_GRAPH_TRACKED_TEXTURE, TShaderParameterTypeInfo<const FRDGTexture*>, const FRDGTexture*,MemberName,,EShaderPrecisionModifier::Float,TEXT(#ShaderType),false)

/** Adds a shader resource view for a render graph tracked texture.
 *
 * Example:
 *	SHADER_PARAMETER_GRAPH_SRV(Texture2D, MySRV)
 */
#define SHADER_PARAMETER_GRAPH_SRV(ShaderType,MemberName) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(UBMT_GRAPH_TRACKED_SRV, TShaderParameterTypeInfo<const FRDGTextureSRV*>, const FRDGTextureSRV*,MemberName,,EShaderPrecisionModifier::Float,TEXT(#ShaderType),false)

/** Adds a unordered access view for a render graph tracked texture.
 *
 * Example:
 *	SHADER_PARAMETER_GRAPH_UAV(RWTexture2D, MyUAV)
 */
#define SHADER_PARAMETER_GRAPH_UAV(ShaderType,MemberName) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(UBMT_GRAPH_TRACKED_UAV, TShaderParameterTypeInfo<const FRDGTextureUAV*>, const FRDGTextureUAV*,MemberName,,EShaderPrecisionModifier::Float,TEXT(#ShaderType),false)

/** Adds a render graph tracked buffer.
 *
 * Example:
 *	SHADER_PARAMETER_GRAPH_BUFFER(Texture2D, MyTexture)
 */
#define SHADER_PARAMETER_GRAPH_BUFFER(ShaderType,MemberName) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(UBMT_GRAPH_TRACKED_BUFFER, TShaderParameterTypeInfo<const FRDGBuffer*>, const FRDGBuffer*,MemberName,,EShaderPrecisionModifier::Float,TEXT(#ShaderType),false)

/** Adds a shader resource view for a render graph tracked buffer.
 *
 * Example:
 *	SHADER_PARAMETER_GRAPH_BUFFER_SRV(Buffer<float4>, MySRV)
 */
#define SHADER_PARAMETER_GRAPH_BUFFER_SRV(ShaderType,MemberName) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(UBMT_GRAPH_TRACKED_BUFFER_SRV, TShaderParameterTypeInfo<const FRDGBufferSRV*>, const FRDGBufferSRV*,MemberName,,EShaderPrecisionModifier::Float,TEXT(#ShaderType),false)

/** Adds a unordered access view for a render graph tracked buffer.
 *
 * Example:
 *	SHADER_PARAMETER_GRAPH_BUFFER_UAV(RWBuffer<float4>, MyUAV)
 */
#define SHADER_PARAMETER_GRAPH_BUFFER_UAV(ShaderType,MemberName) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(UBMT_GRAPH_TRACKED_BUFFER_UAV, TShaderParameterTypeInfo<const FRDGBufferUAV*>, const FRDGBufferUAV*,MemberName,,EShaderPrecisionModifier::Float,TEXT(#ShaderType),false)

/** Nests a shader parameter structure into another one, in C++ and shader code.
 *
 * Example:
 *	BEGIN_SHADER_PARAMETER_STRUCT(FMyNestedStruct,)
 *		SHADER_PARAMETER(float, MyScalar)
 *		// ...
 *	END_SHADER_PARAMETER_STRUCT()
 *
 *	BEGIN_SHADER_PARAMETER_STRUCT(FOtherStruct)
 *		SHADER_PARAMETER_STRUCT(FMyNestedStruct, MyStruct)
 *
 * C++ use case:
 *	FOtherStruct Parameters;
 *	Parameters.MyStruct.MyScalar = 1.0f;
 *
 * Shader code for a globally named shader parameter struct:
 *	float MyScalar = MyGlobalShaderBindingName.MyStruct.MyScalar;
 *
 * Shader code for a unnamed shader parameter struct:
 *	float MyScalar = MyStruct_MyScalar;
 */
#define SHADER_PARAMETER_STRUCT(StructType,MemberName) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(UBMT_NESTED_STRUCT, StructType::FTypeInfo, StructType, MemberName,,EShaderPrecisionModifier::Float,TEXT(#StructType),true)

/** Include a shader parameter structure into another one in shader code.
 *
 * Example:
 *	BEGIN_SHADER_PARAMETER_STRUCT(FMyNestedStruct,)
 *		SHADER_PARAMETER(float, MyScalar)
 *		// ...
 *	END_SHADER_PARAMETER_STRUCT()
 *
 *	BEGIN_SHADER_PARAMETER_STRUCT(FOtherStruct)
 *		SHADER_PARAMETER_STRUCT_INCLUDE(FMyNestedStruct, MyStruct)
 *
 * C++ use case:
 *	FOtherStruct Parameters;
 *	Parameters.MyStruct.MyScalar = 1.0f;
 *
 * Shader code for a globally named shader parameter struct:
 *	float MyScalar = MyGlobalShaderBindingName.MyScalar;
 *
 * Shader code for a unnamed shader parameter struct:
 *	float MyScalarValue = MyScalar;
 */
#define SHADER_PARAMETER_STRUCT_INCLUDE(StructType,MemberName) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(UBMT_INCLUDED_STRUCT, StructType::FTypeInfo, StructType, MemberName,,EShaderPrecisionModifier::Float,TEXT(#StructType),true)

/** Include a binding slot for a globally named shader parameter structure
 *
 * Example:
 *	BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FGlobalViewParameters,)
 *		SHADER_PARAMETER(FVector4, ViewSizeAndInvSize)
 *		// ...
 *	END_GLOBAL_SHADER_PARAMETER_STRUCT()
 *
 *	BEGIN_SHADER_PARAMETER_STRUCT(FOtherStruct)
 *		SHADER_PARAMETER_STRUCT_REF(FMyNestedStruct, MyStruct)
 */
#define SHADER_PARAMETER_STRUCT_REF(StructType,MemberName) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(UBMT_REFERENCED_STRUCT, TShaderParameterTypeInfo<TUniformBufferRef<StructType>>, TUniformBufferRef<StructType>,MemberName,,EShaderPrecisionModifier::Float,TEXT(#StructType),false)

/** Adds bindings slots for render targets on the structure. This is important for rasterizer based pass bind the
 * render target at the RHI pass creation. The name of the struct member will forced to RenderTargets, and
 * its type is a FRenderTargetBindingSlots.
 *
 * Example:
 *	BEGIN_SHADER_PARAMETER_STRUCT(FGlobalViewParameters,)
 *		RENDER_TARGET_BINDING_SLOTS()
 *		// ...
 *	END_SHADER_PARAMETER_STRUCT()
 *
 *	FGlobalViewParameters Parameters;
 *	Parameters.RenderTargets.DepthStencil = FDepthStencilBinding( //... );
 */
#define RENDER_TARGET_BINDING_SLOTS() \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EXPLICIT(UBMT_RENDER_TARGET_BINDING_SLOTS, FRenderTargetBindingSlots::FTypeInfo, FRenderTargetBindingSlots,RenderTargets,,EShaderPrecisionModifier::Float,TEXT(""),false)
