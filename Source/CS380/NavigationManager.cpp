// Fill out your copyright notice in the Description page of Project Settings.


#include "NavigationManager.h"
#include "Components/LineBatchComponent.h"

// Sets default values
ANavigationManager::ANavigationManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ANavigationManager::BeginPlay()
{
	Super::BeginPlay();
	GenerateNavigationVolumePixels();
}

// Called every frame
void ANavigationManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Debug_DrawAllVolumes(DebugVoxelsLineThickness);

}
// Debug Helpers:
static ULineBatchComponent* GetDebugLineBatcher(const UWorld* InWorld, bool bPersistentLines, float LifeTime, bool bDepthIsForeground)
{
	return (InWorld ? (bDepthIsForeground ? InWorld->ForegroundLineBatcher : ((bPersistentLines || (LifeTime > 0.f)) ? InWorld->PersistentLineBatcher : InWorld->LineBatcher)) : NULL);
}

/*
* Used to draw a debug voxel. This is based on code borrowed from DrawDebugHelpers, customized for our specific needs
*/
static void DrawDebugVoxel(const UWorld* InWorld, FVector const& Center, FVector const& Box, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float thickness)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		// this means foreground lines can't be persistent 
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher(InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground));
		if (LineBatcher != NULL)
		{
			float LineLifeTime = (LifeTime > 0.f) ? LifeTime : LineBatcher->DefaultLifeTime;

			LineBatcher->DrawLine(Center + FVector(Box.X, Box.Y, Box.Z), Center + FVector(Box.X, -Box.Y, Box.Z), Color, DepthPriority, thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(Box.X, -Box.Y, Box.Z), Center + FVector(-Box.X, -Box.Y, Box.Z), Color, DepthPriority, thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(-Box.X, -Box.Y, Box.Z), Center + FVector(-Box.X, Box.Y, Box.Z), Color, DepthPriority, thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(-Box.X, Box.Y, Box.Z), Center + FVector(Box.X, Box.Y, Box.Z), Color, DepthPriority, thickness, LineLifeTime);

			LineBatcher->DrawLine(Center + FVector(Box.X, Box.Y, -Box.Z), Center + FVector(Box.X, -Box.Y, -Box.Z), Color, DepthPriority, thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(Box.X, -Box.Y, -Box.Z), Center + FVector(-Box.X, -Box.Y, -Box.Z), Color, DepthPriority, thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(-Box.X, -Box.Y, -Box.Z), Center + FVector(-Box.X, Box.Y, -Box.Z), Color, DepthPriority, thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(-Box.X, Box.Y, -Box.Z), Center + FVector(Box.X, Box.Y, -Box.Z), Color, DepthPriority, thickness, LineLifeTime);

			LineBatcher->DrawLine(Center + FVector(Box.X, Box.Y, Box.Z), Center + FVector(Box.X, Box.Y, -Box.Z), Color, DepthPriority, thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(Box.X, -Box.Y, Box.Z), Center + FVector(Box.X, -Box.Y, -Box.Z), Color, DepthPriority, thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(-Box.X, -Box.Y, Box.Z), Center + FVector(-Box.X, -Box.Y, -Box.Z), Color, DepthPriority, thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(-Box.X, Box.Y, Box.Z), Center + FVector(-Box.X, Box.Y, -Box.Z), Color, DepthPriority, thickness, LineLifeTime);
		}
	}
}

// Graph generation
void ANavigationManager::GenerateNavigationVolumePixels()
{
	UWorld* const World = GetWorld();

	if (!World)
		return;

	float actorX = GetActorLocation().X;
	float actorY = GetActorLocation().Y;
	float actorZ = GetActorLocation().Z;

	NAVVolumeData.X.Reserve(XGridSize);

	for (int i = 0; i < XGridSize; i++)
	{
		FNavVoxelX XPlaneVolumes;
		XPlaneVolumes.Y.Reserve(YGridSize);

		for (int j = 0; j < YGridSize; j++)
		{
			FNavVoxelY YPlaneVolumes;
			YPlaneVolumes.Z.Reserve(ZGridSize);

			for (int k = 0; k < ZGridSize; k++)
			{
				// Progress log: (this costs performance - uncomment only when necessary)
				//FString counter = FString::Printf(TEXT("Creating NAV Volume %d,%d,%d/%d,%d,%d"), i, j, k, XGridSize, YGridSize, ZGridSize);
				//UE_LOG(DoNNavigationLog, Log, TEXT("%s"), *counter);

				float x = i * VoxelSize + actorX + (VoxelSize / 2);
				float y = j * VoxelSize + actorY + (VoxelSize / 2);
				float z = k * VoxelSize + actorZ + (VoxelSize / 2);
				FVector Location = FVector(x, y, z);

				FNavigationVoxel volume;
				volume.X = i;
				volume.Y = j;
				volume.Z = k;
				volume.Location = Location;

				//WILL REIMPLEMENT LATER
				/*if (PerformCollisionChecksOnStartup)
					UpdateVoxelCollision(volume);*/

				YPlaneVolumes.AddZ(volume);
			}

			XPlaneVolumes.AddY(YPlaneVolumes);
		}

		NAVVolumeData.AddX(XPlaneVolumes);
	}
}

