// Copyright Start Games, Inc. All Rights Reserved.

#include "Statics/SGDynamicTextAssetStatics.h"

#include "SGDynamicTextAssetsRuntimeModule.h"
#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetValidationResult.h"
#include "Engine/Engine.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"
#include "Management/SGDynamicTextAssetFileMetadata.h"
#include "Subsystem/SGDynamicTextAssetSubsystem.h"
#include "Engine/GameInstance.h"

#if WITH_EDITOR
#include "Management/SGDynamicTextAssetEditorCache.h"
#endif

namespace SGDynamicTextAssetStaticsInternal
{
	USGDynamicTextAssetSubsystem* GetSubsystem(const UObject* WorldContextObject)
	{
		if (!WorldContextObject)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted NULL WorldContextObject"));
			return nullptr;
		}
		const UWorld* world = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (!world)
		{
			return nullptr;
		}
		UGameInstance* gameInstance = world->GetGameInstance();
		if (!gameInstance)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("NULL gameInstance from world(%s)"), *GetNameSafe(world));
			return nullptr;
		}
		return gameInstance->GetSubsystem<USGDynamicTextAssetSubsystem>();
	}
}

bool USGDynamicTextAssetStatics::IsValidDynamicTextAssetRef(const FSGDynamicTextAssetRef& Ref)
{
	return Ref.IsValid();
}

bool USGDynamicTextAssetStatics::IsDynamicTextAssetRefLoaded(const UObject* WorldContextObject, const FSGDynamicTextAssetRef& Ref)
{
	if (!Ref.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted INVALID Ref"));
		return false;
	}
	USGDynamicTextAssetSubsystem* subsystem = SGDynamicTextAssetStaticsInternal::GetSubsystem(WorldContextObject);
	if (!subsystem)
	{
#if WITH_EDITOR
		return FSGDynamicTextAssetEditorCache::Get().IsCached(Ref.GetId());
#else
		return false;
#endif
	}

	return subsystem->IsDynamicTextAssetCached(Ref.GetId());
}

void USGDynamicTextAssetStatics::SetDynamicTextAssetRefById(FSGDynamicTextAssetRef& Ref, const FSGDynamicTextAssetId& Id)
{
	Ref.SetId(Id);
}

bool USGDynamicTextAssetStatics::SetDynamicTextAssetRefByUserFacingId(FSGDynamicTextAssetRef& Ref, const FString& UserFacingId)
{
	// Scan files to find the ID matching this UserFacingId
	TArray<FString> filePaths;
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(filePaths);

	for (const FString& filePath : filePaths)
	{
		FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(filePath);
		if (metadata.bIsValid && metadata.UserFacingId.Equals(UserFacingId, ESearchCase::IgnoreCase))
		{
			Ref.SetId(metadata.Id);
			return true;
		}
	}

	return false;
}

void USGDynamicTextAssetStatics::ClearDynamicTextAssetRef(FSGDynamicTextAssetRef& Ref)
{
	Ref.Reset();
}

FSGDynamicTextAssetId USGDynamicTextAssetStatics::GetDynamicTextAssetRefId(const FSGDynamicTextAssetRef& Ref)
{
	return Ref.GetId();
}

FString USGDynamicTextAssetStatics::GetDynamicTextAssetRefUserFacingId(const FSGDynamicTextAssetRef& Ref)
{
	return Ref.GetUserFacingId();
}

TScriptInterface<ISGDynamicTextAssetProvider> USGDynamicTextAssetStatics::GetDynamicTextAsset(const UObject* WorldContextObject, const FSGDynamicTextAssetRef& Ref)
{
	return Ref.Get(WorldContextObject);
}

