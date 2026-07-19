// Copyright 2026 QUANLING SHENZHEN NETWORK CO., LTD. All Rights Reserved.

#include "ScatterRegionGenerator.h"

#include "ScatterRegionRecipeDataAsset.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "LandscapeProxy.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr const TCHAR* ScatterRegionCommandType = TEXT("generate_scatter_region");
	constexpr const TCHAR* GeneratedTagName = TEXT("ScatterRegion.Generated");
	constexpr const TCHAR* GeneratorName = TEXT("native_scatter_region_recipe_asset_cpp_v1");

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
		if (Key == TEXT("ScatterRegionprops"))
		{
			return TEXT("ScatterRegion_props");
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

	FString NormalizeObjectPath(const FString& Path);

	struct FScatterRegionSpec
	{
		FString RegionType = TEXT("village");
		FVector Center = FVector::ZeroVector;
		float SizeMeters = 120.0f;
		int32 Seed = 12345;
		FString RecipeAssetPath;
	};

	struct FScatterRegionRecipeMesh
	{
		FString Path;
		UStaticMesh* Mesh = nullptr;
		FVector ScaleMin = FVector::OneVector;
		FVector ScaleMax = FVector::OneVector;
		bool bUniformScale = false;
		float Weight = 1.0f;
	};

	struct FScatterRegionObjectRecipe
	{
		FString SourcePath;
		TArray<FScatterRegionRecipeMesh> Meshes;
		float Density = 1.0f;
	};

	struct FScatterRegionBuildingSlotRecipe
	{
		FString BuildingId;
		bool bEnabled = true;
		float Weight = 1.0f;
		FScatterRegionObjectRecipe MainBuilding;
		FScatterRegionObjectRecipe LocalProps;
	};

	struct FScatterRegionRecipe
	{
		FString RowName;
		bool bUseFence = true;
		bool bUseGate = true;
		float DirtDensity = 1.0f;
		float CropsDensity = 0.82f;
		float ScarecrowDensity = 0.5f;
		float ScarecrowDistanceMultiplicator = 12.0f;
		float TombDensity = 0.9f;
		float PanteonDensity = 0.5f;
		float TombSpacing = 350.0f;
		float SideRoadPropsDensity = 0.28f;
		float ScatterRegionPropsDensity = 0.65f;
		float BuildingMaxSlope = 30.0f;
		TArray<FScatterRegionBuildingSlotRecipe> BuildingSlots;
		TMap<FString, FScatterRegionObjectRecipe> Slots;
		TSet<FString> DisabledSlots;
	};

	struct FScatterRegionGraphProfile
	{
		int32 NumberOfExit = 2;
		float SectorResolution = 1.25f;
		float RoadWidth = 50.0f;
		float RoadDensity = 0.7f;
		float RoadRandomness = 0.8f;
		float BuildingMaxSlope = 30.0f;
		float BuildingDistanceFromEdge = 70.0f;
		float BuildingDistance = 4000.0f;
		float BuildingMinDistanceFromRoad = 20.0f;
		float RoadSidePropsDistance = 500.0f;
		float ScatterRegionFenceLength = 50.0f;
		float HouseFenceLength = 50.0f;
		float ExitRemoveFenceScaleOffset = 0.3f;
		FVector ExitRemoveFenceOffset = FVector(100.0f, 0.0f, 0.0f);
		FString RoadPattern = TEXT("organic");
	};

	struct FScatterRegionRoadSegment
	{
		FVector2D Start = FVector2D::ZeroVector;
		FVector2D End = FVector2D::ZeroVector;
		float YawDegrees = 0.0f;
		bool bPrimary = false;
	};

	struct FScatterRegionPerimeterSample
	{
		FVector2D Location = FVector2D::ZeroVector;
		float YawDegrees = 0.0f;
	};

	struct FScatterRegionPlacementSet
	{
		TArray<FVector2D> Locations;
	};

	struct FScatterRegionCollisionSet
	{
		TArray<FVector2D> Locations;
		TArray<float> Radii;
	};

	struct FScatterRegionRoadTileSet
	{
		TArray<FVector2D> Locations;
		TArray<float> YawDegrees;
		TArray<float> Radii;
	};

	struct FScatterRegionGraphPoint
	{
		FVector2D Location = FVector2D::ZeroVector;
		float LocalZOffsetCm = 0.0f;
		float YawDegrees = 0.0f;
		float Density = 1.0f;
		float BoundsRadius = 250.0f;
		FVector ScaleMultiplier = FVector::OneVector;
	};

	struct FScatterRegionFarmFieldBlock
	{
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D Extent = FVector2D::ZeroVector;
		float YawDegrees = 0.0f;
	};

	struct FScatterRegionFarmDirtTile
	{
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;
		float YawDegrees = 0.0f;
	};

	struct FScatterRegionRoadTileFootprint
	{
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;
		float YawDegrees = 0.0f;
	};

	struct FScatterRegionCemeteryTombBlock
	{
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D Extent = FVector2D::ZeroVector;
		float YawDegrees = 0.0f;
	};

	struct FScatterRegionBuildingCluster
	{
		FVector2D Center = FVector2D::ZeroVector;
		float RoadYawDegrees = 0.0f;
		float Side = 1.0f;
		float Radius = 1200.0f;
		float Density = 1.0f;
		bool bMarketplace = false;
		bool bPrimaryRoad = false;
	};

	struct FScatterRegionBuildContext
	{
		UWorld* World = nullptr;
		AActor* RootActor = nullptr;
		FVector Center = FVector::ZeroVector;
		TArray<TWeakObjectPtr<AActor>> ProjectionIgnoredActors;
		TMap<FString, UInstancedStaticMeshComponent*> Components;
		TMap<FString, int32> SlotCounts;
		TArray<FString> Warnings;
		FBox Bounds = FBox(ForceInit);
		int32 InstanceCount = 0;
		int32 ComponentCount = 0;
		int32 ProjectionAttempts = 0;
		int32 ProjectionHits = 0;
		int32 ProjectionMisses = 0;
	};

	FString NormalizeKey(FString Value)
	{
		Value = Value.ToLower();
		FString Result;
		for (TCHAR Ch : Value)
		{
			if (FChar::IsAlnum(Ch))
			{
				Result.AppendChar(Ch);
			}
		}
		return Result;
	}

	TSharedPtr<FJsonObject> MakeBaseResult(const FScatterRegionSpec& Spec)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("command"), ScatterRegionCommandType);
		Result->SetStringField(TEXT("generator"), GeneratorName);
		Result->SetStringField(TEXT("region_type"), Spec.RegionType);
		Result->SetNumberField(TEXT("seed"), Spec.Seed);
		Result->SetArrayField(TEXT("warnings"), TArray<TSharedPtr<FJsonValue>>());
		Result->SetArrayField(TEXT("errors"), TArray<TSharedPtr<FJsonValue>>());
		return Result;
	}

	TSharedPtr<FJsonObject> MakeError(const FString& Code, const FString& Message)
	{
		TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetStringField(TEXT("code"), Code);
		Error->SetStringField(TEXT("message"), Message);
		return Error;
	}

	void AddError(const TSharedPtr<FJsonObject>& Result, const FString& Code, const FString& Message)
	{
		TArray<TSharedPtr<FJsonValue>> Errors;
		const TArray<TSharedPtr<FJsonValue>>* ExistingErrors = nullptr;
		if (Result->TryGetArrayField(TEXT("errors"), ExistingErrors) && ExistingErrors)
		{
			Errors = *ExistingErrors;
		}
		Errors.Add(MakeShared<FJsonValueObject>(MakeError(Code, Message)));
		Result->SetArrayField(TEXT("errors"), Errors);
	}

	void AddWarning(const TSharedPtr<FJsonObject>& Result, const FString& Message)
	{
		TArray<TSharedPtr<FJsonValue>> Warnings;
		const TArray<TSharedPtr<FJsonValue>>* ExistingWarnings = nullptr;
		if (Result->TryGetArrayField(TEXT("warnings"), ExistingWarnings) && ExistingWarnings)
		{
			Warnings = *ExistingWarnings;
		}
		Warnings.Add(MakeShared<FJsonValueString>(Message));
		Result->SetArrayField(TEXT("warnings"), Warnings);
	}

	TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Value)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		Array.Add(MakeShared<FJsonValueNumber>(Value.X));
		Array.Add(MakeShared<FJsonValueNumber>(Value.Y));
		Array.Add(MakeShared<FJsonValueNumber>(Value.Z));
		return Array;
	}

	TSharedPtr<FJsonObject> BoundsToJson(const FBox& Bounds)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetArrayField(TEXT("min"), VectorToJsonArray(Bounds.Min));
		Object->SetArrayField(TEXT("max"), VectorToJsonArray(Bounds.Max));
		Object->SetArrayField(TEXT("origin"), VectorToJsonArray(Bounds.GetCenter()));
		Object->SetArrayField(TEXT("extent"), VectorToJsonArray(Bounds.GetExtent()));
		return Object;
	}

	TSharedPtr<FJsonObject> IntMapToJson(const TMap<FString, int32>& Map)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		for (const TPair<FString, int32>& Pair : Map)
		{
			Object->SetNumberField(Pair.Key, Pair.Value);
		}
		return Object;
	}

	bool ParseVector(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, FVector& OutVector, FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Params->TryGetArrayField(FieldName, Values) || !Values || Values->Num() < 3)
		{
			OutError = FString::Printf(TEXT("Missing or invalid '%s'; expected [x, y, z]"), *FieldName);
			return false;
		}

		OutVector = FVector(
			static_cast<float>((*Values)[0]->AsNumber()),
			static_cast<float>((*Values)[1]->AsNumber()),
			static_cast<float>((*Values)[2]->AsNumber()));
		return true;
	}

	bool ParseSpec(const TSharedPtr<FJsonObject>& Params, FScatterRegionSpec& OutSpec, FString& OutError)
	{
		if (!Params.IsValid())
		{
			OutError = TEXT("Params object is required");
			return false;
		}

		Params->TryGetStringField(TEXT("region_type"), OutSpec.RegionType);
		OutSpec.RegionType = OutSpec.RegionType.ToLower();
		if (OutSpec.RegionType != TEXT("village") && OutSpec.RegionType != TEXT("farm") && OutSpec.RegionType != TEXT("cemetery"))
		{
			OutError = TEXT("region_type must be one of: village, farm, cemetery");
			return false;
		}

		if (!ParseVector(Params, TEXT("center"), OutSpec.Center, OutError))
		{
			return false;
		}

		double SizeMeters = 0.0;
		if (!Params->TryGetNumberField(TEXT("size_m"), SizeMeters) || SizeMeters <= 0.0)
		{
			OutError = TEXT("Missing or invalid 'size_m'");
			return false;
		}
		OutSpec.SizeMeters = FMath::Clamp(static_cast<float>(SizeMeters), 20.0f, 1000.0f);

		double Seed = 0.0;
		if (Params->TryGetNumberField(TEXT("seed"), Seed))
		{
			OutSpec.Seed = static_cast<int32>(Seed);
		}

		Params->TryGetStringField(TEXT("recipe_asset"), OutSpec.RecipeAssetPath);

		if (OutSpec.RecipeAssetPath.IsEmpty())
		{
			OutError = TEXT("Pass 'recipe_asset'");
			return false;
		}

		return true;
	}

	FString NormalizeObjectPath(const FString& Path)
	{
		FString Trimmed = Path;
		Trimmed.TrimStartAndEndInline();
		if (Trimmed.IsEmpty())
		{
			return Trimmed;
		}
		if (Trimmed.Contains(TEXT("'")))
		{
			FString Left;
			FString Right;
			if (Trimmed.Split(TEXT("'"), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromStart))
			{
				Right.Split(TEXT("'"), &Trimmed, &Right, ESearchCase::CaseSensitive, ESearchDir::FromStart);
			}
		}
		if (Trimmed.Contains(TEXT(".")))
		{
			return Trimmed;
		}

		FString PackagePath;
		FString AssetName;
		if (Trimmed.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd) && !AssetName.IsEmpty())
		{
			return FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
		}
		return Trimmed;
	}

	UObject* TryLoadObjectPath(const FString& Path)
	{
		const FString ObjectPath = NormalizeObjectPath(Path);
		return ObjectPath.IsEmpty() ? nullptr : StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	}

	bool IsObjectUnderRoot(UObject* Object, const FString& Root)
	{
		if (!Object || Root.IsEmpty())
		{
			return true;
		}
		const FString ObjectPath = NormalizeObjectPath(Object->GetPathName());
		FString NormalizedRoot = Root;
		NormalizedRoot.RemoveFromEnd(TEXT("/"));
		return ObjectPath.StartsWith(NormalizedRoot / TEXT(""));
	}

	void AddStaticMesh(FScatterRegionObjectRecipe& Recipe, UStaticMesh* Mesh, const FString& Path, const FVector& ScaleMin, const FVector& ScaleMax, bool bUniformScale, float Weight = 1.0f)
	{
		if (!Mesh)
		{
			return;
		}

		FScatterRegionRecipeMesh RecipeMesh;
		RecipeMesh.Path = Path.IsEmpty() ? Mesh->GetPathName() : NormalizeObjectPath(Path);
		RecipeMesh.Mesh = Mesh;
		RecipeMesh.ScaleMin = ScaleMin;
		RecipeMesh.ScaleMax = ScaleMax;
		RecipeMesh.bUniformScale = bUniformScale;
		RecipeMesh.Weight = FMath::Max(0.0f, Weight);
		for (const FScatterRegionRecipeMesh& Existing : Recipe.Meshes)
		{
			if (Existing.Path == RecipeMesh.Path)
			{
				return;
			}
		}
		Recipe.Meshes.Add(RecipeMesh);
	}

	FScatterRegionRecipeMesh MakeRecipeMesh(UStaticMesh* Mesh, const FString& Path)
	{
		FScatterRegionRecipeMesh RecipeMesh;
		RecipeMesh.Path = Path.IsEmpty() && Mesh ? Mesh->GetPathName() : NormalizeObjectPath(Path);
		RecipeMesh.Mesh = Mesh;
		RecipeMesh.Weight = 1.0f;
		return RecipeMesh;
	}

	FString RegionTypeToString(EScatterRegionRecipeType RegionType)
	{
		switch (RegionType)
		{
		case EScatterRegionRecipeType::Farm:
			return TEXT("farm");
		case EScatterRegionRecipeType::Cemetery:
			return TEXT("cemetery");
		default:
			return TEXT("village");
		}
	}

	bool ConvertNativeMeshRecipe(const FScatterRegionMeshRecipeData& Source, FScatterRegionRecipeMesh& OutMesh, FString& OutError)
	{
		const FString MeshPath = NormalizeObjectPath(Source.Mesh.ToSoftObjectPath().ToString());
		if (MeshPath.IsEmpty())
		{
			OutError = TEXT("Native ScatterRegion mesh recipe has an empty mesh reference");
			return false;
		}

		UStaticMesh* Mesh = Source.Mesh.LoadSynchronous();
		if (!Mesh)
		{
			OutError = FString::Printf(TEXT("Unable to load native ScatterRegion mesh: %s"), *MeshPath);
			return false;
		}

		OutMesh.Path = MeshPath;
		OutMesh.Mesh = Mesh;
		OutMesh.ScaleMin = Source.ScaleMin;
		OutMesh.ScaleMax = Source.ScaleMax;
		OutMesh.bUniformScale = Source.bUniformScale;
		OutMesh.Weight = FMath::Max(0.0f, Source.Weight);
		return true;
	}

	bool ConvertNativeSingleMeshRecipe(const FScatterRegionSingleMeshRecipeData& Source, FScatterRegionRecipeMesh& OutMesh, FString& OutError)
	{
		FScatterRegionMeshRecipeData Mesh;
		Mesh.Mesh = Source.Mesh;
		Mesh.ScaleMin = Source.ScaleMin;
		Mesh.ScaleMax = Source.ScaleMax;
		Mesh.bUniformScale = Source.bUniformScale;
		Mesh.Weight = 1.0f;
		return ConvertNativeMeshRecipe(Mesh, OutMesh, OutError);
	}

	bool ConvertNativeMeshListRecipe(
		const TArray<FScatterRegionMeshRecipeData>& SourceMeshes,
		const FString& SourceName,
		float Density,
		FScatterRegionObjectRecipe& OutSlot,
		TArray<FString>& Warnings,
		FString& OutError)
	{
		OutSlot.SourcePath = SourceName;
		OutSlot.Density = FMath::Max(0.0f, Density);
		for (const FScatterRegionMeshRecipeData& SourceMesh : SourceMeshes)
		{
			FScatterRegionRecipeMesh Mesh;
			if (!ConvertNativeMeshRecipe(SourceMesh, Mesh, OutError))
			{
				return false;
			}
			AddStaticMesh(OutSlot, Mesh.Mesh, Mesh.Path, Mesh.ScaleMin, Mesh.ScaleMax, Mesh.bUniformScale, Mesh.Weight);
		}
		if (OutSlot.Meshes.Num() == 0)
		{
			Warnings.Add(FString::Printf(TEXT("Native ScatterRegion slot '%s' contains no meshes"), *SourceName));
		}
		return true;
	}

	bool ConvertNativeSingleMeshSlotRecipe(
		const FScatterRegionSingleMeshRecipeData& SourceMesh,
		const FString& SourceName,
		float Density,
		FScatterRegionObjectRecipe& OutSlot,
		TArray<FString>& Warnings,
		FString& OutError)
	{
		OutSlot.SourcePath = SourceName;
		OutSlot.Density = FMath::Max(0.0f, Density);
		if (SourceMesh.Mesh.IsNull())
		{
			Warnings.Add(FString::Printf(TEXT("Native ScatterRegion slot '%s' contains no mesh"), *SourceName));
			return true;
		}

		FScatterRegionRecipeMesh Mesh;
		if (!ConvertNativeSingleMeshRecipe(SourceMesh, Mesh, OutError))
		{
			return false;
		}
		AddStaticMesh(OutSlot, Mesh.Mesh, Mesh.Path, Mesh.ScaleMin, Mesh.ScaleMax, Mesh.bUniformScale, 1.0f);
		return true;
	}

	FString NormalizeScatterRegionSlotId(const FName& SlotId)
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
		if (Key == TEXT("sideroadprops"))
		{
			return TEXT("side_road_props");
		}
		if (Key == TEXT("ScatterRegionprops"))
		{
			return TEXT("ScatterRegion_props");
		}
		if (Key == TEXT("pantheon"))
		{
			return TEXT("panteon");
		}
		return Key;
	}

	bool ConvertNativeBoundarySlotRecipe(
		const FScatterRegionBoundarySlotRecipeData& Source,
		FScatterRegionObjectRecipe& OutSlot,
		TArray<FString>& Warnings,
		FString& OutError)
	{
		if (!Source.bEnabled || Source.SlotId.IsNone())
		{
			return true;
		}
		return ConvertNativeSingleMeshSlotRecipe(Source.Mesh, NormalizeScatterRegionSlotId(Source.SlotId), 1.0f, OutSlot, Warnings, OutError);
	}

	bool ConvertNativeDensitySlotRecipe(
		const FScatterRegionDensitySlotRecipeData& Source,
		FScatterRegionObjectRecipe& OutSlot,
		TArray<FString>& Warnings,
		FString& OutError)
	{
		if (!Source.bEnabled || Source.SlotId.IsNone())
		{
			return true;
		}
		return ConvertNativeMeshListRecipe(Source.Meshes, NormalizeScatterRegionSlotId(Source.SlotId), Source.Density, OutSlot, Warnings, OutError);
	}

	bool ConvertNativeBuildingSlotRecipe(
		const FScatterRegionBuildingSlotRecipeData& Source,
		FScatterRegionBuildingSlotRecipe& OutBuilding,
		TArray<FString>& Warnings,
		FString& OutError)
	{
		if (!Source.bEnabled || Source.BuildingId.IsNone())
		{
			return true;
		}
		OutBuilding.BuildingId = Source.BuildingId.ToString();
		OutBuilding.bEnabled = Source.bEnabled;
		OutBuilding.Weight = FMath::Max(0.0f, Source.Weight);
		TArray<FScatterRegionMeshRecipeData> MainBuildingMeshes;
		MainBuildingMeshes.Add(Source.MainBuilding);
		if (!ConvertNativeMeshListRecipe(MainBuildingMeshes, OutBuilding.BuildingId + TEXT(".MainBuilding"), 1.0f, OutBuilding.MainBuilding, Warnings, OutError))
		{
			return false;
		}
		if (Source.LocalProps.Num() > 0)
		{
			if (!ConvertNativeMeshListRecipe(Source.LocalProps, OutBuilding.BuildingId + TEXT(".LocalProps"), 1.0f, OutBuilding.LocalProps, Warnings, OutError))
			{
				return false;
			}
		}
		return OutBuilding.MainBuilding.Meshes.Num() > 0;
	}

	bool LoadNativeRegionRecipe(const FScatterRegionSpec& Spec, FScatterRegionRecipe& OutRecipe, TArray<FString>& Warnings, FString& OutError)
	{
		UScatterRegionRecipeDataAsset* RecipeAsset = Cast<UScatterRegionRecipeDataAsset>(TryLoadObjectPath(Spec.RecipeAssetPath));
		if (!RecipeAsset)
		{
			OutError = FString::Printf(TEXT("Unable to load native ScatterRegion recipe_asset: %s"), *Spec.RecipeAssetPath);
			return false;
		}

		const FString AssetRegionType = RegionTypeToString(RecipeAsset->RegionType);
		if (Spec.RegionType != AssetRegionType)
		{
			OutError = FString::Printf(TEXT("region_type '%s' does not match recipe_asset region type '%s'"), *Spec.RegionType, *AssetRegionType);
			return false;
		}

		OutRecipe.RowName = RecipeAsset->GetName();
		OutRecipe.bUseFence = false;
		OutRecipe.bUseGate = false;

		for (const FScatterRegionBuildingSlotRecipeData& SourceBuilding : RecipeAsset->BuildingSlots)
		{
			FScatterRegionBuildingSlotRecipe BuildingRecipe;
			if (!ConvertNativeBuildingSlotRecipe(SourceBuilding, BuildingRecipe, Warnings, OutError))
			{
				return false;
			}
			if (BuildingRecipe.bEnabled && BuildingRecipe.MainBuilding.Meshes.Num() > 0)
			{
				OutRecipe.BuildingSlots.Add(BuildingRecipe);
			}
		}

		if (!RecipeAsset->RoadSlot.SlotId.IsNone())
		{
			const FString SlotId = NormalizeScatterRegionSlotId(RecipeAsset->RoadSlot.SlotId);
			if (!RecipeAsset->RoadSlot.bEnabled)
			{
				OutRecipe.DisabledSlots.Add(SlotId);
			}
			else
			{
				FScatterRegionObjectRecipe SlotRecipe;
				if (!ConvertNativeBoundarySlotRecipe(RecipeAsset->RoadSlot, SlotRecipe, Warnings, OutError))
				{
					return false;
				}
				if (SlotRecipe.Meshes.Num() > 0)
				{
					OutRecipe.Slots.Add(SlotId, SlotRecipe);
				}
			}
		}

		if (Spec.RegionType == TEXT("village"))
		{
			for (const FScatterRegionBoundarySlotRecipeData& SourceSlot : RecipeAsset->BoundarySlots)
			{
				if (SourceSlot.SlotId.IsNone())
				{
					Warnings.Add(TEXT("Native ScatterRegion recipe skipped a boundary slot with empty SlotId"));
					continue;
				}
				const FString SlotId = NormalizeScatterRegionSlotId(SourceSlot.SlotId);
				FScatterRegionObjectRecipe SlotRecipe;
				if (!ConvertNativeBoundarySlotRecipe(SourceSlot, SlotRecipe, Warnings, OutError))
				{
					return false;
				}
				if (SlotRecipe.Meshes.Num() > 0)
				{
					OutRecipe.Slots.Add(SlotId, SlotRecipe);
					if (SlotId == TEXT("fence"))
					{
						OutRecipe.bUseFence = true;
					}
					else if (SlotId == TEXT("gate"))
					{
						OutRecipe.bUseGate = true;
					}
				}
			}
		}

		for (const FScatterRegionDensitySlotRecipeData& SourceSlot : RecipeAsset->DensitySlots)
		{
			if (SourceSlot.SlotId.IsNone())
			{
				Warnings.Add(TEXT("Native ScatterRegion recipe skipped a density slot with empty SlotId"));
				continue;
			}
			const FString SlotId = NormalizeScatterRegionSlotId(SourceSlot.SlotId);
			if (SlotId == TEXT("road"))
			{
				Warnings.Add(TEXT("Native ScatterRegion recipe skipped density slot 'road'; use RoadSlot instead"));
				continue;
			}
			if (!SourceSlot.bEnabled)
			{
				OutRecipe.DisabledSlots.Add(SlotId);
				continue;
			}
			FScatterRegionObjectRecipe SlotRecipe;
			if (!ConvertNativeDensitySlotRecipe(SourceSlot, SlotRecipe, Warnings, OutError))
			{
				return false;
			}
			if (SlotRecipe.Meshes.Num() > 0)
			{
				OutRecipe.Slots.Add(SlotId, SlotRecipe);
				if (SlotId == TEXT("dirt"))
				{
					OutRecipe.DirtDensity = FMath::Clamp(SourceSlot.Density, 0.0f, 1.0f);
				}
				else if (SlotId == TEXT("crops"))
				{
					OutRecipe.CropsDensity = FMath::Clamp(SourceSlot.Density, 0.0f, 1.0f);
				}
				else if (SlotId == TEXT("scarecrow"))
				{
					OutRecipe.ScarecrowDensity = FMath::Clamp(SourceSlot.Density, 0.0f, 1.0f);
				}
				else if (SlotId == TEXT("tombs"))
				{
					OutRecipe.TombDensity = FMath::Clamp(SourceSlot.Density, 0.0f, 1.0f);
				}
				else if (SlotId == TEXT("panteon"))
				{
					OutRecipe.PanteonDensity = FMath::Clamp(SourceSlot.Density, 0.0f, 1.0f);
				}
				else if (SlotId == TEXT("side_road_props"))
				{
					OutRecipe.SideRoadPropsDensity = FMath::Clamp(SourceSlot.Density, 0.0f, 1.0f);
				}
				else if (SlotId == TEXT("ScatterRegion_props"))
				{
					OutRecipe.ScatterRegionPropsDensity = FMath::Clamp(SourceSlot.Density, 0.0f, 1.0f);
				}
			}
		}

		int32 MeshCount = 0;
		for (const FScatterRegionBuildingSlotRecipe& BuildingRecipe : OutRecipe.BuildingSlots)
		{
			MeshCount += BuildingRecipe.MainBuilding.Meshes.Num();
			MeshCount += BuildingRecipe.LocalProps.Meshes.Num();
		}
		for (const TPair<FString, FScatterRegionObjectRecipe>& Pair : OutRecipe.Slots)
		{
			MeshCount += Pair.Value.Meshes.Num();
		}
		if (MeshCount == 0)
		{
			OutError = FString::Printf(TEXT("Native ScatterRegion recipe_asset produced no StaticMesh references: %s"), *Spec.RecipeAssetPath);
			return false;
		}
		return true;
	}

	FString SanitizeNamePart(const FString& Value)
	{
		FString Result = Value;
		const TCHAR* InvalidChars = TEXT(" .:/\\'");
		for (const TCHAR* Char = InvalidChars; *Char; ++Char)
		{
			Result.ReplaceCharInline(*Char, TCHAR('_'));
		}
		return Result;
	}

	UInstancedStaticMeshComponent* GetOrCreateComponent(FScatterRegionBuildContext& Context, const FString& Slot, const FScatterRegionRecipeMesh& RecipeMesh)
	{
		const FString ComponentKey = Slot + TEXT("|") + RecipeMesh.Path;
		if (UInstancedStaticMeshComponent** Existing = Context.Components.Find(ComponentKey))
		{
			return *Existing;
		}
		if (!Context.RootActor || !RecipeMesh.Mesh)
		{
			return nullptr;
		}

		const FName ComponentName = MakeUniqueObjectName(
			Context.RootActor,
			UInstancedStaticMeshComponent::StaticClass(),
			FName(*FString::Printf(TEXT("ScatterRegion_%s_Instances"), *SanitizeNamePart(Slot))));
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(Context.RootActor, ComponentName);
		Component->CreationMethod = EComponentCreationMethod::Instance;
		Component->SetStaticMesh(RecipeMesh.Mesh);
		Component->SetMobility(EComponentMobility::Static);
		Component->ComponentTags.AddUnique(FName(*FString::Printf(TEXT("ScatterRegion.Slot.%s"), *Slot)));
		Component->AttachToComponent(Context.RootActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		Context.RootActor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		Context.Components.Add(ComponentKey, Component);
		Context.ComponentCount++;
		return Component;
	}

	const FScatterRegionObjectRecipe* RecipeForSlot(const FScatterRegionRecipe& Recipe, const FString& Slot)
	{
		return Recipe.Slots.Find(Slot);
	}

	const FScatterRegionRecipeMesh* PickMesh(const FScatterRegionObjectRecipe* Recipe, FRandomStream& Stream)
	{
		if (!Recipe || Recipe->Meshes.Num() == 0)
		{
			return nullptr;
		}

		float TotalWeight = 0.0f;
		for (const FScatterRegionRecipeMesh& Mesh : Recipe->Meshes)
		{
			TotalWeight += FMath::Max(0.0f, Mesh.Weight);
		}
		if (TotalWeight <= KINDA_SMALL_NUMBER)
		{
			return &Recipe->Meshes[Stream.RandRange(0, Recipe->Meshes.Num() - 1)];
		}

		float Pick = Stream.FRandRange(0.0f, TotalWeight);
		for (const FScatterRegionRecipeMesh& Mesh : Recipe->Meshes)
		{
			Pick -= FMath::Max(0.0f, Mesh.Weight);
			if (Pick <= 0.0f)
			{
				return &Mesh;
			}
		}
		return &Recipe->Meshes.Last();
	}

	const FScatterRegionBuildingSlotRecipe* PickBuildingSlot(const FScatterRegionRecipe& Recipe, FRandomStream& Stream)
	{
		if (Recipe.BuildingSlots.Num() == 0)
		{
			return nullptr;
		}

		float TotalWeight = 0.0f;
		for (const FScatterRegionBuildingSlotRecipe& BuildingSlot : Recipe.BuildingSlots)
		{
			if (BuildingSlot.bEnabled && BuildingSlot.MainBuilding.Meshes.Num() > 0)
			{
				TotalWeight += FMath::Max(0.0f, BuildingSlot.Weight);
			}
		}
		if (TotalWeight <= KINDA_SMALL_NUMBER)
		{
			return &Recipe.BuildingSlots[Stream.RandRange(0, Recipe.BuildingSlots.Num() - 1)];
		}

		float Pick = Stream.FRandRange(0.0f, TotalWeight);
		for (const FScatterRegionBuildingSlotRecipe& BuildingSlot : Recipe.BuildingSlots)
		{
			if (!BuildingSlot.bEnabled || BuildingSlot.MainBuilding.Meshes.Num() == 0)
			{
				continue;
			}
			Pick -= FMath::Max(0.0f, BuildingSlot.Weight);
			if (Pick <= 0.0f)
			{
				return &BuildingSlot;
			}
		}
		return &Recipe.BuildingSlots.Last();
	}

	FScatterRegionObjectRecipe BuildScatterRegionPropsFallbackFromBuildingLocalProps(const FScatterRegionRecipe& Recipe)
	{
		FScatterRegionObjectRecipe Fallback;
		Fallback.SourcePath = TEXT("building_local_props_fallback");
		Fallback.Density = Recipe.ScatterRegionPropsDensity;
		for (const FScatterRegionBuildingSlotRecipe& BuildingSlot : Recipe.BuildingSlots)
		{
			for (const FScatterRegionRecipeMesh& Mesh : BuildingSlot.LocalProps.Meshes)
			{
				if (Mesh.Mesh)
				{
					Fallback.Meshes.Add(Mesh);
				}
			}
		}
		return Fallback;
	}

	FVector PickScale(const FScatterRegionRecipeMesh& Mesh, FRandomStream& Stream)
	{
		if (Mesh.bUniformScale)
		{
			const float MinValue = FMath::Min3(Mesh.ScaleMin.X, Mesh.ScaleMin.Y, Mesh.ScaleMin.Z);
			const float MaxValue = FMath::Max3(Mesh.ScaleMax.X, Mesh.ScaleMax.Y, Mesh.ScaleMax.Z);
			const float Value = Stream.FRandRange(MinValue, FMath::Max(MinValue, MaxValue));
			return FVector(Value);
		}

		return FVector(
			Stream.FRandRange(Mesh.ScaleMin.X, FMath::Max(Mesh.ScaleMin.X, Mesh.ScaleMax.X)),
			Stream.FRandRange(Mesh.ScaleMin.Y, FMath::Max(Mesh.ScaleMin.Y, Mesh.ScaleMax.Y)),
			Stream.FRandRange(Mesh.ScaleMin.Z, FMath::Max(Mesh.ScaleMin.Z, Mesh.ScaleMax.Z)));
	}

	FVector FixedScale(const FScatterRegionRecipeMesh& Mesh)
	{
		if (Mesh.bUniformScale)
		{
			const float MinValue = FMath::Min3(Mesh.ScaleMin.X, Mesh.ScaleMin.Y, Mesh.ScaleMin.Z);
			const float MaxValue = FMath::Max3(Mesh.ScaleMax.X, Mesh.ScaleMax.Y, Mesh.ScaleMax.Z);
			return FVector((MinValue + FMath::Max(MinValue, MaxValue)) * 0.5f);
		}

		return FVector(
			(Mesh.ScaleMin.X + FMath::Max(Mesh.ScaleMin.X, Mesh.ScaleMax.X)) * 0.5f,
			(Mesh.ScaleMin.Y + FMath::Max(Mesh.ScaleMin.Y, Mesh.ScaleMax.Y)) * 0.5f,
			(Mesh.ScaleMin.Z + FMath::Max(Mesh.ScaleMin.Z, Mesh.ScaleMax.Z)) * 0.5f);
	}

	float MeshPlanarRadius(const FScatterRegionRecipeMesh& Mesh, float DefaultRadius)
	{
		if (!Mesh.Mesh)
		{
			return DefaultRadius;
		}
		const FVector Extent = Mesh.Mesh->GetBounds().BoxExtent;
		const FVector Scale = FixedScale(Mesh);
		return FMath::Max(DefaultRadius, FMath::Max(Extent.X * Scale.X, Extent.Y * Scale.Y));
	}

	float GroundedZ(const FScatterRegionRecipeMesh& RecipeMesh, const FVector& Scale)
	{
		if (!RecipeMesh.Mesh)
		{
			return 0.0f;
		}
		const FBoxSphereBounds Bounds = RecipeMesh.Mesh->GetBounds();
		return -(Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale.Z;
	}

	float MeshFixedHeightCm(const FScatterRegionRecipeMesh& Mesh)
	{
		if (!Mesh.Mesh)
		{
			return 0.0f;
		}
		const FBoxSphereBounds Bounds = Mesh.Mesh->GetBounds();
		const FVector Scale = FixedScale(Mesh);
		return FMath::Max(0.0f, Bounds.BoxExtent.Z * 2.0f * Scale.Z);
	}

	float RecipeFixedHeightCm(const FScatterRegionObjectRecipe* Recipe, float DefaultHeightCm)
	{
		if (!Recipe)
		{
			return DefaultHeightCm;
		}
		for (const FScatterRegionRecipeMesh& Mesh : Recipe->Meshes)
		{
			const float Height = MeshFixedHeightCm(Mesh);
			if (Height > KINDA_SMALL_NUMBER)
			{
				return Height;
			}
		}
		return DefaultHeightCm;
	}

	bool IsLandscapeTraceHit(const FHitResult& Hit)
	{
		if (Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>())
		{
			return true;
		}
		if (Hit.GetComponent() && Hit.GetComponent()->GetClass()->GetName().Contains(TEXT("Landscape")))
		{
			return true;
		}
		return false;
	}

	bool ProjectLocalGroundToLandscape(FScatterRegionBuildContext& Context, const FVector& LocalLocation, FVector& OutLocalLocation)
	{
		OutLocalLocation = LocalLocation;
		Context.ProjectionAttempts++;
		if (!Context.World)
		{
			Context.ProjectionMisses++;
			return false;
		}

		const FVector WorldXY = Context.Center + FVector(LocalLocation.X, LocalLocation.Y, 0.0f);
		const FVector Start(WorldXY.X, WorldXY.Y, Context.Center.Z + LocalLocation.Z + 200000.0f);
		const FVector End(WorldXY.X, WorldXY.Y, Context.Center.Z + LocalLocation.Z - 200000.0f);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ScatterRegionLandscapeProjection), true);
		if (Context.RootActor)
		{
			QueryParams.AddIgnoredActor(Context.RootActor);
		}
		for (const TWeakObjectPtr<AActor>& IgnoredActor : Context.ProjectionIgnoredActors)
		{
			if (IgnoredActor.IsValid())
			{
				QueryParams.AddIgnoredActor(IgnoredActor.Get());
			}
		}

		TArray<FHitResult> Hits;
		Context.World->LineTraceMultiByChannel(Hits, Start, End, ECC_Visibility, QueryParams);
		for (const FHitResult& Hit : Hits)
		{
			if (IsLandscapeTraceHit(Hit))
			{
				OutLocalLocation.Z = Hit.ImpactPoint.Z - Context.Center.Z;
				Context.ProjectionHits++;
				return true;
			}
		}

		FHitResult Hit;
		if (!Context.World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, QueryParams) || !IsLandscapeTraceHit(Hit))
		{
			Context.ProjectionMisses++;
			return false;
		}

		OutLocalLocation.Z = Hit.ImpactPoint.Z - Context.Center.Z;
		Context.ProjectionHits++;
		return true;
	}

	bool TraceLandscapeZ(FScatterRegionBuildContext& Context, const FVector2D& LocalLocation, float& OutLocalZ)
	{
		if (!Context.World)
		{
			return false;
		}

		const FVector WorldXY = Context.Center + FVector(LocalLocation.X, LocalLocation.Y, 0.0f);
		const FVector Start(WorldXY.X, WorldXY.Y, Context.Center.Z + 200000.0f);
		const FVector End(WorldXY.X, WorldXY.Y, Context.Center.Z - 200000.0f);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ScatterRegionLandscapeSlopeTrace), true);
		if (Context.RootActor)
		{
			QueryParams.AddIgnoredActor(Context.RootActor);
		}
		for (const TWeakObjectPtr<AActor>& IgnoredActor : Context.ProjectionIgnoredActors)
		{
			if (IgnoredActor.IsValid())
			{
				QueryParams.AddIgnoredActor(IgnoredActor.Get());
			}
		}

		TArray<FHitResult> Hits;
		Context.World->LineTraceMultiByChannel(Hits, Start, End, ECC_Visibility, QueryParams);
		for (const FHitResult& Hit : Hits)
		{
			if (IsLandscapeTraceHit(Hit))
			{
				OutLocalZ = Hit.ImpactPoint.Z - Context.Center.Z;
				return true;
			}
		}
		return false;
	}

	TArray<FScatterRegionGraphPoint> ScatterRegionSlopeFilter(FScatterRegionBuildContext& Context, const TArray<FScatterRegionGraphPoint>& Points, float MaxSlopeDegrees, float SampleRadiusCm)
	{
		TArray<FScatterRegionGraphPoint> Result;
		Result.Reserve(Points.Num());
		const float EffectiveMaxSlope = FMath::Clamp(MaxSlopeDegrees, 0.0f, 89.0f);
		for (const FScatterRegionGraphPoint& Point : Points)
		{
			float CenterZ = 0.0f;
			float XZ = 0.0f;
			float YZ = 0.0f;
			if (!TraceLandscapeZ(Context, Point.Location, CenterZ) ||
				!TraceLandscapeZ(Context, Point.Location + FVector2D(SampleRadiusCm, 0.0f), XZ) ||
				!TraceLandscapeZ(Context, Point.Location + FVector2D(0.0f, SampleRadiusCm), YZ))
			{
				Result.Add(Point);
				continue;
			}

			const float SlopeX = FMath::Atan2(FMath::Abs(XZ - CenterZ), SampleRadiusCm);
			const float SlopeY = FMath::Atan2(FMath::Abs(YZ - CenterZ), SampleRadiusCm);
			const float SlopeDegrees = FMath::RadiansToDegrees(FMath::Max(SlopeX, SlopeY));
			if (SlopeDegrees <= EffectiveMaxSlope)
			{
				Result.Add(Point);
			}
		}
		return Result;
	}

	float MeshMajorStep(const FScatterRegionObjectRecipe* Recipe, float DefaultStep)
	{
		if (!Recipe || Recipe->Meshes.Num() == 0 || !Recipe->Meshes[0].Mesh)
		{
			return DefaultStep;
		}
		const FVector Extent = Recipe->Meshes[0].Mesh->GetBounds().BoxExtent;
		const float Major = FMath::Max(Extent.X, Extent.Y) * 2.0f;
		return FMath::Clamp(Major * 0.95f, DefaultStep * 0.35f, DefaultStep * 2.0f);
	}

	float MeshFixedMajorStep(const FScatterRegionRecipeMesh& Mesh, float DefaultStep)
	{
		if (!Mesh.Mesh)
		{
			return DefaultStep;
		}
		const FVector Extent = Mesh.Mesh->GetBounds().BoxExtent;
		const FVector Scale = FixedScale(Mesh);
		const float Major = FMath::Max(Extent.X * Scale.X, Extent.Y * Scale.Y) * 2.0f;
		return FMath::Max(100.0f, Major);
	}

	float RecipeBoundsRadius(const FScatterRegionObjectRecipe* Recipe, float DefaultRadius)
	{
		if (!Recipe)
		{
			return DefaultRadius;
		}

		float Radius = 0.0f;
		for (const FScatterRegionRecipeMesh& Mesh : Recipe->Meshes)
		{
			if (!Mesh.Mesh)
			{
				continue;
			}
			const FVector Extent = Mesh.Mesh->GetBounds().BoxExtent;
			const FVector ScaleMax = Mesh.bUniformScale ? FVector(FMath::Max3(Mesh.ScaleMax.X, Mesh.ScaleMax.Y, Mesh.ScaleMax.Z)) : Mesh.ScaleMax;
			Radius = FMath::Max(Radius, FMath::Max(Extent.X * ScaleMax.X, Extent.Y * ScaleMax.Y));
		}
		return Radius > KINDA_SMALL_NUMBER ? Radius : DefaultRadius;
	}

	FVector2D RecipePlanarSize(const FScatterRegionObjectRecipe* Recipe, const FVector2D& DefaultSize)
	{
		if (!Recipe)
		{
			return DefaultSize;
		}
		for (const FScatterRegionRecipeMesh& Mesh : Recipe->Meshes)
		{
			if (!Mesh.Mesh)
			{
				continue;
			}
			const FVector Extent = Mesh.Mesh->GetBounds().BoxExtent;
			const FVector Scale = FixedScale(Mesh);
			return FVector2D(
				FMath::Max(1.0f, Extent.X * Scale.X * 2.0f),
				FMath::Max(1.0f, Extent.Y * Scale.Y * 2.0f));
		}
		return DefaultSize;
	}

	bool AddMeshInstance(
		FScatterRegionBuildContext& Context,
		FRandomStream& Stream,
		const FScatterRegionRecipeMesh& RecipeMesh,
		const FString& CountSlot,
		const FVector& GroundLocation,
		float YawDegrees,
		const FVector& ScaleMultiplier = FVector::OneVector,
		bool bRandomScale = true,
		float VerticalOffsetCm = 0.0f)
	{
		if (!RecipeMesh.Mesh)
		{
			return false;
		}

		UInstancedStaticMeshComponent* Component = GetOrCreateComponent(Context, CountSlot, RecipeMesh);
		if (!Component)
		{
			return false;
		}

		const FVector Scale = (bRandomScale ? PickScale(RecipeMesh, Stream) : FixedScale(RecipeMesh)) * ScaleMultiplier;
		FVector ProjectedGroundLocation = GroundLocation;
		ProjectLocalGroundToLandscape(Context, GroundLocation, ProjectedGroundLocation);
		FVector Location = ProjectedGroundLocation;
		Location.Z += GroundedZ(RecipeMesh, Scale);
		Location.Z += VerticalOffsetCm;
		const FTransform Transform(FRotator(0.0f, YawDegrees, 0.0f), Location, Scale);
		Component->AddInstance(Transform);

		Context.InstanceCount++;
		Context.SlotCounts.FindOrAdd(CountSlot)++;
		FTransform WorldTransform = Transform;
		WorldTransform.SetLocation(Context.Center + Location);
		Context.Bounds += RecipeMesh.Mesh->GetBoundingBox().TransformBy(WorldTransform);
		return true;
	}

	bool AddInstance(
		FScatterRegionBuildContext& Context,
		FRandomStream& Stream,
		const FScatterRegionObjectRecipe* Recipe,
		const FString& CountSlot,
		const FVector& GroundLocation,
		float YawDegrees,
		const FVector& ScaleMultiplier = FVector::OneVector,
		bool bRandomScale = true,
		float VerticalOffsetCm = 0.0f)
	{
		const FScatterRegionRecipeMesh* RecipeMesh = PickMesh(Recipe, Stream);
		if (!RecipeMesh || !RecipeMesh->Mesh)
		{
			return false;
		}

		return AddMeshInstance(Context, Stream, *RecipeMesh, CountSlot, GroundLocation, YawDegrees, ScaleMultiplier, bRandomScale, VerticalOffsetCm);
	}

	bool AddRecipeInstance(
		FScatterRegionBuildContext& Context,
		FRandomStream& Stream,
		const FScatterRegionObjectRecipe* Recipe,
		const FString& CountSlot,
		const FVector& GroundLocation,
		float YawDegrees,
		const FVector& ScaleMultiplier = FVector::OneVector,
		float VerticalOffsetCm = 0.0f)
	{
		return AddInstance(Context, Stream, Recipe, CountSlot, GroundLocation, YawDegrees, ScaleMultiplier, true, VerticalOffsetCm);
	}

	bool AddFixedRecipeInstance(
		FScatterRegionBuildContext& Context,
		FRandomStream& Stream,
		const FScatterRegionObjectRecipe* Recipe,
		const FString& CountSlot,
		const FVector& GroundLocation,
		float YawDegrees,
		const FVector& ScaleMultiplier = FVector::OneVector,
		float VerticalOffsetCm = 0.0f)
	{
		return AddInstance(Context, Stream, Recipe, CountSlot, GroundLocation, YawDegrees, ScaleMultiplier, false, VerticalOffsetCm);
	}

	bool AddRegularRoadTile(
		FScatterRegionBuildContext& Context,
		FRandomStream& Stream,
		const FScatterRegionObjectRecipe* Recipe,
		const FString& CountSlot,
		const FVector& GroundLocation,
		float YawDegrees)
	{
		return AddInstance(Context, Stream, Recipe, CountSlot, GroundLocation, YawDegrees, FVector::OneVector, false);
	}

	void WarnIfMissing(FScatterRegionBuildContext& Context, const FScatterRegionRecipe& Recipe, const FString& Slot)
	{
		if (Recipe.DisabledSlots.Contains(Slot))
		{
			return;
		}
		const FScatterRegionObjectRecipe* ObjectRecipe = RecipeForSlot(Recipe, Slot);
		if (!ObjectRecipe || ObjectRecipe->Meshes.Num() == 0)
		{
			Context.Warnings.Add(FString::Printf(TEXT("ScatterRegion recipe has no mesh for slot '%s'"), *Slot));
		}
	}

	FScatterRegionGraphProfile ProfileForRegion(const FScatterRegionSpec& Spec, const FScatterRegionRecipe& Recipe)
	{
		FScatterRegionGraphProfile Profile;
		if (Spec.RegionType == TEXT("farm"))
		{
			Profile.NumberOfExit = 3;
			Profile.RoadWidth = 100.0f;
			Profile.RoadDensity = 0.5f;
			Profile.RoadPattern = TEXT("grid");
			Profile.BuildingDistanceFromEdge = 50.0f;
			Profile.ExitRemoveFenceScaleOffset = 1.0f;
			Profile.ScatterRegionFenceLength = 50.0f;
			Profile.HouseFenceLength = 50.0f;
		}
		else if (Spec.RegionType == TEXT("cemetery"))
		{
			Profile.NumberOfExit = 2;
			Profile.RoadWidth = 50.0f;
			Profile.RoadDensity = 0.35f;
			Profile.RoadPattern = TEXT("radial");
			Profile.BuildingDistanceFromEdge = 50.0f;
			Profile.ExitRemoveFenceOffset = FVector::ZeroVector;
			Profile.ExitRemoveFenceScaleOffset = 1.0f;
			Profile.ScatterRegionFenceLength = 50.0f;
			Profile.HouseFenceLength = 50.0f;
		}

		Profile.RoadDensity = FMath::Clamp(Profile.RoadDensity, 0.05f, 1.0f);
		Profile.RoadWidth = FMath::Clamp(Profile.RoadWidth, 20.0f, 500.0f);
		Profile.RoadRandomness = FMath::Clamp(Profile.RoadRandomness, 0.0f, 1.5f);
		Profile.BuildingDistance = FMath::Clamp(Profile.BuildingDistance, 800.0f, 12000.0f);
		Profile.RoadSidePropsDistance = FMath::Clamp(Profile.RoadSidePropsDistance, 150.0f, 5000.0f);
		Profile.BuildingMaxSlope = Recipe.BuildingMaxSlope;
		Profile.ScatterRegionFenceLength = FMath::Max(Profile.ScatterRegionFenceLength, MeshMajorStep(RecipeForSlot(Recipe, TEXT("fence")), 250.0f));
		return Profile;
	}

	FVector2D Rotate2D(const FVector2D& Value, float AngleRadians)
	{
		const float CosA = FMath::Cos(AngleRadians);
		const float SinA = FMath::Sin(AngleRadians);
		return FVector2D(Value.X * CosA - Value.Y * SinA, Value.X * SinA + Value.Y * CosA);
	}

	float SegmentYawDegrees(const FVector2D& Start, const FVector2D& End)
	{
		const FVector2D Delta = End - Start;
		return FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	}

	FVector2D SegmentDirection(const FScatterRegionRoadSegment& Segment)
	{
		FVector2D Direction = Segment.End - Segment.Start;
		const float Size = Direction.Size();
		if (Size <= KINDA_SMALL_NUMBER)
		{
			return FVector2D(1.0f, 0.0f);
		}
		return Direction / Size;
	}

	FVector2D SegmentNormal(const FScatterRegionRoadSegment& Segment)
	{
		const FVector2D Direction = SegmentDirection(Segment);
		return FVector2D(-Direction.Y, Direction.X);
	}

	FVector2D BoundaryPointAtAngle(float HalfSizeCm, float AngleRadians, FRandomStream& Stream, float Randomness)
	{
		const float CosA = FMath::Cos(AngleRadians);
		const float SinA = FMath::Sin(AngleRadians);
		const float MaxAxis = FMath::Max(FMath::Abs(CosA), FMath::Abs(SinA));
		const float BaseRadius = MaxAxis > KINDA_SMALL_NUMBER ? HalfSizeCm / MaxAxis : HalfSizeCm;
		const float Noise = 1.0f + Stream.FRandRange(-0.08f, 0.08f) * Randomness;
		return FVector2D(CosA, SinA) * BaseRadius * Noise;
	}

	TArray<FVector2D> BuildScatterRegionBoundary(float HalfSizeCm, FRandomStream& Stream, const FScatterRegionGraphProfile& Profile)
	{
		TArray<FVector2D> Points;
		const int32 PointCount = 20;
		Points.Reserve(PointCount);
		for (int32 Index = 0; Index < PointCount; ++Index)
		{
			const float Angle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(PointCount);
			Points.Add(BoundaryPointAtAngle(HalfSizeCm, Angle, Stream, Profile.RoadRandomness));
		}
		return Points;
	}

	TArray<FVector2D> SmoothClosedBoundary(const TArray<FVector2D>& Boundary, int32 Iterations)
	{
		TArray<FVector2D> Result = Boundary;
		for (int32 Iteration = 0; Iteration < Iterations; ++Iteration)
		{
			if (Result.Num() < 3)
			{
				break;
			}

			TArray<FVector2D> Smoothed;
			Smoothed.Reserve(Result.Num() * 2);
			for (int32 Index = 0; Index < Result.Num(); ++Index)
			{
				const FVector2D& A = Result[Index];
				const FVector2D& B = Result[(Index + 1) % Result.Num()];
				Smoothed.Add(FMath::Lerp(A, B, 0.25f));
				Smoothed.Add(FMath::Lerp(A, B, 0.75f));
			}
			Result = MoveTemp(Smoothed);
		}
		return Result;
	}

	TArray<FVector2D> BuildClosedRegionBoundary(float HalfSizeCm, FRandomStream& Stream, const FScatterRegionGraphProfile& Profile, float RadiusScale = 0.94f)
	{
		TArray<FVector2D> Points = BuildScatterRegionBoundary(HalfSizeCm * FMath::Clamp(RadiusScale, 0.2f, 1.0f), Stream, Profile);
		return SmoothClosedBoundary(Points, 2);
	}

	float ClosedPolylineLength(const TArray<FVector2D>& Points)
	{
		if (Points.Num() < 2)
		{
			return 0.0f;
		}

		float TotalLength = 0.0f;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			TotalLength += FVector2D::Distance(Points[Index], Points[(Index + 1) % Points.Num()]);
		}
		return TotalLength;
	}

	float NormalizePerimeterDistance(float Distance, float TotalLength)
	{
		if (TotalLength <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}
		float Normalized = FMath::Fmod(Distance, TotalLength);
		if (Normalized < 0.0f)
		{
			Normalized += TotalLength;
		}
		return Normalized;
	}

	FScatterRegionPerimeterSample SampleClosedPolylineAtDistance(const TArray<FVector2D>& Points, float Distance)
	{
		FScatterRegionPerimeterSample Sample;
		if (Points.Num() == 0)
		{
			return Sample;
		}
		if (Points.Num() == 1)
		{
			Sample.Location = Points[0];
			return Sample;
		}

		const float TotalLength = ClosedPolylineLength(Points);
		float Remaining = NormalizePerimeterDistance(Distance, TotalLength);
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const FVector2D Start = Points[Index];
			const FVector2D End = Points[(Index + 1) % Points.Num()];
			const float SegmentLength = FVector2D::Distance(Start, End);
			if (SegmentLength <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			if (Remaining <= SegmentLength)
			{
				const float T = FMath::Clamp(Remaining / SegmentLength, 0.0f, 1.0f);
				Sample.Location = FMath::Lerp(Start, End, T);
				Sample.YawDegrees = SegmentYawDegrees(Start, End);
				return Sample;
			}
			Remaining -= SegmentLength;
		}

		Sample.Location = Points.Last();
		Sample.YawDegrees = SegmentYawDegrees(Points.Last(), Points[0]);
		return Sample;
	}

	float FindLowestYDistanceOnClosedPolyline(const TArray<FVector2D>& Points)
	{
		const float TotalLength = ClosedPolylineLength(Points);
		if (TotalLength <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		float BestDistance = 0.0f;
		float BestY = TNumericLimits<float>::Max();
		const int32 SampleCount = FMath::Clamp(FMath::RoundToInt(TotalLength / 100.0f), 80, 360);
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			const float Distance = TotalLength * static_cast<float>(Index) / static_cast<float>(SampleCount);
			const FScatterRegionPerimeterSample Sample = SampleClosedPolylineAtDistance(Points, Distance);
			if (Sample.Location.Y < BestY)
			{
				BestY = Sample.Location.Y;
				BestDistance = Distance;
			}
		}
		return BestDistance;
	}

	float RecipeFixedMajorStep(const FScatterRegionObjectRecipe* Recipe, float DefaultStep)
	{
		if (!Recipe || Recipe->Meshes.Num() == 0)
		{
			return DefaultStep;
		}
		return MeshFixedMajorStep(Recipe->Meshes[0], DefaultStep);
	}

	bool IsInsideRegion(const FVector2D& Location, float HalfSizeCm)
	{
		const float MaxAbs = FMath::Max(FMath::Abs(Location.X), FMath::Abs(Location.Y));
		return MaxAbs <= HalfSizeCm * 0.94f;
	}

	bool IsPointInsidePolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon)
	{
		if (Polygon.Num() < 3)
		{
			return false;
		}

		bool bInside = false;
		for (int32 Index = 0, PrevIndex = Polygon.Num() - 1; Index < Polygon.Num(); PrevIndex = Index++)
		{
			const FVector2D& A = Polygon[Index];
			const FVector2D& B = Polygon[PrevIndex];
			const bool bCrossesY = (A.Y > Point.Y) != (B.Y > Point.Y);
			if (!bCrossesY)
			{
				continue;
			}
			const float Denominator = B.Y - A.Y;
			if (FMath::Abs(Denominator) <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const float IntersectX = (B.X - A.X) * (Point.Y - A.Y) / Denominator + A.X;
			if (Point.X < IntersectX)
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	bool IsInsideClosedRegion(const FVector2D& Location, const TArray<FVector2D>& Boundary, float HalfSizeCm)
	{
		return Boundary.Num() >= 3 ? IsPointInsidePolygon(Location, Boundary) : IsInsideRegion(Location, HalfSizeCm);
	}

	FVector2D FindFarthestInsideAlongRay(
		const TArray<FVector2D>& Boundary,
		float HalfSizeCm,
		const FVector2D& Start,
		const FVector2D& Direction,
		float MaxDistanceCm,
		float StepCm)
	{
		FVector2D SafeDirection = Direction.GetSafeNormal();
		if (SafeDirection.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			return Start;
		}

		FVector2D LastInside = Start;
		const float ClampedStep = FMath::Max(100.0f, StepCm);
		for (float Distance = ClampedStep; Distance <= MaxDistanceCm; Distance += ClampedStep)
		{
			const FVector2D Candidate = Start + SafeDirection * Distance;
			if (!IsInsideClosedRegion(Candidate, Boundary, HalfSizeCm))
			{
				break;
			}
			LastInside = Candidate;
		}
		return LastInside;
	}

	bool IsTileInsidePolygon(const FVector2D& Center, const FVector2D& Size, float YawDegrees, const TArray<FVector2D>& Polygon)
	{
		if (!IsPointInsidePolygon(Center, Polygon))
		{
			return false;
		}
		const float YawRadians = FMath::DegreesToRadians(YawDegrees);
		const FVector2D HalfSize = Size * 0.5f;
		const FVector2D Corners[] = {
			FVector2D(-HalfSize.X, -HalfSize.Y),
			FVector2D(HalfSize.X, -HalfSize.Y),
			FVector2D(HalfSize.X, HalfSize.Y),
			FVector2D(-HalfSize.X, HalfSize.Y)
		};
		for (const FVector2D& Corner : Corners)
		{
			if (!IsPointInsidePolygon(Center + Rotate2D(Corner, YawRadians), Polygon))
			{
				return false;
			}
		}
		return true;
	}

	bool IsNearAnyPlacement(const FScatterRegionPlacementSet& PlacementSet, const FVector2D& Location, float MinDistanceCm)
	{
		const float MinDistanceSq = MinDistanceCm * MinDistanceCm;
		for (const FVector2D& Existing : PlacementSet.Locations)
		{
			if (FVector2D::DistSquared(Existing, Location) < MinDistanceSq)
			{
				return true;
			}
		}
		return false;
	}

	bool ReservePlacement(FScatterRegionPlacementSet& PlacementSet, const FVector2D& Location, float MinDistanceCm)
	{
		if (IsNearAnyPlacement(PlacementSet, Location, MinDistanceCm))
		{
			return false;
		}
		PlacementSet.Locations.Add(Location);
		return true;
	}

	bool ReserveCollision(FScatterRegionCollisionSet& CollisionSet, const FVector2D& Location, float RadiusCm, float ExtraPaddingCm = 0.0f)
	{
		const float ClampedRadius = FMath::Max(1.0f, RadiusCm);
		for (int32 Index = 0; Index < CollisionSet.Locations.Num(); ++Index)
		{
			const float ExistingRadius = CollisionSet.Radii.IsValidIndex(Index) ? CollisionSet.Radii[Index] : 1.0f;
			const float MinDistance = ClampedRadius + ExistingRadius + FMath::Max(0.0f, ExtraPaddingCm);
			if (FVector2D::DistSquared(CollisionSet.Locations[Index], Location) < FMath::Square(MinDistance))
			{
				return false;
			}
		}
		CollisionSet.Locations.Add(Location);
		CollisionSet.Radii.Add(ClampedRadius);
		return true;
	}

	bool ReserveRoadTile(FScatterRegionRoadTileSet& TileSet, const FVector2D& Location, float YawDegrees, float RadiusCm)
	{
		const float ClampedRadius = FMath::Max(1.0f, RadiusCm);
		for (int32 Index = 0; Index < TileSet.Locations.Num(); ++Index)
		{
			const float ExistingRadius = TileSet.Radii.IsValidIndex(Index) ? TileSet.Radii[Index] : ClampedRadius;
			const float MinDistance = FMath::Max(ClampedRadius, ExistingRadius);
			if (FVector2D::DistSquared(TileSet.Locations[Index], Location) < FMath::Square(MinDistance))
			{
				return false;
			}
		}

		TileSet.Locations.Add(Location);
		TileSet.YawDegrees.Add(YawDegrees);
		TileSet.Radii.Add(ClampedRadius);
		return true;
	}

	float DistancePointToSegmentSquared(const FVector2D& Point, const FVector2D& Start, const FVector2D& End, FVector2D* OutClosestPoint = nullptr)
	{
		const FVector2D Segment = End - Start;
		const float LengthSq = Segment.SizeSquared();
		if (LengthSq <= KINDA_SMALL_NUMBER)
		{
			if (OutClosestPoint)
			{
				*OutClosestPoint = Start;
			}
			return FVector2D::DistSquared(Point, Start);
		}
		const float T = FMath::Clamp(FVector2D::DotProduct(Point - Start, Segment) / LengthSq, 0.0f, 1.0f);
		const FVector2D Closest = Start + Segment * T;
		if (OutClosestPoint)
		{
			*OutClosestPoint = Closest;
		}
		return FVector2D::DistSquared(Point, Closest);
	}

	float DistanceToRoadsSquared(const FVector2D& Point, const TArray<FScatterRegionRoadSegment>& Segments, FVector2D* OutClosestPoint = nullptr, float* OutRoadYawDegrees = nullptr)
	{
		float BestDistanceSq = TNumericLimits<float>::Max();
		FVector2D BestClosest = FVector2D::ZeroVector;
		float BestYaw = 0.0f;
		for (const FScatterRegionRoadSegment& Segment : Segments)
		{
			FVector2D Closest;
			const float DistanceSq = DistancePointToSegmentSquared(Point, Segment.Start, Segment.End, &Closest);
			if (DistanceSq < BestDistanceSq)
			{
				BestDistanceSq = DistanceSq;
				BestClosest = Closest;
				BestYaw = Segment.YawDegrees;
			}
		}
		if (OutClosestPoint)
		{
			*OutClosestPoint = BestClosest;
		}
		if (OutRoadYawDegrees)
		{
			*OutRoadYawDegrees = BestYaw;
		}
		return BestDistanceSq;
	}

	bool SegmentIntersectsAabb2D(const FVector2D& Start, const FVector2D& End, const FVector2D& BoxMin, const FVector2D& BoxMax)
	{
		const FVector2D Delta = End - Start;
		float TMin = 0.0f;
		float TMax = 1.0f;

		auto ClipAxis = [&](float StartValue, float DeltaValue, float MinValue, float MaxValue) -> bool
		{
			if (FMath::Abs(DeltaValue) <= KINDA_SMALL_NUMBER)
			{
				return StartValue >= MinValue && StartValue <= MaxValue;
			}

			float AxisMin = (MinValue - StartValue) / DeltaValue;
			float AxisMax = (MaxValue - StartValue) / DeltaValue;
			if (AxisMin > AxisMax)
			{
				Swap(AxisMin, AxisMax);
			}
			TMin = FMath::Max(TMin, AxisMin);
			TMax = FMath::Min(TMax, AxisMax);
			return TMin <= TMax;
		};

		return ClipAxis(Start.X, Delta.X, BoxMin.X, BoxMax.X)
			&& ClipAxis(Start.Y, Delta.Y, BoxMin.Y, BoxMax.Y);
	}

	bool RoadSegmentOverlapsTile(const FScatterRegionRoadSegment& Segment, const FVector2D& TileCenter, const FVector2D& TileSize, float RoadHalfWidthCm)
	{
		const FVector2D ExpandedHalfSize = TileSize * 0.5f + FVector2D(RoadHalfWidthCm, RoadHalfWidthCm);
		return SegmentIntersectsAabb2D(Segment.Start, Segment.End, TileCenter - ExpandedHalfSize, TileCenter + ExpandedHalfSize);
	}

	bool AnyRoadOverlapsTile(const TArray<FScatterRegionRoadSegment>& RoadSegments, const FVector2D& TileCenter, const FVector2D& TileSize, float RoadHalfWidthCm)
	{
		for (const FScatterRegionRoadSegment& Segment : RoadSegments)
		{
			if (RoadSegmentOverlapsTile(Segment, TileCenter, TileSize, RoadHalfWidthCm))
			{
				return true;
			}
		}
		return false;
	}

	bool RoadTileFootprintOverlapsDirtTile(const FScatterRegionRoadTileFootprint& RoadTile, const FVector2D& DirtCenter, const FVector2D& DirtSize)
	{
		constexpr float RoadDirtGapCm = 8.0f;
		const FVector2D Delta = RoadTile.Center - DirtCenter;
		const FVector2D DirtHalf = DirtSize * 0.5f;
		const FVector2D RoadHalf = RoadTile.Size * 0.5f;
		const float YawRadians = FMath::DegreesToRadians(RoadTile.YawDegrees);
		const FVector2D RoadAxis(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
		const FVector2D RoadNormal(-RoadAxis.Y, RoadAxis.X);

		const float RoadProjectX = FMath::Abs(RoadAxis.X) * RoadHalf.X + FMath::Abs(RoadNormal.X) * RoadHalf.Y;
		const float RoadProjectY = FMath::Abs(RoadAxis.Y) * RoadHalf.X + FMath::Abs(RoadNormal.Y) * RoadHalf.Y;
		if (FMath::Abs(Delta.X) > DirtHalf.X + RoadProjectX + RoadDirtGapCm)
		{
			return false;
		}
		if (FMath::Abs(Delta.Y) > DirtHalf.Y + RoadProjectY + RoadDirtGapCm)
		{
			return false;
		}

		const float DirtProjectOnRoadAxis = FMath::Abs(RoadAxis.X) * DirtHalf.X + FMath::Abs(RoadAxis.Y) * DirtHalf.Y;
		const float DirtProjectOnRoadNormal = FMath::Abs(RoadNormal.X) * DirtHalf.X + FMath::Abs(RoadNormal.Y) * DirtHalf.Y;
		if (FMath::Abs(FVector2D::DotProduct(Delta, RoadAxis)) > RoadHalf.X + DirtProjectOnRoadAxis + RoadDirtGapCm)
		{
			return false;
		}
		if (FMath::Abs(FVector2D::DotProduct(Delta, RoadNormal)) > RoadHalf.Y + DirtProjectOnRoadNormal + RoadDirtGapCm)
		{
			return false;
		}
		return true;
	}

	bool AnyRoadTileOverlapsDirtTile(const TArray<FScatterRegionRoadTileFootprint>& RoadTiles, const FVector2D& DirtCenter, const FVector2D& DirtSize)
	{
		for (const FScatterRegionRoadTileFootprint& RoadTile : RoadTiles)
		{
			if (RoadTileFootprintOverlapsDirtTile(RoadTile, DirtCenter, DirtSize))
			{
				return true;
			}
		}
		return false;
	}

	TArray<FScatterRegionGraphPoint> ScatterRegionDensityFilter(const TArray<FScatterRegionGraphPoint>& Points, FRandomStream& Stream, float DensityScale)
	{
		TArray<FScatterRegionGraphPoint> Result;
		Result.Reserve(Points.Num());
		for (const FScatterRegionGraphPoint& Point : Points)
		{
			const float Probability = FMath::Clamp(Point.Density * DensityScale, 0.0f, 1.0f);
			if (Stream.FRand() <= Probability)
			{
				Result.Add(Point);
			}
		}
		return Result;
	}

	void ScatterRegionDensityRemap(TArray<FScatterRegionGraphPoint>& Points, float InMin, float InMax, float OutMin, float OutMax)
	{
		const float Denominator = FMath::Max(KINDA_SMALL_NUMBER, InMax - InMin);
		for (FScatterRegionGraphPoint& Point : Points)
		{
			const float Alpha = FMath::Clamp((Point.Density - InMin) / Denominator, 0.0f, 1.0f);
			Point.Density = FMath::Clamp(FMath::Lerp(OutMin, OutMax, Alpha), 0.0f, 1.0f);
		}
	}

	TArray<FScatterRegionGraphPoint> ScatterRegionAttributeFilterDensity(const TArray<FScatterRegionGraphPoint>& Points, float MinDensity, float MaxDensity)
	{
		TArray<FScatterRegionGraphPoint> Result;
		Result.Reserve(Points.Num());
		for (const FScatterRegionGraphPoint& Point : Points)
		{
			if (Point.Density >= MinDensity && Point.Density <= MaxDensity)
			{
				Result.Add(Point);
			}
		}
		return Result;
	}

	void ScatterRegionEnsureAtLeastOnePoint(TArray<FScatterRegionGraphPoint>& Points, const TArray<FScatterRegionGraphPoint>& SourcePoints, FRandomStream& Stream)
	{
		if (Points.Num() > 0 || SourcePoints.Num() == 0)
		{
			return;
		}
		Points.Add(SourcePoints[Stream.RandRange(0, SourcePoints.Num() - 1)]);
	}

	void ScatterRegionTransformPoints(
		TArray<FScatterRegionGraphPoint>& Points,
		FRandomStream& Stream,
		float PositionJitterCm,
		float YawJitterDegrees,
		const FVector& ScaleMin,
		const FVector& ScaleMax,
		bool bUniformScale)
	{
		for (FScatterRegionGraphPoint& Point : Points)
		{
			if (PositionJitterCm > 0.0f)
			{
				Point.Location += FVector2D(
					Stream.FRandRange(-PositionJitterCm, PositionJitterCm),
					Stream.FRandRange(-PositionJitterCm, PositionJitterCm));
			}
			if (YawJitterDegrees > 0.0f)
			{
				Point.YawDegrees += Stream.FRandRange(-YawJitterDegrees, YawJitterDegrees);
			}
			if (bUniformScale)
			{
				const float MinValue = FMath::Min3(ScaleMin.X, ScaleMin.Y, ScaleMin.Z);
				const float MaxValue = FMath::Max3(ScaleMax.X, ScaleMax.Y, ScaleMax.Z);
				Point.ScaleMultiplier *= FVector(Stream.FRandRange(MinValue, FMath::Max(MinValue, MaxValue)));
			}
			else
			{
				Point.ScaleMultiplier *= FVector(
					Stream.FRandRange(ScaleMin.X, FMath::Max(ScaleMin.X, ScaleMax.X)),
					Stream.FRandRange(ScaleMin.Y, FMath::Max(ScaleMin.Y, ScaleMax.Y)),
					Stream.FRandRange(ScaleMin.Z, FMath::Max(ScaleMin.Z, ScaleMax.Z)));
			}
		}
	}

	TArray<FScatterRegionGraphPoint> ScatterRegionDifferenceAgainstRoads(
		const TArray<FScatterRegionGraphPoint>& Points,
		const TArray<FScatterRegionRoadSegment>& Segments,
		float MinRoadDistanceCm,
		float MaxRoadDistanceCm = 0.0f)
	{
		TArray<FScatterRegionGraphPoint> Result;
		Result.Reserve(Points.Num());
		const float MinDistanceSq = FMath::Square(FMath::Max(0.0f, MinRoadDistanceCm));
		const float MaxDistanceSq = MaxRoadDistanceCm > 0.0f ? FMath::Square(MaxRoadDistanceCm) : TNumericLimits<float>::Max();
		for (const FScatterRegionGraphPoint& Point : Points)
		{
			const float DistanceSq = DistanceToRoadsSquared(Point.Location, Segments);
			if (DistanceSq >= MinDistanceSq && DistanceSq <= MaxDistanceSq)
			{
				Result.Add(Point);
			}
		}
		return Result;
	}

	TArray<FScatterRegionGraphPoint> ScatterRegionDifferenceAgainstPlacements(const TArray<FScatterRegionGraphPoint>& Points, const FScatterRegionPlacementSet& PlacementSet, float MinDistanceCm)
	{
		TArray<FScatterRegionGraphPoint> Result;
		Result.Reserve(Points.Num());
		for (const FScatterRegionGraphPoint& Point : Points)
		{
			if (!IsNearAnyPlacement(PlacementSet, Point.Location, FMath::Max(MinDistanceCm, Point.BoundsRadius)))
			{
				Result.Add(Point);
			}
		}
		return Result;
	}

	void ScatterRegionBoundsModifier(TArray<FScatterRegionGraphPoint>& Points, float BoundsRadiusCm)
	{
		for (FScatterRegionGraphPoint& Point : Points)
		{
			Point.BoundsRadius = FMath::Max(1.0f, BoundsRadiusCm);
		}
	}

	TArray<FScatterRegionGraphPoint> ScatterRegionSelfPruning(const TArray<FScatterRegionGraphPoint>& Points, FRandomStream& Stream, float MinDistanceCm, bool bRandomizedPruning)
	{
		TArray<FScatterRegionGraphPoint> Candidates = Points;
		if (bRandomizedPruning)
		{
			for (int32 Index = 0; Index < Candidates.Num(); ++Index)
			{
				const int32 SwapIndex = Stream.RandRange(Index, Candidates.Num() - 1);
				if (Index != SwapIndex)
				{
					Candidates.Swap(Index, SwapIndex);
				}
			}
		}

		TArray<FScatterRegionGraphPoint> Result;
		FScatterRegionPlacementSet Accepted;
		const float EffectiveDistance = FMath::Max(1.0f, MinDistanceCm);
		for (const FScatterRegionGraphPoint& Point : Candidates)
		{
			const float PointDistance = FMath::Max(EffectiveDistance, Point.BoundsRadius);
			if (ReservePlacement(Accepted, Point.Location, PointDistance))
			{
				Result.Add(Point);
			}
		}
		return Result;
	}

	void ScatterRegionLookAtRoad(TArray<FScatterRegionGraphPoint>& Points, const TArray<FScatterRegionRoadSegment>& Segments, float YawOffsetDegrees = 0.0f)
	{
		for (FScatterRegionGraphPoint& Point : Points)
		{
			FVector2D Closest;
			float RoadYaw = 0.0f;
			DistanceToRoadsSquared(Point.Location, Segments, &Closest, &RoadYaw);
			const FVector2D ToRoad = Closest - Point.Location;
			if (ToRoad.SizeSquared() > KINDA_SMALL_NUMBER)
			{
				Point.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(ToRoad.Y, ToRoad.X)) + YawOffsetDegrees;
			}
			else
			{
				Point.YawDegrees = RoadYaw + YawOffsetDegrees;
			}
		}
	}

	bool CommitScatterRegionGraphPoint(
		FScatterRegionBuildContext& Context,
		FRandomStream& Stream,
		const FScatterRegionObjectRecipe* Recipe,
		const FString& Slot,
		const FScatterRegionGraphPoint& Point)
	{
		return AddRecipeInstance(Context, Stream, Recipe, Slot, FVector(Point.Location.X, Point.Location.Y, 0.0f), Point.YawDegrees, Point.ScaleMultiplier, Point.LocalZOffsetCm);
	}

	int32 CommitScatterRegionGraphPoints(
		FScatterRegionBuildContext& Context,
		FRandomStream& Stream,
		const FScatterRegionObjectRecipe* Recipe,
		const FString& Slot,
		const TArray<FScatterRegionGraphPoint>& Points)
	{
		int32 AddedCount = 0;
		for (const FScatterRegionGraphPoint& Point : Points)
		{
			if (CommitScatterRegionGraphPoint(Context, Stream, Recipe, Slot, Point))
			{
				++AddedCount;
			}
		}
		return AddedCount;
	}

	int32 CommitScatterRegionGraphPointsAvoiding(
		FScatterRegionBuildContext& Context,
		FRandomStream& Stream,
		const FScatterRegionObjectRecipe* Recipe,
		const FString& Slot,
		const TArray<FScatterRegionGraphPoint>& Points,
		FScatterRegionCollisionSet& CollisionSet,
		float ExtraPaddingCm = 0.0f)
	{
		int32 AddedCount = 0;
		for (const FScatterRegionGraphPoint& Point : Points)
		{
			const FScatterRegionRecipeMesh* RecipeMesh = PickMesh(Recipe, Stream);
			if (!RecipeMesh || !RecipeMesh->Mesh)
			{
				continue;
			}

			const float PointScaleMax = FMath::Max3(FMath::Abs(Point.ScaleMultiplier.X), FMath::Abs(Point.ScaleMultiplier.Y), FMath::Abs(Point.ScaleMultiplier.Z));
			const float Radius = FMath::Max(Point.BoundsRadius, MeshPlanarRadius(*RecipeMesh, Point.BoundsRadius) * FMath::Max(0.01f, PointScaleMax));
			if (!ReserveCollision(CollisionSet, Point.Location, Radius, ExtraPaddingCm))
			{
				continue;
			}
			if (AddMeshInstance(Context, Stream, *RecipeMesh, Slot, FVector(Point.Location.X, Point.Location.Y, 0.0f), Point.YawDegrees, Point.ScaleMultiplier, false, Point.LocalZOffsetCm))
			{
				++AddedCount;
			}
		}
		return AddedCount;
	}

	FScatterRegionRoadSegment MakeRoadSegment(const FVector2D& Start, const FVector2D& End, bool bPrimary)
	{
		FScatterRegionRoadSegment Segment;
		Segment.Start = Start;
		Segment.End = End;
		Segment.YawDegrees = SegmentYawDegrees(Start, End);
		Segment.bPrimary = bPrimary;
		return Segment;
	}

	TArray<FScatterRegionRoadSegment> BuildOrganicRoadGraph(float HalfSizeCm, FRandomStream& Stream, const FScatterRegionGraphProfile& Profile)
	{
		TArray<FScatterRegionRoadSegment> Segments;
		const float CoreRadius = HalfSizeCm * 0.18f;
		const int32 ExitCount = FMath::Clamp(Profile.NumberOfExit, 1, 6);
		const float AngleOffset = Stream.FRandRange(-PI, PI);

		for (int32 Index = 0; Index < ExitCount; ++Index)
		{
			const float Angle = AngleOffset + 2.0f * PI * static_cast<float>(Index) / static_cast<float>(ExitCount);
			const FVector2D Exit = BoundaryPointAtAngle(HalfSizeCm * 0.92f, Angle, Stream, Profile.RoadRandomness * 0.45f);
			const FVector2D Inner = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * CoreRadius * Stream.FRandRange(0.4f, 0.9f);
			Segments.Add(MakeRoadSegment(Inner, Exit, true));
		}

		const int32 RingCount = FMath::Clamp(FMath::RoundToInt(4.0f + Profile.RoadDensity * 6.0f), 4, 10);
		TArray<FVector2D> RingPoints;
		for (int32 Index = 0; Index < RingCount; ++Index)
		{
			const float Angle = AngleOffset + 2.0f * PI * static_cast<float>(Index) / static_cast<float>(RingCount);
			const float Radius = HalfSizeCm * Stream.FRandRange(0.22f, 0.42f);
			RingPoints.Add(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		for (int32 Index = 0; Index < RingPoints.Num(); ++Index)
		{
			Segments.Add(MakeRoadSegment(RingPoints[Index], RingPoints[(Index + 1) % RingPoints.Num()], false));
		}

		const int32 BranchCount = FMath::Clamp(FMath::RoundToInt(Profile.RoadDensity * static_cast<float>(ExitCount) * 2.0f), 1, 10);
		for (int32 Index = 0; Index < BranchCount; ++Index)
		{
			const FScatterRegionRoadSegment& Source = Segments[Stream.RandRange(0, ExitCount - 1)];
			const FVector2D Direction = SegmentDirection(Source);
			const FVector2D Normal = SegmentNormal(Source) * (Stream.RandRange(0, 1) == 0 ? -1.0f : 1.0f);
			const float T = Stream.FRandRange(0.24f, 0.76f);
			const FVector2D Start = FMath::Lerp(Source.Start, Source.End, T);
			const FVector2D End = Start + (Direction * Stream.FRandRange(600.0f, 1600.0f) + Normal * Stream.FRandRange(HalfSizeCm * 0.16f, HalfSizeCm * 0.34f));
			if (!IsInsideRegion(End, HalfSizeCm))
			{
				continue;
			}
			Segments.Add(MakeRoadSegment(Start, End, false));
		}

		return Segments;
	}

	TArray<FScatterRegionRoadSegment> BuildGridRoadGraph(float HalfSizeCm, FRandomStream& Stream, const FScatterRegionGraphProfile& Profile)
	{
		TArray<FScatterRegionRoadSegment> Segments;
		const float Extent = HalfSizeCm * 0.86f;
		const int32 ParallelCount = FMath::Clamp(FMath::RoundToInt(1.0f + Profile.RoadDensity * 3.0f), 1, 4);
		const float OffsetStep = Extent / static_cast<float>(ParallelCount + 1);
		const float Jitter = OffsetStep * 0.14f * Profile.RoadRandomness;

		Segments.Add(MakeRoadSegment(FVector2D(-Extent, 0.0f), FVector2D(Extent, 0.0f), true));
		Segments.Add(MakeRoadSegment(FVector2D(0.0f, -Extent), FVector2D(0.0f, Extent), true));
		for (int32 Index = 1; Index <= ParallelCount; ++Index)
		{
			const float Offset = OffsetStep * static_cast<float>(Index);
			for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
			{
				const float Sign = SideIndex == 0 ? -1.0f : 1.0f;
				const float JitteredOffset = Sign * Offset + Stream.FRandRange(-Jitter, Jitter);
				Segments.Add(MakeRoadSegment(FVector2D(-Extent, JitteredOffset), FVector2D(Extent, JitteredOffset), false));
				Segments.Add(MakeRoadSegment(FVector2D(JitteredOffset, -Extent), FVector2D(JitteredOffset, Extent), false));
			}
		}
		return Segments;
	}

	TArray<FScatterRegionRoadSegment> BuildRadialRoadGraph(float HalfSizeCm, FRandomStream& Stream, const FScatterRegionGraphProfile& Profile)
	{
		TArray<FScatterRegionRoadSegment> Segments;
		const int32 ExitCount = FMath::Clamp(Profile.NumberOfExit + FMath::RoundToInt(Profile.RoadDensity * 2.0f), 2, 7);
		const float AngleOffset = Stream.FRandRange(-PI, PI);
		const float CenterJitter = HalfSizeCm * 0.04f * Profile.RoadRandomness;
		const FVector2D Center(Stream.FRandRange(-CenterJitter, CenterJitter), Stream.FRandRange(-CenterJitter, CenterJitter));
		TArray<FVector2D> InnerRing;
		InnerRing.Reserve(ExitCount);

		for (int32 Index = 0; Index < ExitCount; ++Index)
		{
			const float Angle = AngleOffset + 2.0f * PI * static_cast<float>(Index) / static_cast<float>(ExitCount);
			const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
			const FVector2D Inner = Center + Direction * HalfSizeCm * Stream.FRandRange(0.18f, 0.32f);
			const FVector2D Exit = BoundaryPointAtAngle(HalfSizeCm * 0.9f, Angle, Stream, Profile.RoadRandomness * 0.25f);
			Segments.Add(MakeRoadSegment(Center, Exit, true));
			InnerRing.Add(Inner);
		}

		for (int32 Index = 0; Index < InnerRing.Num(); ++Index)
		{
			Segments.Add(MakeRoadSegment(InnerRing[Index], InnerRing[(Index + 1) % InnerRing.Num()], false));
		}
		return Segments;
	}

	TArray<FScatterRegionRoadSegment> BuildRoadGraph(float HalfSizeCm, FRandomStream& Stream, const FScatterRegionGraphProfile& Profile)
	{
		if (Profile.RoadPattern == TEXT("grid"))
		{
			return BuildGridRoadGraph(HalfSizeCm, Stream, Profile);
		}
		if (Profile.RoadPattern == TEXT("radial"))
		{
			return BuildRadialRoadGraph(HalfSizeCm, Stream, Profile);
		}
		return BuildOrganicRoadGraph(HalfSizeCm, Stream, Profile);
	}

	bool TryGetGateSampleFromBoundary(const TArray<FVector2D>& Boundary, FScatterRegionPerimeterSample& OutGateSample)
	{
		if (Boundary.Num() < 2)
		{
			return false;
		}
		const TArray<FVector2D> SmoothedBoundary = SmoothClosedBoundary(Boundary, 2);
		if (SmoothedBoundary.Num() < 2)
		{
			return false;
		}
		const float GateDistance = FindLowestYDistanceOnClosedPolyline(SmoothedBoundary);
		OutGateSample = SampleClosedPolylineAtDistance(SmoothedBoundary, GateDistance);
		return true;
	}

	void AddBentRoadPath(
		TArray<FScatterRegionRoadSegment>& Segments,
		const FVector2D& Start,
		const FVector2D& End,
		FRandomStream& Stream,
		float BendStrengthCm,
		bool bPrimary)
	{
		const FVector2D Delta = End - Start;
		const float Length = Delta.Size();
		if (Length <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		const FVector2D Direction = Delta / Length;
		const FVector2D Normal(-Direction.Y, Direction.X);
		const FVector2D Control = (Start + End) * 0.5f + Normal * Stream.FRandRange(-BendStrengthCm, BendStrengthCm);
		const int32 SegmentCount = FMath::Clamp(FMath::RoundToInt(Length / 1500.0f), 3, 7);
		FVector2D Previous = Start;
		for (int32 Index = 1; Index <= SegmentCount; ++Index)
		{
			const float T = static_cast<float>(Index) / static_cast<float>(SegmentCount);
			const FVector2D A = FMath::Lerp(Start, Control, T);
			const FVector2D B = FMath::Lerp(Control, End, T);
			const FVector2D Current = FMath::Lerp(A, B, T);
			if (FVector2D::Distance(Previous, Current) > 120.0f)
			{
				Segments.Add(MakeRoadSegment(Previous, Current, bPrimary));
			}
			Previous = Current;
		}
	}

	TArray<FScatterRegionRoadSegment> BuildGateRootedRoadGraph(
		float HalfSizeCm,
		const TArray<FVector2D>& Boundary,
		const FScatterRegionPerimeterSample& GateSample,
		FRandomStream& Stream,
		const FScatterRegionGraphProfile& Profile)
	{
		TArray<FScatterRegionRoadSegment> Segments;
		const FVector2D GateLocation = GateSample.Location;
		FVector2D Inward = -GateLocation.GetSafeNormal();
		if (Inward.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			Inward = FVector2D(0.0f, 1.0f);
		}
		const FVector2D Tangent(-Inward.Y, Inward.X);
		const FVector2D DeepestPoint = FindFarthestInsideAlongRay(Boundary, HalfSizeCm, GateLocation, Inward, HalfSizeCm * 1.85f, 260.0f);
		FVector2D VillageCore = FMath::Lerp(GateLocation, DeepestPoint, Stream.FRandRange(0.62f, 0.76f));
		VillageCore += Tangent * Stream.FRandRange(-HalfSizeCm * 0.06f, HalfSizeCm * 0.06f);
		if (!IsInsideClosedRegion(VillageCore, Boundary, HalfSizeCm) || FVector2D::Distance(GateLocation, VillageCore) < HalfSizeCm * 0.42f)
		{
			VillageCore = GateLocation + Inward * HalfSizeCm * 0.62f;
		}
		AddBentRoadPath(Segments, GateLocation, VillageCore, Stream, HalfSizeCm * 0.035f, true);

		const int32 BranchCount = FMath::Clamp(FMath::RoundToInt(HalfSizeCm / 1450.0f), 2, 7);
		for (int32 Index = 0; Index < BranchCount; ++Index)
		{
			const float T = BranchCount == 1
				? Stream.FRandRange(0.48f, 0.76f)
				: FMath::Lerp(0.26f, 0.88f, static_cast<float>(Index) / static_cast<float>(BranchCount - 1));
			const FVector2D Start = FMath::Lerp(GateLocation, VillageCore, T);
			const float Side = (Index % 2) == 0 ? 1.0f : -1.0f;
			FVector2D Direction = (Tangent * Side * Stream.FRandRange(0.78f, 1.0f) + Inward * Stream.FRandRange(0.12f, 0.52f)).GetSafeNormal();
			if (Direction.SizeSquared() <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const FVector2D Farthest = FindFarthestInsideAlongRay(Boundary, HalfSizeCm, Start, Direction, HalfSizeCm * 1.45f, 220.0f);
			FVector2D End = FMath::Lerp(Start, Farthest, Stream.FRandRange(0.72f, 0.94f));
			if (!IsInsideClosedRegion(End, Boundary, HalfSizeCm) || FVector2D::Distance(Start, End) < HalfSizeCm * 0.22f)
			{
				continue;
			}

			if (DistanceToRoadsSquared(End, Segments) < FMath::Square(HalfSizeCm * 0.11f))
			{
				continue;
			}
			AddBentRoadPath(Segments, Start, End, Stream, HalfSizeCm * 0.025f, false);
		}

		const int32 OuterSpurCount = FMath::Clamp(FMath::RoundToInt(HalfSizeCm / 2600.0f), 1, 4);
		for (int32 Index = 0; Index < OuterSpurCount; ++Index)
		{
			const float Side = (Index % 2) == 0 ? -1.0f : 1.0f;
			const FVector2D Start = FMath::Lerp(GateLocation, VillageCore, Stream.FRandRange(0.56f, 0.92f));
			const FVector2D Direction = (Tangent * Side * Stream.FRandRange(0.42f, 0.72f) - Inward * Stream.FRandRange(0.18f, 0.38f)).GetSafeNormal();
			const FVector2D Farthest = FindFarthestInsideAlongRay(Boundary, HalfSizeCm, Start, Direction, HalfSizeCm * 0.9f, 220.0f);
			const FVector2D End = FMath::Lerp(Start, Farthest, Stream.FRandRange(0.62f, 0.84f));
			if (FVector2D::Distance(Start, End) > HalfSizeCm * 0.2f && DistanceToRoadsSquared(End, Segments) > FMath::Square(HalfSizeCm * 0.12f))
			{
				AddBentRoadPath(Segments, Start, End, Stream, HalfSizeCm * 0.02f, false);
			}
		}
		return Segments;
	}

	TArray<FScatterRegionRoadSegment> BuildCemeteryAccessRoads(float HalfSizeCm, const TArray<FVector2D>& Boundary)
	{
		TArray<FScatterRegionRoadSegment> Segments;
		FVector2D Entrance(0.0f, -HalfSizeCm * 0.86f);
		if (Boundary.Num() >= 3)
		{
			Entrance = SampleClosedPolylineAtDistance(Boundary, FindLowestYDistanceOnClosedPolyline(Boundary)).Location;
		}
		FVector2D Inward = (-Entrance).GetSafeNormal();
		if (Inward.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			Inward = FVector2D(0.0f, 1.0f);
		}
		const FVector2D MemorialApproach = Entrance + Inward * HalfSizeCm * 0.58f;
		Segments.Add(MakeRoadSegment(Entrance, MemorialApproach, true));
		return Segments;
	}

	void AddScatterRegionBuildingCluster(
		TArray<FScatterRegionBuildingCluster>& Clusters,
		float HalfSizeCm,
		FRandomStream& Stream,
		const FScatterRegionRoadSegment& Segment,
		float T,
		float Side,
		const FScatterRegionGraphProfile& Profile,
		int32 ClusterOrdinal)
	{
		const FVector2D Normal = SegmentNormal(Segment);
		const FVector2D Base = FMath::Lerp(Segment.Start, Segment.End, FMath::Clamp(T, 0.05f, 0.95f));
		const float Offset = Side * Stream.FRandRange(
			Profile.RoadWidth * 0.5f + Profile.BuildingMinDistanceFromRoad + 950.0f,
			Profile.RoadWidth * 0.5f + Profile.BuildingMinDistanceFromRoad + 1850.0f);
		const FVector2D Center = Base + Normal * Offset;
		if (!IsInsideRegion(Center, HalfSizeCm) || FMath::Max(FMath::Abs(Center.X), FMath::Abs(Center.Y)) > HalfSizeCm * 0.82f)
		{
			return;
		}

		FScatterRegionBuildingCluster Cluster;
		Cluster.Center = Center;
		Cluster.RoadYawDegrees = Segment.YawDegrees;
		Cluster.Side = Side;
		Cluster.Radius = Stream.FRandRange(1050.0f, 1750.0f);
		Cluster.Density = Segment.bPrimary ? 0.96f : 0.68f;
		Cluster.bMarketplace = Center.Size() < HalfSizeCm * 0.24f || (ClusterOrdinal % 9) == 0;
		Cluster.bPrimaryRoad = Segment.bPrimary;
		Clusters.Add(Cluster);
	}

	TArray<FScatterRegionBuildingCluster> BuildScatterRegionBuildingClusters(
		float HalfSizeCm,
		FRandomStream& Stream,
		const TArray<FScatterRegionRoadSegment>& RoadSegments,
		const FScatterRegionGraphProfile& Profile)
	{
		TArray<FScatterRegionBuildingCluster> Clusters;
		int32 ClusterOrdinal = 0;
		for (const FScatterRegionRoadSegment& Segment : RoadSegments)
		{
			const float Length = FVector2D::Distance(Segment.Start, Segment.End);
			const float ClusterStep = FMath::Clamp(Profile.BuildingDistance * 0.62f, 1700.0f, 3000.0f);
			const int32 Count = FMath::Clamp(FMath::FloorToInt(Length / ClusterStep), 1, 8);
			for (int32 Index = 0; Index <= Count; ++Index)
			{
				if (Stream.FRand() > (Segment.bPrimary ? 0.98f : 0.68f))
				{
					continue;
				}

				const float T = (static_cast<float>(Index) + Stream.FRandRange(0.2f, 0.8f)) / static_cast<float>(Count + 1);
				const float Side = ((ClusterOrdinal + Index) % 2) == 0 ? -1.0f : 1.0f;
				AddScatterRegionBuildingCluster(Clusters, HalfSizeCm, Stream, Segment, T, Side, Profile, ClusterOrdinal);
				++ClusterOrdinal;

				if (Segment.bPrimary && Length > ClusterStep * 2.0f && Stream.FRand() < 0.45f)
				{
					AddScatterRegionBuildingCluster(Clusters, HalfSizeCm, Stream, Segment, T + Stream.FRandRange(-0.04f, 0.04f), -Side, Profile, ClusterOrdinal);
					++ClusterOrdinal;
				}
			}
		}
		return Clusters;
	}

	TArray<FScatterRegionGraphPoint> BuildScatterRegionBuildingClusterPoints(
		const TArray<FScatterRegionBuildingCluster>& Clusters,
		float HalfSizeCm,
		FRandomStream& Stream,
		const FScatterRegionGraphProfile& Profile,
		bool bMarketplace)
	{
		TArray<FScatterRegionGraphPoint> Points;
		for (const FScatterRegionBuildingCluster& Cluster : Clusters)
		{
			if (Cluster.bMarketplace != bMarketplace)
			{
				continue;
			}

			const float YawRadians = FMath::DegreesToRadians(Cluster.RoadYawDegrees);
			const FVector2D Along(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
			const FVector2D Normal(-Along.Y, Along.X);
			const int32 BuildingCount = bMarketplace ? (Stream.FRand() < 0.35f ? 2 : 1) : Stream.RandRange(2, 4);
			const float AlongSpacing = bMarketplace
				? FMath::Clamp(Profile.BuildingDistance * 0.2f, 720.0f, 1250.0f)
				: FMath::Clamp(Profile.BuildingDistance * 0.24f, 820.0f, 1450.0f);

			for (int32 Index = 0; Index < BuildingCount; ++Index)
			{
				const float CenteredIndex = static_cast<float>(Index) - (static_cast<float>(BuildingCount - 1) * 0.5f);
				const float AlongOffset = CenteredIndex * AlongSpacing + Stream.FRandRange(-120.0f, 120.0f);
				const float DepthOffset = bMarketplace
					? Stream.FRandRange(-120.0f, 220.0f)
					: ((Index % 2) == 0 ? Stream.FRandRange(-120.0f, 180.0f) : Cluster.Side * Stream.FRandRange(420.0f, 820.0f));
				const FVector2D Location2D = Cluster.Center + Along * AlongOffset + Normal * DepthOffset;
				if (!IsInsideRegion(Location2D, HalfSizeCm))
				{
					continue;
				}

				FScatterRegionGraphPoint Point;
				Point.Location = Location2D;
				Point.YawDegrees = Cluster.RoadYawDegrees + (Cluster.Side > 0.0f ? -90.0f : 90.0f) + Stream.FRandRange(-5.0f, 5.0f);
				Point.Density = Cluster.Density * (bMarketplace ? 0.98f : Stream.FRandRange(0.78f, 1.0f));
				Point.BoundsRadius = bMarketplace ? Cluster.Radius * 0.48f : Cluster.Radius * 0.42f;
				Points.Add(Point);
			}
		}
		return Points;
	}

	TArray<FScatterRegionGraphPoint> BuildScatterRegionSecondaryBuildingRegionPoints(
		const TArray<FScatterRegionBuildingCluster>& Clusters,
		float HalfSizeCm,
		FRandomStream& Stream,
		const FScatterRegionGraphProfile& Profile,
		int32 RegionOrdinal)
	{
		TArray<FScatterRegionGraphPoint> Points;
		const float NormalizedOrdinal = FMath::Clamp(static_cast<float>(RegionOrdinal - 3), 0.0f, 2.0f);
		const float KeepChance = FMath::Clamp(0.78f - NormalizedOrdinal * 0.16f, 0.42f, 0.82f);
		const float DepthMin = Profile.BuildingMinDistanceFromRoad + 720.0f + NormalizedOrdinal * 360.0f;
		const float DepthMax = DepthMin + 760.0f + NormalizedOrdinal * 220.0f;
		const float AlongSpacing = FMath::Clamp(Profile.BuildingDistance * (0.26f + NormalizedOrdinal * 0.03f), 900.0f, 1700.0f);

		for (const FScatterRegionBuildingCluster& Cluster : Clusters)
		{
			if (Cluster.bMarketplace || Stream.FRand() > KeepChance)
			{
				continue;
			}

			const float YawRadians = FMath::DegreesToRadians(Cluster.RoadYawDegrees);
			const FVector2D Along(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
			const FVector2D Normal(-Along.Y, Along.X);
			const int32 BuildingCount = RegionOrdinal == 3 ? Stream.RandRange(1, 2) : 1;
			for (int32 Index = 0; Index < BuildingCount; ++Index)
			{
				const float CenteredIndex = static_cast<float>(Index) - (static_cast<float>(BuildingCount - 1) * 0.5f);
				const float AlongOffset = CenteredIndex * AlongSpacing + Stream.FRandRange(-180.0f, 180.0f);
				const float DepthOffset = Cluster.Side * Stream.FRandRange(DepthMin, DepthMax);
				FVector2D Location2D = Cluster.Center + Along * AlongOffset + Normal * DepthOffset;

				if (RegionOrdinal == 5)
				{
					const FVector2D FromCenter = Location2D.GetSafeNormal();
					Location2D += FromCenter * Stream.FRandRange(260.0f, 780.0f);
				}

				if (!IsInsideRegion(Location2D, HalfSizeCm * 0.93f))
				{
					continue;
				}

				FScatterRegionGraphPoint Point;
				Point.Location = Location2D;
				Point.YawDegrees = Cluster.RoadYawDegrees + (Cluster.Side > 0.0f ? -90.0f : 90.0f) + Stream.FRandRange(-8.0f, 8.0f);
				Point.Density = Cluster.Density * FMath::Clamp(0.74f - NormalizedOrdinal * 0.1f, 0.48f, 0.78f);
				Point.BoundsRadius = Cluster.Radius * FMath::Clamp(0.38f + NormalizedOrdinal * 0.04f, 0.36f, 0.48f);
				Points.Add(Point);
			}
		}
		return Points;
	}

	TArray<FScatterRegionRoadSegment> BuildFarmAccessRoads(float HalfSizeCm, const TArray<FVector2D>& Boundary)
	{
		TArray<FScatterRegionRoadSegment> Segments;
		if (Boundary.Num() < 3)
		{
			const float RoadExtent = HalfSizeCm * 0.76f;
			Segments.Add(MakeRoadSegment(FVector2D(-RoadExtent, -HalfSizeCm * 0.52f), FVector2D(RoadExtent, -HalfSizeCm * 0.52f), true));
			return Segments;
		}

		const float TotalLength = ClosedPolylineLength(Boundary);
		const FScatterRegionPerimeterSample Entrance = SampleClosedPolylineAtDistance(Boundary, FindLowestYDistanceOnClosedPolyline(Boundary));
		const FVector2D Inward = (-Entrance.Location).GetSafeNormal();
		const FVector2D Start = Entrance.Location;
		const FVector2D End = FindFarthestInsideAlongRay(Boundary, HalfSizeCm, Start, Inward, HalfSizeCm * 1.62f, 220.0f);
		Segments.Add(MakeRoadSegment(Start, End, true));

		if (TotalLength > KINDA_SMALL_NUMBER)
		{
			const FVector2D Tangent(-Inward.Y, Inward.X);
			const float BranchLength = HalfSizeCm * 0.42f;
			const float BranchParams[] = {0.42f, 0.72f};
			for (float BranchParam : BranchParams)
			{
				const FVector2D BranchCenter = FMath::Lerp(Start, End, BranchParam);
				const FVector2D Left = FindFarthestInsideAlongRay(Boundary, HalfSizeCm, BranchCenter, -Tangent, BranchLength, 160.0f);
				const FVector2D Right = FindFarthestInsideAlongRay(Boundary, HalfSizeCm, BranchCenter, Tangent, BranchLength, 160.0f);
				if (FVector2D::Distance(Left, Right) > HalfSizeCm * 0.28f)
				{
					Segments.Add(MakeRoadSegment(Left, Right, false));
				}
			}
		}
		return Segments;
	}

	TArray<FScatterRegionFarmDirtTile> BuildFarmDirtTiles(
		float HalfSizeCm,
		const FScatterRegionRecipe& Recipe,
		const FScatterRegionObjectRecipe* Dirt,
		const TArray<FVector2D>& Boundary,
		const TArray<FScatterRegionRoadTileFootprint>& RoadTiles)
	{
		TArray<FScatterRegionFarmDirtTile> Tiles;
		const FVector2D DirtSize = RecipePlanarSize(Dirt, FVector2D(400.0f, 400.0f));
		constexpr float DirtGapCm = 2.0f;
		const float StepX = FMath::Max(50.0f, DirtSize.X + DirtGapCm);
		const float StepY = FMath::Max(50.0f, DirtSize.Y + DirtGapCm);
		const float CoverageDensity = FMath::Clamp(Recipe.DirtDensity > 0.0f ? Recipe.DirtDensity : 1.0f, 0.0f, 1.0f);
		const float MinX = -HalfSizeCm * 0.9f;
		const float MaxX = HalfSizeCm * 0.9f;
		const float MinY = -HalfSizeCm * 0.9f;
		const float MaxY = HalfSizeCm * 0.9f;
		for (float X = MinX; X <= MaxX; X += StepX)
		{
			for (float Y = MinY; Y <= MaxY; Y += StepY)
			{
				if (CoverageDensity < 1.0f && FMath::Frac(FMath::Abs(X * 0.017f + Y * 0.031f)) > CoverageDensity)
				{
					continue;
				}

				const FVector2D Location2D(X, Y);
				if (!IsTileInsidePolygon(Location2D, DirtSize, 0.0f, Boundary))
				{
					continue;
				}
				if (AnyRoadTileOverlapsDirtTile(RoadTiles, Location2D, DirtSize))
				{
					continue;
				}

				FScatterRegionFarmDirtTile Tile;
				Tile.Center = Location2D;
				Tile.Size = DirtSize;
				Tile.YawDegrees = 0.0f;
				Tiles.Add(Tile);
			}
		}
		return Tiles;
	}

	TArray<FScatterRegionGraphPoint> BuildFarmDirtTilePoints(const TArray<FScatterRegionFarmDirtTile>& Tiles)
	{
		TArray<FScatterRegionGraphPoint> DirtPoints;
		DirtPoints.Reserve(Tiles.Num());
		for (const FScatterRegionFarmDirtTile& Tile : Tiles)
		{
			FScatterRegionGraphPoint Point;
			Point.Location = Tile.Center;
			Point.YawDegrees = Tile.YawDegrees;
			Point.Density = 1.0f;
			Point.BoundsRadius = FMath::Max(Tile.Size.X, Tile.Size.Y) * 0.5f;
			DirtPoints.Add(Point);
		}
		return DirtPoints;
	}

	TArray<FScatterRegionGraphPoint> BuildFarmCropTilePoints(
		const TArray<FScatterRegionFarmDirtTile>& Tiles,
		const FScatterRegionRecipe& Recipe,
		const FScatterRegionObjectRecipe* Crops,
		float CropLocalZOffsetCm,
		FRandomStream& Stream)
	{
		TArray<FScatterRegionGraphPoint> CropPoints;
		const float CropRadius = RecipeBoundsRadius(Crops, 52.0f);
		for (const FScatterRegionFarmDirtTile& Tile : Tiles)
		{
			const float Density = FMath::Clamp(Recipe.CropsDensity, 0.0f, 1.0f);
			if (Density <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			int32 MinCropCount = 1;
			int32 MaxCropCount = 4;
			if (Density < 0.5f)
			{
				const float Alpha = Density / 0.5f;
				MinCropCount = FMath::RoundToInt(FMath::Lerp(1.0f, 3.0f, Alpha));
				MaxCropCount = FMath::RoundToInt(FMath::Lerp(4.0f, 6.0f, Alpha));
			}
			else
			{
				const float Alpha = (Density - 0.5f) / 0.5f;
				MinCropCount = FMath::RoundToInt(FMath::Lerp(3.0f, 5.0f, Alpha));
				MaxCropCount = FMath::RoundToInt(FMath::Lerp(6.0f, 10.0f, Alpha));
			}
			MinCropCount = FMath::Clamp(MinCropCount, 1, 10);
			MaxCropCount = FMath::Clamp(FMath::Max(MinCropCount, MaxCropCount), MinCropCount, 10);
			const int32 CropCount = Stream.RandRange(MinCropCount, MaxCropCount);
			for (int32 Index = 0; Index < CropCount; ++Index)
			{
				const FVector2D HalfSafe(
					FMath::Max(1.0f, Tile.Size.X * 0.5f - CropRadius * 0.75f),
					FMath::Max(1.0f, Tile.Size.Y * 0.5f - CropRadius * 0.75f));
				const FVector2D Local(
					Stream.FRandRange(-HalfSafe.X, HalfSafe.X),
					Stream.FRandRange(-HalfSafe.Y, HalfSafe.Y));
				FScatterRegionGraphPoint Point;
				Point.Location = Tile.Center + Rotate2D(Local, FMath::DegreesToRadians(Tile.YawDegrees));
				Point.LocalZOffsetCm = CropLocalZOffsetCm;
				Point.YawDegrees = Tile.YawDegrees + Stream.FRandRange(-12.0f, 12.0f);
				Point.Density = 1.0f;
				Point.BoundsRadius = CropRadius;
				CropPoints.Add(Point);
			}
		}
		return CropPoints;
	}

	TArray<FScatterRegionGraphPoint> BuildFarmScarecrowPoints(
		const TArray<FScatterRegionFarmDirtTile>& Tiles,
		const FScatterRegionRecipe& Recipe,
		const TArray<FScatterRegionRoadSegment>& RoadSegments,
		FRandomStream& Stream)
	{
		TArray<FScatterRegionGraphPoint> Points;
		if (Tiles.Num() == 0)
		{
			return Points;
		}

		const float Density = FMath::Clamp(Recipe.ScarecrowDensity, 0.0f, 1.0f);
		const int32 TargetCount = FMath::Clamp(FMath::RoundToInt(static_cast<float>(Tiles.Num()) / 30.0f * FMath::Max(0.25f, Density)), 1, 10);
		for (int32 Attempt = 0; Attempt < TargetCount * 14 && Points.Num() < TargetCount; ++Attempt)
		{
			const FScatterRegionFarmDirtTile& Tile = Tiles[Stream.RandRange(0, Tiles.Num() - 1)];
			if (Points.Num() > 0 && Stream.FRand() > FMath::Clamp(Density + 0.35f, 0.0f, 1.0f))
			{
				continue;
			}
			if (DistanceToRoadsSquared(Tile.Center, RoadSegments) < FMath::Square(FMath::Max(Tile.Size.X, Tile.Size.Y) * 0.5f + 120.0f))
			{
				continue;
			}
			const FVector2D Offset(
				(Stream.FRand() < 0.5f ? -1.0f : 1.0f) * Tile.Size.X * Stream.FRandRange(0.25f, 0.58f),
				(Stream.FRand() < 0.5f ? -1.0f : 1.0f) * Tile.Size.Y * Stream.FRandRange(0.25f, 0.58f));

			FScatterRegionGraphPoint Point;
			Point.Location = Tile.Center + Offset;
			Point.YawDegrees = Stream.FRandRange(0.0f, 360.0f);
			Point.Density = 1.0f;
			Point.BoundsRadius = FMath::Clamp(Recipe.ScarecrowDistanceMultiplicator * 42.0f, 420.0f, 1300.0f);
			Points.Add(Point);
		}
		return Points;
	}

	TArray<FScatterRegionGraphPoint> BuildCemeteryTombGridPoints(
		float HalfSizeCm,
		const FScatterRegionRecipe& Recipe,
		const FScatterRegionObjectRecipe* Tombs,
		const TArray<FVector2D>& Boundary,
		const TArray<FScatterRegionRoadSegment>& RoadSegments,
		const FScatterRegionPlacementSet& MemorialPlacements,
		FRandomStream& Stream)
	{
		TArray<FScatterRegionGraphPoint> TombPoints;
		const float TombRadius = RecipeBoundsRadius(Tombs, Recipe.TombSpacing * 0.22f);
		const float Density = FMath::Clamp(Recipe.TombDensity, 0.0f, 1.0f);
		const float RowSpacing = FMath::Clamp(FMath::Lerp(560.0f, 300.0f, Density), 260.0f, 620.0f);
		const float ColumnSpacing = RowSpacing * 0.86f;
		const float Extent = HalfSizeCm * 0.86f;
		for (float Y = -Extent; Y <= Extent; Y += RowSpacing)
		{
			const float RowOffset = FMath::Frac(FMath::Abs(Y * 0.017f)) > 0.5f ? ColumnSpacing * 0.5f : 0.0f;
			for (float X = -Extent + RowOffset; X <= Extent; X += ColumnSpacing)
			{
				if (Stream.FRand() > FMath::Clamp(0.35f + Density * 0.65f, 0.0f, 1.0f))
				{
					continue;
				}
				const FVector2D Location2D(X + Stream.FRandRange(-14.0f, 14.0f), Y + Stream.FRandRange(-10.0f, 10.0f));
				if (!IsInsideClosedRegion(Location2D, Boundary, HalfSizeCm))
				{
					continue;
				}
				if (DistanceToRoadsSquared(Location2D, RoadSegments) < FMath::Square(RowSpacing * 0.6f + 170.0f))
				{
					continue;
				}
				if (IsNearAnyPlacement(MemorialPlacements, Location2D, FMath::Max(Recipe.TombSpacing, TombRadius * 2.0f)))
				{
					continue;
				}

				FScatterRegionGraphPoint Point;
				Point.Location = Location2D;
				Point.YawDegrees = 0.0f + Stream.FRandRange(-2.0f, 2.0f);
				Point.Density = 1.0f;
				Point.BoundsRadius = FMath::Max(TombRadius, RowSpacing * 0.24f);
				TombPoints.Add(Point);
			}
		}
		return TombPoints;
	}

	TArray<FScatterRegionGraphPoint> BuildCemeteryPanteonPoints(
		float HalfSizeCm,
		const FScatterRegionRecipe& Recipe,
		const TArray<FVector2D>& Boundary,
		const TArray<FScatterRegionRoadSegment>& RoadSegments,
		FRandomStream& Stream)
	{
		TArray<FScatterRegionGraphPoint> Points;
		const float RegionMeters = HalfSizeCm / 50.0f;
		const int32 TargetCount = FMath::Clamp(FMath::RoundToInt((RegionMeters * RegionMeters) / 1600.0f * FMath::Clamp(Recipe.PanteonDensity, 0.0f, 1.0f)), 1, 4);
		FScatterRegionGraphPoint CenterPoint;
		if (RoadSegments.Num() > 0)
		{
			const FScatterRegionRoadSegment& EntryRoad = RoadSegments[0];
			CenterPoint.Location = FMath::Lerp(EntryRoad.Start, EntryRoad.End, 0.78f) + SegmentNormal(EntryRoad) * HalfSizeCm * 0.18f;
		}
		else
		{
			CenterPoint.Location = FVector2D(HalfSizeCm * 0.18f, HalfSizeCm * 0.16f);
		}
		CenterPoint.YawDegrees = 180.0f;
		CenterPoint.Density = 1.0f;
		CenterPoint.BoundsRadius = FMath::Clamp(HalfSizeCm * 0.18f, 1200.0f, 2600.0f);
		Points.Add(CenterPoint);

		for (int32 Attempt = 0; Attempt < TargetCount * 16 && Points.Num() < TargetCount; ++Attempt)
		{
			const FVector2D Location2D(
				Stream.FRandRange(-HalfSizeCm * 0.68f, HalfSizeCm * 0.68f),
				Stream.FRandRange(-HalfSizeCm * 0.48f, HalfSizeCm * 0.72f));
			if (!IsInsideClosedRegion(Location2D, Boundary, HalfSizeCm))
			{
				continue;
			}
			if (DistanceToRoadsSquared(Location2D, RoadSegments) < FMath::Square(820.0f))
			{
				continue;
			}
			bool bTooClose = false;
			for (const FScatterRegionGraphPoint& Existing : Points)
			{
				if (FVector2D::Distance(Location2D, Existing.Location) < FMath::Clamp(HalfSizeCm * 0.28f, 1200.0f, 2400.0f))
				{
					bTooClose = true;
					break;
				}
			}
			if (bTooClose)
			{
				continue;
			}

			FScatterRegionGraphPoint Point;
			Point.Location = Location2D;
			Point.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(-Location2D.Y, -Location2D.X));
			Point.Density = 1.0f;
			Point.BoundsRadius = FMath::Clamp(HalfSizeCm * 0.1f, 900.0f, 1900.0f);
			Points.Add(Point);
		}
		return Points;
	}

	void PlaceAlongSegment(
		FScatterRegionBuildContext& Context,
		FRandomStream& Stream,
		const FScatterRegionObjectRecipe* Recipe,
		const FString& Slot,
		const FScatterRegionRoadSegment& Segment,
		float StepCm,
		float LateralOffsetCm,
		float Density,
		FScatterRegionPlacementSet* OptionalPlacementSet = nullptr,
		float MinPlacementDistanceCm = 0.0f)
	{
		const float Length = FVector2D::Distance(Segment.Start, Segment.End);
		if (Length <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const FVector2D Normal = SegmentNormal(Segment);
		const int32 Count = FMath::Max(1, FMath::FloorToInt(Length / FMath::Max(100.0f, StepCm)));
		for (int32 Index = 0; Index <= Count; ++Index)
		{
			if (Stream.FRand() > Density)
			{
				continue;
			}
			const float T = (static_cast<float>(Index) + Stream.FRandRange(-0.18f, 0.18f)) / FMath::Max(1.0f, static_cast<float>(Count));
			const FVector2D Base = FMath::Lerp(Segment.Start, Segment.End, FMath::Clamp(T, 0.0f, 1.0f));
			const float Side = LateralOffsetCm >= 0.0f ? 1.0f : -1.0f;
			const FVector2D Location2D = Base + Normal * LateralOffsetCm;
			if (OptionalPlacementSet && !ReservePlacement(*OptionalPlacementSet, Location2D, MinPlacementDistanceCm))
			{
				continue;
			}
			const float Yaw = Segment.YawDegrees + (Side > 0.0f ? -90.0f : 90.0f);
			AddRecipeInstance(Context, Stream, Recipe, Slot, FVector(Location2D.X, Location2D.Y, 0.0f), Yaw);
		}
	}

	void GenerateRoadNetwork(FScatterRegionBuildContext& Context, const FScatterRegionRecipe& Recipe, FRandomStream& Stream, const TArray<FScatterRegionRoadSegment>& Segments, const FScatterRegionGraphProfile& Profile, int32 ForcedLaneCount = 0)
	{
		const FScatterRegionObjectRecipe* Road = RecipeForSlot(Recipe, TEXT("road"));
		WarnIfMissing(Context, Recipe, TEXT("road"));
		FScatterRegionRoadTileSet RoadTiles;

		auto AddRoadSegmentTiles = [&](bool bPrimaryPass)
		{
			for (const FScatterRegionRoadSegment& Segment : Segments)
			{
				if (Segment.bPrimary != bPrimaryPass)
				{
					continue;
				}
				if (!Segment.bPrimary && Stream.FRand() > Profile.RoadDensity)
				{
					continue;
				}

				const float Length = FVector2D::Distance(Segment.Start, Segment.End);
				if (Length <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				const float Step = (Road && Road->Meshes.Num() > 0) ? MeshFixedMajorStep(Road->Meshes[0], 200.0f) : 200.0f;
				const FVector2D RoadSize = RecipePlanarSize(Road, FVector2D(200.0f, 100.0f));
				const int32 LaneCount = ForcedLaneCount > 0 ? ForcedLaneCount : (Profile.RoadPattern == TEXT("organic") ? (Segment.bPrimary ? 3 : 2) : 1);
				const float LaneSpacing = FMath::Max(45.0f, RoadSize.Y * 0.92f);
				const int32 TileCount = FMath::Max(1, FMath::RoundToInt(Length / Step));
				const FVector2D Direction = SegmentDirection(Segment);
				const FVector2D Normal = SegmentNormal(Segment);
				const float UsedLength = static_cast<float>(TileCount - 1) * Step;
				// ScatterRegion style packs use regular slab-like road meshes, so keep road tiles aligned.
				const FVector2D Start = Segment.Start + Direction * ((Length - UsedLength) * 0.5f);
				const float TileConflictRadius = FMath::Max(140.0f, Step * 0.72f);
				for (int32 Index = 0; Index < TileCount; ++Index)
				{
					const FVector2D Location2D = Start + Direction * (static_cast<float>(Index) * Step);
					if (!ReserveRoadTile(RoadTiles, Location2D, Segment.YawDegrees, TileConflictRadius))
					{
						continue;
					}
					for (int32 LaneIndex = 0; LaneIndex < LaneCount; ++LaneIndex)
					{
						const float CenteredLane = static_cast<float>(LaneIndex) - (static_cast<float>(LaneCount - 1) * 0.5f);
						const FVector2D LaneLocation = Location2D + Normal * (CenteredLane * LaneSpacing);
						AddRegularRoadTile(Context, Stream, Road, TEXT("road"), FVector(LaneLocation.X, LaneLocation.Y, 0.0f), Segment.YawDegrees);
					}
				}
			}
		};

		AddRoadSegmentTiles(true);
		AddRoadSegmentTiles(false);
	}

	TArray<FScatterRegionRoadTileFootprint> BuildRoadTileFootprints(
		const FScatterRegionObjectRecipe* Road,
		FRandomStream& Stream,
		const TArray<FScatterRegionRoadSegment>& Segments,
		const FScatterRegionGraphProfile& Profile,
		int32 ForcedLaneCount = 0)
	{
		TArray<FScatterRegionRoadTileFootprint> Tiles;
		FScatterRegionRoadTileSet RoadTiles;
		const FVector2D RoadSize = RecipePlanarSize(Road, FVector2D(200.0f, 100.0f));

		auto AddRoadSegmentTiles = [&](bool bPrimaryPass)
		{
			for (const FScatterRegionRoadSegment& Segment : Segments)
			{
				if (Segment.bPrimary != bPrimaryPass)
				{
					continue;
				}
				if (!Segment.bPrimary && Stream.FRand() > Profile.RoadDensity)
				{
					continue;
				}

				const float Length = FVector2D::Distance(Segment.Start, Segment.End);
				if (Length <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				const float Step = (Road && Road->Meshes.Num() > 0) ? MeshFixedMajorStep(Road->Meshes[0], 200.0f) : 200.0f;
				const int32 LaneCount = ForcedLaneCount > 0 ? ForcedLaneCount : (Profile.RoadPattern == TEXT("organic") ? (Segment.bPrimary ? 3 : 2) : 1);
				const float LaneSpacing = FMath::Max(45.0f, RoadSize.Y * 0.92f);
				const int32 TileCount = FMath::Max(1, FMath::RoundToInt(Length / Step));
				const FVector2D Direction = SegmentDirection(Segment);
				const FVector2D Normal = SegmentNormal(Segment);
				const float UsedLength = static_cast<float>(TileCount - 1) * Step;
				const FVector2D Start = Segment.Start + Direction * ((Length - UsedLength) * 0.5f);
				const float TileConflictRadius = FMath::Max(140.0f, Step * 0.72f);
				for (int32 Index = 0; Index < TileCount; ++Index)
				{
					const FVector2D Location2D = Start + Direction * (static_cast<float>(Index) * Step);
					if (!ReserveRoadTile(RoadTiles, Location2D, Segment.YawDegrees, TileConflictRadius))
					{
						continue;
					}
					for (int32 LaneIndex = 0; LaneIndex < LaneCount; ++LaneIndex)
					{
						const float CenteredLane = static_cast<float>(LaneIndex) - (static_cast<float>(LaneCount - 1) * 0.5f);
						FScatterRegionRoadTileFootprint Tile;
						Tile.Center = Location2D + Normal * (CenteredLane * LaneSpacing);
						Tile.Size = RoadSize;
						Tile.YawDegrees = Segment.YawDegrees;
						Tiles.Add(Tile);
					}
				}
			}
		};

		AddRoadSegmentTiles(true);
		AddRoadSegmentTiles(false);
		return Tiles;
	}

	void AddRoadTileFootprints(FScatterRegionBuildContext& Context, FRandomStream& Stream, const FScatterRegionObjectRecipe* Road, const TArray<FScatterRegionRoadTileFootprint>& RoadTiles)
	{
		for (const FScatterRegionRoadTileFootprint& Tile : RoadTiles)
		{
			AddRegularRoadTile(Context, Stream, Road, TEXT("road"), FVector(Tile.Center.X, Tile.Center.Y, 0.0f), Tile.YawDegrees);
		}
	}

	void GenerateSplinePerimeter(
		FScatterRegionBuildContext& Context,
		const FScatterRegionRecipe& Recipe,
		FRandomStream& Stream,
		const TArray<FVector2D>& Boundary,
		const FScatterRegionGraphProfile& Profile,
		FScatterRegionPerimeterSample* OutGateSample = nullptr)
	{
		if (!Recipe.bUseFence || Boundary.Num() < 2)
		{
			return;
		}

		const FScatterRegionObjectRecipe* Fence = RecipeForSlot(Recipe, TEXT("fence"));
		WarnIfMissing(Context, Recipe, TEXT("fence"));
		if (!Fence || Fence->Meshes.Num() == 0)
		{
			return;
		}

		const TArray<FVector2D> SmoothedBoundary = SmoothClosedBoundary(Boundary, 2);
		const float TotalLength = ClosedPolylineLength(SmoothedBoundary);
		const float Step = FMath::Max(100.0f, RecipeFixedMajorStep(Fence, Profile.ScatterRegionFenceLength));
		if (TotalLength <= Step)
		{
			return;
		}

		const FScatterRegionObjectRecipe* Gate = RecipeForSlot(Recipe, TEXT("gate"));
		const bool bCanPlaceGate = Recipe.bUseGate && Gate && Gate->Meshes.Num() > 0;
		if (Recipe.bUseGate && !bCanPlaceGate)
		{
			WarnIfMissing(Context, Recipe, TEXT("gate"));
		}

		if (Recipe.bUseGate)
		{
			if (bCanPlaceGate)
			{
				const float GateDistance = FindLowestYDistanceOnClosedPolyline(SmoothedBoundary);
				const FVector2D GatePlanarSize = RecipePlanarSize(Gate, FVector2D(Step * 2.0f, Step));
				const float GateWidth = FMath::Max(GatePlanarSize.X, Step);
				const float OpeningHalfWidth = FMath::Clamp(GateWidth * 0.56f, Step * 0.65f, Step * 2.25f);
				const float AvailableLength = TotalLength - OpeningHalfWidth * 2.0f;
				const FScatterRegionPerimeterSample GateSample = SampleClosedPolylineAtDistance(SmoothedBoundary, GateDistance);
				if (OutGateSample)
				{
					*OutGateSample = GateSample;
				}

				AddFixedRecipeInstance(Context, Stream, Gate, TEXT("gate"), FVector(GateSample.Location.X, GateSample.Location.Y, 0.0f), GateSample.YawDegrees);

				if (AvailableLength > Step)
				{
					const int32 FenceCount = FMath::Max(1, FMath::FloorToInt(AvailableLength / Step));
					const float Spacing = AvailableLength / static_cast<float>(FenceCount);
					for (int32 Index = 0; Index < FenceCount; ++Index)
					{
						const float Distance = GateDistance + OpeningHalfWidth + Spacing * (static_cast<float>(Index) + 0.5f);
						const FScatterRegionPerimeterSample Sample = SampleClosedPolylineAtDistance(SmoothedBoundary, Distance);
						AddFixedRecipeInstance(Context, Stream, Fence, TEXT("fence"), FVector(Sample.Location.X, Sample.Location.Y, 0.0f), Sample.YawDegrees);
					}
				}
				return;
			}
		}

		const int32 FenceCount = FMath::Max(3, FMath::FloorToInt(TotalLength / Step));
		const float Spacing = TotalLength / static_cast<float>(FenceCount);
		for (int32 Index = 0; Index < FenceCount; ++Index)
		{
			const FScatterRegionPerimeterSample Sample = SampleClosedPolylineAtDistance(SmoothedBoundary, Spacing * static_cast<float>(Index));
			AddFixedRecipeInstance(Context, Stream, Fence, TEXT("fence"), FVector(Sample.Location.X, Sample.Location.Y, 0.0f), Sample.YawDegrees);
		}
	}

	bool PlaceScatterRegionBuildingCluster(
		FScatterRegionBuildContext& Context,
		FRandomStream& Stream,
		const FScatterRegionBuildingSlotRecipe& BuildingSlot,
		const FVector2D& ClusterCenter,
		float HalfSizeCm,
		const TArray<FScatterRegionRoadSegment>& RoadSegments,
		const FScatterRegionGraphProfile& Profile,
		FScatterRegionPlacementSet& BuildingPlacements,
		FScatterRegionCollisionSet& BuildingCollision)
	{
		FVector2D ClosestRoad = FVector2D::ZeroVector;
		float ClosestRoadYaw = 0.0f;
		DistanceToRoadsSquared(ClusterCenter, RoadSegments, &ClosestRoad, &ClosestRoadYaw);
		const FVector2D BuildingToRoad = ClosestRoad - ClusterCenter;
		const float BuildingYaw = BuildingToRoad.SizeSquared() > FMath::Square(100.0f)
			? FMath::RadiansToDegrees(FMath::Atan2(BuildingToRoad.Y, BuildingToRoad.X))
			: ClosestRoadYaw + 90.0f;
		const float MainBuildingRadius = RecipeBoundsRadius(&BuildingSlot.MainBuilding, FMath::Clamp(HalfSizeCm * 0.1f, 800.0f, 2200.0f));
		const float ClusterReserveRadius = FMath::Clamp(MainBuildingRadius + HalfSizeCm * 0.045f, MainBuildingRadius + 500.0f, MainBuildingRadius + 1800.0f);
		if (!IsInsideRegion(ClusterCenter, HalfSizeCm * 0.86f))
		{
			return false;
		}
		if (DistanceToRoadsSquared(ClusterCenter, RoadSegments) < FMath::Square(Profile.RoadWidth * 0.5f + MainBuildingRadius + 80.0f))
		{
			return false;
		}
		if (!ReserveCollision(BuildingCollision, ClusterCenter, ClusterReserveRadius, 120.0f))
		{
			return false;
		}
		if (!AddFixedRecipeInstance(Context, Stream, &BuildingSlot.MainBuilding, BuildingSlot.BuildingId, FVector(ClusterCenter.X, ClusterCenter.Y, 0.0f), BuildingYaw))
		{
			return false;
		}
		ReservePlacement(BuildingPlacements, ClusterCenter, ClusterReserveRadius);

		if (BuildingSlot.LocalProps.Meshes.Num() == 0)
		{
			return true;
		}

		TArray<int32> PropMeshIndices;
		for (int32 Index = 0; Index < BuildingSlot.LocalProps.Meshes.Num(); ++Index)
		{
			PropMeshIndices.Add(Index);
		}
		for (int32 Index = 0; Index < PropMeshIndices.Num(); ++Index)
		{
			const int32 SwapIndex = Stream.RandRange(Index, PropMeshIndices.Num() - 1);
			if (Index != SwapIndex)
			{
				PropMeshIndices.Swap(Index, SwapIndex);
			}
		}

		const int32 TargetPropCount = FMath::Min(PropMeshIndices.Num(), FMath::Clamp(FMath::RoundToInt(HalfSizeCm / 950.0f), 5, 10));
		const FVector2D RoadSideReference = ClusterCenter - ClosestRoad;
		const bool bHasRoadSideReference = RoadSideReference.SizeSquared() > FMath::Square(120.0f);
		for (int32 PropOrderIndex = 0; PropOrderIndex < TargetPropCount; ++PropOrderIndex)
		{
			const FScatterRegionRecipeMesh& PropMesh = BuildingSlot.LocalProps.Meshes[PropMeshIndices[PropOrderIndex]];
			if (!PropMesh.Mesh)
			{
				continue;
			}
			const float PropRadius = MeshPlanarRadius(PropMesh, 160.0f);
			const float MinRadius = FMath::Clamp(MainBuildingRadius + PropRadius + 120.0f, 360.0f, 1600.0f);
			const float MaxRadius = FMath::Clamp(MainBuildingRadius + 950.0f, MinRadius + 180.0f, 2600.0f);
			for (int32 Attempt = 0; Attempt < 36; ++Attempt)
			{
				const float Angle = FMath::DegreesToRadians(BuildingYaw + Stream.FRandRange(-115.0f, 115.0f));
				const float Radius = Stream.FRandRange(MinRadius, MaxRadius);
				const FVector2D PropLocation = ClusterCenter + FVector2D(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);
				if (!IsInsideRegion(PropLocation, HalfSizeCm * 0.86f))
				{
					continue;
				}
				if (bHasRoadSideReference && FVector2D::DotProduct(PropLocation - ClosestRoad, RoadSideReference) < 0.0f)
				{
					continue;
				}
				if (DistanceToRoadsSquared(PropLocation, RoadSegments) < FMath::Square(Profile.RoadWidth * 0.5f + PropRadius + 120.0f))
				{
					continue;
				}
				if (!ReserveCollision(BuildingCollision, PropLocation, PropRadius, 100.0f))
				{
					continue;
				}
				const float PropYaw = BuildingYaw + Stream.FRandRange(-140.0f, 140.0f);
				if (AddMeshInstance(Context, Stream, PropMesh, BuildingSlot.BuildingId + TEXT("_local_props"), FVector(PropLocation.X, PropLocation.Y, 0.0f), PropYaw, FVector::OneVector, false))
				{
					ReservePlacement(BuildingPlacements, PropLocation, PropRadius);
					break;
				}
			}
		}
		return true;
	}

	void GeneratePerimeter(FScatterRegionBuildContext& Context, const FScatterRegionRecipe& Recipe, FRandomStream& Stream, float HalfSizeCm)
	{
		if (!Recipe.bUseFence)
		{
			return;
		}
		const FScatterRegionObjectRecipe* Fence = RecipeForSlot(Recipe, TEXT("fence"));
		WarnIfMissing(Context, Recipe, TEXT("fence"));
		const float Step = MeshMajorStep(Fence, 300.0f);
		for (float X = -HalfSizeCm; X <= HalfSizeCm; X += Step)
		{
			if (Recipe.bUseGate && FMath::Abs(X) < Step)
			{
				continue;
			}
			AddRecipeInstance(Context, Stream, Fence, TEXT("fence"), FVector(X, -HalfSizeCm, 0.0f), 0.0f);
			AddRecipeInstance(Context, Stream, Fence, TEXT("fence"), FVector(X, HalfSizeCm, 0.0f), 180.0f);
		}
		for (float Y = -HalfSizeCm; Y <= HalfSizeCm; Y += Step)
		{
			AddRecipeInstance(Context, Stream, Fence, TEXT("fence"), FVector(-HalfSizeCm, Y, 0.0f), 90.0f);
			AddRecipeInstance(Context, Stream, Fence, TEXT("fence"), FVector(HalfSizeCm, Y, 0.0f), 270.0f);
		}

		if (Recipe.bUseGate)
		{
			const FScatterRegionObjectRecipe* Gate = RecipeForSlot(Recipe, TEXT("gate"));
			WarnIfMissing(Context, Recipe, TEXT("gate"));
			AddRecipeInstance(Context, Stream, Gate, TEXT("gate"), FVector(0.0f, -HalfSizeCm, 0.0f), 0.0f);
		}
	}

	void GenerateRoadCross(FScatterRegionBuildContext& Context, const FScatterRegionRecipe& Recipe, FRandomStream& Stream, float HalfSizeCm)
	{
		const FScatterRegionObjectRecipe* Road = RecipeForSlot(Recipe, TEXT("road"));
		WarnIfMissing(Context, Recipe, TEXT("road"));
		const float Step = MeshMajorStep(Road, 380.0f);
		for (float X = -HalfSizeCm * 0.92f; X <= HalfSizeCm * 0.92f; X += Step)
		{
			AddRecipeInstance(Context, Stream, Road, TEXT("road"), FVector(X, 0.0f, 0.0f), 0.0f);
		}
		for (float Y = -HalfSizeCm * 0.92f; Y <= HalfSizeCm * 0.92f; Y += Step)
		{
			AddRecipeInstance(Context, Stream, Road, TEXT("road"), FVector(0.0f, Y, 0.0f), 90.0f);
		}
	}

	void GenerateVillage(FScatterRegionBuildContext& Context, const FScatterRegionSpec& Spec, const FScatterRegionRecipe& Recipe, FRandomStream& Stream)
	{
		const float HalfSizeCm = Spec.SizeMeters * 50.0f;
		const FScatterRegionGraphProfile Profile = ProfileForRegion(Spec, Recipe);
		TArray<FVector2D> Boundary = BuildClosedRegionBoundary(HalfSizeCm, Stream, Profile, 0.96f);
		FScatterRegionPerimeterSample GateSample;
		if (!TryGetGateSampleFromBoundary(Boundary, GateSample))
		{
			GateSample.Location = FVector2D(0.0f, -HalfSizeCm * 0.92f);
			GateSample.YawDegrees = 0.0f;
		}
		TArray<FScatterRegionRoadSegment> RoadSegments = BuildGateRootedRoadGraph(HalfSizeCm, Boundary, GateSample, Stream, Profile);
		GenerateRoadNetwork(Context, Recipe, Stream, RoadSegments, Profile);
		GenerateSplinePerimeter(Context, Recipe, Stream, Boundary, Profile, &GateSample);

		if (Recipe.BuildingSlots.Num() == 0)
		{
			Context.Warnings.Add(TEXT("Native Village recipe has no enabled BuildingSlots"));
		}

		FScatterRegionPlacementSet BuildingPlacements;
		FScatterRegionCollisionSet BuildingCollision;
		const int32 TargetBuildingClusterCount = FMath::Clamp(FMath::RoundToInt((Spec.SizeMeters * Spec.SizeMeters) / 1900.0f), 5, 28);
		int32 PlacedBuildingClusterCount = 0;

		auto TryPlaceBuildingCluster = [&](int32 ClusterIndex, const FVector2D& ClusterCenter) -> bool
		{
			const FScatterRegionBuildingSlotRecipe* BuildingSlot = PickBuildingSlot(Recipe, Stream);
			if (!BuildingSlot)
			{
				return false;
			}
			if (!IsInsideClosedRegion(ClusterCenter, Boundary, HalfSizeCm) || FMath::Max(FMath::Abs(ClusterCenter.X), FMath::Abs(ClusterCenter.Y)) > HalfSizeCm * 0.86f)
			{
				return false;
			}
			return PlaceScatterRegionBuildingCluster(Context, Stream, *BuildingSlot, ClusterCenter, HalfSizeCm, RoadSegments, Profile, BuildingPlacements, BuildingCollision);
		};

		const int32 RoadsideTargetCount = FMath::Clamp(FMath::RoundToInt(TargetBuildingClusterCount * 0.68f), 3, TargetBuildingClusterCount);
		for (int32 ClusterIndex = 0; ClusterIndex < RoadsideTargetCount; ++ClusterIndex)
		{
			for (int32 Attempt = 0; Attempt < 80; ++Attempt)
			{
				if (RoadSegments.Num() == 0)
				{
					break;
				}
				const FScatterRegionRoadSegment& Segment = RoadSegments[Stream.RandRange(0, RoadSegments.Num() - 1)];
				const float Length = FVector2D::Distance(Segment.Start, Segment.End);
				if (Length <= KINDA_SMALL_NUMBER)
				{
					continue;
				}
				const FVector2D Base = FMath::Lerp(Segment.Start, Segment.End, Stream.FRandRange(0.18f, 0.92f));
				const FVector2D Normal = SegmentNormal(Segment);
				const float Side = ((ClusterIndex + Attempt) % 2) == 0 ? -1.0f : 1.0f;
				const float Offset = Stream.FRandRange(1100.0f, FMath::Clamp(HalfSizeCm * 0.36f, 2200.0f, 3600.0f));
				const FVector2D ClusterCenter = Base + Normal * Side * Offset;
				if (TryPlaceBuildingCluster(ClusterIndex, ClusterCenter))
				{
					++PlacedBuildingClusterCount;
					break;
				}
			}
		}

		const int32 CoverageGrid = FMath::Clamp(FMath::CeilToInt(FMath::Sqrt(static_cast<float>(TargetBuildingClusterCount)) * 1.7f), 4, 8);
		const float CoverageExtent = HalfSizeCm * 0.82f;
		const float MaxRoadReachCm = FMath::Clamp(HalfSizeCm * 0.48f, 3200.0f, 7600.0f);
		for (int32 ClusterIndex = PlacedBuildingClusterCount; ClusterIndex < TargetBuildingClusterCount; ++ClusterIndex)
		{
			bool bPlaced = false;
			for (int32 Attempt = 0; Attempt < 180 && !bPlaced; ++Attempt)
			{
				const int32 CellIndex = (ClusterIndex * 7 + Attempt * 3) % FMath::Max(1, CoverageGrid * CoverageGrid);
				const int32 CellX = CellIndex % CoverageGrid;
				const int32 CellY = CellIndex / CoverageGrid;
				const float CellSize = (CoverageExtent * 2.0f) / static_cast<float>(CoverageGrid);
				const FVector2D CellBase(
					-CoverageExtent + (static_cast<float>(CellX) + 0.5f) * CellSize,
					-CoverageExtent + (static_cast<float>(CellY) + 0.5f) * CellSize);
				const FVector2D ClusterCenter = CellBase + FVector2D(
					Stream.FRandRange(-CellSize * 0.32f, CellSize * 0.32f),
					Stream.FRandRange(-CellSize * 0.32f, CellSize * 0.32f));
				if (!IsInsideClosedRegion(ClusterCenter, Boundary, HalfSizeCm))
				{
					continue;
				}
				if (DistanceToRoadsSquared(ClusterCenter, RoadSegments) > FMath::Square(MaxRoadReachCm) && Attempt < 120)
				{
					continue;
				}
				if (TryPlaceBuildingCluster(ClusterIndex, ClusterCenter))
				{
					++PlacedBuildingClusterCount;
					bPlaced = true;
				}
			}
		}
		if (PlacedBuildingClusterCount < TargetBuildingClusterCount)
		{
			Context.Warnings.Add(FString::Printf(TEXT("Village placed %d/%d requested building clusters after spacing and road avoidance"), PlacedBuildingClusterCount, TargetBuildingClusterCount));
		}

		const FScatterRegionObjectRecipe* SideRoadProps = RecipeForSlot(Recipe, TEXT("side_road_props"));
		const FScatterRegionObjectRecipe* ScatterRegionProps = RecipeForSlot(Recipe, TEXT("ScatterRegion_props"));
		FScatterRegionObjectRecipe ScatterRegionPropsFallback;
		if (!ScatterRegionProps || ScatterRegionProps->Meshes.Num() == 0)
		{
			ScatterRegionPropsFallback = BuildScatterRegionPropsFallbackFromBuildingLocalProps(Recipe);
			if (ScatterRegionPropsFallback.Meshes.Num() > 0)
			{
				ScatterRegionProps = &ScatterRegionPropsFallback;
			}
		}
		FScatterRegionPlacementSet PropPlacements;
		FScatterRegionCollisionSet DecorCollision = BuildingCollision;
		TArray<FScatterRegionGraphPoint> SidePropPoints;
		const float SideRoadPropRadius = RecipeBoundsRadius(SideRoadProps, 90.0f);
		for (const FScatterRegionRoadSegment& Segment : RoadSegments)
		{
			const float Length = FVector2D::Distance(Segment.Start, Segment.End);
			const int32 Count = FMath::Max(1, FMath::FloorToInt(Length / FMath::Max(1300.0f, Profile.RoadSidePropsDistance * 2.4f)));
			const FVector2D Normal = SegmentNormal(Segment);
			for (int32 Index = 0; Index <= Count; ++Index)
			{
				const float T = (static_cast<float>(Index) + Stream.FRandRange(-0.18f, 0.18f)) / FMath::Max(1.0f, static_cast<float>(Count));
				const FVector2D Base = FMath::Lerp(Segment.Start, Segment.End, FMath::Clamp(T, 0.0f, 1.0f));
				for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
				{
					const float Side = SideIndex == 0 ? -1.0f : 1.0f;
					FScatterRegionGraphPoint Point;
					Point.Location = Base + Normal * Side * (Profile.RoadWidth * 0.5f + SideRoadPropRadius + 160.0f);
					Point.YawDegrees = Segment.YawDegrees + (Side > 0.0f ? -90.0f : 90.0f);
					Point.Density = FMath::Clamp(Recipe.SideRoadPropsDensity, 0.0f, 1.0f);
					Point.BoundsRadius = SideRoadPropRadius;
					if (IsInsideRegion(Point.Location, HalfSizeCm))
					{
						SidePropPoints.Add(Point);
					}
				}
			}
		}
		ScatterRegionBoundsModifier(SidePropPoints, SideRoadPropRadius * 2.2f);
		SidePropPoints = ScatterRegionDensityFilter(SidePropPoints, Stream, 1.0f);
		SidePropPoints = ScatterRegionDifferenceAgainstRoads(SidePropPoints, RoadSegments, Profile.RoadWidth * 0.5f + SideRoadPropRadius + 80.0f, Profile.RoadWidth * 0.5f + SideRoadPropRadius + 900.0f);
		SidePropPoints = ScatterRegionDifferenceAgainstPlacements(SidePropPoints, BuildingPlacements, SideRoadPropRadius + 520.0f);
		SidePropPoints = ScatterRegionSelfPruning(SidePropPoints, Stream, FMath::Max(420.0f, SideRoadPropRadius * 1.8f), true);
		ScatterRegionLookAtRoad(SidePropPoints, RoadSegments);
		ScatterRegionTransformPoints(SidePropPoints, Stream, 80.0f, 12.0f, FVector(0.85f), FVector(1.15f), true);
		for (const FScatterRegionGraphPoint& Point : SidePropPoints)
		{
			ReservePlacement(PropPlacements, Point.Location, Point.BoundsRadius);
		}
		CommitScatterRegionGraphPointsAvoiding(Context, Stream, SideRoadProps, TEXT("side_road_props"), SidePropPoints, DecorCollision, 120.0f);

		TArray<FScatterRegionGraphPoint> ScatterRegionPropPoints;
		const int32 PropCount = FMath::Clamp(FMath::RoundToInt(Spec.SizeMeters / 12.0f * FMath::Max(0.08f, Recipe.ScatterRegionPropsDensity) * 2.0f), 8, 90);
		for (int32 Index = 0; Index < PropCount; ++Index)
		{
			const FVector2D Location2D(
				Stream.FRandRange(-HalfSizeCm * 0.82f, HalfSizeCm * 0.82f),
				Stream.FRandRange(-HalfSizeCm * 0.82f, HalfSizeCm * 0.82f));
			if (!IsInsideRegion(Location2D, HalfSizeCm) || Location2D.Size() < HalfSizeCm * 0.2f)
			{
				continue;
			}
			FScatterRegionGraphPoint Point;
			Point.Location = Location2D;
			Point.YawDegrees = Stream.FRandRange(0.0f, 360.0f);
			Point.Density = Recipe.ScatterRegionPropsDensity;
			Point.BoundsRadius = 700.0f;
			ScatterRegionPropPoints.Add(Point);
		}
		ScatterRegionPropPoints = ScatterRegionAttributeFilterDensity(ScatterRegionPropPoints, 0.05f, 1.0f);
		ScatterRegionPropPoints = ScatterRegionDifferenceAgainstRoads(ScatterRegionPropPoints, RoadSegments, Profile.RoadWidth * 0.5f + 900.0f);
		const float ScatterRegionPropBounds = RecipeBoundsRadius(ScatterRegionProps, 140.0f) * 5.0f;
		ScatterRegionBoundsModifier(ScatterRegionPropPoints, ScatterRegionPropBounds);
		ScatterRegionPropPoints = ScatterRegionDifferenceAgainstPlacements(ScatterRegionPropPoints, BuildingPlacements, ScatterRegionPropBounds);
		ScatterRegionPropPoints = ScatterRegionDifferenceAgainstPlacements(ScatterRegionPropPoints, PropPlacements, ScatterRegionPropBounds);
		ScatterRegionPropPoints = ScatterRegionSelfPruning(ScatterRegionPropPoints, Stream, ScatterRegionPropBounds, true);
		ScatterRegionTransformPoints(ScatterRegionPropPoints, Stream, 180.0f, 25.0f, FVector(0.75f), FVector(1.25f), true);
		CommitScatterRegionGraphPointsAvoiding(Context, Stream, ScatterRegionProps, TEXT("ScatterRegion_props"), ScatterRegionPropPoints, DecorCollision, 160.0f);
	}

	void GenerateFarm(FScatterRegionBuildContext& Context, const FScatterRegionSpec& Spec, const FScatterRegionRecipe& Recipe, FRandomStream& Stream)
	{
		const float HalfSizeCm = Spec.SizeMeters * 50.0f;
		const FScatterRegionGraphProfile Profile = ProfileForRegion(Spec, Recipe);
		const FScatterRegionObjectRecipe* Road = RecipeForSlot(Recipe, TEXT("road"));
		const FScatterRegionObjectRecipe* Dirt = RecipeForSlot(Recipe, TEXT("dirt"));
		const FScatterRegionObjectRecipe* Crops = RecipeForSlot(Recipe, TEXT("crops"));
		const FScatterRegionObjectRecipe* Scarecrow = RecipeForSlot(Recipe, TEXT("scarecrow"));
		WarnIfMissing(Context, Recipe, TEXT("dirt"));
		WarnIfMissing(Context, Recipe, TEXT("crops"));

		const TArray<FVector2D> FarmBoundary = BuildClosedRegionBoundary(HalfSizeCm, Stream, Profile, 0.90f);
		TArray<FScatterRegionRoadSegment> RoadSegments = BuildFarmAccessRoads(HalfSizeCm, FarmBoundary);
		const TArray<FScatterRegionRoadTileFootprint> RoadTiles = BuildRoadTileFootprints(Road, Stream, RoadSegments, Profile, 2);
		const TArray<FScatterRegionFarmDirtTile> DirtTiles = BuildFarmDirtTiles(HalfSizeCm, Recipe, Dirt, FarmBoundary, RoadTiles);
		AddRoadTileFootprints(Context, Stream, Road, RoadTiles);

		const TArray<FScatterRegionGraphPoint> DirtPoints = BuildFarmDirtTilePoints(DirtTiles);
		for (const FScatterRegionGraphPoint& Point : DirtPoints)
		{
			AddFixedRecipeInstance(Context, Stream, Dirt, TEXT("dirt"), FVector(Point.Location.X, Point.Location.Y, 0.0f), Point.YawDegrees);
		}

		const float CropLocalZOffsetCm = FMath::Max(0.0f, RecipeFixedHeightCm(Dirt, 0.0f) - 2.0f);
		TArray<FScatterRegionGraphPoint> CropPoints = BuildFarmCropTilePoints(DirtTiles, Recipe, Crops, CropLocalZOffsetCm, Stream);
		FScatterRegionCollisionSet CropCollision;
		CommitScatterRegionGraphPointsAvoiding(Context, Stream, Crops, TEXT("crops"), CropPoints, CropCollision, 18.0f);

		const float ScarecrowSpacing = FMath::Clamp(Recipe.ScarecrowDistanceMultiplicator * 100.0f, 600.0f, 3000.0f);
		TArray<FScatterRegionGraphPoint> ScarecrowPoints = BuildFarmScarecrowPoints(DirtTiles, Recipe, RoadSegments, Stream);
		const TArray<FScatterRegionGraphPoint> ScarecrowSourcePoints = ScarecrowPoints;
		ScarecrowPoints = ScatterRegionDensityFilter(ScarecrowPoints, Stream, 1.0f);
		ScarecrowPoints = ScatterRegionSelfPruning(ScarecrowPoints, Stream, ScarecrowSpacing, true);
		if (Scarecrow)
		{
			ScatterRegionEnsureAtLeastOnePoint(ScarecrowPoints, ScarecrowSourcePoints, Stream);
		}
		ScatterRegionTransformPoints(ScarecrowPoints, Stream, 100.0f, 20.0f, FVector(0.9f), FVector(1.1f), true);
		FScatterRegionCollisionSet ScarecrowCollision;
		CommitScatterRegionGraphPointsAvoiding(Context, Stream, Scarecrow, TEXT("scarecrow"), ScarecrowPoints, ScarecrowCollision, 120.0f);
	}

	void GenerateCemetery(FScatterRegionBuildContext& Context, const FScatterRegionSpec& Spec, const FScatterRegionRecipe& Recipe, FRandomStream& Stream)
	{
		const float HalfSizeCm = Spec.SizeMeters * 50.0f;
		const FScatterRegionGraphProfile Profile = ProfileForRegion(Spec, Recipe);
		const TArray<FVector2D> CemeteryBoundary = BuildClosedRegionBoundary(HalfSizeCm, Stream, Profile, 0.90f);
		TArray<FScatterRegionRoadSegment> RoadSegments = BuildCemeteryAccessRoads(HalfSizeCm, CemeteryBoundary);
		GenerateRoadNetwork(Context, Recipe, Stream, RoadSegments, Profile, 2);

		const FScatterRegionObjectRecipe* Tombs = RecipeForSlot(Recipe, TEXT("tombs"));
		const FScatterRegionObjectRecipe* Panteon = RecipeForSlot(Recipe, TEXT("panteon"));
		WarnIfMissing(Context, Recipe, TEXT("tombs"));

		FScatterRegionPlacementSet MemorialPlacements;
		TArray<FScatterRegionGraphPoint> PanteonPoints = BuildCemeteryPanteonPoints(HalfSizeCm, Recipe, CemeteryBoundary, RoadSegments, Stream);
		ScatterRegionBoundsModifier(PanteonPoints, FMath::Clamp(RecipeBoundsRadius(Panteon, Profile.BuildingDistance * 0.28f), 900.0f, 1900.0f));
		ScatterRegionDensityRemap(PanteonPoints, 0.0f, 1.0f, 0.55f, Recipe.PanteonDensity);
		const TArray<FScatterRegionGraphPoint> PanteonSourcePoints = PanteonPoints;
		PanteonPoints = ScatterRegionDifferenceAgainstRoads(PanteonPoints, RoadSegments, Profile.RoadWidth * 0.5f + 140.0f);
		PanteonPoints = ScatterRegionSlopeFilter(Context, PanteonPoints, Profile.BuildingMaxSlope, Profile.BuildingDistance * 0.25f);
		PanteonPoints = ScatterRegionDensityFilter(PanteonPoints, Stream, 1.0f);
		PanteonPoints = ScatterRegionSelfPruning(PanteonPoints, Stream, FMath::Clamp(Profile.BuildingDistance * 0.36f, 1200.0f, 2100.0f), true);
		if (Panteon && PanteonSourcePoints.Num() > 0)
		{
			const FScatterRegionGraphPoint& CenterMemorialPoint = PanteonSourcePoints[0];
			bool bHasCenterMemorial = false;
			for (const FScatterRegionGraphPoint& Point : PanteonPoints)
			{
				if (FVector2D::Distance(Point.Location, CenterMemorialPoint.Location) < FMath::Max(300.0f, CenterMemorialPoint.BoundsRadius * 0.25f))
				{
					bHasCenterMemorial = true;
					break;
				}
			}
			if (!bHasCenterMemorial)
			{
				PanteonPoints.Insert(CenterMemorialPoint, 0);
			}
		}
		ScatterRegionLookAtRoad(PanteonPoints, RoadSegments);
		ScatterRegionTransformPoints(PanteonPoints, Stream, 140.0f, 8.0f, FVector(0.95f), FVector(1.05f), true);

		FScatterRegionCollisionSet CemeteryPlacements;
		for (const FScatterRegionGraphPoint& Point : PanteonPoints)
		{
			ReservePlacement(MemorialPlacements, Point.Location, Point.BoundsRadius * 1.15f);
		}
		CommitScatterRegionGraphPointsAvoiding(Context, Stream, Panteon, TEXT("panteon"), PanteonPoints, CemeteryPlacements, 140.0f);

		TArray<FScatterRegionGraphPoint> TombPoints = BuildCemeteryTombGridPoints(HalfSizeCm, Recipe, Tombs, CemeteryBoundary, RoadSegments, MemorialPlacements, Stream);
		ScatterRegionBoundsModifier(TombPoints, FMath::Max(Recipe.TombSpacing * 0.28f, RecipeBoundsRadius(Tombs, Recipe.TombSpacing * 0.18f)));
		ScatterRegionDensityRemap(TombPoints, 0.0f, 1.0f, 0.55f, Recipe.TombDensity);
		TombPoints = ScatterRegionDensityFilter(TombPoints, Stream, 1.0f);
		TombPoints = ScatterRegionDifferenceAgainstRoads(TombPoints, RoadSegments, Profile.RoadWidth * 0.5f + 220.0f);
		TombPoints = ScatterRegionDifferenceAgainstPlacements(TombPoints, MemorialPlacements, Recipe.TombSpacing * 1.25f);
		TombPoints = ScatterRegionSelfPruning(TombPoints, Stream, Recipe.TombSpacing * 0.46f, false);
		ScatterRegionTransformPoints(TombPoints, Stream, 16.0f, 2.0f, FVector(0.94f), FVector(1.06f), true);
		CommitScatterRegionGraphPointsAvoiding(Context, Stream, Tombs, TEXT("tombs"), TombPoints, CemeteryPlacements, 60.0f);
	}
}

TSharedPtr<FJsonObject> FScatterRegionGenerator::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == ScatterRegionCommandType)
	{
		return HandleGenerateScatterRegion(Params);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), false);
	Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown scatter region command: %s"), *CommandType));
	return Result;
}

TSharedPtr<FJsonObject> FScatterRegionGenerator::HandleGenerateScatterRegion(const TSharedPtr<FJsonObject>& Params)
{
	FScatterRegionSpec Spec;
	FString SpecError;
	const bool bSpecValid = ParseSpec(Params, Spec, SpecError);
	TSharedPtr<FJsonObject> Result = MakeBaseResult(Spec);
	UE_LOG(LogTemp, Display, TEXT("ScatterRegionAgent: Generate recipe region type=%s seed=%d center=%s size_m=%.1f recipe_asset=%s"),
		*Spec.RegionType,
		Spec.Seed,
		*Spec.Center.ToString(),
		Spec.SizeMeters,
		*Spec.RecipeAssetPath);

	if (!bSpecValid)
	{
		AddError(Result, TEXT("INVALID_SPEC"), SpecError);
		return Result;
	}

	if (!GEditor)
	{
		AddError(Result, TEXT("WORLD_UNAVAILABLE"), TEXT("GEditor is not available"));
		return Result;
	}
	if (GEditor->PlayWorld != nullptr)
	{
		AddError(Result, TEXT("WORLD_UNAVAILABLE"), TEXT("PIE is active; stop play mode before generating an ScatterRegion region"));
		return Result;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World || !World->bIsWorldInitialized || World->bIsTearingDown)
	{
		AddError(Result, TEXT("WORLD_UNAVAILABLE"), TEXT("Current editor world is unavailable"));
		return Result;
	}

	FScatterRegionRecipe Recipe;
	TArray<FString> RecipeWarnings;
	FString RecipeError;
	if (!LoadNativeRegionRecipe(Spec, Recipe, RecipeWarnings, RecipeError))
	{
		AddError(Result, TEXT("ASSET_NOT_FOUND"), RecipeError);
		for (const FString& Warning : RecipeWarnings)
		{
			AddWarning(Result, Warning);
		}
		return Result;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(
		World->PersistentLevel,
		AActor::StaticClass(),
		FName(*FString::Printf(TEXT("ScatterRegion_%s_%d"), *Spec.RegionType, Spec.Seed)));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* RootActor = World->SpawnActor<AActor>(AActor::StaticClass(), Spec.Center, FRotator::ZeroRotator, SpawnParams);
	if (!RootActor)
	{
		AddError(Result, TEXT("BUILD_FAILED"), TEXT("Failed to spawn ScatterRegion region root actor"));
		return Result;
	}

	FScatterRegionBuildContext Context;
	Context.World = World;
	Context.RootActor = RootActor;
	Context.Center = Spec.Center;
	Context.Warnings = RecipeWarnings;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (Actor && Actor != RootActor && Actor->Tags.Contains(FName(GeneratedTagName)))
		{
			Context.ProjectionIgnoredActors.Add(Actor);
		}
	}
	USceneComponent* RootComponent = NewObject<USceneComponent>(RootActor, TEXT("ScatterRegionRoot"));
	RootComponent->CreationMethod = EComponentCreationMethod::Instance;
	RootComponent->SetMobility(EComponentMobility::Static);
	RootActor->SetRootComponent(RootComponent);
	RootActor->AddInstanceComponent(RootComponent);
	RootComponent->RegisterComponent();
	RootActor->SetActorLocation(Spec.Center);

	RootActor->Tags.AddUnique(FName(GeneratedTagName));
	RootActor->Tags.AddUnique(FName(*FString::Printf(TEXT("ScatterRegion.RegionType.%s"), *Spec.RegionType)));
	RootActor->Tags.AddUnique(FName(*FString::Printf(TEXT("ScatterRegion.Seed.%d"), Spec.Seed)));
	RootActor->Tags.AddUnique(FName(*FString::Printf(TEXT("ScatterRegion.RecipeRow.%s"), *Recipe.RowName)));
#if WITH_EDITOR
	RootActor->SetActorLabel(FString::Printf(TEXT("ScatterRegion_%s_%d"), *Spec.RegionType, Spec.Seed));
#endif

	FRandomStream Stream(Spec.Seed);
	if (Spec.RegionType == TEXT("farm"))
	{
		GenerateFarm(Context, Spec, Recipe, Stream);
	}
	else if (Spec.RegionType == TEXT("cemetery"))
	{
		GenerateCemetery(Context, Spec, Recipe, Stream);
	}
	else if (Spec.RegionType == TEXT("village"))
	{
		GenerateVillage(Context, Spec, Recipe, Stream);
	}
	else
	{
		AddError(Result, TEXT("INVALID_SPEC"), TEXT("region_type must be one of: village, farm, cemetery"));
	}

	for (const FString& Warning : Context.Warnings)
	{
		AddWarning(Result, Warning);
	}
	if (Context.ProjectionAttempts > 0 && Context.ProjectionHits == 0)
	{
		AddWarning(Result, TEXT("ScatterRegion generation did not hit Landscape during projection; instances used local recipe height"));
	}
	else if (Context.ProjectionMisses > 0)
	{
		AddWarning(Result, FString::Printf(TEXT("ScatterRegion landscape projection missed %d of %d placement points"), Context.ProjectionMisses, Context.ProjectionAttempts));
	}
	if (Context.InstanceCount <= 0)
	{
		AddError(Result, TEXT("BUILD_FAILED"), TEXT("ScatterRegion generation produced no instances"));
	}

	World->MarkPackageDirty();
	RootActor->MarkPackageDirty();

#if WITH_EDITOR
	Result->SetStringField(TEXT("region_actor"), RootActor->GetActorLabel());
#else
	Result->SetStringField(TEXT("region_actor"), RootActor->GetName());
#endif
	Result->SetStringField(TEXT("recipe_row"), Recipe.RowName);
	Result->SetStringField(TEXT("recipe_asset"), Spec.RecipeAssetPath);
	Result->SetNumberField(TEXT("instance_count"), Context.InstanceCount);
	Result->SetNumberField(TEXT("component_count"), Context.ComponentCount);
	Result->SetNumberField(TEXT("projection_attempts"), Context.ProjectionAttempts);
	Result->SetNumberField(TEXT("projection_hits"), Context.ProjectionHits);
	Result->SetNumberField(TEXT("projection_misses"), Context.ProjectionMisses);
	Result->SetObjectField(TEXT("slot_counts"), IntMapToJson(Context.SlotCounts));
	if (Context.Bounds.IsValid)
	{
		Result->SetObjectField(TEXT("bounds"), BoundsToJson(Context.Bounds));
	}
	else
	{
		AddError(Result, TEXT("BUILD_FAILED"), TEXT("Generated bounds are unavailable"));
	}

	const TArray<TSharedPtr<FJsonValue>>* Errors = nullptr;
	const bool bHasErrors = Result->TryGetArrayField(TEXT("errors"), Errors) && Errors && Errors->Num() > 0;
	Result->SetBoolField(TEXT("success"), !bHasErrors);
	UE_LOG(LogTemp, Display, TEXT("ScatterRegionAgent: Recipe generate finished success=%d instances=%d components=%d"),
		Result->GetBoolField(TEXT("success")) ? 1 : 0,
		Context.InstanceCount,
		Context.ComponentCount);
	return Result;
}


TSharedPtr<FJsonObject> FScatterRegionGenerator::GenerateFromJson(const TSharedPtr<FJsonObject>& Params)
{
	return HandleGenerateScatterRegion(Params);
}