void ANavigationManager::BuildNAVNetwork()
{
	// This is a legacy function used back when the navigation system was static and baked into the map
	// For a dynamic solution with a large map the amount of time taken by this function is too huge to consider using it to pre-cache neighbors
	// Therefore, lazy loading is currently the preferred method of finding voxel neighbors. 
	//
	// This function is mainly used to profile nav graph cache performance at full saturation (i.e. all neighbor links calculated and cached)

	NavGraphCache.Reserve(XGridSize * YGridSize * ZGridSize);

	for (int32 i = 0; i < NAVVolumeData.X.Num(); i++)
	{
		for (int32 j = 0; j < NAVVolumeData.X[i].Y.Num(); j++)
		{
			for (int32 k = 0; k < NAVVolumeData.X[i].Y[j].Z.Num(); k++)
			{
				auto& volume = NAVVolumeData.X[i].Y[j].Z[k];
				FindOrSetupNeighborsForVolume(&volume);
			}
		}
	}
}

void ANavigationManager::DiscoverNeighborsForVolume(int32 x, int32 y, int32 z, TArray<FNavigationVoxel*>& neighbors)
{
	bool bNeedsValidaion = x == 0 || y == 0 || z == 0 || x == XGridSize - 1 || y == YGridSize - 1 || z == ZGridSize - 1;

	neighbors.Reserve(Volume6DOF + VolumeImplicitDOF);

	// 6 DOF neighbors (Direct neighbors)
	for (int32 i = 0; i < Volume6DOF; i++)
	{
		int32 xN = x + x6DOFCoords[i];
		int32 yN = y + y6DOFCoords[i];
		int32 zN = z + z6DOFCoords[i];

		if (!bNeedsValidaion || IsValidVolume(xN, yN, zN))
			neighbors.Add(&VolumeAtUnsafe(xN, yN, zN));
	}
}

void ANavigationManager::AppendImplictDOFNeighborsForVolume(int32 x, int32 y, int32 z, TArray<FNavigationVoxel*>& Neighbors)
{
	bool bNeedsValidaion = x == 0 || y == 0 || z == 0 || x == XGridSize - 1 || y == YGridSize - 1 || z == ZGridSize - 1;

	if (!bNeedsValidaion || (IsValidVolume(x + 1, y, z + 1) && IsValidVolume(x - 1, y, z + 1) && IsValidVolume(x + 1, y, z - 1) && IsValidVolume(x - 1, y, z - 1) &&
		IsValidVolume(x, y + 1, z + 1) && IsValidVolume(x, y - 1, z + 1) && IsValidVolume(x, y + 1, z - 1) && IsValidVolume(x, y - 1, z - 1) &&
		IsValidVolume(x + 1, y + 1, z) && IsValidVolume(x - 1, y + 1, z) && IsValidVolume(x + 1, y - 1, z - 1) && IsValidVolume(x - 1, y - 1, z))
		)
	{
		// X		

		if (CanNavigate(&VolumeAtUnsafe(x + 1, y, z)) && CanNavigate(&VolumeAtUnsafe(x, y, z + 1)))
			Neighbors.Add(&VolumeAtUnsafe(x + 1, y, z + 1));

		if (CanNavigate(&VolumeAtUnsafe(x - 1, y, z)) && CanNavigate(&VolumeAtUnsafe(x, y, z + 1)))
			Neighbors.Add(&VolumeAtUnsafe(x - 1, y, z + 1));

		if (CanNavigate(&VolumeAtUnsafe(x + 1, y, z)) && CanNavigate(&VolumeAtUnsafe(x, y, z - 1)))
			Neighbors.Add(&VolumeAtUnsafe(x + 1, y, z - 1));

		if (CanNavigate(&VolumeAtUnsafe(x - 1, y, z)) && CanNavigate(&VolumeAtUnsafe(x, y, z - 1)))
			Neighbors.Add(&VolumeAtUnsafe(x - 1, y, z - 1));

		//Y
		if (CanNavigate(&VolumeAtUnsafe(x, y + 1, z)) && CanNavigate(&VolumeAtUnsafe(x, y, z + 1)))
			Neighbors.Add(&VolumeAtUnsafe(x, y + 1, z + 1));

		if (CanNavigate(&VolumeAtUnsafe(x, y - 1, z)) && CanNavigate(&VolumeAtUnsafe(x, y, z + 1)))
			Neighbors.Add(&VolumeAtUnsafe(x, y - 1, z + 1));

		if (CanNavigate(&VolumeAtUnsafe(x, y + 1, z)) && CanNavigate(&VolumeAtUnsafe(x, y, z - 1)))
			Neighbors.Add(&VolumeAtUnsafe(x, y + 1, z - 1));

		if (CanNavigate(&VolumeAtUnsafe(x, y - 1, z)) && CanNavigate(&VolumeAtUnsafe(x, y, z - 1)))
			Neighbors.Add(&VolumeAtUnsafe(x, y - 1, z - 1));

		//Z
		if (CanNavigate(&VolumeAtUnsafe(x + 1, y, z)) && CanNavigate(&VolumeAtUnsafe(x, y + 1, z)))
			Neighbors.Add(&VolumeAtUnsafe(x + 1, y + 1, z));

		if (CanNavigate(&VolumeAtUnsafe(x - 1, y, z)) && CanNavigate(&VolumeAtUnsafe(x, y + 1, z)))
			Neighbors.Add(&VolumeAtUnsafe(x - 1, y + 1, z));

		if (CanNavigate(&VolumeAtUnsafe(x + 1, y, z)) && CanNavigate(&VolumeAtUnsafe(x, y - 1, z)))
			Neighbors.Add(&VolumeAtUnsafe(x + 1, y - 1, z));

		if (CanNavigate(&VolumeAtUnsafe(x - 1, y, z)) && CanNavigate(&VolumeAtUnsafe(x, y - 1, z)))
			Neighbors.Add(&VolumeAtUnsafe(x - 1, y - 1, z));
	}
}

