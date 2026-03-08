// Copyright Start Games, Inc. All Rights Reserved.

#include "Commandlets/SGDynamicTextAssetValidationCommandlet.h"

#include "SGDynamicTextAssetsEditorModule.h"
#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetRef.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileMetadata.h"
#include "Serialization/SGDynamicTextAssetSerializer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"

USGDynamicTextAssetValidationCommandlet::USGDynamicTextAssetValidationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 USGDynamicTextAssetValidationCommandlet::Main(const FString& Params)
{
	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== SGDynamicTextAssetValidation Commandlet Started ==="));

	// Parse command line parameters
	TArray<FString> tokens;
	TArray<FString> switches;
	TMap<FString, FString> paramMap;
	ParseCommandLine(*Params, tokens, switches, paramMap);

	bool bStrict = switches.Contains(TEXT("strict"));
	bool bVerbose = switches.Contains(TEXT("verbose"));

	if (bStrict)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Mode: Strict (warnings treated as errors)"));
	}
	if (bVerbose)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Mode: Verbose logging enabled"));
	}

	// Step 1: Build a set of all known IDs from disk
	TSet<FSGDynamicTextAssetId> knownIds;
	CollectKnownIds(knownIds);

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Found %d dynamic text asset(s) on disk"), knownIds.Num());

	if (knownIds.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("No dynamic text asset files found. Nothing to validate against."));
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== SGDynamicTextAssetValidation Commandlet Finished (0 references) ==="));
		return 0;
	}

	// Step 2: Scan all assets for FSGDynamicTextAssetRef properties and validate
	TArray<FBrokenReference> brokenRefs;
	TArray<FValidationWarning> warnings;

	ValidateBlueprintAssets(knownIds, brokenRefs, warnings, bVerbose);
	ValidateDynamicTextAssetFiles(knownIds, brokenRefs, warnings, bVerbose);

	// Step 3: Report results
	if (!warnings.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("--- Warnings (%d) ---"), warnings.Num());
		for (const FValidationWarning& warning : warnings)
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("  [%s] %s :: %s"),
				*warning.SourceAsset, *warning.PropertyPath, *warning.Message);
		}
	}

	if (!brokenRefs.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("--- Broken References (%d) ---"), brokenRefs.Num());
		for (const FBrokenReference& broken : brokenRefs)
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Error,
				TEXT("  BROKEN REF in '%s' at '%s': ID %s does not exist on disk"),
				*broken.SourceDisplayName, *broken.PropertyPath, *broken.ReferencedId.ToString());
		}
	}

	int32 errorCount = brokenRefs.Num();
	if (bStrict)
	{
		errorCount += warnings.Num();
	}

	if (errorCount > 0)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Validation FAILED: %d broken reference(s), %d warning(s)"),
			brokenRefs.Num(), warnings.Num());
		if (bStrict && warnings.Num() > 0)
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("  (Strict mode: %d warning(s) treated as errors)"), warnings.Num());
		}
	}
	else
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Validation PASSED: 0 broken references, %d warning(s)"), warnings.Num());
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("=== SGDynamicTextAssetValidation Commandlet Finished ==="));

	return errorCount > 0 ? 1 : 0;
}

void USGDynamicTextAssetValidationCommandlet::CollectKnownIds(TSet<FSGDynamicTextAssetId>& OutKnownIds) const
{
	OutKnownIds.Reset();

	TArray<FString> allFiles;
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(allFiles);

	for (const FString& filePath : allFiles)
	{
		// Only consider JSON source files as the authoritative set
		if (FSGDynamicTextAssetFileManager::GetSupportedExtensionForFile(filePath).IsEmpty())
		{
			continue;
		}

		const FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(filePath);
		if (metadata.bIsValid && metadata.Id.IsValid())
		{
			OutKnownIds.Add(metadata.Id);
		}
	}
}

