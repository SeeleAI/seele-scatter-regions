// Copyright 2026 QUANLING SHENZHEN NETWORK CO., LTD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "ScatterRegionTypes.generated.h"

UENUM(BlueprintType)
enum class EScatterRegionRecipeType : uint8
{
	Village UMETA(DisplayName = "Village"),
	Farm UMETA(DisplayName = "Farm"),
	Cemetery UMETA(DisplayName = "Cemetery")
};

USTRUCT(BlueprintType)
struct SEELESCATTERREGIONS_API FScatterRegionMeshRecipeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	FVector ScaleMin = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	FVector ScaleMax = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	bool bUniformScale = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
};

USTRUCT(BlueprintType)
struct SEELESCATTERREGIONS_API FScatterRegionSingleMeshRecipeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	FVector ScaleMin = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	FVector ScaleMax = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	bool bUniformScale = false;
};

USTRUCT(BlueprintType)
struct SEELESCATTERREGIONS_API FScatterRegionBuildingSlotRecipeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	FName BuildingId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	FScatterRegionMeshRecipeData MainBuilding;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	TArray<FScatterRegionMeshRecipeData> LocalProps;
};

USTRUCT(BlueprintType)
struct SEELESCATTERREGIONS_API FScatterRegionBoundarySlotRecipeData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	FName SlotId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	FScatterRegionSingleMeshRecipeData Mesh;

	UPROPERTY()
	TArray<FScatterRegionMeshRecipeData> Meshes;
};

USTRUCT(BlueprintType)
struct SEELESCATTERREGIONS_API FScatterRegionDensitySlotRecipeData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	FName SlotId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Density = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	TArray<FScatterRegionMeshRecipeData> Meshes;
};