void USGDynamicTextAssetStatics::LoadDynamicTextAssetRefAsync(const UObject* WorldContextObject, const FSGDynamicTextAssetRef& Ref, FOnDynamicTextAssetRefLoaded OnLoaded, const FString& FilePath)
{
	if (!Ref.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted INVALID Ref"));
		OnLoaded.ExecuteIfBound(TScriptInterface<ISGDynamicTextAssetProvider>(), false);
		return;
	}
	USGDynamicTextAssetSubsystem* subsystem = SGDynamicTextAssetStaticsInternal::GetSubsystem(WorldContextObject);
	if (!subsystem)
	{
#if WITH_EDITOR
		TScriptInterface<ISGDynamicTextAssetProvider> result = FSGDynamicTextAssetEditorCache::Get().LoadDynamicTextAsset(Ref.GetId());
		OnLoaded.ExecuteIfBound(result, result.GetObject() != nullptr);
#else
		OnLoaded.ExecuteIfBound(TScriptInterface<ISGDynamicTextAssetProvider>(), false);
#endif
		return;
	}

	// Check if already cached
	TScriptInterface<ISGDynamicTextAssetProvider> cached = subsystem->GetDynamicTextAsset(Ref.GetId());
	if (cached.GetObject())
	{
		OnLoaded.ExecuteIfBound(cached, true);
		return;
	}

	// If no path provided, use lazy search via subsystem
	if (FilePath.IsEmpty())
	{
		subsystem->LoadDynamicTextAssetAsync(Ref.GetId(), nullptr,
			FOnDynamicTextAssetLoaded::CreateLambda([OnLoaded](const TScriptInterface<ISGDynamicTextAssetProvider>& Provider, bool bSuccess)
			{
				OnLoaded.ExecuteIfBound(Provider, bSuccess);
			}));
		return;
	}

	// Load async using the subsystem with provided path
	subsystem->LoadDynamicTextAssetFromFileAsync(FilePath, nullptr,
		FOnDynamicTextAssetLoaded::CreateLambda([OnLoaded](const TScriptInterface<ISGDynamicTextAssetProvider>& Provider, bool bSuccess)
		{
			OnLoaded.ExecuteIfBound(Provider, bSuccess);
		}));
}

bool USGDynamicTextAssetStatics::UnloadDynamicTextAssetRef(const UObject* WorldContextObject, const FSGDynamicTextAssetRef& Ref)
{
	if (!Ref.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted INVALID Ref"));
		return false;
	}
	USGDynamicTextAssetSubsystem* subsystem = SGDynamicTextAssetStaticsInternal::GetSubsystem(WorldContextObject);
	if (!subsystem)
	{
#if WITH_EDITOR
		return FSGDynamicTextAssetEditorCache::Get().RemoveFromCache(Ref.GetId());
#else
		return false;
#endif
	}
	return subsystem->RemoveFromCache(Ref.GetId());
}

bool USGDynamicTextAssetStatics::EqualEqual_DynamicTextAssetRef(const FSGDynamicTextAssetRef& A,
	const FSGDynamicTextAssetRef& B)
{
	return A == B;
}

bool USGDynamicTextAssetStatics::NotEqual_DynamicTextAssetRef(const FSGDynamicTextAssetRef& A,
	const FSGDynamicTextAssetRef& B)
{
	return A != B;
}

bool USGDynamicTextAssetStatics::Equals_DynamicTextAssetRefDynamicTextAssetId(const FSGDynamicTextAssetRef& Ref, const FSGDynamicTextAssetId& Id)
{
	return Ref == Id;
}

bool USGDynamicTextAssetStatics::NotEquals_DynamicTextAssetRefDynamicTextAssetId(const FSGDynamicTextAssetRef& Ref, const FSGDynamicTextAssetId& Id)
{
	return Ref != Id;
}

bool USGDynamicTextAssetStatics::EqualEqual_DynamicTextAssetId(const FSGDynamicTextAssetId& A, const FSGDynamicTextAssetId& B)
{
	return A == B;
}

bool USGDynamicTextAssetStatics::NotEqual_DynamicTextAssetId(const FSGDynamicTextAssetId& A, const FSGDynamicTextAssetId& B)
{
	return A != B;
}

