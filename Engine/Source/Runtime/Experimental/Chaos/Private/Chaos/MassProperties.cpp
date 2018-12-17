// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "Chaos/MassProperties.h"
#include "Chaos/Rotation.h"
#include "Chaos/Matrix.h"

namespace Chaos
{
	template<class T, int d>
	TRotation<T, d> TransformToLocalSpace(PMatrix<T, d, d>& Inertia)
	{
		TRotation<T, d> FinalRotation;

		// Extract Eigenvalues
		T OffDiagSize = FMath::Square(Inertia.M[1][0]) + FMath::Square(Inertia.M[2][0]) + FMath::Square(Inertia.M[2][1]);
		if (OffDiagSize < SMALL_NUMBER)
		{
			return TRotation<T, d>(TVector<T, d>(0), 1);
		}
		T Trace = (Inertia.M[0][0] + Inertia.M[1][1] + Inertia.M[2][2]) / 3;
		T Size = FMath::Sqrt((FMath::Square(Inertia.M[0][0] - Trace) + FMath::Square(Inertia.M[1][1] - Trace) + FMath::Square(Inertia.M[2][2] - Trace) + 2 * OffDiagSize) / 6);
		PMatrix<T, d, d> NewMat = (Inertia - FMatrix::Identity * Trace) * (1 / Size);
		T HalfDeterminant = NewMat.Determinant() / 2;
		T Angle = HalfDeterminant <= -1 ? PI / 3 : (HalfDeterminant >= 1 ? 0 : acos(HalfDeterminant) / 3);
		T m00 = Trace + 2 * Size * cos(Angle), m11 = Trace + 2 * Size * cos(Angle + (2 * PI / 3)), m22 = 3 * Trace - m00 - m11;

		// Extract Eigenvectors
		bool DoSwap = ((m00 - m11) > (m11 - m22)) ? false : true;
		TVector<T, d> Eigenvector0 = (Inertia.SubtractDiagonal(DoSwap ? m22 : m00)).SymmetricCofactorMatrix().LargestColumnNormalized();
		TVector<T, d> Orthogonal = Eigenvector0.GetOrthogonalVector().GetSafeNormal();
		PMatrix<T, d, d - 1> Cofactors(Orthogonal, TVector<T, d>::CrossProduct(Eigenvector0, Orthogonal));
		PMatrix<T, d, d - 1> CofactorsScaled = Inertia * Cofactors;
		PMatrix<T, d - 1, d - 1> IR(
			CofactorsScaled.M[0] * Cofactors.M[0] + CofactorsScaled.M[1] * Cofactors.M[1] + CofactorsScaled.M[2] * Cofactors.M[2],
			CofactorsScaled.M[3] * Cofactors.M[0] + CofactorsScaled.M[4] * Cofactors.M[1] + CofactorsScaled.M[5] * Cofactors.M[2],
			CofactorsScaled.M[3] * Cofactors.M[3] + CofactorsScaled.M[4] * Cofactors.M[4] + CofactorsScaled.M[5] * Cofactors.M[5]);
		PMatrix<T, d - 1, d - 1> IM1 = IR.SubtractDiagonal(DoSwap ? m00 : m22);
		T OffDiag = IM1.M[1] * IM1.M[1];
		T IM1Scale0 = IM1.M[3] * IM1.M[3] + OffDiag;
		T IM1Scale1 = IM1.M[0] * IM1.M[0] + OffDiag;
		TVector<T, d - 1> SmallEigenvector2 = IM1Scale0 > IM1Scale1 ? (TVector<T, d - 1>(IM1.M[3], -IM1.M[1]) / FMath::Sqrt(IM1Scale0)) : (IM1Scale1 > 0 ? (TVector<T, d - 1>(-IM1.M[1], IM1.M[0]) / FMath::Sqrt(IM1Scale1)) : TVector<T, d - 1>(1, 0));
		TVector<T, d> Eigenvector2 = (Cofactors * SmallEigenvector2).GetSafeNormal();
		TVector<T, d> Eigenvector1 = TVector<T, d>::CrossProduct(Eigenvector2, Eigenvector0).GetSafeNormal();

		// Return results
		Inertia = PMatrix<T, d, d>(m00, 0, 0, m11, 0, m22);
		PMatrix<T, d, d> RotationMatrix = DoSwap ? PMatrix<T, d, d>(Eigenvector2, Eigenvector1, -Eigenvector0).GetTransposed() : PMatrix<T, d, d>(Eigenvector0, Eigenvector1, Eigenvector2).GetTransposed();
		FinalRotation = TRotation<T,d>(RotationMatrix);

		//@todo(ocohen): Why do we need to do this? I assume the matrix to quat code is 
		//               more efficient by not ensuring normalized quat, but we are 
		//               passing in a matrix with unit eigenvectors.
		//@comment(brice): That's a kind assumption, I suspect a bug. 
		FinalRotation.Normalize();
		
		return FinalRotation;
	}
	template TRotation<float, 3> TransformToLocalSpace(PMatrix<float, 3, 3>& Inertia);

