#include "ScatterRegionRecipeDataAsset.h"

namespace
{
	FString ScatterRegionSlotKey(const FName& SlotId)
	{
		FString Raw = SlotId.ToString().TrimStartAndEnd().ToLower();
		FString Key;
		for (TCHAR Ch : Raw)
		{
			if (FChar::IsAlnum(Ch))
			{
				Key.AppendChar(Ch);
			}
		}
		return Key;
	}

	FName CanonicalScatterRegionSlotId(const FName& SlotId)
	{
		const FString Key = ScatterRegionSlotKey(SlotId);
		if (Key == TEXT("sideroadprops"))
		{
			return TEXT("side_road_props");
		}
		if (Key == TEXT("scatterprops") || Key == TEXT("irregularprops"))
		{
			return TEXT("scatter_props");
		}
		if (Key == TEXT("pantheon"))
		{
			return TEXT("panteon");
		}
		return FName(*Key);
	}

	bool HasSingleMesh(const FScatterRegionSingleMeshRecipeData& Mesh)
	{
		return !Mesh.Mesh.IsNull();
	}

	FScatterRegionSingleMeshRecipeData ToSingleMesh(const FScatterRegionMeshRecipeData& Mesh)
	{
		FScatterRegionSingleMeshRecipeData SingleMesh;
		SingleMesh.Mesh = Mesh.Mesh;
		SingleMesh.ScaleMin = Mesh.ScaleMin;
		SingleMesh.ScaleMax = Mesh.ScaleMax;
		SingleMesh.bUniformScale = Mesh.bUniformScale;
		return SingleMesh;
	}

	void MigrateLegacySingleMesh(FScatterRegionBoundarySlotRecipeData& Slot)
	{
		if (!HasSingleMesh(Slot.Mesh) && Slot.Meshes.Num() > 0)
		{
			Slot.Mesh = ToSingleMesh(Slot.Meshes[0]);
		}
		Slot.Meshes.Reset();
	}

	FScatterRegionBoundarySlotRecipeData FindOrMakeBoundarySlot(
		const TArray<FScatterRegionBoundarySlotRecipeData>& ExistingSlots,
		const FName& SlotId)
	{
		const FString TargetKey = ScatterRegionSlotKey(SlotId);
		for (const FScatterRegionBoundarySlotRecipeData& ExistingSlot : ExistingSlots)
		{
			if (ScatterRegionSlotKey(ExistingSlot.SlotId) == TargetKey)
			{
				FScatterRegionBoundarySlotRecipeData Slot = ExistingSlot;
				Slot.SlotId = SlotId;
				MigrateLegacySingleMesh(Slot);
				return Slot;
			}
		}

		FScatterRegionBoundarySlotRecipeData Slot;
		Slot.SlotId = SlotId;
		return Slot;
	}

	FScatterRegionBoundarySlotRecipeData FindOrMakeRoadSlot(
		const FScatterRegionBoundarySlotRecipeData& ExistingRoadSlot,
		const TArray<FScatterRegionDensitySlotRecipeData>& ExistingDensitySlots)
	{
		FScatterRegionBoundarySlotRecipeData Slot = ExistingRoadSlot;
		Slot.SlotId = FName(TEXT("road"));
		MigrateLegacySingleMesh(Slot);
		if (!HasSingleMesh(Slot.Mesh))
		{
			for (const FScatterRegionDensitySlotRecipeData& ExistingSlot : ExistingDensitySlots)
			{
				if (ScatterRegionSlotKey(ExistingSlot.SlotId) == TEXT("road") && ExistingSlot.Meshes.Num() > 0)
				{
					Slot.bEnabled = ExistingSlot.bEnabled;
					Slot.Mesh = ToSingleMesh(ExistingSlot.Meshes[0]);
					break;
				}
			}
		}
		return Slot;
	}

	FScatterRegionDensitySlotRecipeData FindOrMakeDensitySlot(
		const TArray<FScatterRegionDensitySlotRecipeData>& ExistingSlots,
		const FName& SlotId,
		float DefaultDensity)
	{
		const FString TargetKey = ScatterRegionSlotKey(SlotId);
		for (const FScatterRegionDensitySlotRecipeData& ExistingSlot : ExistingSlots)
		{
			if (ScatterRegionSlotKey(ExistingSlot.SlotId) == TargetKey)
			{
				FScatterRegionDensitySlotRecipeData Slot = ExistingSlot;
				Slot.SlotId = SlotId;
				return Slot;
			}
		}

		FScatterRegionDensitySlotRecipeData Slot;
		Slot.SlotId = SlotId;
		Slot.Density = DefaultDensity;
		return Slot;
	}

