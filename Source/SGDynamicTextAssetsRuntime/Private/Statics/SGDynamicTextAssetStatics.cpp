// Copyright Start Games, Inc. All Rights Reserved.

#include "Statics/SGDynamicTextAssetStatics.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetValidationResult.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"
#include "Management/SGDynamicTextAssetFileMetadata.h"
#include "SGDynamicTextAssetLogs.h"
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
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("SGDynamicTextAssetStaticsInternal::GetSubsystem: Inputted NULL WorldContextObject"));
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
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("SGDynamicTextAssetStaticsInternal::GetSubsystem:NULL gameInstance from world(%s)"), *GetNameSafe(world));
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
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("USGDynamicTextAssetStatics::IsDynamicTextAssetRefLoaded: Inputted INVALID Ref"));
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

void USGDynamicTextAssetStatics::LoadDynamicTextAssetRefAsync(const UObject* WorldContextObject, const FSGDynamicTextAssetRef& Ref,
	FOnDynamicTextAssetRefLoaded OnLoaded, const TArray<FName>& BundleNames, const FString& FilePath)
{
	Ref.LoadAsync(WorldContextObject, MoveTemp(OnLoaded), BundleNames, FilePath);
}

bool USGDynamicTextAssetStatics::UnloadDynamicTextAssetRef(const UObject* WorldContextObject, const FSGDynamicTextAssetRef& Ref)
{
	if (!Ref.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("USGDynamicTextAssetStatics::UnloadDynamicTextAssetRef: Inputted INVALID Ref"));
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
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("USGDynamicTextAssetStatics::GetAllDynamicTextAssetUserFacingIdsByClass: Inputted NULL DynamicTextAssetClass"));
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
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("USGDynamicTextAssetStatics::GetAllLoadedDynamicTextAssetsOfClass: Inputted NULL DynamicTextAssetClass"));
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

bool USGDynamicTextAssetStatics::GetBundleDataFromRef(const UObject* WorldContextObject, const FSGDynamicTextAssetRef& Ref, FSGDynamicTextAssetBundleData& OutBundleData)
{
	if (!Ref.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("USGDynamicTextAssetStatics::GetBundleDataFromRef: Inputted INVALID Ref"));
		return false;
	}

	USGDynamicTextAssetSubsystem* subsystem = SGDynamicTextAssetStaticsInternal::GetSubsystem(WorldContextObject);
	if (subsystem)
	{
		return subsystem->GetSGDTAssetBundleDataCopy(Ref.GetId(), OutBundleData);
	}

#if WITH_EDITOR
	TScriptInterface<ISGDynamicTextAssetProvider> provider = FSGDynamicTextAssetEditorCache::Get().LoadDynamicTextAsset(Ref.GetId());
	if (provider.GetInterface())
	{
		OutBundleData = provider->GetSGDTAssetBundleData();
		return OutBundleData.HasBundles();
	}
#endif

	return false;
}

bool USGDynamicTextAssetStatics::GetBundleDataFromProvider(const TScriptInterface<ISGDynamicTextAssetProvider>& Provider, FSGDynamicTextAssetBundleData& OutBundleData)
{
	if (!Provider.GetInterface())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("USGDynamicTextAssetStatics::GetBundleDataFromProvider: Inputted NULL Provider"));
		return false;
	}

	OutBundleData = Provider->GetSGDTAssetBundleData();
	return OutBundleData.HasBundles();
}

bool USGDynamicTextAssetStatics::HasBundles(const FSGDynamicTextAssetBundleData& BundleData)
{
	return BundleData.HasBundles();
}

int32 USGDynamicTextAssetStatics::GetBundleCount(const FSGDynamicTextAssetBundleData& BundleData)
{
	return BundleData.Bundles.Num();
}

void USGDynamicTextAssetStatics::GetBundleNames(const FSGDynamicTextAssetBundleData& BundleData, TArray<FName>& OutBundleNames)
{
	BundleData.GetBundleNames(OutBundleNames);
}

bool USGDynamicTextAssetStatics::GetPathsForBundle(const FSGDynamicTextAssetBundleData& BundleData, FName BundleName, TArray<FSoftObjectPath>& OutPaths)
{
	return BundleData.GetPathsForBundle(BundleName, OutPaths);
}

