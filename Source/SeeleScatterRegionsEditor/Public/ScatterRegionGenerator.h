// Copyright 2026 QUANLING SHENZHEN NETWORK CO., LTD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

class SEELESCATTERREGIONSEDITOR_API FScatterRegionGenerator
{
public:
	TSharedPtr<FJsonObject> GenerateFromJson(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> HandleGenerateScatterRegion(const TSharedPtr<FJsonObject>& Params);
};
