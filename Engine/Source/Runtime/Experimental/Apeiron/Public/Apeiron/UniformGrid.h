// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Vector.h"

namespace Apeiron
{
template<class T, int d>
class TArrayND;
template<class T, int d>
class TArrayFaceND;

template<class T, int d>
class APEIRON_API TUniformGridBase
{
  protected:
	TUniformGridBase() {}
	TUniformGridBase(const TVector<T, d>& MinCorner, const TVector<T, d>& MaxCorner, const TVector<int32, d>& Cells, const uint32 GhostCells)
	    : MMinCorner(MinCorner), MMaxCorner(MaxCorner), MCells(Cells)
	{
		MDx = TVector<T, d>(MMaxCorner - MMinCorner) / MCells;
		if (GhostCells > 0)
		{
			MMinCorner -= MDx * GhostCells;
			MMaxCorner += MDx * GhostCells;
			MCells += 2 * GhostCells;
		}
	}
	TUniformGridBase(std::istream& Stream)
	    : MMinCorner(Stream), MMaxCorner(Stream), MCells(Stream)
	{
		MDx = TVector<T, d>(MMaxCorner - MMinCorner) / MCells;
	}

	~TUniformGridBase() {}

  public:
	void Write(std::ostream& Stream) const
	{
		MMinCorner.Write(Stream);
		MMaxCorner.Write(Stream);
		MCells.Write(Stream);
	}
	TVector<T, d> Location(const TVector<int32, d>& Cell) const
	{
		return MDx * Cell + MMinCorner + (MDx / 2);
	}
	TVector<T, d> Location(const Pair<int32, TVector<int32, 3>>& Face) const
	{
		return MDx * Face.Second + MMinCorner + (TVector<T, d>(1) - TVector<T, d>::AxisVector(Face.First)) * (MDx / 2);
	}
	TVector<int32, d> Cell(const TVector<T, d>& X) const
	{
		return (X - MMinCorner) / MDx;
	}
	TVector<int32, d> Face(const TVector<T, d>& X, const int32 Component) const
	{
		return Cell(X + (MDx / 2) * TVector<T, d>::AxisVector(Component));
	}
	TVector<T, d> DomainSize() const
	{
		return (MMaxCorner - MMinCorner);
	}
	int32 GetNumCells() const
	{
		return MCells.Product();
	}
	template<class T_SCALAR>
	T_SCALAR LinearlyInterpolate(const TArrayND<T_SCALAR, d>& ScalarN, const TVector<T, d>& X) const;
	T LinearlyInterpolateComponent(const TArrayND<T, d>& ScalarNComponent, const TVector<T, d>& X, const int32 Axis) const;
	TVector<T, d> LinearlyInterpolate(const TArrayFaceND<T, d>& ScalarN, const TVector<T, d>& X) const;
	TVector<T, d> LinearlyInterpolate(const TArrayFaceND<T, d>& ScalarN, const TVector<T, d>& X, const Pair<int32, TVector<int32, d>> Index) const;
	const TVector<int32, d>& Counts() const { return MCells; }
	const TVector<T, d>& Dx() const { return MDx; }
	const TVector<T, d>& MinCorner() const { return MMinCorner; }
	const TVector<T, d>& MaxCorner() const { return MMaxCorner; }

  protected:
	TVector<T, d> MMinCorner;
	TVector<T, d> MMaxCorner;
	TVector<int32, d> MCells;
	TVector<T, d> MDx;
};

template<class T, int d>
class APEIRON_API TUniformGrid : public TUniformGridBase<T, d>
{
	using TUniformGridBase<T, d>::MCells;
	using TUniformGridBase<T, d>::MMinCorner;
	using TUniformGridBase<T, d>::MMaxCorner;
	using TUniformGridBase<T, d>::MDx;

  public:
	using TUniformGridBase<T, d>::Location;

	TUniformGrid(const TVector<T, d>& MinCorner, const TVector<T, d>& MaxCorner, const TVector<int32, d>& Cells, const uint32 GhostCells = 0)
	    : TUniformGridBase<T, d>(MinCorner, MaxCorner, Cells, GhostCells) {}
	TUniformGrid(std::istream& Stream)
	    : TUniformGridBase<T, d>(Stream) {}
	~TUniformGrid() {}
	TVector<int32, d> GetIndex(const int32 Index) const;
	TVector<T, d> Center(const int32 Index) const
	{
		return TUniformGridBase<T, d>::Location(GetIndex(Index));
	}
	TVector<int32, d> ClampIndex(const TVector<int32, d>& Index) const;
	TVector<T, d> Clamp(const TVector<T, d>& X) const;
	TVector<T, d> ClampMinusHalf(const TVector<T, d>& X) const;
};

template<class T>
class APEIRON_API TUniformGrid<T, 3> : public TUniformGridBase<T, 3>
{
	using TUniformGridBase<T, 3>::MCells;
	using TUniformGridBase<T, 3>::MMinCorner;
	using TUniformGridBase<T, 3>::MMaxCorner;
	using TUniformGridBase<T, 3>::MDx;

  public:
	using TUniformGridBase<T, 3>::GetNumCells;
	using TUniformGridBase<T, 3>::Location;

	TUniformGrid() {}
	TUniformGrid(const TVector<T, 3>& MinCorner, const TVector<T, 3>& MaxCorner, const TVector<int32, 3>& Cells, const uint32 GhostCells = 0)
	    : TUniformGridBase<T, 3>(MinCorner, MaxCorner, Cells, GhostCells) {}
	TUniformGrid(std::istream& Stream)
	    : TUniformGridBase<T, 3>(Stream) {}
	~TUniformGrid() {}
	TVector<int32, 3> GetIndex(const int32 Index) const;
	Pair<int32, TVector<int32, 3>> GetFaceIndex(int32 Index) const;
	int32 GetNumFaces() const
	{
		return GetNumCells() * 3 + MCells[0] * MCells[1] + MCells[1] * MCells[2] + MCells[0] * MCells[3];
	}
	TVector<T, 3> Center(const int32 Index) const
	{
		return TUniformGridBase<T, 3>::Location(GetIndex(Index));
	}
	TVector<int32, 3> ClampIndex(const TVector<int32, 3>& Index) const;
	TVector<T, 3> Clamp(const TVector<T, 3>& X) const;
	TVector<T, 3> ClampMinusHalf(const TVector<T, 3>& X) const;
};
}