void USGDynamicTextAssetStatics::GetAllDynamicTextAssetIdsByClass(UClass* DynamicTextAssetClass, bool bIncludeSubclasses, TArray<FSGDynamicTextAssetId>& OutIds)
{
	OutIds.Empty();

	if (!DynamicTextAssetClass)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted NULL DynamicTextAssetClass"));
		return;
	}

	TArray<FString> filePaths;
	FSGDynamicTextAssetFileManager::FindAllFilesForClass(DynamicTextAssetClass, filePaths, bIncludeSubclasses);

	for (const FString& filePath : filePaths)
	{
		FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(filePath);
		if (metadata.bIsValid)
		{
			OutIds.Add(metadata.Id);
		}
	}
}

void USGDynamicTextAssetStatics::GetAllDynamicTextAssetUserFacingIdsByClass(UClass* DynamicTextAssetClass, bool bIncludeSubclasses, TArray<FString>& OutUserFacingIds)
{
	OutUserFacingIds.Empty();

	if (!DynamicTextAssetClass)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted NULL DynamicTextAssetClass"));
		return;
	}

	TArray<FString> filePaths;
	FSGDynamicTextAssetFileManager::FindAllFilesForClass(DynamicTextAssetClass, filePaths, bIncludeSubclasses);

	for (const FString& filePath : filePaths)
	{
		FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(filePath);
		if (metadata.bIsValid)
		{
			OutUserFacingIds.Add(metadata.UserFacingId);
		}
	}
}

void USGDynamicTextAssetStatics::GetAllLoadedDynamicTextAssetsOfClass(const UObject* WorldContextObject, UClass* DynamicTextAssetClass, bool bIncludeSubclasses, TArray<TScriptInterface<ISGDynamicTextAssetProvider>>& OutDynamicTextAssets)
{
	OutDynamicTextAssets.Empty();

	if (!DynamicTextAssetClass)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("Inputted NULL DynamicTextAssetClass"));
		return;
	}
	USGDynamicTextAssetSubsystem* subsystem = SGDynamicTextAssetStaticsInternal::GetSubsystem(WorldContextObject);
	if (!subsystem)
	{
		return;
	}

	// Get all ids first
	TArray<FSGDynamicTextAssetId> ids;
	GetAllDynamicTextAssetIdsByClass(DynamicTextAssetClass, bIncludeSubclasses, ids);

	// Filter to only loaded providers
	for (const FSGDynamicTextAssetId& id : ids)
	{
		TScriptInterface<ISGDynamicTextAssetProvider> provider = subsystem->GetDynamicTextAsset(id);
		if (provider.GetObject())
		{
			OutDynamicTextAssets.Add(provider);
		}
	}
}

FSGDynamicTextAssetRef USGDynamicTextAssetStatics::MakeDynamicTextAssetRef(const FSGDynamicTextAssetId& Id)
{
	return FSGDynamicTextAssetRef(Id);
}

UClass* USGDynamicTextAssetStatics::GetDynamicTextAssetTypeFromId(const FSGDynamicTextAssetId& Id)
{
	if (!Id.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("USGDynamicTextAssetStatics::GetDynamicTextAssetTypeFromId:: Inputted INVALID ID"));
		return nullptr;
	}
	TArray<FString> outFiles;
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(outFiles);

	for (const FString& filePath : outFiles)
	{
		const FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(filePath);
		if (!metadata.bIsValid || !metadata.Id.IsValid())
		{
			continue;
		}
		if (metadata.Id != Id)
		{
			continue;
		}
		// Relying on editor, cook, and deserialization validation to ensure its a valid dynamic text asset
		// otherwise this would return null.
		return FindFirstObject<UClass>(*metadata.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
	}
	return nullptr;
}

FString USGDynamicTextAssetStatics::IdToString(const FSGDynamicTextAssetId& Id)
{
	return Id.ToString();
}

bool USGDynamicTextAssetStatics::StringToId(const FString& IdString, FSGDynamicTextAssetId& OutId)
{
	return OutId.ParseString(IdString);
}

