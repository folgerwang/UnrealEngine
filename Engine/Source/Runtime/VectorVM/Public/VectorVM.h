// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Math/RandomStream.h"

//TODO: move to a per platform header and have VM scale vectorization according to vector width.
#define VECTOR_WIDTH (128)
#define VECTOR_WIDTH_BYTES (16)
#define VECTOR_WIDTH_FLOATS (4)

DECLARE_DELEGATE_OneParam(FVMExternalFunction, struct FVectorVMContext& /*Context*/);

UENUM()
enum class EVectorVMBaseTypes : uint8
{
	Float,
	Int,
	Bool,
	Num UMETA(Hidden),
};

UENUM()
enum class EVectorVMOperandLocation : uint8
{
	Register,
	Constant,
	Num
};

UENUM()
enum class EVectorVMOp : uint8
{
	done,
	add,
	sub,
	mul,
	div,
	mad,
	lerp,
	rcp,
	rsq,
	sqrt,
	neg,
	abs,
	exp,
	exp2,
	log,
	log2,
	sin,
	cos,
	tan,
	asin,
	acos,
	atan,
	atan2,
	ceil,
	floor,
	fmod,
	frac,
	trunc,
	clamp,
	min,
	max,
	pow,
	round,
	sign,
	step,
	random,
	noise,

	//Comparison ops.
	cmplt,
	cmple,
	cmpgt,
	cmpge,
	cmpeq,
	cmpneq,
	select,

// 	easein,  Pretty sure these can be replaced with just a single smoothstep implementation.
// 	easeinout,

	//Integer ops
	addi,
	subi,
	muli,
	//divi,//SSE Integer division is not implemented as an intrinsic. Will have to do some manual implementation.
	clampi,
	mini,
	maxi,
	absi,
	negi,
	signi,
	randomi,
	cmplti,
	cmplei,
	cmpgti,
	cmpgei,
	cmpeqi,
	cmpneqi,
	bit_and,
	bit_or,
	bit_xor,
	bit_not,

	//"Boolean" ops. Currently handling bools as integers.
	logic_and,
	logic_or,
	logic_xor,
	logic_not,

	//conversions
	f2i,
	i2f,
	f2b,
	b2f,
	i2b,
	b2i,

	// data read/write
	inputdata_32bit,
	inputdata_noadvance_32bit,
	outputdata_32bit,
	acquireindex,

	external_func_call,

	/** Returns the index of each instance in the current execution context. */
	exec_index,

	noise2D,
	noise3D,

	/** Utility ops for hooking into the stats system for performance analysis. */
	enter_stat_scope,
	exit_stat_scope,

	//updates an ID in the ID table
	update_id,
	//acquires a new ID from the free list.
	acquire_id,

	NumOpcodes
};


//TODO: 
//All of this stuff can be handled by the VM compiler rather than dirtying the VM code.
//Some require RWBuffer like support.
struct FDataSetMeta
{
	uint8 **InputRegisters;
	uint8 NumVariables;
	uint32 DataSetSizeInBytes;
	int32 DataSetAccessIndex;	// index for individual elements of this set
	int32 DataSetOffset;		// offset in the register table

	int32 InstanceOffset;		// offset of the first instance processed 
	
	TArray<int32>*RESTRICT IDTable;
	TArray<int32>*RESTRICT FreeIDTable;

	/** Number of free IDs in the FreeIDTable */
	int32* NumFreeIDs;

	/** MaxID used in this execution. */
	int32* MaxUsedID;

	int32 IDAcquireTag;

	FDataSetMeta(uint32 DataSetSize, uint8 **Data, uint8 InNumVariables, int32 InInstanceOffset, TArray<int32>* InIDTable, TArray<int32>* InFreeIDTable, int32* InNumFreeIDs, int32* InMaxUsedID, int32 InIDAcquireTag)
		: InputRegisters(Data), NumVariables(InNumVariables), DataSetSizeInBytes(DataSetSize), DataSetAccessIndex(INDEX_NONE), DataSetOffset(0), InstanceOffset(InInstanceOffset)
		, IDTable(InIDTable), FreeIDTable(InFreeIDTable), NumFreeIDs(InNumFreeIDs), MaxUsedID(InMaxUsedID), IDAcquireTag(InIDAcquireTag)
	{
	}

	FDataSetMeta() 
		: InputRegisters(nullptr), NumVariables(0), DataSetSizeInBytes(0), DataSetAccessIndex(INDEX_NONE), DataSetOffset(0), InstanceOffset(0)
		, IDTable(nullptr), FreeIDTable(nullptr), NumFreeIDs(nullptr), MaxUsedID(nullptr), IDAcquireTag(0)
	{}
};

