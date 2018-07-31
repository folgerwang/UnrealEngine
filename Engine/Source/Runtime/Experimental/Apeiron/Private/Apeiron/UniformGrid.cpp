// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "Apeiron/UniformGrid.h"

#include "Apeiron/ArrayFaceND.h"
#include "Apeiron/ArrayND.h"

using namespace Apeiron;

template<class T_SCALAR, class T>
T_SCALAR LinearlyInterpolate1D(const T_SCALAR& Prev, const T_SCALAR& Next, const T Alpha)
{
	return Next * Alpha + Prev * (1 - Alpha);
}

template<class T_SCALAR, class T, int d>
T_SCALAR LinearlyInterpolateHelper(const TArrayND<T_SCALAR, d>& ScalarN, const TVector<int32, d>& CellPrev, const TVector<T, d>& Alpha)
{
	check(false);
}

template<class T_SCALAR, class T>
T_SCALAR LinearlyInterpolateHelper(const TArrayND<T_SCALAR, 2>& ScalarN, const TVector<int32, 2>& CellPrev, const TVector<T, 2>& Alpha)
{
	const T_SCALAR interpx1 = LinearlyInterpolate1D(ScalarN(CellPrev), ScalarN(CellPrev + TVector<int32, 2>({1, 0})), Alpha[0]);
	const T_SCALAR interpx2 = LinearlyInterpolate1D(ScalarN(CellPrev + TVector<int32, 2>({0, 1})), ScalarN(CellPrev + TVector<int32, 2>({1, 1})), Alpha[0]);
	return LinearlyInterpolate1D(interpx1, interpx2, Alpha[1]);
}

template<class T_SCALAR, class T>
T_SCALAR LinearlyInterpolateHelper(const TArrayND<T_SCALAR, 3>& ScalarN, const TVector<int32, 3>& CellPrev, const TVector<T, 3>& Alpha)
{
	const T_SCALAR interpx1 = LinearlyInterpolate1D(ScalarN(CellPrev), ScalarN(CellPrev + TVector<int32, 3>({1, 0, 0})), Alpha[0]);
	const T_SCALAR interpx2 = LinearlyInterpolate1D(ScalarN(CellPrev + TVector<int32, 3>({0, 1, 0})), ScalarN(CellPrev + TVector<int32, 3>({1, 1, 0})), Alpha[0]);
	const T_SCALAR interpx3 = LinearlyInterpolate1D(ScalarN(CellPrev + TVector<int32, 3>({0, 0, 1})), ScalarN(CellPrev + TVector<int32, 3>({1, 0, 1})), Alpha[0]);
	const T_SCALAR interpx4 = LinearlyInterpolate1D(ScalarN(CellPrev + TVector<int32, 3>({0, 1, 1})), ScalarN(CellPrev + TVector<int32, 3>({1, 1, 1})), Alpha[0]);
	const T_SCALAR interpy1 = LinearlyInterpolate1D(interpx1, interpx2, Alpha[1]);
	const T_SCALAR interpy2 = LinearlyInterpolate1D(interpx3, interpx4, Alpha[1]);
	return LinearlyInterpolate1D(interpy1, interpy2, Alpha[2]);
}

template<class T, int d>
template<class T_SCALAR>
T_SCALAR TUniformGridBase<T, d>::LinearlyInterpolate(const TArrayND<T_SCALAR, d>& ScalarN, const TVector<T, d>& X) const
{
	TVector<int32, d> XCell = Cell(X);
	TVector<T, d> XCenter = Location(XCell);
	TVector<int32, d> CellPrev;
	for (int32 i = 0; i < d; ++i)
	{
		CellPrev[i] = X[i] > XCenter[i] ? XCell[i] : XCell[i] - 1;
	}
	TVector<T, d> Alpha = (X - Location(CellPrev)) / MDx;
	// Clamp correctly when on boarder
	for (int32 i = 0; i < d; ++i)
	{
		if (CellPrev[i] == -1)
		{
			CellPrev[i] = 0;
			Alpha[i] = 0;
		}
		if (CellPrev[i] == Counts()[i] - 1)
		{
			CellPrev[i] = Counts()[i] - 2;
			Alpha[i] = 1;
		}
	}
	return LinearlyInterpolateHelper(ScalarN, CellPrev, Alpha);
}