void USGDynamicTextAssetValidationCommandlet::ValidateBlueprintAssets(
	const TSet<FSGDynamicTextAssetId>& KnownIds,
	TArray<FBrokenReference>& OutBrokenRefs,
	TArray<FValidationWarning>& OutWarnings,
	bool bVerbose) const
{
	IAssetRegistry* assetRegistry = IAssetRegistry::Get();
	if (!assetRegistry)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Asset Registry not available, skipping Blueprint scan"));
		return;
	}

	TArray<FAssetData> blueprintAssets;
	const FTopLevelAssetPath blueprintClassPath = UBlueprint::StaticClass()->GetClassPathName();
	assetRegistry->GetAssetsByClass(blueprintClassPath, blueprintAssets, true);

	int32 assetsScanned = 0;

	for (const FAssetData& assetData : blueprintAssets)
	{
		UBlueprint* blueprint = Cast<UBlueprint>(assetData.GetAsset());
		if (!blueprint)
		{
			continue;
		}
		UClass* generatedClass = blueprint->GeneratedClass;
		if (!generatedClass)
		{
			continue;
		}
		UObject* cdo = generatedClass->GetDefaultObject(false);
		if (!cdo)
		{
			continue;
		}

		const FString sourceAsset = assetData.GetSoftObjectPath().ToString();
		const FString displayName = assetData.AssetName.ToString();
		assetsScanned++;

		if (bVerbose)
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("  Scanning Blueprint: %s"), *displayName);
		}

		for (TFieldIterator<FProperty> propIt(generatedClass); propIt; ++propIt)
		{
			const FProperty* property = *propIt;
			ValidateRefsInProperty(property, cdo, property->GetName(),
				KnownIds, sourceAsset, displayName, OutBrokenRefs, OutWarnings);
		}
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Scanned %d Blueprint asset(s)"), assetsScanned);
}

void USGDynamicTextAssetValidationCommandlet::ValidateDynamicTextAssetFiles(
	const TSet<FSGDynamicTextAssetId>& KnownIds,
	TArray<FBrokenReference>& OutBrokenRefs,
	TArray<FValidationWarning>& OutWarnings,
	bool bVerbose) const
{
	TArray<FString> allFiles;
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(allFiles);

	int32 filesScanned = 0;

	for (const FString& filePath : allFiles)
	{
		if (FSGDynamicTextAssetFileManager::GetSupportedExtensionForFile(filePath).IsEmpty())
		{
			continue;
		}

		FString jsonString;
		if (!FSGDynamicTextAssetFileManager::ReadRawFileContents(filePath, jsonString))
		{
			continue;
		}

		// Extract metadata for display
		const FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(filePath);
		if (!metadata.bIsValid)
		{
			continue;
		}

		TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(filePath);
		if (!serializer.IsValid())
		{
			continue;
		}

		UClass* dataObjectClass = nullptr;
		for (TObjectIterator<UClass> classIt; classIt; ++classIt)
		{
			if (classIt->GetName() == metadata.ClassName && classIt->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
			{
				dataObjectClass = *classIt;
				break;
			}
		}

		if (!dataObjectClass)
		{
			continue;
		}

		// Create transient object and deserialize
		TScriptInterface<ISGDynamicTextAssetProvider> tempObject = NewObject<UObject>(GetTransientPackage(), dataObjectClass);
		if (!tempObject)
		{
			continue;
		}

		bool migrated;
		if (!serializer->DeserializeProvider(jsonString, tempObject.GetInterface(), migrated))
		{
			continue;
		}

		const FString displayName = metadata.UserFacingId.IsEmpty()
			? FSGDynamicTextAssetFileManager::ExtractUserFacingIdFromPath(filePath)
			: metadata.UserFacingId;
		filesScanned++;

		if (bVerbose)
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("  Scanning DynamicTextAsset: %s (%s)"), *displayName, *metadata.ClassName);
		}

		for (TFieldIterator<FProperty> propIt(dataObjectClass); propIt; ++propIt)
		{
			const FProperty* property = *propIt;
			ValidateRefsInProperty(property, tempObject.GetObject(), property->GetName(),
				KnownIds, filePath, displayName, OutBrokenRefs, OutWarnings);
		}
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Scanned %d dynamic text asset file(s)"), filesScanned);
}

