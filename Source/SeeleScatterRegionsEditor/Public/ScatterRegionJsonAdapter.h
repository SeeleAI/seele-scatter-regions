// Copyright 2026 QUANLING SHENZHEN NETWORK CO., LTD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SEELESCATTERREGIONSEDITOR_API FScatterRegionJsonAdapter
{
public:
	static bool ExecuteJsonCommand(const FString& CommandType, const FString& ParamsJson, FString& OutResultJson);
};
