// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionExampleDecimation.h"

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/TransformCollection.h"
#include "GeometryCollection/GeometryCollectionProximityUtility.h"

#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/Particles.h"
#include "Chaos/TriangleMesh.h"
#include "Chaos/Vector.h"

#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include "../Resource/BoxGeometry.h"
#include "../Resource/CylinderGeometry.h"
#include "../Resource/EllipsoidGeometry.h"
#include "../Resource/EllipsoidGeometry2.h"
#include "../Resource/EllipsoidGeometry3.h"
#include "../Resource/FracturedGeometry.h"
#include "../Resource/SphereGeometry.h"
#include "../Resource/TorusGeometry.h"

//#define VERBOSE
#define WRITE_OBJ_FILES

namespace GeometryCollectionExample
{
	
	Chaos::TParticles<float, 3>
	BuildParticlesFeomGeomCollection(FGeometryCollection *TestCollection)
	{
		TManagedArray<FVector> &Vertex = *TestCollection->Vertex;
		const int numParticles = Vertex.Num();
		Chaos::TParticles<float, 3> particles;
		particles.AddParticles(numParticles);
		for (int i = 0; i < numParticles; i++)
			particles.X(i).Set(Vertex[i][0], Vertex[i][1], Vertex[i][2]);
		return particles;
	}

	Chaos::TTriangleMesh<float>
	BuildTriMeshFromGeomCollection(FGeometryCollection *TestCollection)
	{
		TManagedArray<FIntVector> &Indices = *TestCollection->Indices;
		const int numTris = Indices.Num();
		TArray<Chaos::TVector<int32, 3>> tris;
		tris.SetNumUninitialized(numTris);
		for (int i = 0; i < numTris; i++)
			tris[i] = Chaos::TVector<int32, 3>(Indices[i][0], Indices[i][1], Indices[i][2]);

		Chaos::TTriangleMesh<float> triMesh(MoveTemp(tris));
		return triMesh;
	}

	void
	PrintBoolArray(const TManagedArray<bool> &bArray)
	{
#ifdef VERBOSE
		for (int j = 0; j < bArray.Num(); j++)
			std::cout << (bArray[j] ? "1" : "0");
		std::cout << std::endl;
#endif
	}

	void
	WriteImportanceOrderObjs(
		FGeometryCollection *TestCollection,
		const TArray<int32> &importance,
		const TArray<int32> &coincidentVertices,
		const char *baseName, 
		const char *path)
	{
		// Add an array to the vertices group for visibility
		TManagedArray<bool> &visibility =
			*TestCollection->AddAttribute<bool>(
				"VertexVisibility", FGeometryCollection::VerticesGroup);
		// Initialize visibility
		const int numParticles = importance.Num();// visibility.Num();
		check(numParticles <= visibility.Num());
		const int numGoodParticles = numParticles - coincidentVertices.Num();
		check(numParticles >= numGoodParticles);
		for (int i = 0; i < numParticles; i++)
			visibility[i] = false;

#ifdef VERBOSE
		std::cout << baseName << " - Num points: " << numParticles 
			<< " Num coincident: " << coincidentVertices.Num()
			<< " - visibility:" << std::endl;
#endif
#ifdef WRITE_OBJ_FILES
		TestCollection->WriteDataToOBJFile(FString(baseName), path, true, false);
#endif
		int i = 0;
		for (; i < FMath::Min(4, numGoodParticles); i++)
			visibility[importance[i]] = true;
#ifdef WRITE_OBJ_FILES
		TestCollection->WriteDataToOBJFile(FString(baseName)+"_4", path, false, true);
#endif
		PrintBoolArray(visibility);

		for (; i < FMath::Min(8, numGoodParticles); i++)
			visibility[importance[i]] = true;
#ifdef WRITE_OBJ_FILES
		TestCollection->WriteDataToOBJFile(FString(baseName) +"_8", path, false, true);
#endif
		PrintBoolArray(visibility);

		for (; i < static_cast<int32>(ceil(numGoodParticles * .1)); i++)
			visibility[importance[i]] = true;
#ifdef WRITE_OBJ_FILES
		TestCollection->WriteDataToOBJFile(FString(baseName) +"_10pct", path, false, true);
#endif
		PrintBoolArray(visibility);

		for (; i < static_cast<int32>(ceil(numGoodParticles * .25)); i++)
			visibility[importance[i]] = true;
#ifdef WRITE_OBJ_FILES
		TestCollection->WriteDataToOBJFile(FString(baseName) +"_25pct", path, false, true);
#endif
		PrintBoolArray(visibility);

		for (; i < static_cast<int32>(ceil(numGoodParticles * .5)); i++)
			visibility[importance[i]] = true;
#ifdef WRITE_OBJ_FILES
		TestCollection->WriteDataToOBJFile(FString(baseName) +"_50pct", path, false, true);
#endif
		PrintBoolArray(visibility);
	}

