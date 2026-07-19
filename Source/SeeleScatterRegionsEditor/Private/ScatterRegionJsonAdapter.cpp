// Copyright 2026 QUANLING SHENZHEN NETWORK CO., LTD. All Rights Reserved.

#include "ScatterRegionJsonAdapter.h"

#include "ScatterRegionGenerator.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FScatterRegionJsonAdapter::ExecuteJsonCommand(const FString& CommandType, const FString& ParamsJson, FString& OutResultJson)
{
	TSharedPtr<FJsonObject> Params;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ParamsJson);
	if (!FJsonSerializer::Deserialize(Reader, Params) || !Params.IsValid())
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("Invalid JSON params"));

		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutResultJson);
		FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
		return false;
	}

	FScatterRegionGenerator Generator;
	const TSharedPtr<FJsonObject> Result = Generator.HandleCommand(CommandType, Params);
	const bool bSuccess = Result.IsValid() && Result->HasTypedField<EJson::Boolean>(TEXT("success")) && Result->GetBoolField(TEXT("success"));

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutResultJson);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
	return bSuccess;
}