namespace VectorVM
{
	/** Constants. */
	enum
	{
		NumTempRegisters = 400,
		MaxInputRegisters = 400,
		MaxOutputRegisters = MaxInputRegisters,
		MaxConstants = 256,
		FirstTempRegister = 0,
		FirstInputRegister = NumTempRegisters,
		FirstOutputRegister = FirstInputRegister + MaxInputRegisters,
		MaxRegisters = NumTempRegisters + MaxInputRegisters + MaxOutputRegisters + MaxConstants,
	};

	/** Get total number of op-codes */
	VECTORVM_API uint8 GetNumOpCodes();

#if WITH_EDITOR
	VECTORVM_API FString GetOpName(EVectorVMOp Op);
	VECTORVM_API FString GetOperandLocationName(EVectorVMOperandLocation Location);
#endif

	VECTORVM_API uint8 CreateSrcOperandMask(EVectorVMOperandLocation Type0, EVectorVMOperandLocation Type1 = EVectorVMOperandLocation::Register, EVectorVMOperandLocation Type2 = EVectorVMOperandLocation::Register);

	/**
	 * Execute VectorVM bytecode.
	 */
	VECTORVM_API void Exec(
		uint8 const* Code,
		uint8** InputRegisters,
		int32 NumInputRegisters,
		uint8** OutputRegisters,
		int32 NumOutputRegisters,
		uint8 const* ConstantTable,
		TArray<FDataSetMeta> &DataSetMetaTable,
		FVMExternalFunction* ExternalFunctionTable,
		void** UserPtrTable,
		int32 NumInstances
#if STATS
		, const TArray<TStatId>& StatScopes
#endif
		);

	VECTORVM_API void Init();
} // namespace VectorVM


  /**
  * Context information passed around during VM execution.
  */
struct FVectorVMContext : TThreadSingleton<FVectorVMContext>
{
	/** Pointer to the next element in the byte code. */
	uint8 const* RESTRICT Code;
	/** Pointer to the constant table. */
	uint8 const* RESTRICT ConstantTable;
	/** Pointer to the data set index counter table */
	int32* RESTRICT DataSetIndexTable;
	int32* RESTRICT DataSetOffsetTable;
	int32 NumSecondaryDataSets;
	/** Pointer to the shared data table. */
	FVMExternalFunction* RESTRICT ExternalFunctionTable;
	/** Table of user pointers.*/
	void** UserPtrTable;
	/** Number of instances to process. */
	int32 NumInstances;
	/** Start instance of current chunk. */
	int32 StartInstance;

	/** Array of meta data on data sets. TODO: This struct should be removed and all features it contains be handled by more general vm ops and the compiler's knowledge of offsets etc. */
	FDataSetMeta*RESTRICT DataSetMetaTable;

#if STATS
	TArray<FCycleCounter, TInlineAllocator<64>> StatCounterStack;
	const TArray<TStatId>* StatScopes;
#endif

	TArray<uint8, TAlignedHeapAllocator<VECTOR_WIDTH_BYTES>> TempRegTable;
	uint8 *RESTRICT RegisterTable[VectorVM::MaxRegisters];

	FRandomStream RandStream;

	FVectorVMContext();

	void PrepareForExec(
		uint8*RESTRICT*RESTRICT InputRegisters,
		uint8*RESTRICT*RESTRICT OutputRegisters,
		int32 NumInputRegisters,
		int32 NumOutputRegisters,
		const uint8* InConstantTable,
		int32 *InDataSetIndexTable,
		int32 *InDataSetOffsetTable,
		int32 InNumSecondaryDatasets,
		FVMExternalFunction* InExternalFunctionTable,
		void** InUserPtrTable,
		FDataSetMeta*RESTRICT InDataSetMetaTable
#if STATS
		, const TArray<TStatId>* InStatScopes
#endif
	);

	void PrepareForChunk(const uint8* InCode, int32 InNumInstances, int32 InStartInstance)
	{
		Code = InCode;
		NumInstances = InNumInstances;
		StartInstance = InStartInstance;
	}
};

FORCEINLINE uint8 DecodeU8(FVectorVMContext& Context)
{
	return *Context.Code++;
}

FORCEINLINE uint16 DecodeU16(FVectorVMContext& Context)
{
	return ((uint16)DecodeU8(Context) << 8) + DecodeU8(Context);
}

FORCEINLINE uint32 DecodeU32(FVectorVMContext& Context)
{
	return ((uint32)DecodeU8(Context) << 24) + (uint32)(DecodeU8(Context) << 16) + (uint32)(DecodeU8(Context) << 8) + DecodeU8(Context);
}

