// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/UniformGrid.h"
#include "Apeiron/Vector.h"

namespace Apeiron
{
template<class T_DERIVED, class T, int d>
class TArrayNDBase
{
  public:
	TArrayNDBase() {}
	TArrayNDBase(const TVector<int32, d>& Counts, const TArray<T>& Array)
	    : MCounts(Counts), MArray(Array) {}
	TArrayNDBase(const TArrayNDBase<T_DERIVED, T, d>& Other) = delete;
	TArrayNDBase(TArrayNDBase<T_DERIVED, T, d>&& Other)
	    : MCounts(Other.MCounts), MArray(MoveTemp(Other.MArray)) {}
	TArrayNDBase(std::istream& Stream)
	    : MCounts(Stream)
	{
		MArray.SetNum(MCounts[0] * MCounts[1] * MCounts[2]);
		Stream.read(reinterpret_cast<char*>(MArray.GetData()), sizeof(T) * MArray.Num());
	}
	void Write(std::ostream& Stream) const
	{
		MCounts.Write(Stream);
		Stream.write(reinterpret_cast<const char*>(MArray.GetData()), sizeof(T) * MArray.Num());
	}
	TArrayNDBase<T_DERIVED, T, d>& operator=(const TArrayNDBase<T_DERIVED, T, d>& Other) = delete;
	TArrayNDBase<T_DERIVED, T, d>& operator=(TArrayNDBase<T_DERIVED, T, d>&& Other)
	{
		MCounts = Other.MCounts;
		MArray = MoveTemp(Other.MArray);
		return *this;
	}
	T_DERIVED Copy() const { return T_DERIVED(MCounts, MArray); }
	void Fill(const T& Value)
	{
		for (auto& Elem : MArray)
		{
			Elem = Value;
		}
	}
	const T& operator[](const int32 i) const { return MArray[i]; }
	T& operator[](const int32 i) { return MArray[i]; }

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
	TArrayND(const TVector<int32, d>& Counts) { MArray.SetNum(Counts.Product()); }
	TArrayND(const TVector<int32, d>& Counts, const TArray<T>& Array)
	    : Base(Counts, Array) {}
	TArrayND(const TArrayND<T, d>& Other) = delete;
	TArrayND(TArrayND<T, d>&& Other)
	    : Base(MoveTemp(Other)) {}
	TArrayND(std::istream& Stream)
	    : Base(Stream) {}
	T& operator()(const TVector<int32, d>& Index)
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
	TArrayND() {}
	TArrayND(const TUniformGrid<float, 3>& grid)
	{
		MCounts = grid.Counts();
		MArray.SetNum(MCounts[0] * MCounts[1] * MCounts[2]);
	}
	TArrayND(const TVector<int32, 3>& Counts)
	{
		MCounts = Counts;
		MArray.SetNum(MCounts[0] * MCounts[1] * MCounts[2]);
	}
	TArrayND(const TVector<int32, 3>& Counts, const TArray<T>& Array)
	    : Base(Counts, Array) { check(Counts.Product() == Array.Num()); }
	TArrayND(const TArrayND<T, 3>& Other) = delete;
	TArrayND(TArrayND<T, 3>&& Other)
	    : Base(MoveTemp(Other)) {}
	TArrayND(std::istream& Stream)
	    : Base(Stream) {}
	TArrayND<T, 3>& operator=(TArrayND<T, 3>&& Other)
	{
		Base::operator=(MoveTemp(Other));
		return *this;
	}
	T& operator()(const TVector<int32, 3>& Index) { return (*this)(Index[0], Index[1], Index[2]); }
	const T& operator()(const TVector<int32, 3>& Index) const { return (*this)(Index[0], Index[1], Index[2]); }
	T& operator()(const int32& x, const int32& y, const int32& z)
	{
		return MArray[(x * MCounts[1] + y) * MCounts[2] + z];
	}
	const T& operator()(const int32& x, const int32& y, const int32& z) const
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
	TArrayND() {}
	TArrayND(const TUniformGrid<float, 3>& grid)
	{
		MCounts = grid.Counts();
		MArray.SetNum(MCounts[0] * MCounts[1] * MCounts[2]);
	}
	TArrayND(const Vector<int32, 3>& Counts)
	{
		MCounts = Counts;
		MArray.SetNum(MCounts[0] * MCounts[1] * MCounts[2]);
	}
	TArrayND(const Vector<int32, 3>& Counts, const TArray<char>& Array)
	    : Base(Counts, Array) {}
	TArrayND(const TArrayND<T, 3>& Other) = delete;
	TArrayND(TArrayND<T, 3>&& Other)
	    : Base(std::move(Other)) {}
	char& operator()(const Vector<int32, 3>& Index) { return (*this)(Index[0], Index[1], Index[2]); }
	const T& operator()(const Vector<int32, 3>& Index) const { return (*this)(Index[0], Index[1], Index[2]); }
	char& operator()(const int32& x, const int32& y, const int32& z)
	{
		return MArray[(x * MCounts[1] + y) * MCounts[2] + z];
	}
	const T& operator()(const int32& x, const int32& y, const int32& z) const
	{
		return MArray[(x * MCounts[1] + y) * MCounts[2] + z];
	}
};
#endif
}
