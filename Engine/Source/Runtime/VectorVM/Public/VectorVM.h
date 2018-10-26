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

	//Temporary lock we're using for thread safety when writing to the FreeIDTable.
	//TODO: A lock free algorithm is possible here. We can create a specialized lock free list and reuse the IDTable slots for FreeIndices as Next pointers for our LFL.
	//This would also work well on the GPU. 
	//UE-65856 for tracking this work.
	FCriticalSection FreeTableLock;

	FORCEINLINE void LockFreeTable();
	FORCEINLINE void UnlockFreeTable();

	FDataSetMeta(uint32 DataSetSize, uint8 **Data, uint8 InNumVariables, int32 InInstanceOffset, TArray<int32>* InIDTable, TArray<int32>* InFreeIDTable, int32* InNumFreeIDs, int32* InMaxUsedID, int32 InIDAcquireTag)
		: InputRegisters(Data), NumVariables(InNumVariables), DataSetSizeInBytes(DataSetSize), DataSetAccessIndex(INDEX_NONE), DataSetOffset(0), InstanceOffset(InInstanceOffset)
		, IDTable(InIDTable), FreeIDTable(InFreeIDTable), NumFreeIDs(InNumFreeIDs), MaxUsedID(InMaxUsedID), IDAcquireTag(InIDAcquireTag)
	{
	}

	FDataSetMeta() 
		: InputRegisters(nullptr), NumVariables(0), DataSetSizeInBytes(0), DataSetAccessIndex(INDEX_NONE), DataSetOffset(0), InstanceOffset(0)
		, IDTable(nullptr), FreeIDTable(nullptr), NumFreeIDs(nullptr), MaxUsedID(nullptr), IDAcquireTag(0)
	{}

private:
	// Non-copyable and non-movable
	FDataSetMeta(FDataSetMeta&&) = delete;
	FDataSetMeta(const FDataSetMeta&) = delete;
	FDataSetMeta& operator=(FDataSetMeta&&) = delete;
	FDataSetMeta& operator=(const FDataSetMeta&) = delete;
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
}

//Data the VM will keep on each dataset locally per thread which is then thread safely pushed to it's destination at the end of execution.
struct FDataSetThreadLocalTempData
{
	FDataSetThreadLocalTempData()
		:MaxID(INDEX_NONE)
	{}

	TArray<int32> IDsToFree;
	int32 MaxID;

	//TODO: Possibly store output data locally and memcpy to the real buffers. Could avoid false sharing in parallel execution and so improve perf.
	//using _mm_stream_ps on platforms that support could also work for this?
	//TArray<TArray<float>> OutputFloatData;
	//TArray<TArray<int32>> OutputIntData;
};

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
	TArray<FDataSetMeta>* RESTRICT DataSetMetaTable;

	TArray<FDataSetThreadLocalTempData> ThreadLocalTempData;

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
		TArray<FDataSetMeta>& RESTRICT InDataSetMetaTable
#if STATS
		, const TArray<TStatId>* InStatScopes
#endif
	);

	void FinishExec();

	void PrepareForChunk(const uint8* InCode, int32 InNumInstances, int32 InStartInstance)
	{
		Code = InCode;
		NumInstances = InNumInstances;
		StartInstance = InStartInstance;
	}
};

namespace VectorVM
{
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

#define VVM_EXT_FUNC_INPUT_LOC_BIT (unsigned short)(1<<15)
#define VVM_EXT_FUNC_INPUT_LOC_MASK (unsigned short)~VVM_EXT_FUNC_INPUT_LOC_BIT

	template<typename T>
	struct FUserPtrHandler
	{
		int32 UserPtrIdx;
		T* Ptr;
		FUserPtrHandler(FVectorVMContext& Context)
			: UserPtrIdx(*(int32*)(Context.ConstantTable + (DecodeU16(Context))))
			, Ptr((T*)Context.UserPtrTable[UserPtrIdx])
		{
			check(UserPtrIdx != INDEX_NONE);
		}
		FORCEINLINE T* Get() { return Ptr; }
		FORCEINLINE T* operator->() { return Ptr; }
		FORCEINLINE operator T*() { return Ptr; }
	};

	// A flexible handler that can deal with either constant or register inputs.
	template<typename T>
	struct FExternalFuncInputHandler
	{
	private:
		/** Either byte offset into constant table or offset into register table deepening on VVM_INPUT_LOCATION_BIT */
		int32 InputOffset;
		T* RESTRICT InputPtr;
		int32 AdvanceOffset;

	public:
		FORCEINLINE FExternalFuncInputHandler(FVectorVMContext& Context)
			: InputOffset(DecodeU16(Context))
			, InputPtr(IsConstant() ? (T*)(Context.ConstantTable + GetOffset()) : (T*)Context.RegisterTable[GetOffset()])
			, AdvanceOffset(IsConstant() ? 0 : 1)
		{}

		FORCEINLINE bool IsConstant()const { return !IsRegister(); }
		FORCEINLINE bool IsRegister()const { return (InputOffset & VVM_EXT_FUNC_INPUT_LOC_BIT) != 0; }
		FORCEINLINE int32 GetOffset()const { return InputOffset & VVM_EXT_FUNC_INPUT_LOC_MASK; }

		FORCEINLINE const T Get() { return *InputPtr; }
		FORCEINLINE T* GetDest() { return InputPtr; }
		FORCEINLINE void Advance() { InputPtr += AdvanceOffset; }
		FORCEINLINE const T GetAndAdvance()
		{
			T* Ret = InputPtr;
			InputPtr += AdvanceOffset;
			return *Ret;
		}
		FORCEINLINE T* GetDestAndAdvance()
		{
			T* Ret = InputPtr;
			InputPtr += AdvanceOffset;
			return Ret;
		}
	};
	
	template<typename T>
	struct FExternalFuncRegisterHandler
	{
	private:
		int32 RegisterIndex;
		int32 AdvanceOffset;
		T Dummy;
		T* RESTRICT Register;
	public:
		FORCEINLINE FExternalFuncRegisterHandler(FVectorVMContext& Context)
			: RegisterIndex(DecodeU16(Context) & VVM_EXT_FUNC_INPUT_LOC_MASK)
			, AdvanceOffset(IsValid() ? 1 : 0)
		{
			if (IsValid())
			{
				check(RegisterIndex < VectorVM::MaxRegisters);
				Register = (T*)Context.RegisterTable[RegisterIndex];
			}
			else
			{
				Register = &Dummy;
			}
		}

		FORCEINLINE bool IsValid() const { return RegisterIndex != (uint16)VVM_EXT_FUNC_INPUT_LOC_MASK; }

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

	template<typename T>
	struct FExternalFuncConstHandler
	{
		uint16 ConstantIndex;
		T Constant;
		FExternalFuncConstHandler(FVectorVMContext& Context)
			: ConstantIndex(VectorVM::DecodeU16(Context) & VVM_EXT_FUNC_INPUT_LOC_MASK)
			, Constant(*((T*)(Context.ConstantTable + ConstantIndex)))
		{}
		FORCEINLINE const T& Get() { return Constant; }
		FORCEINLINE const T& GetAndAdvance() { return Constant; }
		FORCEINLINE void Advance() { }
	};
} // namespace VectorVM



