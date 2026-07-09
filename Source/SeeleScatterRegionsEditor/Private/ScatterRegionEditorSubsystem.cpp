#include "ScatterRegionEditorSubsystem.h"

#include "ScatterRegionJsonAdapter.h"

bool UScatterRegionEditorSubsystem::ExecuteScatterRegionJsonCommand(const FString& CommandType, const FString& ParamsJson, FString& OutResultJson)
{
	return FScatterRegionJsonAdapter::ExecuteJsonCommand(CommandType, ParamsJson, OutResultJson);
}
