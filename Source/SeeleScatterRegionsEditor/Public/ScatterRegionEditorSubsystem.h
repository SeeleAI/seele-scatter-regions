#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "ScatterRegionEditorSubsystem.generated.h"

UCLASS()
class SEELESCATTERREGIONSEDITOR_API UScatterRegionEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Scatter Region")
	bool ExecuteScatterRegionJsonCommand(const FString& CommandType, const FString& ParamsJson, FString& OutResultJson);
};