template<class T, int d>
T TUniformGridBase<T, d>::LinearlyInterpolateComponent(const TArrayND<T, d>& ScalarNComponent, const TVector<T, d>& X, const int32 Axis) const
{
	TVector<int32, d> CellCounts = Counts() + TVector<int32, d>::AxisVector(Axis);
	TVector<int32, d> FaceIndex = Face(X, Axis);
	TVector<T, d> XCenter = Location(MakePair(Axis, FaceIndex));
	TVector<int32, d> FacePrev;
	for (int32 i = 0; i < d; ++i)
	{
		FacePrev[i] = X[i] > XCenter[i] ? FaceIndex[i] : FaceIndex[i] - 1;
	}
	TVector<T, d> Alpha = (X - Location(MakePair(Axis, FacePrev))) / MDx;
	// Clamp correctly when on boarder
	for (int32 i = 0; i < d; ++i)
	{
		if (FacePrev[i] == -1)
		{
			FacePrev[i] = 0;
			Alpha[i] = 0;
		}
		if (FacePrev[i] == CellCounts[i] - 1)
		{
			FacePrev[i] = CellCounts[i] - 2;
			Alpha[i] = 1;
		}
	}
	return LinearlyInterpolateHelper(ScalarNComponent, FacePrev, Alpha);
}

template<class T, int d>
TVector<T, d> TUniformGridBase<T, d>::LinearlyInterpolate(const TArrayFaceND<T, d>& ScalarN, const TVector<T, d>& X) const
{
	TVector<T, d> Result;
	for (int32 i = 0; i < d; ++i)
	{
		Result[i] = LinearlyInterpolateComponent(ScalarN.GetComponent(i), X, i);
	}
	return Result;
}

template<class T, int d>
TVector<T, d> TUniformGridBase<T, d>::LinearlyInterpolate(const TArrayFaceND<T, d>& ScalarN, const TVector<T, d>& X, const Pair<int32, TVector<int32, d>> Index) const
{
	TVector<T, d> Result;
	for (int32 i = 0; i < d; ++i)
	{
		if (i == Index.First)
		{
			Result[i] = ScalarN(Index);
		}
		else
		{
			Result[i] = LinearlyInterpolateComponent(ScalarN.GetComponent(i), X, Index.First);
		}
	}
	return Result;
}

template<class T, int d>
TVector<int32, d> TUniformGrid<T, d>::GetIndex(const int32 Index) const
{
	TVector<int32, d> NDIndex;
	int32 product = 1, Remainder = Index;
	for (int32 i = 0; i < d; ++i)
	{
		product *= MCells[i];
	}
	for (int32 i = 0; i < d; ++i)
	{
		product /= MCells[i];
		NDIndex[i] = Remainder / product;
		Remainder -= NDIndex[i] * product;
	}
	return NDIndex;
}

template<class T, int d>
TVector<int32, d> TUniformGrid<T, d>::ClampIndex(const TVector<int32, d>& Index) const
{
	TVector<int32, d> Result;
	for (int32 i = 0; i < d; ++i)
	{
		if (Index[i] >= MCells[i])
			Result[i] = MCells[i] - 1;
		else if (Index[i] < 0)
			Result[i] = 0;
		else
			Result[i] = Index[i];
	}
	return Result;
}

template<class T, int d>
TVector<T, d> TUniformGrid<T, d>::Clamp(const TVector<T, d>& X) const
{
	TVector<T, d> Result;
	for (int32 i = 0; i < d; ++i)
	{
		if (X[i] > MMaxCorner[i])
			Result[i] = MMaxCorner[i];
		else if (X[i] < MMinCorner[i])
			Result[i] = MMinCorner[i];
		else
			Result[i] = X[i];
	}
	return Result;
}

template<class T, int d>
TVector<T, d> TUniformGrid<T, d>::ClampMinusHalf(const TVector<T, d>& X) const
{
	TVector<T, d> Result;
	TVector<T, d> Max = MMaxCorner - MDx * 0.5;
	TVector<T, d> Min = MMinCorner + MDx * 0.5;
	for (int32 i = 0; i < d; ++i)
	{
		if (X[i] > Max[i])
			Result[i] = Max[i];
		else if (X[i] < Min[i])
			Result[i] = Min[i];
		else
			Result[i] = X[i];
	}
	return Result;
}

