#pragma once

#include "CoreMinimal.h"
#include "ScatterRegionResult.generated.h"

USTRUCT(BlueprintType)
struct SEELESCATTERREGIONS_API FScatterRegionGenerationSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter Region")
	FString RegionType = TEXT("village");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter Region")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter Region", meta = (ClampMin = "20.0", ClampMax = "1000.0"))
	float SizeMeters = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter Region")
	int32 Seed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter Region")
	FString RecipeAssetPath;
};

USTRUCT(BlueprintType)
struct SEELESCATTERREGIONS_API FScatterRegionGenerationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Scatter Region")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Scatter Region")
	FString RegionActor;

	UPROPERTY(BlueprintReadOnly, Category = "Scatter Region")
	int32 InstanceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scatter Region")
	int32 ProjectionHits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scatter Region")
	TArray<FString> Warnings;

	UPROPERTY(BlueprintReadOnly, Category = "Scatter Region")
	TArray<FString> Errors;
};
