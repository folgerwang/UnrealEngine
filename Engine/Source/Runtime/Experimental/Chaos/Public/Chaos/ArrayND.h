// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/UniformGrid.h"
#include "Chaos/Vector.h"

namespace Chaos
{
template<class T_DERIVED, class T, int d>
class TArrayNDBase
{
  public:
	FORCEINLINE TArrayNDBase() {}
	FORCEINLINE TArrayNDBase(const TVector<int32, d>& Counts, const TArray<T>& Array)
	    : MCounts(Counts), MArray(Array) {}
	FORCEINLINE TArrayNDBase(const TArrayNDBase<T_DERIVED, T, d>& Other) = delete;
	FORCEINLINE TArrayNDBase(TArrayNDBase<T_DERIVED, T, d>&& Other)
	    : MCounts(Other.MCounts), MArray(MoveTemp(Other.MArray)) {}
	FORCEINLINE TArrayNDBase(std::istream& Stream)
	    : MCounts(Stream)
	{
		MArray.SetNum(MCounts[0] * MCounts[1] * MCounts[2]);
		Stream.read(reinterpret_cast<char*>(MArray.GetData()), sizeof(T) * MArray.Num());
	}
	FORCEINLINE void Write(std::ostream& Stream) const
	{
		MCounts.Write(Stream);
		Stream.write(reinterpret_cast<const char*>(MArray.GetData()), sizeof(T) * MArray.Num());
	}
	FORCEINLINE TArrayNDBase<T_DERIVED, T, d>& operator=(const TArrayNDBase<T_DERIVED, T, d>& Other) = delete;
	FORCEINLINE TArrayNDBase<T_DERIVED, T, d>& operator=(TArrayNDBase<T_DERIVED, T, d>&& Other)
	{
		MCounts = Other.MCounts;
		MArray = MoveTemp(Other.MArray);
		return *this;
	}
	FORCEINLINE T_DERIVED Copy() const { return T_DERIVED(MCounts, MArray); }
	FORCEINLINE void Fill(const T& Value)
	{
		for (auto& Elem : MArray)
		{
			Elem = Value;
		}
	}
	FORCEINLINE const T& operator[](const int32 i) const { return MArray[i]; }
	FORCEINLINE T& operator[](const int32 i) { return MArray[i]; }

  protected:
	TVector<int32, d> MCounts;
	TArray<T> MArray;
};

template<class T, int d>
class TArrayND : public TArrayNDBase<TArrayND<T, d>, T, d>
{
	typedef TArrayNDBase<TArrayND<T, d>, T, d> Base;
	using Base::MArray;
	using Base::MCounts;

  public:
	FORCEINLINE TArrayND(const TVector<int32, d>& Counts) { MArray.SetNum(Counts.Product()); }
	FORCEINLINE TArrayND(const TVector<int32, d>& Counts, const TArray<T>& Array)
	    : Base(Counts, Array) {}
	FORCEINLINE TArrayND(const TArrayND<T, d>& Other) = delete;
	FORCEINLINE TArrayND(TArrayND<T, d>&& Other)
	    : Base(MoveTemp(Other)) {}
	FORCEINLINE TArrayND(std::istream& Stream)
	    : Base(Stream) {}
	FORCEINLINE T& operator()(const TVector<int32, d>& Index)
	{
		int32 SingleIndex = 0;
		int32 count = 1;
		for (int32 i = d - 1; i >= 0; ++i)
		{
			SingleIndex += count * Index[i];
			count *= MCounts[i];
		}
		return MArray[SingleIndex];
	}
};

template<class T>
class TArrayND<T, 3> : public TArrayNDBase<TArrayND<T, 3>, T, 3>
{
	typedef TArrayNDBase<TArrayND<T, 3>, T, 3> Base;
	using Base::MArray;
	using Base::MCounts;

  public:
	  FORCEINLINE TArrayND() {}
	FORCEINLINE TArrayND(const TUniformGrid<float, 3>& grid)
	{
		MCounts = grid.Counts();
		MArray.SetNum(MCounts[0] * MCounts[1] * MCounts[2]);
	}
	FORCEINLINE TArrayND(const TVector<int32, 3>& Counts)
	{
		MCounts = Counts;
		MArray.SetNum(MCounts[0] * MCounts[1] * MCounts[2]);
	}
	FORCEINLINE TArrayND(const TVector<int32, 3>& Counts, const TArray<T>& Array)
	    : Base(Counts, Array) { check(Counts.Product() == Array.Num()); }
	FORCEINLINE TArrayND(const TArrayND<T, 3>& Other) = delete;
	FORCEINLINE TArrayND(TArrayND<T, 3>&& Other)
	    : Base(MoveTemp(Other)) {}
	FORCEINLINE TArrayND(std::istream& Stream)
	    : Base(Stream) {}
	FORCEINLINE TArrayND<T, 3>& operator=(TArrayND<T, 3>&& Other)
	{
		Base::operator=(MoveTemp(Other));
		return *this;
	}
	FORCEINLINE T& operator()(const TVector<int32, 3>& Index) { return (*this)(Index[0], Index[1], Index[2]); }
	FORCEINLINE const T& operator()(const TVector<int32, 3>& Index) const { return (*this)(Index[0], Index[1], Index[2]); }
	FORCEINLINE T& operator()(const int32& x, const int32& y, const int32& z)
	{
		return MArray[(x * MCounts[1] + y) * MCounts[2] + z];
	}
	FORCEINLINE const T& operator()(const int32& x, const int32& y, const int32& z) const
	{
		return MArray[(x * MCounts[1] + y) * MCounts[2] + z];
	}
};

#if COMPILE_WITHOUT_UNREAL_SUPPORT
template<>
class TArrayND<bool, 3> : public TArrayNDBase<TArrayND<bool, 3>, char, 3>
{
	typedef bool T;
	typedef TArrayNDBase<TArrayND<T, 3>, char, 3> Base;
	using Base::MArray;
	using Base::MCounts;

  public:
	FORCEINLINE TArrayND() {}
	FORCEINLINE TArrayND(const TUniformGrid<float, 3>& grid)
	{
		MCounts = grid.Counts();
		MArray.SetNum(MCounts[0] * MCounts[1] * MCounts[2]);
	}
	FORCEINLINE TArrayND(const Vector<int32, 3>& Counts)
	{
		MCounts = Counts;
		MArray.SetNum(MCounts[0] * MCounts[1] * MCounts[2]);
	}
	FORCEINLINE TArrayND(const Vector<int32, 3>& Counts, const TArray<char>& Array)
	    : Base(Counts, Array) {}
	FORCEINLINE TArrayND(const TArrayND<T, 3>& Other) = delete;
	FORCEINLINE TArrayND(TArrayND<T, 3>&& Other)
	    : Base(std::move(Other)) {}
	FORCEINLINE char& operator()(const Vector<int32, 3>& Index) { return (*this)(Index[0], Index[1], Index[2]); }
	FORCEINLINE const T& operator()(const Vector<int32, 3>& Index) const { return (*this)(Index[0], Index[1], Index[2]); }
	FORCEINLINE char& operator()(const int32& x, const int32& y, const int32& z)
	{
		return MArray[(x * MCounts[1] + y) * MCounts[2] + z];
	}
	FORCEINLINE const T& operator()(const int32& x, const int32& y, const int32& z) const
	{
		return MArray[(x * MCounts[1] + y) * MCounts[2] + z];
	}
};
#endif
}