template<class T>
TVector<int32, 3> TUniformGrid<T, 3>::GetIndex(const int32 Index) const
{
	int32 Remainder;
	TVector<int32, 3> NDIndex;
	NDIndex[0] = Index / (MCells[1] * MCells[2]);
	Remainder = Index - NDIndex[0] * MCells[1] * MCells[2];
	NDIndex[1] = Remainder / MCells[2];
	Remainder = Remainder - NDIndex[1] * MCells[2];
	NDIndex[2] = Remainder;
	return NDIndex;
}

template<class T>
Pair<int32, TVector<int32, 3>> TUniformGrid<T, 3>::GetFaceIndex(int32 Index) const
{
	int32 Remainder;
	int32 NumXFaces = (MCells + TVector<int32, 3>::AxisVector(0)).Product();
	int32 NumYFaces = (MCells + TVector<int32, 3>::AxisVector(1)).Product();
	int32 Axis = 0;
	if (Index > NumXFaces)
	{
		Axis = 1;
		Index -= NumXFaces;
		if (Index > NumYFaces)
		{
			Axis = 2;
			Index -= NumYFaces;
		}
	}
	TVector<int32, 3> Faces = MCells + TVector<int32, 3>::AxisVector(Axis);
	TVector<int32, 3> NDIndex;
	NDIndex[0] = Index / (Faces[1] * Faces[2]);
	Remainder = Index - NDIndex[0] * Faces[1] * Faces[2];
	NDIndex[1] = Remainder / Faces[2];
	Remainder = Remainder - NDIndex[1] * Faces[2];
	NDIndex[2] = Remainder;
	return MakePair(Axis, NDIndex);
}

template<class T>
TVector<int32, 3> TUniformGrid<T, 3>::ClampIndex(const TVector<int32, 3>& Index) const
{
	TVector<int32, 3> Result;
	Result[0] = Index[0] >= MCells[0] ? MCells[0] - 1 : (Index[0] < 0 ? 0 : Index[0]);
	Result[1] = Index[1] >= MCells[1] ? MCells[1] - 1 : (Index[1] < 0 ? 0 : Index[1]);
	Result[2] = Index[2] >= MCells[2] ? MCells[2] - 1 : (Index[2] < 0 ? 0 : Index[2]);
	return Result;
}

template<class T>
TVector<T, 3> TUniformGrid<T, 3>::Clamp(const TVector<T, 3>& X) const
{
	TVector<T, 3> Result;
	Result[0] = X[0] > MMaxCorner[0] ? MMaxCorner[0] : (X[0] < MMinCorner[0] ? MMinCorner[0] : X[0]);
	Result[1] = X[1] > MMaxCorner[1] ? MMaxCorner[1] : (X[1] < MMinCorner[1] ? MMinCorner[1] : X[1]);
	Result[2] = X[2] > MMaxCorner[2] ? MMaxCorner[2] : (X[2] < MMinCorner[2] ? MMinCorner[2] : X[2]);
	return Result;
}

template<class T>
TVector<T, 3> TUniformGrid<T, 3>::ClampMinusHalf(const TVector<T, 3>& X) const
{
	TVector<T, 3> Result;
	TVector<T, 3> Max = MMaxCorner - MDx * 0.5;
	TVector<T, 3> Min = MMinCorner + MDx * 0.5;
	Result[0] = X[0] > Max[0] ? Max[0] : (X[0] < Min[0] ? Min[0] : X[0]);
	Result[1] = X[1] > Max[1] ? Max[1] : (X[1] < Min[1] ? Min[1] : X[1]);
	Result[2] = X[2] > Max[2] ? Max[2] : (X[2] < Min[2] ? Min[2] : X[2]);
	return Result;
}

template class Apeiron::TUniformGridBase<float, 3>;
template class Apeiron::TUniformGrid<float, 3>;
template TVector<float, 3> Apeiron::TUniformGridBase<float, 3>::LinearlyInterpolate<TVector<float, 3>>(const TArrayND<TVector<float, 3>, 3>&, const TVector<float, 3>&) const;
template APEIRON_API float Apeiron::TUniformGridBase<float, 3>::LinearlyInterpolate<float>(const TArrayND<float, 3>&, const TVector<float, 3>&) const;