	template <class T>
	bool
	RunGeomDecimationTest(FGeometryCollection* TestCollection, ExampleResponse&  R, const char *BaseName, const char *OutputDir, const uint32 ExpectedHash, const bool RestrictToLocalIndexRange = false)
	{
		Chaos::TParticles<T, 3> Particles = BuildParticlesFeomGeomCollection(TestCollection);
		Chaos::TTriangleMesh<T> TriMesh = BuildTriMeshFromGeomCollection(TestCollection);

		const Chaos::TArrayCollectionArray<Chaos::TVector<T, 3>>& X = Particles.X();
		const TArrayView<Chaos::TVector<T, 3>> XV(const_cast<Chaos::TVector<T, 3>*>(X.GetData()), X.Num());
		TArray<int32> CoincidentVertices;
		const TArray<int32> Importance = TriMesh.GetVertexImportanceOrdering(XV, &CoincidentVertices, RestrictToLocalIndexRange);
		check(CoincidentVertices.Num() < Importance.Num());

		const int numParticles = Particles.Size();
		R.ExpectTrue((!RestrictToLocalIndexRange && Importance.Num() == numParticles) ||
					 (RestrictToLocalIndexRange && Importance.Num() <= numParticles)); // got the right number of indices
		R.ExpectTrue(TSet<int32>(Importance).Num() == Importance.Num()); // indices were unique

		WriteImportanceOrderObjs(
			TestCollection, Importance, CoincidentVertices, BaseName, OutputDir);

		const uint32 hash = GetTypeHash(Importance);
#ifdef VERBOSE
		std::cout << BaseName << " importance ordering hash: " << hash << std::endl;
#endif
		if (hash != ExpectedHash)
		{
			std::cout << "GeometryCollectionTestDecimation - " << BaseName
				<< " - expected importance ordering hash: " << ExpectedHash
				<< " got: " << hash << ".  Failing." << std::endl;
		}
		return hash == ExpectedHash;
	}

	template <class T, class TGeom>
	bool
	RunGeomDecimationTest(ExampleResponse& R, const char *BaseName, const char *OutputDir, const uint32 ExpectedHash, const bool RestrictToLocalIndexRange = false)
	{
		TGeom Geom;
		FGeometryCollection* TestCollection =
			FGeometryCollection::NewGeometryCollection(
				Geom.RawVertexArray,
				Geom.RawIndicesArray);
		return RunGeomDecimationTest<T>(TestCollection, R, BaseName, OutputDir, ExpectedHash, RestrictToLocalIndexRange);
	}

	template <class T, class TGeom>
	bool
	RunGlobalGeomDecimationTest(ExampleResponse& R, const char *BaseName, const char *OutputDir, const uint32 ExpectedHash, const bool RestrictToLocalIndexRange = true)
	{
		TGeom Geom;
		FGeometryCollection* TestCollection =
			FGeometryCollection::NewGeometryCollection(
				Geom.RawVertexArray,
				Geom.RawIndicesArray1);
		return RunGeomDecimationTest<T>(TestCollection, R, BaseName, OutputDir, ExpectedHash, RestrictToLocalIndexRange);
	}

	template<class T>
	bool TestGeometryDecimation(ExampleResponse&& R)
	{
		bool Success = true;
		// If E:\TestGeometry\Decimation doesn't already exist, the files aren't written.

		// Standalone point pools.
		Success &= RunGeomDecimationTest<T, BoxGeometry>(R, "box", "E:\\TestGeometry\\Decimation\\", 4024338882);
		Success &= RunGeomDecimationTest<T, CylinderGeometry>(R, "cylinder", "E:\\TestGeometry\\Decimation\\", 2477299646);
		Success &= RunGeomDecimationTest<T, EllipsoidGeometry>(R, "ellipsoid", "E:\\TestGeometry\\Decimation\\", 1158371240);
		Success &= RunGeomDecimationTest<T, EllipsoidGeometry2>(R, "ellipsoid2", "E:\\TestGeometry\\Decimation\\", 554754926);
		Success &= RunGeomDecimationTest<T, EllipsoidGeometry3>(R, "ellipsoid3", "E:\\TestGeometry\\Decimation\\", 2210765036);
		Success &= RunGeomDecimationTest<T, FracturedGeometry>(R, "fractured", "E:\\TestGeometry\\Decimation\\", 2030682536);
		Success &= RunGeomDecimationTest<T, SphereGeometry>(R, "sphere", "E:\\TestGeometry\\Decimation\\", 4119721232);
		Success &= RunGeomDecimationTest<T, TorusGeometry>(R, "torus", "E:\\TestGeometry\\Decimation\\", 2519379615);
		
		// Geometry in a global point pool.
		Success &= RunGeomDecimationTest<T, GlobalFracturedGeometry>(R, "globalFractured", "E:\\TestGeometry\\Decimation\\", 1018810169, true);
		Success &= RunGlobalGeomDecimationTest<T, GlobalFracturedGeometry>(R, "globalFracturedMerged", "E:\\TestGeometry\\Decimation\\", 1018810169, true);

		R.ExpectTrue(Success);

		return !R.HasError();
	}
	template bool TestGeometryDecimation<float>(ExampleResponse&& R);
	
}