bool USGDynamicTextAssetStatics::IsValid_DTA_TypeId(const FSGDynamicTextAssetTypeId& AssetTypeId)
{
	return AssetTypeId.IsValid();
}

bool USGDynamicTextAssetStatics::EqualEqual_DTA_TypeId(const FSGDynamicTextAssetTypeId& A, const FSGDynamicTextAssetTypeId& B)
{
	return A == B;
}

bool USGDynamicTextAssetStatics::NotEqual_DTA_TypeId(const FSGDynamicTextAssetTypeId& A, const FSGDynamicTextAssetTypeId& B)
{
	return A != B;
}

FString USGDynamicTextAssetStatics::ToString_DTA_TypeId(const FSGDynamicTextAssetTypeId& AssetTypeId)
{
	return AssetTypeId.ToString();
}

bool USGDynamicTextAssetStatics::FromString_DTA_TypeId(const FString& AssetTypeIdString, FSGDynamicTextAssetTypeId& OutAssetTypeId)
{
	return OutAssetTypeId.ParseString(AssetTypeIdString);
}

FSGDynamicTextAssetTypeId USGDynamicTextAssetStatics::FromGuid_DTA_TypeId(const FGuid& Guid)
{
	return FSGDynamicTextAssetTypeId(Guid);
}

FGuid USGDynamicTextAssetStatics::GetGuid_DTA_TypeId(const FSGDynamicTextAssetTypeId& AssetTypeId)
{
	return AssetTypeId.GetGuid();
}

FSGDynamicTextAssetTypeId USGDynamicTextAssetStatics::GenerateDynamicAssetTypeId()
{
#if WITH_EDITORONLY_DATA
	return FSGDynamicTextAssetTypeId::NewGeneratedId();
#else
	return FSGDynamicTextAssetTypeId::INVALID_TYPE_ID;
#endif
}

bool USGDynamicTextAssetStatics::IsVersionValid(const FSGDynamicTextAssetVersion& Version)
{
	return Version.IsValid();
}

bool USGDynamicTextAssetStatics::IsVersionCompatibleWith(const FSGDynamicTextAssetVersion& A, const FSGDynamicTextAssetVersion& B)
{
	return A.IsCompatibleWith(B);
}

bool USGDynamicTextAssetStatics::EqualEqual_DynamicTextAssetVersionDynamicTextAssetVersion(const FSGDynamicTextAssetVersion& A,
	const FSGDynamicTextAssetVersion& B)
{
	return A == B;
}

bool USGDynamicTextAssetStatics::NotEqual_DynamicTextAssetVersionDynamicTextAssetVersion(const FSGDynamicTextAssetVersion& A,
	const FSGDynamicTextAssetVersion& B)
{
	return A != B;
}

FSGDynamicTextAssetId USGDynamicTextAssetStatics::GetDynamicTextAssetId_Provider(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider)
{
	if (!Provider)
	{
		return FSGDynamicTextAssetId();
	}
	return Provider->GetDynamicTextAssetId();
}

FString USGDynamicTextAssetStatics::GetUserFacingId_Provider(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider)
{
	if (!Provider)
	{
		return FString();
	}
	return Provider->GetUserFacingId();
}

int32 USGDynamicTextAssetStatics::GetCurrentMajorVersion_Provider(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider)
{
	if (!Provider)
	{
		return 0;
	}
	return Provider->GetCurrentMajorVersion();
}

FSGDynamicTextAssetVersion USGDynamicTextAssetStatics::GetVersion_Provider(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider)
{
	if (!Provider)
	{
		return FSGDynamicTextAssetVersion();
	}
	return Provider->GetVersion();
}

FSGDynamicTextAssetVersion USGDynamicTextAssetStatics::GetCurrentVersion_Provider(
	const TScriptInterface<ISGDynamicTextAssetProvider>& Provider)
{
	if (!Provider)
	{
		return FSGDynamicTextAssetVersion();
	}
	return Provider->GetCurrentVersion();
}

