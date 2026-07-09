#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ScatterRegionTypes.h"
#include "UObject/UnrealType.h"
#include "ScatterRegionRecipeDataAsset.generated.h"

UCLASS(BlueprintType)
class SEELESCATTERREGIONS_API UScatterRegionRecipeDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UScatterRegionRecipeDataAsset();

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void EnsureRegionSlotLayout();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	EScatterRegionRecipeType RegionType = EScatterRegionRecipeType::Village;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region", meta = (EditCondition = "RegionType == EScatterRegionRecipeType::Village", EditConditionHides))
	TArray<FScatterRegionBuildingSlotRecipeData> BuildingSlots;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadOnly, Category = "Scatter Region", meta = (EditCondition = "RegionType == EScatterRegionRecipeType::Village", EditConditionHides))
	TArray<FScatterRegionBoundarySlotRecipeData> BoundarySlots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter Region")
	FScatterRegionBoundarySlotRecipeData RoadSlot;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadOnly, Category = "Scatter Region")
	TArray<FScatterRegionDensitySlotRecipeData> DensitySlots;
};
