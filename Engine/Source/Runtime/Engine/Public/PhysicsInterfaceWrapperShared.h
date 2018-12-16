// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

enum class EQueryFlags : uint16
{
	None = 0,
	PreFilter = (1 << 2),
	PostFilter = (1 << 3),
	AnyHit = (1 << 4)
};

inline EQueryFlags operator| (EQueryFlags lhs, EQueryFlags rhs)
{
	return static_cast<EQueryFlags>(static_cast<uint16>(lhs) | static_cast<uint16>(rhs));
}

inline EQueryFlags operator&(EQueryFlags lhs, EQueryFlags rhs)
{
	return static_cast<EQueryFlags>(static_cast<uint16>(lhs) & static_cast<uint16>(rhs));
}

struct FQueryFlags
{
	FQueryFlags(EQueryFlags InFlags) : QueryFlags(InFlags) {}
	operator bool() const { return !!static_cast<uint16>(QueryFlags); }
	FQueryFlags operator |(EQueryFlags Rhs) const
	{
		return FQueryFlags(QueryFlags | Rhs);
	}

	FQueryFlags& operator |=(EQueryFlags Rhs)
	{
		*this = *this | Rhs;
		return *this;
	}

	FQueryFlags operator &(EQueryFlags Rhs) const
	{
		return FQueryFlags(QueryFlags & Rhs);
	}

	FQueryFlags& operator &=(EQueryFlags Rhs)
	{
		*this = *this & Rhs;
		return *this;
	}

	EQueryFlags QueryFlags;
};

/** Possible results from a scene query */
enum class EHitFlags : uint16
{
	None = 0,
	Position = (1 << 0),
	Normal = (1 << 1),
	Distance = (1 << 2),
	UV = (1 << 3),
	MTD = (1 << 9),
	FaceIndex = (1 << 10)
};

inline EHitFlags operator|(EHitFlags lhs, EHitFlags rhs)
{
	return static_cast<EHitFlags>(static_cast<uint16>(lhs) | static_cast<uint16>(rhs));
}

inline EHitFlags operator&(EHitFlags lhs, EHitFlags rhs)
{
	return static_cast<EHitFlags>(static_cast<uint16>(lhs) & static_cast<uint16>(rhs));
}

struct FHitFlags
{
	FHitFlags(EHitFlags InFlags) : HitFlags(InFlags) {}
	operator bool() const { return !!static_cast<uint16>(HitFlags); }
	FHitFlags operator |(EHitFlags Rhs) const
	{
		return FHitFlags(HitFlags | Rhs);
	}

	FHitFlags& operator |=(EHitFlags Rhs)
	{
		*this = *this | Rhs;
		return *this;
	}

	FHitFlags operator &(EHitFlags Rhs) const
	{
		return FHitFlags(HitFlags & Rhs);
	}

	FHitFlags& operator &=(EHitFlags Rhs)
	{
		*this = *this & Rhs;
		return *this;
	}

	EHitFlags HitFlags;
};
