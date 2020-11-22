// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldCollision.h"
#include "Components/BoxComponent.h"
#include "NavigationManager.generated.h"

USTRUCT()
struct FNavigationVoxel
{
	GENERATED_USTRUCT_BODY()	

	int32 X;
	int32 Y;
	int32 Z;

	FVector Location;
	uint8 NumResidents = 0;
	bool bIsInitialized = false;

	//collision stuff
	//TArray<FDonNavigationDynamicCollisionNotifyee> DynamicCollisionNotifyees;

	bool FORCEINLINE CanNavigate() { return NumResidents == 0; }

	void SetNavigability(bool CanNavigate)
	{
		if (!CanNavigate)
			NumResidents++;
		else
			NumResidents = NumResidents > 0 ? NumResidents - 1 : 0;
	}

	//void BroadcastCollisionUpdates();

	friend bool operator== (const FNavigationVoxel& A, const FNavigationVoxel& B)
	{
		return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
	}

	FNavigationVoxel() {}
};

// Finite World data structure :
// Nested Structs for aggregating world voxels across three axes:
USTRUCT()
struct FNavVoxelY
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FNavigationVoxel> Z;

	void AddZ(FNavigationVoxel volume)
	{
		Z.Add(volume);
	}

	FNavVoxelY()
	{
	}
};

USTRUCT()
struct FNavVoxelX
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FNavVoxelY> Y;

	void AddY(FNavVoxelY YPlaneVolume)
	{
		Y.Add(YPlaneVolume);
	}

	FNavVoxelX()
	{
	}
};

USTRUCT()
struct FNavVoxelXYZ
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FNavVoxelX> X;

	void AddX(FNavVoxelX XPlaneVolume)
	{
		X.Add(XPlaneVolume);
	}

	void ClearAll()
	{
		for (FNavVoxelX _x : X)
		{
			for (FNavVoxelY _y : _x.Y)
				_y.Z.Empty();

			_x.Y.Empty();
		}

		X.Empty();
	}

	FNavVoxelXYZ()
	{
	}
};