bool USGDynamicTextAssetStatics::GetBundleEntries(const FSGDynamicTextAssetBundleData& BundleData, FName BundleName, TArray<FSGDynamicTextAssetBundleEntry>& OutEntries)
{
	const FSGDynamicTextAssetBundle* bundle = BundleData.FindBundle(BundleName);
	if (!bundle)
	{
		return false;
	}

	OutEntries = bundle->Entries;
	return true;
}

void USGDynamicTextAssetStatics::ExtractBundleDataFromObject(UObject* Object, FSGDynamicTextAssetBundleData& BundleData)
{
	if (!Object)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("USGDynamicTextAssetStatics::ExtractBundleDataFromObject: Inputted NULL Object"));
		return;
	}
	BundleData.ExtractFromObject(Object);
}

void USGDynamicTextAssetStatics::ResetBundleData(FSGDynamicTextAssetBundleData& BundleData)
{
	BundleData.Reset();
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

void USGDynamicTextAssetStatics::ValidateSoftPathsInProperty(const FProperty* Property, const void* ContainerPtr,
	const FString& PropertyPath, FSGDynamicTextAssetValidationResult& OutResult)
{
	if (!Property)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("USGDynamicTextAssetStatics::ValidateSoftPathsInProperty: Inputted NULL Property"));
        return;
    }
    if (!ContainerPtr)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("USGDynamicTextAssetStatics::ValidateSoftPathsInProperty: Inputted NULL ContainerPtr"));
        return;
    }

    auto validateAssetPath = [&OutResult, &PropertyPath](const FSoftObjectPath& AssetPath, bool bIsSoftClassProperty)
    {
        if (AssetPath.IsValid() && !AssetPath.IsNull())
        {
            FAssetRegistryModule& assetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
            FAssetData assetData = assetRegistryModule.Get().GetAssetByObjectPath(AssetPath);

            // If it failed to find the asset, check if it's a Blueprint Class reference ending in "_C"
            if (!assetData.IsValid())
            {
                FString pathString = AssetPath.ToString();
                if (pathString.EndsWith(TEXT("_C")))
                {
                    pathString.RemoveFromEnd(TEXT("_C"));
                    FSoftObjectPath strippedPath(pathString);
                    assetData = assetRegistryModule.Get().GetAssetByObjectPath(strippedPath);
                }
            }

            // If still not found, check if this is a sub-object path (e.g., a level actor instance).
            // Sub-object paths use ':' to separate the top-level asset from the sub-object within it.
            // The Asset Registry only tracks top-level assets, so validate the parent asset instead.
            // Examples:
            //   /Game/Lvl_Basic.Lvl_Basic:PersistentLevel.BP_TestActor_C_2 -> validates /Game/Lvl_Basic.Lvl_Basic
            //   /Game/SomeBP.SomeBP:MyComponent -> validates /Game/SomeBP.SomeBP
            if (!assetData.IsValid())
            {
                FString subPathString = AssetPath.GetSubPathString();
                if (!subPathString.IsEmpty())
                {
                    FSoftObjectPath parentPath(AssetPath.GetAssetPath(), FString());
                    assetData = assetRegistryModule.Get().GetAssetByObjectPath(parentPath);
                }
            }

            if (!assetData.IsValid())
            {
                OutResult.AddError(
                    FText::Format(INVTEXT("Missing asset for path: {0}"), FText::FromString(AssetPath.ToString())),
                    PropertyPath,
                    INVTEXT("The asset referenced by this soft path could not be found in the Asset Registry. It may have been deleted, moved, or renamed.")
                );
            }
            else if (!bIsSoftClassProperty)
            {
                // Only warn about UBlueprint references for TSoftObjectPtr properties.
                // TSoftClassPtr references resolve to the Blueprint's generated class (_C suffix),
                // which survives cooking and works correctly in packaged builds.
                if (assetData.IsInstanceOf<UBlueprint>())
                {
                    OutResult.AddWarning(
                        FText::Format(
                            INVTEXT("Soft reference to Blueprint asset: {0}"),
                            FText::FromString(AssetPath.ToString())),
                        PropertyPath,
                        INVTEXT("UBlueprint objects are stripped in packaged builds, so this TSoftObjectPtr will resolve to null at runtime. "
                        	"Use TSoftClassPtr to reference the Blueprint's generated class, or reference a non-Blueprint asset (DataAsset, Material, Texture, etc.) instead.")
                    );
                }
            }
        }
    };

    // Instanced object properties own sub-objects whose properties may contain soft paths.
    // Walk into the sub-object and recurse on its properties.
    if (const FObjectProperty* objectProp = CastField<FObjectProperty>(Property))
    {
        if (Property->HasAllPropertyFlags(CPF_InstancedReference))
        {
            const void* valuePtr = objectProp->ContainerPtrToValuePtr<void>(ContainerPtr);
            if (const UObject* subObject = objectProp->GetObjectPropertyValue(valuePtr))
            {
                for (TFieldIterator<FProperty> innerIt(subObject->GetClass()); innerIt; ++innerIt)
                {
                    const FProperty* innerProp = *innerIt;
                    FString nestedPath = FString::Printf(TEXT("%s.%s"), *PropertyPath, *innerProp->GetName());
                    ValidateSoftPathsInProperty(innerProp, subObject, nestedPath, OutResult);
                }
            }
            return;
        }
    }

    if (const FSoftClassProperty* softClassProp = CastField<FSoftClassProperty>(Property))
    {
        const void* valuePtr = softClassProp->ContainerPtrToValuePtr<void>(ContainerPtr);
        if (const FSoftObjectPtr* softPtr = static_cast<const FSoftObjectPtr*>(valuePtr))
        {
            validateAssetPath(softPtr->ToSoftObjectPath(), true);
        }
        return;
    }

    if (const FSoftObjectProperty* softObjProp = CastField<FSoftObjectProperty>(Property))
    {
        const void* valuePtr = softObjProp->ContainerPtrToValuePtr<void>(ContainerPtr);
        if (const FSoftObjectPtr* softPtr = static_cast<const FSoftObjectPtr*>(valuePtr))
        {
            validateAssetPath(softPtr->ToSoftObjectPath(), false);
        }
        return;
    }

    if (const FStructProperty* structProp = CastField<FStructProperty>(Property))
    {
        const void* structPtr = structProp->ContainerPtrToValuePtr<void>(ContainerPtr);
        for (TFieldIterator<FProperty> innerIt(structProp->Struct); innerIt; ++innerIt)
        {
            const FProperty* innerProp = *innerIt;
            FString nestedPath = FString::Printf(TEXT("%s.%s"), *PropertyPath, *innerProp->GetName());
            ValidateSoftPathsInProperty(innerProp, structPtr, nestedPath, OutResult);
        }
        return;
    }

    if (const FArrayProperty* arrayProp = CastField<FArrayProperty>(Property))
    {
        const void* arrayPtr = arrayProp->ContainerPtrToValuePtr<void>(ContainerPtr);
        FScriptArrayHelper arrayHelper(arrayProp, arrayPtr);

        for (int32 index = 0; index < arrayHelper.Num(); ++index)
        {
            const void* elementPtr = arrayHelper.GetRawPtr(index);
            FString elementPath = FString::Printf(TEXT("%s[%d]"), *PropertyPath, index);
            ValidateSoftPathsInProperty(arrayProp->Inner, elementPtr, elementPath, OutResult);
        }
        return;
    }

    if (const FMapProperty* mapProp = CastField<FMapProperty>(Property))
    {
        const void* mapPtr = mapProp->ContainerPtrToValuePtr<void>(ContainerPtr);
        FScriptMapHelper mapHelper(mapProp, mapPtr);

        for (FScriptMapHelper::FIterator itr = mapHelper.CreateIterator(); itr; ++itr)
        {
            const void* valuePtr = mapHelper.GetValuePtr(itr.GetInternalIndex());
            FString valuePath = FString::Printf(TEXT("%s[Value]"), *PropertyPath);
            ValidateSoftPathsInProperty(mapProp->ValueProp, valuePtr, valuePath, OutResult);
        }
    }
}
