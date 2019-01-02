// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MeshElement.h"
#include "EditableMesh.h"

FMeshElement::FMeshElement() : Component(nullptr),
ElementAddress(),
LastHoverTime(0.0),
LastSelectTime(0.0)
{

}

FMeshElement::FMeshElement(UPrimitiveComponent* InComponent, const FEditableMeshSubMeshAddress& InSubMeshAddress, FVertexID InVertexID, double InLastHoverTime /*= 0.0*/, double InLastSelectTime /*= 0.0 */) : Component(InComponent),
ElementAddress(InSubMeshAddress, InVertexID),
LastHoverTime(InLastHoverTime),
LastSelectTime(InLastSelectTime)
{

}

FMeshElement::FMeshElement(UPrimitiveComponent* InComponent, const FEditableMeshSubMeshAddress& InSubMeshAddress, FEdgeID InEdgeID, double InLastHoverTime /*= 0.0*/, double InLastSelectTime /*= 0.0 */) : Component(InComponent),
ElementAddress(InSubMeshAddress, InEdgeID),
LastHoverTime(InLastHoverTime),
LastSelectTime(InLastSelectTime)
{

}

FMeshElement::FMeshElement(UPrimitiveComponent* InComponent, const FEditableMeshSubMeshAddress& InSubMeshAddress, FPolygonID InPolygonID, double InLastHoverTime /*= 0.0*/, double InLastSelectTime /*= 0.0 */) : Component(InComponent),
ElementAddress(InSubMeshAddress, InPolygonID),
LastHoverTime(InLastHoverTime),
LastSelectTime(InLastSelectTime)
{

}

bool FMeshElement::IsValidMeshElement() const
{
	return
		(Component.IsValid() &&
			ElementAddress.SubMeshAddress.EditableMeshFormat != nullptr &&
			ElementAddress.ElementType != EEditableMeshElementType::Invalid);
}

bool FMeshElement::IsSameMeshElement(const FMeshElement& Other) const
{
	// NOTE: We only care that the element addresses are the same, not other transient state
	return Component == Other.Component && ElementAddress == Other.ElementAddress;
}

FString FMeshElement::ToString() const
{
	return FString::Printf(
		TEXT("Component:%s, %s"),
		Component.IsValid() ? *Component->GetName() : TEXT("<Invalid>"),
		*ElementAddress.ToString());
}

bool FMeshElement::IsElementIDValid(const UEditableMesh* EditableMesh) const
{
	bool bIsValid = false;

	if (EditableMesh != nullptr && ElementAddress.ElementID != FElementID::Invalid)
	{
		switch (ElementAddress.ElementType)
		{
		case EEditableMeshElementType::Vertex:
			bIsValid = EditableMesh->IsValidVertex(FVertexID(ElementAddress.ElementID));
			break;

		case EEditableMeshElementType::Edge:
			bIsValid = EditableMesh->IsValidEdge(FEdgeID(ElementAddress.ElementID));
			break;

		case EEditableMeshElementType::Polygon:
			bIsValid = EditableMesh->IsValidPolygon(FPolygonID(ElementAddress.ElementID));
			break;
		}
	}

	return bIsValid;
}