/** Navigation Query Debug Parameters  */
USTRUCT(BlueprintType)
struct FNavigationDebugParams
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Navigation")
	bool DrawDebugVolumes = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Navigation")
		bool VisualizeRawPath = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Navigation")
		bool VisualizeOptimizedPath = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Navigation")
		bool VisualizeInRealTime = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Navigation")
		float LineThickness = 2.f;

	/* -1 signifies persistent lines that need to be flushed out manually to clear them*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Navigation")
		float LineDuration = -1.f;

	FNavigationDebugParams() {}

	FNavigationDebugParams(bool _DrawDebugVolumes, bool _VisualizeRawPath, bool _VisualizeOptimizedPath, bool _VisualizeInRealTime, float _LineThickness)
	{
		DrawDebugVolumes = _DrawDebugVolumes;
		VisualizeRawPath = _VisualizeRawPath;
		VisualizeOptimizedPath = _VisualizeOptimizedPath;
		VisualizeInRealTime = _VisualizeInRealTime;
		LineThickness = _LineThickness;
	}
};

UCLASS()
class CS380_API ANavigationManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ANavigationManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


	FCollisionShape VoxelCollisionShape;
	//FCollisionObjectQueryParams VoxelCollisionObjectParams;
	//FCollisionQueryParams VoxelCollisionQueryParams;
	//FCollisionQueryParams VoxelCollisionQueryParams2;
	TMap<FNavigationVoxel*, TArray <FNavigationVoxel*>> NavGraphCache;
	//TMap<FMeshIdentifier, FVoxelCollisionProfile> VoxelCollisionProfileCache_WorkerThread;
	//TMap<FMeshIdentifier, FVoxelCollisionProfile> VoxelCollisionProfileCache_GameThread;

	bool bRegistrationCompleteForComponents;
	int32 RegistrationIndexCurrent;
	int32 MaxRegistrationsPerTick;

	float VoxelSizeSquared;

	// DOF based travel
	static const int32 Volume6DOF = 6;	       // 6 degrees of freedm: Forward, Backward, Left, Right, Up and Down
	static const int32 VolumeImplicitDOF = 12; // Implicit degrees of freedom: formed by the combination of any 2 direct degrees of freedom proving implicit access to a diagonal neighboring voxel

	// 6 DOF - directly usable for travel
	//						        0    1    2     3    4    5   
	int x6DOFCoords[Volume6DOF] = { 0,   0,   1,   -1,   0,   0 };
	int y6DOFCoords[Volume6DOF] = { 1,  -1,   0,    0,   0,   0, };
	int z6DOFCoords[Volume6DOF] = { 0,   0,   0,    0,   1,  -1, };

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void SetIsUnbound(bool bIsUnboundIn);

	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	bool bIsUnbound = false;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Translation)
	USceneComponent* SceneComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Translation)
	UBillboardComponent* Billboard;

	FNavVoxelXYZ NAVVolumeData;

	/* Represents the side of the cube used to build the voxel. Eg: a value of 300 produces a cube 300x300x300*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Dimensions")
	float VoxelSize;

	/* The number of voxels to build along the X axis (offset from NAV actor)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Dimensions")
	int32 XGridSize;

	/* The number of voxels to build along the Y axis (offset from NAV actor)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Dimensions")
	int32 YGridSize;

	/* The number of voxels to build along the Z axis (offset from NAV actor)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Dimensions")
	int32 ZGridSize;

private:
	// Graph generation
	void GenerateNavigationVolumePixels();
	void BuildNAVNetwork();
	void DiscoverNeighborsForVolume(int32 x, int32 y, int32 z, TArray<FNavigationVoxel*>& neighbors);
	void AppendImplictDOFNeighborsForVolume(int32 x, int32 y, int32 z, TArray<FNavigationVoxel*>& Neighbors);
	TArray<FNavigationVoxel*> FindOrSetupNeighborsForVolume(FNavigationVoxel* Volume);
public:

	// Debug Visualization:
	UPROPERTY()
	UBoxComponent* WorldBoundaryVisualizer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDisplayWorldBoundary = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDisplayWorldBoundaryInGame = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	float DebugVoxelsLineThickness = 2.f;

	//utility functions
	FORCEINLINE FVector NavVolumeExtent() { return FVector(VoxelSize / 2, VoxelSize / 2, VoxelSize / 2); }

	FORCEINLINE FVector WorldBoundsExtent() { return FVector(XGridSize * VoxelSize / 2, YGridSize * VoxelSize / 2, ZGridSize * VoxelSize / 2); }

	FORCEINLINE FString VoxelUniqueKey(int x, int y, int z) { return FString::Printf(TEXT("%d-%d-%d"), x, y, z); }

	FORCEINLINE bool IsValidVolume(int x, int y, int z)
	{
		return NAVVolumeData.X.IsValidIndex(x) && NAVVolumeData.X[x].Y.IsValidIndex(y) && NAVVolumeData.X[x].Y[y].Z.IsValidIndex(z);
	}

	inline FVector LocationAtId(int32 X, int32 Y, int32 Z)
	{
		return GetActorLocation() + VoxelSize * FVector(X, Y, Z) + FVector(VoxelSize / 2, VoxelSize / 2, VoxelSize / 2);
	}

	inline FVector LocationAtId(FVector Location, int32 X, int32 Y, int32 Z)
	{
		return Location + VoxelSize * FVector(X, Y, Z);
	}

	inline FVector VolumeIdAt(FVector WorldLocation)
	{
		int32 x = ((WorldLocation.X - GetActorLocation().X) / VoxelSize) + (WorldLocation.X < GetActorLocation().X ? -1 : 0);
		int32 y = ((WorldLocation.Y - GetActorLocation().Y) / VoxelSize) + (WorldLocation.Y < GetActorLocation().Y ? -1 : 0);
		int32 z = ((WorldLocation.Z - GetActorLocation().Z) / VoxelSize) + (WorldLocation.Z < GetActorLocation().Z ? -1 : 0);

		return FVector(x, y, z);
	}

	inline FVector VolumeOriginAt(FVector WorldLocation)
	{
		FVector volumeId = VolumeIdAt(WorldLocation);

		return LocationAtId(volumeId.X, volumeId.Y, volumeId.Z);

	}

	inline FNavigationVoxel* VolumeAt(FVector WorldLocation)
	{
		int32 x = (WorldLocation.X - GetActorLocation().X) / VoxelSize;
		int32 y = (WorldLocation.Y - GetActorLocation().Y) / VoxelSize;
		int32 z = (WorldLocation.Z - GetActorLocation().Z) / VoxelSize;

		if (IsValidVolume(x, y, z))
			return &NAVVolumeData.X[x].Y[y].Z[z];
		else
			return NULL;
	}

	inline FNavigationVoxel* VolumeAtSafe(int32 x, int32 y, int32 z)
	{
		if (IsValidVolume(x, y, z))
			return &NAVVolumeData.X[x].Y[y].Z[z];
		else
			return NULL;
	}

	/*
	* Fetch volume by index. Unsafe, because it assumes that you've already checked index validity. It is faster, but will crash with invalid input
	 *  i.e. similar to Unreal's GetUnsafeNormal() function
	 */
	inline FNavigationVoxel& VolumeAtUnsafe(int32 x, int32 y, int32 z)
	{
		return NAVVolumeData.X[x].Y[y].Z[z];
	}

	inline FNavigationVoxel* NeighborAt(FNavigationVoxel* Volume, FVector NeighborOffset)
	{
		if (!Volume)
			return NULL;

		int32 x = Volume->X + NeighborOffset.X;
		int32 y = Volume->Y + NeighborOffset.Y;
		int32 z = Volume->Z + NeighborOffset.Z;

		return VolumeAtSafe(x, y, z);
	}

	/* Clamps a vector to the navigation bounds as defined by the grid configuration of the navigation object you've placed in the map*/
	UFUNCTION(BlueprintPure, Category = "Navigation")
	FVector ClampLocationToNavigableWorld(FVector DesiredLocation)
	{
		if (bIsUnbound)
			return DesiredLocation;

		FVector origin = GetActorLocation();
		float xClamped = FMath::Clamp(DesiredLocation.X, origin.X, origin.X + XGridSize * VoxelSize);
		float yClamped = FMath::Clamp(DesiredLocation.Y, origin.Y, origin.Y + YGridSize * VoxelSize);
		float zClamped = FMath::Clamp(DesiredLocation.Z, origin.Z, origin.Z + ZGridSize * VoxelSize);

		return FVector(xClamped, yClamped, zClamped);
	}

	UFUNCTION(BlueprintPure, Category = "Navigation")
	bool IsLocationWithinNavigableWorld(FVector DesiredLocation) const
	{
		if (bIsUnbound)
			return true;

		const FVector origin = GetActorLocation();

		return  DesiredLocation.X >= origin.X && DesiredLocation.X <= (origin.X + XGridSize * VoxelSize) &&
			DesiredLocation.Y >= origin.Y && DesiredLocation.Y <= (origin.Y + YGridSize * VoxelSize) &&
			DesiredLocation.Z >= origin.Z && DesiredLocation.Z <= (origin.Z + ZGridSize * VoxelSize);
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Navigation")
	bool CanNavigate(FVector Location);

	bool CanNavigate(FNavigationVoxel* Volume);

	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void Debug_DrawAllVolumes(float LineThickness);

private:
	//drawing
	//void DrawDebugLine_Safe(UWorld* World, FVector LineStart, FVector LineEnd, FColor Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness);
	//void DrawDebugPoint_Safe(UWorld* World, FVector PointLocation, float PointThickness, FColor Color, bool bPersistentLines, float LifeTime);
	//void DrawDebugSphere_Safe(UWorld* World, FVector Center, float Radius, float Segments, FColor Color, bool bPersistentLines, float LifeTime);
	void DrawDebugVoxel_Safe(UWorld* World, FVector Center, FVector Box, FColor Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness);
};