	template<class T, int d>
	TMassProperties<T, d> CalculateMassProperties(
		const TParticles<T, d> & Vertices,
		const TTriangleMesh<T>& Surface,
		const T & Mass)
	{
		TMassProperties<T, d> MassProperties;
		if (!Surface.GetSurfaceElements().Num())
		{
			MassProperties.Volume = 0;
			return MassProperties;
		}

		check(Mass > 0);
		T Volume = 0;
		TVector<T, d> VolumeTimesSum(0);
		TVector<T, d> Center = Vertices.X(Surface.GetSurfaceElements()[0][0]);
		for (const auto& Element : Surface.GetSurfaceElements())
		{
			PMatrix<T, d, d> DeltaMatrix;
			TVector<T, d> PerElementSize;
			for (int32 i = 0; i < Element.Num(); ++i)
			{
				TVector<T, d> DeltaVector = Vertices.X(Element[i]) - Center;
				DeltaMatrix.M[0][i] = DeltaVector[0];
				DeltaMatrix.M[1][i] = DeltaVector[1];
				DeltaMatrix.M[2][i] = DeltaVector[2];
			}
			PerElementSize[0] = DeltaMatrix.M[0][0] + DeltaMatrix.M[0][1] + DeltaMatrix.M[0][2];
			PerElementSize[1] = DeltaMatrix.M[1][0] + DeltaMatrix.M[1][1] + DeltaMatrix.M[1][2];
			PerElementSize[2] = DeltaMatrix.M[2][0] + DeltaMatrix.M[2][1] + DeltaMatrix.M[2][2];
			T Det = DeltaMatrix.M[0][0] * (DeltaMatrix.M[1][1] * DeltaMatrix.M[2][2] - DeltaMatrix.M[1][2] * DeltaMatrix.M[2][1]) -
				DeltaMatrix.M[0][1] * (DeltaMatrix.M[1][0] * DeltaMatrix.M[2][2] - DeltaMatrix.M[1][2] * DeltaMatrix.M[2][0]) +
				DeltaMatrix.M[0][2] * (DeltaMatrix.M[1][0] * DeltaMatrix.M[2][1] - DeltaMatrix.M[1][1] * DeltaMatrix.M[2][0]);
			Volume += Det;
			VolumeTimesSum += Det * PerElementSize;
		}
		// @todo(mlentine): Should add suppoert for thin shell mass properties
		if (FMath::Abs(Volume) < SMALL_NUMBER)
		{
			MassProperties.Volume = 0;
			return MassProperties;
		}
		MassProperties.CenterOfMass = Center + VolumeTimesSum / (4 * Volume);
		MassProperties.Volume = Volume / 6;

		static const PMatrix<T, d, d> Standard(2, 1, 1, 2, 1, 2);
		PMatrix<T, d, d> Covariance(0);
		for (const auto& Element : Surface.GetSurfaceElements())
		{
			PMatrix<T, d, d> DeltaMatrix(0);
			for (int32 i = 0; i < Element.Num(); ++i)
			{
				TVector<T, d> DeltaVector = Vertices.X(Element[i]) - MassProperties.CenterOfMass;
				DeltaMatrix.M[0][i] = DeltaVector[0];
				DeltaMatrix.M[1][i] = DeltaVector[1];
				DeltaMatrix.M[2][i] = DeltaVector[2];
			}
			T Det = DeltaMatrix.M[0][0] * (DeltaMatrix.M[1][1] * DeltaMatrix.M[2][2] - DeltaMatrix.M[1][2] * DeltaMatrix.M[2][1]) -
				DeltaMatrix.M[0][1] * (DeltaMatrix.M[1][0] * DeltaMatrix.M[2][2] - DeltaMatrix.M[1][2] * DeltaMatrix.M[2][0]) +
				DeltaMatrix.M[0][2] * (DeltaMatrix.M[1][0] * DeltaMatrix.M[2][1] - DeltaMatrix.M[1][1] * DeltaMatrix.M[2][0]);
			const PMatrix<T, d, d> ScaledStandard = Standard * Det;
			Covariance += DeltaMatrix * ScaledStandard * DeltaMatrix.GetTransposed();
		}
		T Trace = Covariance.M[0][0] + Covariance.M[1][1] + Covariance.M[2][2];
		PMatrix<T, d, d> TraceMat(Trace, Trace, Trace);
		MassProperties.InertiaTensor = (TraceMat - Covariance) * (1 / (T)120) * (Mass / MassProperties.Volume);
		MassProperties.RotationOfMass = TransformToLocalSpace(MassProperties.InertiaTensor);

		return MassProperties;
	}
	template CHAOS_API TMassProperties<float, 3> CalculateMassProperties(const TParticles<float, 3> & Vertices,
		const TTriangleMesh<float>& Surface, const float & Mass);
}