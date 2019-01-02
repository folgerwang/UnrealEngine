// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Field/FieldSystem.h"
#include "Field/FieldSystemNodes.h"

DEFINE_LOG_CATEGORY_STATIC(FSC_Log, NoLogging, All);

template<> FFieldNodeBase::EFieldType FFieldNode<int32>::StaticType() { return EFieldType::EField_Int32; }
template<> FFieldNodeBase::EFieldType FFieldNode<float>::StaticType() { return EFieldType::EField_Float; }
template<> FFieldNodeBase::EFieldType FFieldNode<FVector>::StaticType() { return EFieldType::EField_FVector; }

int32 FFieldNodeBase::Invalid = -1;


FFieldSystem::~FFieldSystem()
{
	for (int32 Index = 0; Index < Nodes.Num(); Index++)
	{
		delete Nodes[Index];
	}
}

template<class NODE_T> NODE_T & FFieldSystem::NewNode(const FName & Name)
{
	NODE_T * Node = new NODE_T(Name);
	Node->SetTerminalID(Nodes.Num());
	Node->SetFieldSystem(this);
	Nodes.Add(Node);
	return *Node;
}

template FIELDSYSTEMCORE_API FRadialIntMask& FFieldSystem::NewNode<FRadialIntMask>(const FName &);
template FIELDSYSTEMCORE_API FRadialFalloff& FFieldSystem::NewNode<FRadialFalloff>(const FName &);
template FIELDSYSTEMCORE_API FUniformVector& FFieldSystem::NewNode<FUniformVector>(const FName &);
template FIELDSYSTEMCORE_API FRadialVector& FFieldSystem::NewNode<FRadialVector>(const FName &);
template FIELDSYSTEMCORE_API FSumVector& FFieldSystem::NewNode<FSumVector>(const FName &);
template FIELDSYSTEMCORE_API FSumScalar& FFieldSystem::NewNode<FSumScalar>(const FName &);

int32 FFieldSystem::TerminalIndex(const FName & FieldTerminalName) const
{
	for (int32 Index = 0; Index < Nodes.Num(); Index++)
	{
		if (Nodes[Index] && Nodes[Index]->GetName().IsEqual(FieldTerminalName) )
		{
			return Index;
		}
	}
	return FFieldNodeBase::Invalid;
}

void FFieldSystem::BuildFrom(const FFieldSystem& Other)
{
	Nodes.Reset(Other.Nodes.Num());
	for(const FFieldNodeBase* Node : Other.Nodes)
	{
		Nodes.Add(Node->Clone());
		Nodes.Last()->SetFieldSystem(this);
	}
}

template<class T>
void FFieldSystem::Evaluate(const FFieldContext & Context, TArrayView<T> & Results) const
{
	ensure(Context.Terminal < Nodes.Num());
	ensure(Context.SampleIndices.Num() == Results.Num());
	ensure(FFieldNode<T>::StaticType() == Nodes[Context.Terminal]->Type());

	static_cast<FFieldNode<T>*>(Nodes[Context.Terminal])->Evaluate(Context, Results);
}

template FIELDSYSTEMCORE_API void FFieldSystem::Evaluate<int32>(const FFieldContext &, TArrayView<int32> & Results) const;
template FIELDSYSTEMCORE_API void FFieldSystem::Evaluate<float>(const FFieldContext &, TArrayView<float> & Results) const;
template FIELDSYSTEMCORE_API void FFieldSystem::Evaluate<FVector>(const FFieldContext &, TArrayView<FVector> & Results) const;

UFieldSystem::UFieldSystem(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer)
{
	check(ObjectInitializer.GetClass() == GetClass());
}

void UFieldSystem::FinishDestroy()
{
	Reset();
	Super::FinishDestroy();
}

template<class T>
void UFieldSystem::Evaluate(const FFieldContext & Context, TArrayView<T> & Results) const
{
	FieldSystem.Evaluate(Context, Results);
}

template FIELDSYSTEMCORE_API void UFieldSystem::Evaluate<int32>(const FFieldContext &, TArrayView<int32> & Results) const;
template FIELDSYSTEMCORE_API void UFieldSystem::Evaluate<float>(const FFieldContext &, TArrayView<float> & Results) const; 
template FIELDSYSTEMCORE_API void UFieldSystem::Evaluate<FVector>(const FFieldContext &, TArrayView<FVector> & Results) const;
