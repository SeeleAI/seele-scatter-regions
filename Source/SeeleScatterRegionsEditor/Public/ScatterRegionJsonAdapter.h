#pragma once

#include "CoreMinimal.h"

class SEELESCATTERREGIONSEDITOR_API FScatterRegionJsonAdapter
{
public:
	static bool ExecuteJsonCommand(const FString& CommandType, const FString& ParamsJson, FString& OutResultJson);
};