TArray<FNavigationVoxel*> ANavigationManager::FindOrSetupNeighborsForVolume(FNavigationVoxel* Volume)
{
	if (NavGraphCache.Contains(Volume))
	{
		auto neighbors = *NavGraphCache.Find(Volume); // copy by value so we don't pollute the cache implicit DOFs

		AppendImplictDOFNeighborsForVolume(Volume->X, Volume->Y, Volume->Z, neighbors);

		return neighbors;
	}
	else
	{
		// Neighbors not found, Lazy loading of NAV Graph for given volume commences...
		TArray<FNavigationVoxel*> neighbors;
		DiscoverNeighborsForVolume(Volume->X, Volume->Y, Volume->Z, neighbors);
		NavGraphCache.Add(Volume, neighbors);

		AppendImplictDOFNeighborsForVolume(Volume->X, Volume->Y, Volume->Z, neighbors);

		return neighbors;
	}
}

//need to be implemented later
bool ANavigationManager::CanNavigate(FVector Location)
{
	// Note:- UpdateVoxelCollision currently performs a similar check to determine navigability. For consistency both should use the same path.

	/*TArray<FOverlapResult> outOverlaps;

	bool const bHit = GetWorld()->OverlapMultiByObjectType(outOverlaps, Location, FQuat::Identity, VoxelCollisionObjectParams, VoxelCollisionShape, VoxelCollisionQueryParams);

	bool CanNavigate = !outOverlaps.Num();

	return CanNavigate;*/

	return true;
}

//need to be implemented later
bool ANavigationManager::CanNavigate(FNavigationVoxel* Volume)
{
	/*if (!Volume->bIsInitialized)
		UpdateVoxelCollision(*Volume);

	return Volume->CanNavigate();*/

	return true;
}

void ANavigationManager::Debug_DrawAllVolumes(float LineThickness)
{
	float actorX = GetActorLocation().X;
	float actorY = GetActorLocation().Y;
	float actorZ = GetActorLocation().Z;

	for (int i = 0; i < XGridSize; i++)
	{
		for (int j = 0; j < YGridSize; j++)
		{
			for (int k = 0; k < ZGridSize; k++)
			{
				float x = i * VoxelSize + actorX + (VoxelSize / 2);
				float y = j * VoxelSize + actorY + (VoxelSize / 2);
				float z = k * VoxelSize + actorZ + (VoxelSize / 2);
				FVector Location = FVector(x, y, z);

				DrawDebugVoxel_Safe(GetWorld(), Location, NavVolumeExtent() * 0.95f, FColor::Yellow, false, 2.5f, 0, LineThickness);
			}
		}
	}
}

void ANavigationManager::DrawDebugVoxel_Safe(UWorld* World, FVector Center, FVector Box, FColor Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	//if (!bMultiThreadingEnabled)
		DrawDebugVoxel(World, Center, Box, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
	//else
		//DrawDebugVoxelsQueue.Enqueue(FDrawDebugVoxelRequest(Center, Box, Color, bPersistentLines, LifeTime, DepthPriority, Thickness));
}