bool USGDynamicTextAssetStatics::ValidationResultHasErrors(const FSGDynamicTextAssetValidationResult& Result)
{
	return Result.HasErrors();
}

bool USGDynamicTextAssetStatics::ValidationResultHasWarnings(const FSGDynamicTextAssetValidationResult& Result)
{
	return Result.HasWarnings();
}

bool USGDynamicTextAssetStatics::ValidationResultHasInfos(const FSGDynamicTextAssetValidationResult& Result)
{
	return Result.HasInfos();
}

bool USGDynamicTextAssetStatics::IsValidationResultEmpty(const FSGDynamicTextAssetValidationResult& Result)
{
	return Result.IsEmpty();
}

bool USGDynamicTextAssetStatics::IsValidationResultValid(const FSGDynamicTextAssetValidationResult& Result)
{
	return Result.IsValid();
}

int32 USGDynamicTextAssetStatics::GetValidationResultTotalCount(const FSGDynamicTextAssetValidationResult& Result)
{
	return Result.GetTotalCount();
}

void USGDynamicTextAssetStatics::GetValidationResultErrors(const FSGDynamicTextAssetValidationResult& Result, TArray<FSGDynamicTextAssetValidationEntry>& OutErrors)
{
	OutErrors = Result.Errors;
}

void USGDynamicTextAssetStatics::GetValidationResultWarnings(const FSGDynamicTextAssetValidationResult& Result, TArray<FSGDynamicTextAssetValidationEntry>& OutWarnings)
{
	OutWarnings = Result.Warnings;
}

void USGDynamicTextAssetStatics::GetValidationResultInfos(const FSGDynamicTextAssetValidationResult& Result, TArray<FSGDynamicTextAssetValidationEntry>& OutInfos)
{
	OutInfos = Result.Infos;
}

FString USGDynamicTextAssetStatics::ValidationResultToString(const FSGDynamicTextAssetValidationResult& Result)
{
	return Result.ToFormattedString();
}

void USGDynamicTextAssetStatics::LogRegisteredSerializers()
{
#if !UE_BUILD_SHIPPING || SG_DYNAMIC_TEXT_ASSETS_LOG_SERIALIZERS_SHIPPING
	// Purposely excluding shipping builds from being able to run this function to avoid cheaters/hackers
	// correlating information to break your app.
	//
	// @see SGDynamicTextAssetsRuntime.Build.cs for enabling SG_DYNAMIC_TEXT_ASSETS_LOG_SERIALIZERS_SHIPPING in your project.

	TArray<FString> descriptions;
	FSGDynamicTextAssetFileManager::GetAllRegisteredSerializerDescriptions(descriptions);
	UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("=== Registered SGDynamicTextAsset Serializers (%d) ==="), descriptions.Num());
	for (const FString& desc : descriptions)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Log, TEXT("  %s"), *desc);
	}
#endif
}

void USGDynamicTextAssetStatics::GetRegisteredSerializerDescriptions(TArray<FString>& OutDescriptions)
{
	FSGDynamicTextAssetFileManager::GetAllRegisteredSerializerDescriptions(OutDescriptions);
}

TSharedPtr<ISGDynamicTextAssetSerializer> USGDynamicTextAssetStatics::FindSerializerForTypeId(uint32 TypeId)
{
	return FSGDynamicTextAssetFileManager::FindSerializerForTypeId(TypeId);
}

TSharedPtr<ISGDynamicTextAssetSerializer> USGDynamicTextAssetStatics::FindSerializerForDynamicTextAssetId(const FSGDynamicTextAssetId& Id)
{
	FString filePath;
	if (!FSGDynamicTextAssetFileManager::FindFileForId(Id, filePath))
	{
		return nullptr;
	}
	return FSGDynamicTextAssetFileManager::FindSerializerForFile(filePath);
}

uint32 USGDynamicTextAssetStatics::GetTypeIdForExtension(const FString& Extension)
{
	return FSGDynamicTextAssetFileManager::GetTypeIdForExtension(Extension);
}