/** Decode the next operation contained in the bytecode. */
FORCEINLINE EVectorVMOp DecodeOp(FVectorVMContext& Context)
{
	return static_cast<EVectorVMOp>(DecodeU8(Context));
}

FORCEINLINE uint8 DecodeSrcOperandTypes(FVectorVMContext& Context)
{
	return DecodeU8(Context);
}

//////////////////////////////////////////////////////////////////////////
/** Constant handler. */

struct FConstantHandlerBase
{
	uint16 ConstantIndex;
	FConstantHandlerBase(FVectorVMContext& Context)
		: ConstantIndex(DecodeU16(Context))
	{}

	FORCEINLINE void Advance() { }
};

template<typename T>
struct FConstantHandler : public FConstantHandlerBase
{
	T Constant;
	FConstantHandler(FVectorVMContext& Context)
		: FConstantHandlerBase(Context)
		, Constant(*((T*)(Context.ConstantTable + ConstantIndex)))
	{}
	FORCEINLINE const T& Get() { return Constant; }
	FORCEINLINE const T& GetAndAdvance() { return Constant; }
};



struct FDataSetOffsetHandler : FConstantHandlerBase
{
	uint32 Offset;
	FDataSetOffsetHandler(FVectorVMContext& Context)
		: FConstantHandlerBase(Context)
		, Offset(Context.DataSetOffsetTable[ConstantIndex])
	{}
	FORCEINLINE const uint32 Get() { return Offset; }
	FORCEINLINE const uint32 GetAndAdvance() { return Offset; }
};



template<>
struct FConstantHandler<VectorRegister> : public FConstantHandlerBase
{
	VectorRegister Constant;
	FConstantHandler(FVectorVMContext& Context)
		: FConstantHandlerBase(Context)
		, Constant(VectorLoadFloat1(&Context.ConstantTable[ConstantIndex]))
	{}
	FORCEINLINE const VectorRegister Get() { return Constant; }
	FORCEINLINE const VectorRegister GetAndAdvance() { return Constant; }
};

template<>
struct FConstantHandler<VectorRegisterInt> : public FConstantHandlerBase
{
	VectorRegisterInt Constant;
	FConstantHandler(FVectorVMContext& Context)
		: FConstantHandlerBase(Context)
		, Constant(VectorIntLoad1(&Context.ConstantTable[ConstantIndex]))
	{}
	FORCEINLINE const VectorRegisterInt Get() { return Constant; }
	FORCEINLINE const VectorRegisterInt GetAndAdvance() { return Constant; }
};

//////////////////////////////////////////////////////////////////////////
// Register handlers.
// Handle reading of a register, advancing the pointer with each read.

struct FRegisterHandlerBase
{
	int32 RegisterIndex;
	FORCEINLINE FRegisterHandlerBase(FVectorVMContext& Context)
		: RegisterIndex(DecodeU16(Context))
	{}

	FORCEINLINE bool IsValid() const { return RegisterIndex != 0xFFFF; }
};

template<typename T>
struct FUserPtrHandler
{
	int32 UserPtrIdx;
	T* Ptr;
	FUserPtrHandler(FVectorVMContext& Context)
		: UserPtrIdx(*(int32*)(Context.ConstantTable + DecodeU16(Context)))
		, Ptr((T*)Context.UserPtrTable[UserPtrIdx])
	{
		check(UserPtrIdx != INDEX_NONE);
	}
	FORCEINLINE T* Get() { return Ptr; }
	FORCEINLINE T* operator->() { return Ptr; }
	FORCEINLINE operator T*() { return Ptr; }
};

template<typename T>
struct FRegisterHandler : public FRegisterHandlerBase
{
private:
	T Dummy;
	T* RESTRICT Register;
	uint32 AdvanceOffset;
public:
	FORCEINLINE FRegisterHandler(FVectorVMContext& Context)
		: FRegisterHandlerBase(Context)
		, Register(IsValid() ? (T*)Context.RegisterTable[RegisterIndex] : &Dummy)
		, AdvanceOffset(IsValid() ? 1 : 0)
	{}
	FORCEINLINE const T Get() { return *Register; }
	FORCEINLINE T* GetDest() { return Register; }
	FORCEINLINE void Advance() { Register += AdvanceOffset; }
	FORCEINLINE const T GetAndAdvance()
	{
		T* Ret = Register;
		Register += AdvanceOffset;
		return *Ret;
	}
	FORCEINLINE T* GetDestAndAdvance()
	{
		T* Ret = Register;
		Register += AdvanceOffset;
		return Ret;
	}
};