void USGDynamicTextAssetValidationCommandlet::ValidateRefsInProperty(
	const FProperty* Property,
	const void* ContainerPtr,
	const FString& PropertyPath,
	const TSet<FSGDynamicTextAssetId>& KnownIds,
	const FString& SourceAsset,
	const FString& SourceDisplayName,
	TArray<FBrokenReference>& OutBrokenRefs,
	TArray<FValidationWarning>& OutWarnings) const
{
	if (!Property || !ContainerPtr)
	{
		return;
	}

	// Check direct struct property
	if (const FStructProperty* structProp = CastField<FStructProperty>(Property))
	{
		if (structProp->Struct == FSGDynamicTextAssetRef::StaticStruct())
		{
			const void* valuePtr = structProp->ContainerPtrToValuePtr<void>(ContainerPtr);
			const FSGDynamicTextAssetRef* ref = static_cast<const FSGDynamicTextAssetRef*>(valuePtr);
			if (ref)
			{
				if (ref->IsValid())
				{
					// Valid ID - check if it exists on disk
					if (!KnownIds.Contains(ref->GetId()))
					{
						FBrokenReference broken;
						broken.ReferencedId = ref->GetId();
						broken.SourceAsset = SourceAsset;
						broken.PropertyPath = PropertyPath;
						broken.SourceDisplayName = SourceDisplayName;
						OutBrokenRefs.Add(MoveTemp(broken));
					}
				}
				// Null/empty refs are not errors - they may be intentionally unset
			}
			return;
		}

		// Recurse into non-target struct properties
		const void* structPtr = structProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		for (TFieldIterator<FProperty> innerIt(structProp->Struct); innerIt; ++innerIt)
		{
			const FProperty* innerProp = *innerIt;
			FString nestedPath = FString::Printf(TEXT("%s.%s"), *PropertyPath, *innerProp->GetName());
			ValidateRefsInProperty(innerProp, structPtr, nestedPath,
				KnownIds, SourceAsset, SourceDisplayName, OutBrokenRefs, OutWarnings);
		}
		return;
	}

	// Check array property
	if (const FArrayProperty* arrayProp = CastField<FArrayProperty>(Property))
	{
		const void* arrayPtr = arrayProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		FScriptArrayHelper arrayHelper(arrayProp, arrayPtr);

		for (int32 index = 0; index < arrayHelper.Num(); ++index)
		{
			const void* elementPtr = arrayHelper.GetRawPtr(index);
			FString elementPath = FString::Printf(TEXT("%s[%d]"), *PropertyPath, index);

			if (const FStructProperty* innerStruct = CastField<FStructProperty>(arrayProp->Inner))
			{
				if (innerStruct->Struct == FSGDynamicTextAssetRef::StaticStruct())
				{
					const FSGDynamicTextAssetRef* ref = static_cast<const FSGDynamicTextAssetRef*>(elementPtr);
					if (ref && ref->IsValid())
					{
						if (!KnownIds.Contains(ref->GetId()))
						{
							FBrokenReference broken;
							broken.ReferencedId = ref->GetId();
							broken.SourceAsset = SourceAsset;
							broken.PropertyPath = elementPath;
							broken.SourceDisplayName = SourceDisplayName;
							OutBrokenRefs.Add(MoveTemp(broken));
						}
					}
				}
				else
				{
					// Recurse into struct elements
					for (TFieldIterator<FProperty> innerIt(innerStruct->Struct); innerIt; ++innerIt)
					{
						const FProperty* innerProp = *innerIt;
						FString nestedPath = FString::Printf(TEXT("%s.%s"), *elementPath, *innerProp->GetName());
						ValidateRefsInProperty(innerProp, elementPtr, nestedPath,
							KnownIds, SourceAsset, SourceDisplayName, OutBrokenRefs, OutWarnings);
					}
				}
			}
		}
		return;
	}

	// Check map property
	if (const FMapProperty* mapProp = CastField<FMapProperty>(Property))
	{
		const void* mapPtr = mapProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		FScriptMapHelper mapHelper(mapProp, mapPtr);

		for (FScriptMapHelper::FIterator it = mapHelper.CreateIterator(); it; ++it)
		{
			const void* valuePtr = mapHelper.GetValuePtr(it.GetInternalIndex());
			FString valuePath = FString::Printf(TEXT("%s[Value]"), *PropertyPath);

			if (const FStructProperty* valueStruct = CastField<FStructProperty>(mapProp->ValueProp))
			{
				if (valueStruct->Struct == FSGDynamicTextAssetRef::StaticStruct())
				{
					const FSGDynamicTextAssetRef* ref = static_cast<const FSGDynamicTextAssetRef*>(valuePtr);
					if (ref && ref->IsValid())
					{
						if (!KnownIds.Contains(ref->GetId()))
						{
							FBrokenReference broken;
							broken.ReferencedId = ref->GetId();
							broken.SourceAsset = SourceAsset;
							broken.PropertyPath = valuePath;
							broken.SourceDisplayName = SourceDisplayName;
							OutBrokenRefs.Add(MoveTemp(broken));
						}
					}
				}
				else
				{
					for (TFieldIterator<FProperty> innerIt(valueStruct->Struct); innerIt; ++innerIt)
					{
						const FProperty* innerProp = *innerIt;
						FString nestedPath = FString::Printf(TEXT("%s.%s"), *valuePath, *innerProp->GetName());
						ValidateRefsInProperty(innerProp, valuePtr, nestedPath,
							KnownIds, SourceAsset, SourceDisplayName, OutBrokenRefs, OutWarnings);
					}
				}
			}
		}
	}
}
