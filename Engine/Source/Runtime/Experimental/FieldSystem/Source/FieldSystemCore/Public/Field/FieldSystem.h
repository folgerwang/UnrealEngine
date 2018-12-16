// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Queue.h"
#include "Field/FieldSystemTypes.h"
#include "Math/Vector.h"

#include "FieldSystem.generated.h"

class FFieldSystem;
class UFieldSystem;

/**
* FieldNodeBase
*/
class FIELDSYSTEMCORE_API FFieldNodeBase
{
public:
	static int32 Invalid;

	enum EFieldType
	{
		EField_None = 0,
		EField_Int32,
		EField_Float,
		EField_FVector
	};

	virtual ~FFieldNodeBase() {}
	virtual EFieldType Type() const { check(false); return EField_None; }
	virtual FFieldNodeBase* Clone() const = 0;

	int32 GetTerminalID() const { check(TerminalID != -1);  return TerminalID; }
	void  SetTerminalID(int32 ID) { TerminalID = ID; }

	FName GetName() const { return Name; }
	void  SetName(const FName & NameIn) { Name = NameIn; }

	const FFieldSystem * GetFieldSystem() const { return FieldSystem; }
	void SetFieldSystem(const FFieldSystem* SystemIn) { FieldSystem = SystemIn; }

protected:

	FFieldNodeBase(FName NameIn) : TerminalID(-1), Name(NameIn) {};
	int32 TerminalID;
	FName Name;
	const FFieldSystem * FieldSystem;

private:

	FFieldNodeBase() : TerminalID(Invalid), Name(""), FieldSystem(nullptr) {}
};


/**
* FieldNode<T>
*/
template<class T>
class FIELDSYSTEMCORE_API FFieldNode : public FFieldNodeBase
{
	typedef FFieldNodeBase Super;

public:
	
	FFieldNode() = delete;
	virtual ~FFieldNode() {}

	virtual void Evaluate(const FFieldContext &, TArrayView<T> & Results) const { check(false); }

	static EFieldType StaticType();
	virtual EFieldType Type() const { return StaticType(); }

protected :
	FFieldNode(FName Name) : Super(Name) {};
};

/**
* UFieldSystem (UObject)
*
*  Engine for field evaluation
*
*/
class FIELDSYSTEMCORE_API FFieldSystem
{
public:

	FFieldSystem() {}

	virtual ~FFieldSystem();

	template<class NODE_T> NODE_T & NewNode(const FName & Name);

	template<class T> void Evaluate(const FFieldContext &, TArrayView<T> & Results) const;

	int32 TerminalIndex(const FName & FieldTerminalName) const;

	void BuildFrom(const FFieldSystem& Other);

	int32 Num() const { return Nodes.Num(); }

	FFieldNodeBase * GetNode(int32 Index) { return (0 <= Index && Index < Nodes.Num()) ? Nodes[Index] : nullptr; }
	const FFieldNodeBase * GetNode(int32 Index) const { return (0 <= Index && Index < Nodes.Num()) ? Nodes[Index] : nullptr;}

	void Reset() { Nodes.Reset(0); }

protected:

	TArray<FFieldNodeBase*> Nodes;
};

/**
* UFieldSystem (UObject)
*
*  Engine for field evaluation
*
*/
UCLASS(customconstructor)
class FIELDSYSTEMCORE_API UFieldSystem : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UFieldSystem(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	bool IsVisible() { return true; }

	template<class NODE_T> NODE_T & NewNode(const FName & Name) { return FieldSystem.NewNode<NODE_T>(Name); }

	virtual void FinishDestroy() override;

	template<class T> void Evaluate(const FFieldContext & Context, TArrayView<T> & Results) const;

	void Reset() { FieldSystem = FFieldSystem(); }

	int32 TerminalIndex(const FName & FieldTerminalName) const { return FieldSystem.TerminalIndex(FieldTerminalName); }

	FFieldSystem& GetFieldData() { return FieldSystem; }
	const FFieldSystem& GetFieldData() const { return FieldSystem; }

protected:

	FFieldSystem FieldSystem;
};