	FScatterRegionBuildingSlotRecipeData MakeBuildingSlot(const FName& BuildingId)
	{
		FScatterRegionBuildingSlotRecipeData Slot;
		Slot.BuildingId = BuildingId;
		return Slot;
	}

	void NormalizeBuildingSlots(TArray<FScatterRegionBuildingSlotRecipeData>& BuildingSlots)
	{
		if (BuildingSlots.Num() == 0)
		{
			BuildingSlots.Add(MakeBuildingSlot(TEXT("Building1")));
			BuildingSlots.Add(MakeBuildingSlot(TEXT("Building2")));
			return;
		}

		for (int32 Index = 0; Index < BuildingSlots.Num(); ++Index)
		{
			if (BuildingSlots[Index].BuildingId.IsNone())
			{
				BuildingSlots[Index].BuildingId = FName(*FString::Printf(TEXT("Building%d"), Index + 1));
			}
		}
	}
}

UScatterRegionRecipeDataAsset::UScatterRegionRecipeDataAsset()
{
	EnsureRegionSlotLayout();
}

void UScatterRegionRecipeDataAsset::PostLoad()
{
	Super::PostLoad();
	EnsureRegionSlotLayout();
}

#if WITH_EDITOR
void UScatterRegionRecipeDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	EnsureRegionSlotLayout();
}
#endif

void UScatterRegionRecipeDataAsset::EnsureRegionSlotLayout()
{
	if (RegionType == EScatterRegionRecipeType::Village)
	{
		NormalizeBuildingSlots(BuildingSlots);

		const TArray<FScatterRegionBoundarySlotRecipeData> ExistingBoundarySlots = BoundarySlots;
		const TArray<FScatterRegionDensitySlotRecipeData> ExistingDensitySlots = DensitySlots;
		BoundarySlots.Reset();
		BoundarySlots.Add(FindOrMakeBoundarySlot(ExistingBoundarySlots, TEXT("fence")));
		BoundarySlots.Add(FindOrMakeBoundarySlot(ExistingBoundarySlots, TEXT("gate")));
		RoadSlot = FindOrMakeRoadSlot(RoadSlot, ExistingDensitySlots);

		DensitySlots.Reset();
		DensitySlots.Add(FindOrMakeDensitySlot(ExistingDensitySlots, TEXT("side_road_props"), 0.28f));
		DensitySlots.Add(FindOrMakeDensitySlot(ExistingDensitySlots, TEXT("scatter_props"), 0.65f));
		return;
	}

	BuildingSlots.Reset();
	BoundarySlots.Reset();

	const TArray<FScatterRegionDensitySlotRecipeData> ExistingDensitySlots = DensitySlots;
	RoadSlot = FindOrMakeRoadSlot(RoadSlot, ExistingDensitySlots);
	DensitySlots.Reset();
	if (RegionType == EScatterRegionRecipeType::Farm)
	{
		DensitySlots.Add(FindOrMakeDensitySlot(ExistingDensitySlots, TEXT("dirt"), 1.0f));
		DensitySlots.Add(FindOrMakeDensitySlot(ExistingDensitySlots, TEXT("crops"), 0.82f));
		DensitySlots.Add(FindOrMakeDensitySlot(ExistingDensitySlots, TEXT("scarecrow"), 0.5f));
	}
	else if (RegionType == EScatterRegionRecipeType::Cemetery)
	{
		DensitySlots.Add(FindOrMakeDensitySlot(ExistingDensitySlots, TEXT("tombs"), 0.9f));
		DensitySlots.Add(FindOrMakeDensitySlot(ExistingDensitySlots, TEXT("panteon"), 0.5f));
	}

	for (FScatterRegionDensitySlotRecipeData& Slot : DensitySlots)
	{
		Slot.SlotId = CanonicalScatterRegionSlotId(Slot.SlotId);
	}
}
