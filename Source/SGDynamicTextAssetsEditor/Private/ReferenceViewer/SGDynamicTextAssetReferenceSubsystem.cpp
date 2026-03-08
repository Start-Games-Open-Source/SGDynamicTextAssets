// Copyright Start Games, Inc. All Rights Reserved.

#include "ReferenceViewer/SGDynamicTextAssetReferenceSubsystem.h"

#include "SGDynamicTextAssetsEditorModule.h"
#include "Settings/SGDynamicTextAssetEditorSettings.h"
#include "Core/ISGDynamicTextAssetProvider.h"
#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetRef.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileMetadata.h"
#include "Serialization/SGDynamicTextAssetJsonSerializer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "GameFramework/Actor.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/SimpleConstructionScript.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void USGDynamicTextAssetReferenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bCachePopulated = false;
	bScanningInProgress = false;
	bForceFullRescan = false;

	// Load persistent cache from disk
	LoadCacheFromDisk();

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("SGDynamicTextAssetReferenceSubsystem initialized"));
}

void USGDynamicTextAssetReferenceSubsystem::Deinitialize()
{
	ReferencerCache.Empty();

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("SGDynamicTextAssetReferenceSubsystem deinitialized"));

	Super::Deinitialize();
}

void USGDynamicTextAssetReferenceSubsystem::FindReferencers(const FSGDynamicTextAssetId& Id, TArray<FSGDynamicTextAssetReferenceEntry>& OutReferencers, bool bForceRescan)
{
	OutReferencers.Reset();

	if (!Id.IsValid())
	{
		return;
	}
	if (!bCachePopulated || bForceRescan)
	{
		RebuildReferenceCache();
	}
	if (const TArray<FSGDynamicTextAssetReferenceEntry>* found = ReferencerCache.Find(Id))
	{
		OutReferencers = *found;
	}
}

void USGDynamicTextAssetReferenceSubsystem::FindDependencies(const FSGDynamicTextAssetId& Id, TArray<FSGDynamicTextAssetDependencyEntry>& OutDependencies)
{
	OutDependencies.Reset();

	if (!Id.IsValid())
	{
		return;
	}

	// Find the file for this ID
	FString filePath;
	if (!FSGDynamicTextAssetFileManager::FindFileForId(Id, filePath))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("FindDependencies: Could not find file for Id %s"), *Id.ToString());
		return;
	}

	// Read and deserialize the dynamic text asset to inspect its properties
	FString jsonString;
	if (!FSGDynamicTextAssetFileManager::ReadRawFileContents(filePath, jsonString))
	{
		return;
	}

	// Extract metadata to instantiate the correct type
	FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(filePath);
	if (!metadata.bIsValid)
	{
		return;
	}

	// Find the UClass using FindFirstObject which handles class lookup by name correctly
	UClass* dataObjectClass = FindFirstObject<UClass>(*metadata.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
	
	// Verify it's a valid dynamic text asset class
	if (dataObjectClass && !dataObjectClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
	{
		dataObjectClass = nullptr;
	}

	if (!dataObjectClass)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("FindDependencies: Could not find class '%s' for ID %s"), *metadata.ClassName, *Id.ToString());
		return;
	}

	// Skip abstract classes — they cannot be instantiated
	if (dataObjectClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("FindDependencies: Class '%s' is abstract, cannot instantiate for ID %s"), *metadata.ClassName, *Id.ToString());
		return;
	}

	// Create a transient object and deserialize into it
	UObject* tempObject = NewObject<UObject>(GetTransientPackage(), dataObjectClass);
	if (!tempObject)
	{
		return;
	}

	ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(tempObject);
	if (!provider)
	{
		return;
	}

	TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(filePath);
	if (!serializer.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("FindDependencies: Could not find serializer for file: %s"), *filePath);
		return;
	}

	bool bMigrated = false;
	if (!serializer->DeserializeProvider(jsonString, provider, bMigrated))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("FindDependencies: Failed to deserialize ID %s"), *Id.ToString());
		return;
	}

	ExtractDependenciesFromObject(tempObject, OutDependencies);
}

int32 USGDynamicTextAssetReferenceSubsystem::GetReferencerCount(const FSGDynamicTextAssetId& Id)
{
	if (!Id.IsValid())
	{
		return 0;
	}
	if (!bCachePopulated)
	{
		RebuildReferenceCache();
	}
	if (const TArray<FSGDynamicTextAssetReferenceEntry>* found = ReferencerCache.Find(Id))
	{
		return found->Num();
	}
	return 0;
}

void USGDynamicTextAssetReferenceSubsystem::InvalidateCache()
{
	ReferencerCache.Empty();
	bCachePopulated = false;
}

void USGDynamicTextAssetReferenceSubsystem::RebuildReferenceCache()
{
	ReferencerCache.Empty();

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Rebuilding dynamic text asset reference cache (synchronous)..."));

	// Show blocking progress dialog for synchronous scan
	FScopedSlowTask slowTask(3.0f, INVTEXT("Scanning for Dynamic Text Asset references..."));
	slowTask.MakeDialog();

	slowTask.EnterProgressFrame(1.0f, INVTEXT("Scanning Blueprint assets..."));
	ScanBlueprintAssets();

	slowTask.EnterProgressFrame(1.0f, INVTEXT("Scanning Level assets..."));
	ScanLevelAssets();

	slowTask.EnterProgressFrame(1.0f, INVTEXT("Scanning Dynamic Text Asset files..."));
	ScanDynamicTextAssetFiles();

	bCachePopulated = true;

	int32 totalRefs = 0;
	for (const TPair<FSGDynamicTextAssetId, TArray<FSGDynamicTextAssetReferenceEntry>>& pair : ReferencerCache)
	{
		totalRefs += pair.Value.Num();
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Reference cache built: %d dynamic text assets with %d total references"),
		ReferencerCache.Num(), totalRefs);
}

void USGDynamicTextAssetReferenceSubsystem::RebuildReferenceCacheAsync()
{
	// If already scanning, don't start another scan
	if (bScanningInProgress)
	{
		return;
	}

	bScanningInProgress = true;

	// First, rebuild ReferencerCache from existing PersistentAssetCache
	// This gives us immediate results for cached entries
	RebuildReferencerCacheFromPersistent();

	// Gather all assets to scan (fast - no loading)
	PendingBlueprintAssets.Empty();
	PendingLevelAssets.Empty();
	PendingDynamicTextAssetFiles.Empty();

	// Get editor settings for filtering
	const USGDynamicTextAssetEditorSettings* settings = USGDynamicTextAssetEditorSettings::Get();
	const bool bScanEngine = settings ? settings->bScanEngineContent : false;
	const bool bScanPlugins = settings ? settings->bScanPluginContent : true;

	if (IAssetRegistry* assetRegistry = IAssetRegistry::Get())
	{
		// Get Blueprint assets from registry (no loading yet)
		TArray<FAssetData> allBlueprints;
		const FTopLevelAssetPath blueprintClassPath = UBlueprint::StaticClass()->GetClassPathName();
		assetRegistry->GetAssetsByClass(blueprintClassPath, allBlueprints, true);

		// Filter and check timestamps for Blueprints
		for (const FAssetData& assetData : allBlueprints)
		{
			FString packagePath = assetData.PackageName.ToString();
			ESGReferenceCacheContentType contentType = GetContentTypeForPath(packagePath);

			// Apply content type filtering
			if (contentType == ESGReferenceCacheContentType::EngineContent && !bScanEngine)
			{
				continue;
			}
			if (contentType == ESGReferenceCacheContentType::PluginContent && !bScanPlugins)
			{
				continue;
			}

			// Check if asset needs rescan (timestamp comparison)
			FDateTime assetTimestamp = FDateTime::MinValue();
			FString packageFilename;
			if (FPackageName::DoesPackageExist(packagePath, &packageFilename))
			{
				assetTimestamp = IFileManager::Get().GetTimeStamp(*packageFilename);
			}

			if (DoesAssetNeedRescan(packagePath, assetTimestamp))
			{
				PendingBlueprintAssets.Add(assetData);
			}
		}

		// Get Level/World assets from registry (no loading yet)
		TArray<FAssetData> allLevels;
		const FTopLevelAssetPath worldClassPath = UWorld::StaticClass()->GetClassPathName();
		assetRegistry->GetAssetsByClass(worldClassPath, allLevels, true);

		// Filter and check timestamps for Levels
		for (const FAssetData& assetData : allLevels)
		{
			FString packagePath = assetData.PackageName.ToString();
			ESGReferenceCacheContentType contentType = GetContentTypeForPath(packagePath);

			// Apply content type filtering
			if (contentType == ESGReferenceCacheContentType::EngineContent && !bScanEngine)
			{
				continue;
			}
			if (contentType == ESGReferenceCacheContentType::PluginContent && !bScanPlugins)
			{
				continue;
			}

			// Check if asset needs rescan
			FDateTime assetTimestamp = FDateTime::MinValue();
			FString packageFilename;
			if (FPackageName::DoesPackageExist(packagePath, &packageFilename))
			{
				assetTimestamp = IFileManager::Get().GetTimeStamp(*packageFilename);
			}

			if (DoesAssetNeedRescan(packagePath, assetTimestamp))
			{
				PendingLevelAssets.Add(assetData);
			}
		}
	}

	// Get dynamic text asset files (fast - just file listing)
	TArray<FString> allDynamicTextAssetFiles;
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(allDynamicTextAssetFiles);

	// Check timestamps for dynamic text asset files
	for (const FString& filePath : allDynamicTextAssetFiles)
	{
		FDateTime fileTimestamp = IFileManager::Get().GetTimeStamp(*filePath);
		if (DoesAssetNeedRescan(filePath, fileTimestamp))
		{
			PendingDynamicTextAssetFiles.Add(filePath);
		}
	}

	// Calculate totals for progress
	TotalItemsToScan = PendingBlueprintAssets.Num() + PendingLevelAssets.Num() + PendingDynamicTextAssetFiles.Num();
	ItemsScanned = 0;
	CurrentScanPhase = 0; // Start with Blueprints

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Starting async reference scan: %d Blueprints, %d Levels, %d Dynamic Text Assets (incremental: %s)"),
		PendingBlueprintAssets.Num(), PendingLevelAssets.Num(), PendingDynamicTextAssetFiles.Num(),
		bForceFullRescan ? TEXT("no - forced full") : TEXT("yes"));

	// If nothing to scan, complete immediately
	if (TotalItemsToScan == 0)
	{
		bCachePopulated = true;
		bScanningInProgress = false;
		bForceFullRescan = false;

		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Reference cache up to date - no assets need rescanning"));
		OnReferenceScanComplete.Broadcast();
		return;
	}

	// Start the ticker to process batches each frame
	ScanTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &USGDynamicTextAssetReferenceSubsystem::ProcessScanBatch),
		0.0f // Every frame
	);
}

bool USGDynamicTextAssetReferenceSubsystem::ProcessScanBatch(float DeltaTime)
{
	// Time budget per frame (10ms to keep UI responsive)
	constexpr double TIME_BUDGET_SECONDS = 0.010;
	const double startTime = FPlatformTime::Seconds();

	while ((FPlatformTime::Seconds() - startTime) < TIME_BUDGET_SECONDS)
	{
		// Phase 0: Blueprints
		if (CurrentScanPhase == 0)
		{
			if (PendingBlueprintAssets.Num() > 0)
			{
				FAssetData assetData = PendingBlueprintAssets.Pop(EAllowShrinking::No);
				ProcessBlueprintAsset(assetData);
				ItemsScanned++;
			}
			else
			{
				CurrentScanPhase = 1; // Move to Levels
			}
		}
		// Phase 1: Levels
		else if (CurrentScanPhase == 1)
		{
			if (PendingLevelAssets.Num() > 0)
			{
				FAssetData assetData = PendingLevelAssets.Pop(EAllowShrinking::No);
				ProcessLevelAsset(assetData);
				ItemsScanned++;
			}
			else
			{
				CurrentScanPhase = 2; // Move to Dynamic Text Assets
			}
		}
		// Phase 2: Dynamic Text Asset files
		else if (CurrentScanPhase == 2)
		{
			if (PendingDynamicTextAssetFiles.Num() > 0)
			{
				FString filePath = PendingDynamicTextAssetFiles.Pop(EAllowShrinking::No);
				ProcessDynamicTextAssetFile(filePath);
				ItemsScanned++;
			}
			else
			{
				// All done!
				bCachePopulated = true;
				bScanningInProgress = false;
				bForceFullRescan = false;

				// Rebuild referencer cache from the updated persistent cache
				RebuildReferencerCacheFromPersistent();

				// Save the cache to disk
				SaveCacheToDisk();

				int32 totalRefs = 0;
				for (const TPair<FSGDynamicTextAssetId, TArray<FSGDynamicTextAssetReferenceEntry>>& pair : ReferencerCache)
				{
					totalRefs += pair.Value.Num();
				}

				UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Async reference cache complete: %d dynamic text assets with %d total references, %d cached assets"),
					ReferencerCache.Num(), totalRefs, PersistentAssetCache.Num());

				// Close toast notification with success
				CloseScanNotification(true);

				// Broadcast completion
				OnReferenceScanComplete.Broadcast();

				return false; // Remove ticker
			}
		}
	}

	// Broadcast progress
	FText statusText;
	if (CurrentScanPhase == 0)
	{
		statusText = FText::Format(INVTEXT("SG Dynamic Text Assets\nScanning Blueprints... ({0} remaining)"),
			FText::AsNumber(PendingBlueprintAssets.Num()));
	}
	else if (CurrentScanPhase == 1)
	{
		statusText = FText::Format(INVTEXT("SG Dynamic Text Assets\nScanning Levels... ({0} remaining)"),
			FText::AsNumber(PendingLevelAssets.Num()));
	}
	else
	{
		statusText = FText::Format(INVTEXT("SG Dynamic Text Assets\nScanning Dynamic Text Assets... ({0} remaining)"),
			FText::AsNumber(PendingDynamicTextAssetFiles.Num()));
	}

	// Update toast notification
	float progress = (TotalItemsToScan > 0) ? static_cast<float>(ItemsScanned) / static_cast<float>(TotalItemsToScan) : 0.0f;
	UpdateScanNotification(statusText, progress);

	// Broadcast progress to listeners
	OnReferenceScanProgress.Broadcast(ItemsScanned, TotalItemsToScan, statusText);

	return true; // Continue ticking
}

void USGDynamicTextAssetReferenceSubsystem::ProcessBlueprintAsset(const FAssetData& AssetData)
{
	// Load the Blueprint to access its generated class
	UBlueprint* blueprint = Cast<UBlueprint>(AssetData.GetAsset());
	if (!blueprint)
	{
		return;
	}

	UClass* generatedClass = blueprint->GeneratedClass;
	if (!generatedClass)
	{
		return;
	}

	// Get the CDO to read property values
	UObject* cdo = generatedClass->GetDefaultObject(false);
	if (!cdo)
	{
		return;
	}

	const FString packagePath = AssetData.PackageName.ToString();
	const FSoftObjectPath sourcePath = AssetData.GetSoftObjectPath();
	const FString displayName = AssetData.AssetName.ToString();

	// Create/update cache entry
	FSGReferenceCacheEntry& cacheEntry = PersistentAssetCache.FindOrAdd(packagePath);
	cacheEntry.AssetPath = packagePath;
	cacheEntry.SourceDisplayName = displayName;
	cacheEntry.ReferenceType = ESGReferenceType::Blueprint;
	cacheEntry.ContentType = GetContentTypeForPath(packagePath);
	cacheEntry.FoundReferences.Empty();

	// Get file timestamp
	FString packageFilename;
	if (FPackageName::DoesPackageExist(packagePath, &packageFilename))
	{
		cacheEntry.LastModifiedTime = IFileManager::Get().GetTimeStamp(*packageFilename);
	}
	else
	{
		cacheEntry.LastModifiedTime = FDateTime::UtcNow();
	}

	// Collect all references into a local array
	TArray<FSGDynamicTextAssetReferenceEntry> localEntries;
	ExtractRefsFromObject(generatedClass, cdo, sourcePath, displayName, ESGReferenceType::Blueprint, localEntries);

	// Scan default subobjects (components) for Actor Blueprints only.
	// Non-Actor Blueprints skip the component scan but still contribute their CDO entries above.
	AActor* cdoActor = Cast<AActor>(cdo);
	if (cdoActor)
	{
		AActor::ForEachComponentOfActorClassDefault(cdoActor->GetClass(), UActorComponent::StaticClass(), [this, &displayName, &sourcePath, &localEntries](const UActorComponent* component)
		{
			FString strippedComponentName = component->GetName();
			// Removing the suffix `_GEN_VARIABLE` which signals that it was added in the editor and not in C++
			if (strippedComponentName.EndsWith(USimpleConstructionScript::ComponentTemplateNameSuffix))
			{
				strippedComponentName = strippedComponentName.LeftChop(USimpleConstructionScript::ComponentTemplateNameSuffix.Len());
			}

			// Build context: "BP_Name > ComponentName"
			FString componentDisplayName = FString::Printf(TEXT("%s > %s"), *displayName, *strippedComponentName);
			ExtractRefsFromObject(component->GetClass(), component, sourcePath, componentDisplayName, ESGReferenceType::Blueprint, localEntries);
			return true;
		});
	}

	// Encode all collected entries directly into the persistent cache entry
	for (const FSGDynamicTextAssetReferenceEntry& entry : localEntries)
	{
		cacheEntry.FoundReferences.FindOrAdd(entry.ReferencedId).AddUnique(entry.SourceDisplayName + TEXT("|") + entry.PropertyPath);
	}
}

void USGDynamicTextAssetReferenceSubsystem::ProcessLevelAsset(const FAssetData& AssetData)
{
	// Load the World to access its actors
	UWorld* world = Cast<UWorld>(AssetData.GetAsset());
	if (!world)
	{
		return;
	}

	const FString packagePath = AssetData.PackageName.ToString();
	const FSoftObjectPath sourcePath = AssetData.GetSoftObjectPath();
	const FString displayName = AssetData.AssetName.ToString();

	// Create/update cache entry
	FSGReferenceCacheEntry& cacheEntry = PersistentAssetCache.FindOrAdd(packagePath);
	cacheEntry.AssetPath = packagePath;
	cacheEntry.SourceDisplayName = displayName;
	cacheEntry.ReferenceType = ESGReferenceType::Level;
	cacheEntry.ContentType = GetContentTypeForPath(packagePath);
	cacheEntry.FoundReferences.Empty();

	// Get file timestamp
	FString packageFilename;
	if (FPackageName::DoesPackageExist(packagePath, &packageFilename))
	{
		cacheEntry.LastModifiedTime = IFileManager::Get().GetTimeStamp(*packageFilename);
	}
	else
	{
		cacheEntry.LastModifiedTime = FDateTime::UtcNow();
	}

	// Collect all references into a local array
	TArray<FSGDynamicTextAssetReferenceEntry> localEntries;
	if (ULevel* persistentLevel = world->PersistentLevel)
	{
		for (AActor* actor : persistentLevel->Actors)
		{
			if (!actor)
			{
				continue;
			}

			// Build context: "LevelName > ActorLabel"
			FString actorDisplayName = FString::Printf(TEXT("%s > %s"), *displayName, *actor->GetActorNameOrLabel());

			// Scan the actor itself
			ExtractRefsFromObject(actor->GetClass(), actor, sourcePath, actorDisplayName, ESGReferenceType::Level, localEntries);

			// Scan all components on the actor
			TArray<UActorComponent*> components;
			actor->GetComponents(components);
			for (UActorComponent* component : components)
			{
				if (component)
				{
					// Build context: "LevelName > ActorLabel > ComponentName"
					FString componentDisplayName = FString::Printf(TEXT("%s > %s"), *actorDisplayName, *component->GetName());
					ExtractRefsFromObject(component->GetClass(), component, sourcePath, componentDisplayName, ESGReferenceType::Level, localEntries);
				}
			}
		}
	}

	// Encode all collected entries directly into the persistent cache entry
	for (const FSGDynamicTextAssetReferenceEntry& entry : localEntries)
	{
		cacheEntry.FoundReferences.FindOrAdd(entry.ReferencedId).AddUnique(entry.SourceDisplayName + TEXT("|") + entry.PropertyPath);
	}
}

void USGDynamicTextAssetReferenceSubsystem::ProcessDynamicTextAssetFile(const FString& FilePath)
{
	// Only scan dynamic text asset files
	if (FSGDynamicTextAssetFileManager::GetSupportedExtensionForFile(FilePath).IsEmpty())
	{
		return;
	}

	FString jsonString;
	if (!FSGDynamicTextAssetFileManager::ReadRawFileContents(FilePath, jsonString))
	{
		return;
	}

	// Extract metadata for display
	FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(FilePath);
	if (!metadata.bIsValid)
	{
		return;
	}

	// Use FindFirstObject which handles class lookup by name correctly
	UClass* dataObjectClass = FindFirstObject<UClass>(*metadata.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);

	// Verify it's a valid dynamic text asset class
	if (!dataObjectClass || !dataObjectClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
	{
		return;
	}

	// Skip abstract classes — they cannot be instantiated
	if (dataObjectClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return;
	}

	// Create transient object and deserialize
	UObject* tempObject = NewObject<UObject>(GetTransientPackage(), dataObjectClass);
	if (!tempObject)
	{
		return;
	}

	ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(tempObject);
	if (!provider)
	{
		return;
	}

	TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(FilePath);
	if (!serializer.IsValid())
	{
		return;
	}

	bool bMigrated = false;
	if (!serializer->DeserializeProvider(jsonString, provider, bMigrated))
	{
		return;
	}

	// This dynamic text asset is the "source" - find what IDs it references
	const FSoftObjectPath sourcePath(FilePath);
	const FString displayName = metadata.UserFacingId.IsEmpty()
		? FSGDynamicTextAssetFileManager::ExtractUserFacingIdFromPath(FilePath)
		: metadata.UserFacingId;

	// Create/update cache entry
	FSGReferenceCacheEntry& cacheEntry = PersistentAssetCache.FindOrAdd(FilePath);
	cacheEntry.AssetPath = FilePath;
	cacheEntry.SourceDisplayName = displayName;
	cacheEntry.ReferenceType = ESGReferenceType::DynamicTextAsset;
	cacheEntry.ContentType = ESGReferenceCacheContentType::DynamicTextAssets;
	cacheEntry.FoundReferences.Empty();
	cacheEntry.LastModifiedTime = IFileManager::Get().GetTimeStamp(*FilePath);

	// Collect all references into a local array
	TArray<FSGDynamicTextAssetReferenceEntry> localEntries;
	ExtractRefsFromObject(dataObjectClass, tempObject, sourcePath, displayName, ESGReferenceType::DynamicTextAsset, localEntries);

	// Encode all collected entries directly into the persistent cache entry
	for (const FSGDynamicTextAssetReferenceEntry& entry : localEntries)
	{
		cacheEntry.FoundReferences.FindOrAdd(entry.ReferencedId).AddUnique(entry.SourceDisplayName + TEXT("|") + entry.PropertyPath);
	}
}

void USGDynamicTextAssetReferenceSubsystem::ScanBlueprintAssets()
{
	IAssetRegistry* assetRegistry = IAssetRegistry::Get();
	if (!assetRegistry)
	{
		return;
	}

	TArray<FAssetData> blueprintAssets;
	const FTopLevelAssetPath blueprintClassPath = UBlueprint::StaticClass()->GetClassPathName();
	assetRegistry->GetAssetsByClass(blueprintClassPath, blueprintAssets, true);

	for (const FAssetData& assetData : blueprintAssets)
	{
		// Load the Blueprint to access its generated class
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
		// Get the CDO to read property values
		UObject* cdo = generatedClass->GetDefaultObject(false);
		if (!cdo)
		{
			continue;
		}

		const FSoftObjectPath sourcePath = assetData.GetSoftObjectPath();
		const FString displayName = assetData.AssetName.ToString();

		// Collect all references for this Blueprint into a local array
		TArray<FSGDynamicTextAssetReferenceEntry> localEntries;
		ExtractRefsFromObject(generatedClass, cdo, sourcePath, displayName, ESGReferenceType::Blueprint, localEntries);

		// Scan default subobjects (components) for Actor Blueprints only.
		// Non-Actor Blueprints skip the component scan but still contribute their CDO entries above.
		AActor* cdoActor = Cast<AActor>(cdo);
		if (cdoActor)
		{
			AActor::ForEachComponentOfActorClassDefault(cdoActor->GetClass(), UActorComponent::StaticClass(),
				[this, &displayName, &sourcePath, &localEntries](const UActorComponent* component)
				{
					FString strippedComponentName = component->GetName();
					// Removing the suffix `_GEN_VARIABLE` which signals that it was added in the editor and not in C++
					if (strippedComponentName.EndsWith(USimpleConstructionScript::ComponentTemplateNameSuffix))
					{
						strippedComponentName = strippedComponentName.LeftChop(USimpleConstructionScript::ComponentTemplateNameSuffix.Len());
					}

					// Build context: "BP_Name > ComponentName"
					FString componentDisplayName = FString::Printf(TEXT("%s > %s"), *displayName, *strippedComponentName);
					ExtractRefsFromObject(component->GetClass(), component, sourcePath, componentDisplayName, ESGReferenceType::Blueprint, localEntries);
					return true;
				});
		}

		// Write all collected entries into the reference cache
		for (FSGDynamicTextAssetReferenceEntry& entry : localEntries)
		{
			ReferencerCache.FindOrAdd(entry.ReferencedId).Add(MoveTemp(entry));
		}
	}
}

void USGDynamicTextAssetReferenceSubsystem::ScanDynamicTextAssetFiles()
{
	TArray<FString> allFiles;
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(allFiles);

	for (const FString& filePath : allFiles)
	{
		// Only scan dynamic text asset files
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
		FSGDynamicTextAssetFileMetadata metadata = FSGDynamicTextAssetFileManager::ExtractMetadataFromFile(filePath);
		if (!metadata.bIsValid)
		{
			continue;
		}

		// Use FindFirstObject which handles class lookup by name correctly
		UClass* dataObjectClass = FindFirstObject<UClass>(*metadata.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);
		
		// Verify it's a valid dynamic text asset class
		if (!dataObjectClass || !dataObjectClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
		{
			continue;
		}

		// Skip abstract classes — they cannot be instantiated
		if (dataObjectClass->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		// Create transient object and deserialize
		UObject* tempObject = NewObject<UObject>(GetTransientPackage(), dataObjectClass);
		if (!tempObject)
		{
			continue;
		}

		ISGDynamicTextAssetProvider* provider = Cast<ISGDynamicTextAssetProvider>(tempObject);
		if (!provider)
		{
			continue;
		}

		TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFile(filePath);
		if (!serializer.IsValid())
		{
			continue;
		}

		bool bMigrated = false;
		if (!serializer->DeserializeProvider(jsonString, provider, bMigrated))
		{
			continue;
		}

		// This dynamic text asset is the "source" - find what IDs it references
		const FSoftObjectPath sourcePath(filePath);
		const FString displayName = metadata.UserFacingId.IsEmpty()
			? FSGDynamicTextAssetFileManager::ExtractUserFacingIdFromPath(filePath)
			: metadata.UserFacingId;

		// Collect all references for this dynamic text asset file into a local array
		TArray<FSGDynamicTextAssetReferenceEntry> localEntries;
		ExtractRefsFromObject(dataObjectClass, tempObject, sourcePath, displayName, ESGReferenceType::DynamicTextAsset, localEntries);

		// Write all collected entries into the reference cache
		for (FSGDynamicTextAssetReferenceEntry& entry : localEntries)
		{
			ReferencerCache.FindOrAdd(entry.ReferencedId).Add(MoveTemp(entry));
		}
	}
}

void USGDynamicTextAssetReferenceSubsystem::ScanLevelAssets()
{
	IAssetRegistry* assetRegistry = IAssetRegistry::Get();
	if (!assetRegistry)
	{
		return;
	}

	// Get all World assets (levels)
	TArray<FAssetData> worldAssets;
	const FTopLevelAssetPath worldClassPath = UWorld::StaticClass()->GetClassPathName();
	assetRegistry->GetAssetsByClass(worldClassPath, worldAssets, true);

	for (const FAssetData& assetData : worldAssets)
	{
		// Load the World to access its actors
		UWorld* world = Cast<UWorld>(assetData.GetAsset());
		if (!world)
		{
			continue;
		}

		const FSoftObjectPath sourcePath = assetData.GetSoftObjectPath();
		const FString displayName = assetData.AssetName.ToString();

		// Collect all references for this level into a local array
		TArray<FSGDynamicTextAssetReferenceEntry> localEntries;
		if (ULevel* persistentLevel = world->PersistentLevel)
		{
			for (AActor* actor : persistentLevel->Actors)
			{
				if (!actor)
				{
					continue;
				}

				// Build context: "LevelName > ActorLabel"
				FString actorDisplayName = FString::Printf(TEXT("%s > %s"), *displayName, *actor->GetActorNameOrLabel());

				// Scan the actor itself
				ExtractRefsFromObject(actor->GetClass(), actor, sourcePath, actorDisplayName, ESGReferenceType::Level, localEntries);

				// Scan all components on the actor
				TArray<UActorComponent*> components;
				actor->GetComponents(components);
				for (UActorComponent* component : components)
				{
					if (component)
					{
						// Build context: "LevelName > ActorLabel > ComponentName"
						FString componentDisplayName = FString::Printf(TEXT("%s > %s"), *actorDisplayName, *component->GetName());
						ExtractRefsFromObject(component->GetClass(), component, sourcePath, componentDisplayName, ESGReferenceType::Level, localEntries);
					}
				}
			}
		}

		// Write all collected entries into the reference cache
		for (FSGDynamicTextAssetReferenceEntry& entry : localEntries)
		{
			ReferencerCache.FindOrAdd(entry.ReferencedId).Add(MoveTemp(entry));
		}
	}
}

void USGDynamicTextAssetReferenceSubsystem::ExtractRefsFromObject(
	const UClass* ObjectClass,
	const UObject* ObjectInstance,
	const FSoftObjectPath& SourceAsset,
	const FString& SourceDisplayName,
	ESGReferenceType ReferenceType,
	TArray<FSGDynamicTextAssetReferenceEntry>& OutEntries)
{
	if (!ObjectClass || !ObjectInstance)
	{
		return;
	}

	for (TFieldIterator<FProperty> propIt(ObjectClass); propIt; ++propIt)
	{
		const FProperty* property = *propIt;
		ExtractRefsFromProperty(property, ObjectInstance, property->GetDisplayNameText().ToString(), SourceAsset, SourceDisplayName, ReferenceType, OutEntries);
	}
}

void USGDynamicTextAssetReferenceSubsystem::ExtractRefsFromProperty(
	const FProperty* Property,
	const void* ContainerPtr,
	const FString& PropertyPath,
	const FSoftObjectPath& SourceAsset,
	const FString& SourceDisplayName,
	ESGReferenceType ReferenceType,
	TArray<FSGDynamicTextAssetReferenceEntry>& OutEntries)
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
			if (ref && ref->IsValid())
			{
				FSGDynamicTextAssetReferenceEntry entry(
					ref->GetId(),
					SourceAsset,
					PropertyPath,
					ReferenceType);
				entry.SourceDisplayName = SourceDisplayName;
				if (ReferenceType == ESGReferenceType::DynamicTextAsset)
				{
					entry.SourceFilePath = SourceAsset.ToString();
				}

				OutEntries.Add(MoveTemp(entry));
			}
			return;
		}

		// Recurse into non-target struct properties to find nested refs
		const void* structPtr = structProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		for (TFieldIterator<FProperty> innerIt(structProp->Struct); innerIt; ++innerIt)
		{
			const FProperty* innerProp = *innerIt;
			FString nestedPath = FString::Printf(TEXT("%s.%s"), *PropertyPath, *innerProp->GetDisplayNameText().ToString());
			ExtractRefsFromProperty(innerProp, structPtr, nestedPath, SourceAsset, SourceDisplayName, ReferenceType, OutEntries);
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
						FSGDynamicTextAssetReferenceEntry entry(
							ref->GetId(),
							SourceAsset,
							elementPath,
							ReferenceType);
						entry.SourceDisplayName = SourceDisplayName;
						if (ReferenceType == ESGReferenceType::DynamicTextAsset)
						{
							entry.SourceFilePath = SourceAsset.ToString();
						}

						OutEntries.Add(MoveTemp(entry));
					}
				}
				else
				{
					// Recurse into struct elements
					for (TFieldIterator<FProperty> innerIt(innerStruct->Struct); innerIt; ++innerIt)
					{
						const FProperty* innerProp = *innerIt;
						FString nestedPath = FString::Printf(TEXT("%s.%s"), *elementPath, *innerProp->GetDisplayNameText().ToString());
						ExtractRefsFromProperty(innerProp, elementPtr, nestedPath, SourceAsset, SourceDisplayName, ReferenceType, OutEntries);
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
			// Check map values (keys are unlikely to be FSGDynamicTextAssetRef but check both)
			const void* valuePtr = mapHelper.GetValuePtr(it.GetInternalIndex());
			FString valuePath = FString::Printf(TEXT("%s[Value]"), *PropertyPath);

			if (const FStructProperty* valueStruct = CastField<FStructProperty>(mapProp->ValueProp))
			{
				if (valueStruct->Struct == FSGDynamicTextAssetRef::StaticStruct())
				{
					const FSGDynamicTextAssetRef* ref = static_cast<const FSGDynamicTextAssetRef*>(valuePtr);
					if (ref && ref->IsValid())
					{
						FSGDynamicTextAssetReferenceEntry entry(
							ref->GetId(),
							SourceAsset,
							valuePath,
							ReferenceType);
						entry.SourceDisplayName = SourceDisplayName;
						if (ReferenceType == ESGReferenceType::DynamicTextAsset)
						{
							entry.SourceFilePath = SourceAsset.ToString();
						}

						OutEntries.Add(MoveTemp(entry));
					}
				}
				else
				{
					for (TFieldIterator<FProperty> innerIt(valueStruct->Struct); innerIt; ++innerIt)
					{
						const FProperty* innerProp = *innerIt;
						FString nestedPath = FString::Printf(TEXT("%s.%s"), *valuePath, *innerProp->GetDisplayNameText().ToString());
						ExtractRefsFromProperty(innerProp, valuePtr, nestedPath, SourceAsset, SourceDisplayName, ReferenceType, OutEntries);
					}
				}
			}
		}
	}
}

void USGDynamicTextAssetReferenceSubsystem::ExtractDependenciesFromObject(
	const UObject* DataObject,
	TArray<FSGDynamicTextAssetDependencyEntry>& OutDependencies) const
{
	if (!DataObject)
	{
		return;
	}

	const UClass* objectClass = DataObject->GetClass();

	for (TFieldIterator<FProperty> propIt(objectClass); propIt; ++propIt)
	{
		const FProperty* property = *propIt;
		ExtractDepsFromProperty(property, DataObject, property->GetDisplayNameText().ToString(), OutDependencies);
	}
}

void USGDynamicTextAssetReferenceSubsystem::ExtractDepsFromProperty(
	const FProperty* Property,
	const void* ContainerPtr,
	const FString& PropertyPath,
	TArray<FSGDynamicTextAssetDependencyEntry>& OutDependencies) const
{
	if (!Property || !ContainerPtr)
	{
		return;
	}

	// Check soft object property
	if (const FSoftObjectProperty* softObjProp = CastField<FSoftObjectProperty>(Property))
	{
		const void* valuePtr = softObjProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		const FSoftObjectPtr* softPtr = static_cast<const FSoftObjectPtr*>(valuePtr);
		if (softPtr && !softPtr->IsNull())
		{
			const ESGReferenceType refType = (softObjProp->PropertyClass && softObjProp->PropertyClass->IsChildOf(UWorld::StaticClass()))
				? ESGReferenceType::Level : ESGReferenceType::Blueprint;
			OutDependencies.Emplace(softPtr->ToSoftObjectPath(), PropertyPath,
				softObjProp->PropertyClass ? softObjProp->PropertyClass->GetFName() : NAME_None, refType);
		}
		return;
	}

	// Check soft class property
	if (const FSoftClassProperty* softClassProp = CastField<FSoftClassProperty>(Property))
	{
		const void* valuePtr = softClassProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		const FSoftObjectPtr* softPtr = static_cast<const FSoftObjectPtr*>(valuePtr);
		if (softPtr && !softPtr->IsNull())
		{
			const ESGReferenceType refType = (softClassProp->MetaClass && softClassProp->MetaClass->IsChildOf(UWorld::StaticClass()))
				? ESGReferenceType::Level : ESGReferenceType::Blueprint;
			OutDependencies.Emplace(softPtr->ToSoftObjectPath(), PropertyPath,
				softClassProp->MetaClass ? softClassProp->MetaClass->GetFName() : NAME_None, refType);
		}
		return;
	}

	// Check direct struct property
	if (const FStructProperty* structProp = CastField<FStructProperty>(Property))
	{
		if (structProp->Struct == FSGDynamicTextAssetRef::StaticStruct())
		{
			const void* valuePtr = structProp->ContainerPtrToValuePtr<void>(ContainerPtr);
			const FSGDynamicTextAssetRef* ref = static_cast<const FSGDynamicTextAssetRef*>(valuePtr);
			if (ref && ref->IsValid())
			{
				OutDependencies.Emplace(ref->GetId(), PropertyPath, ref->GetUserFacingId());
			}
			return;
		}

		// Recurse into nested structs
		const void* structPtr = structProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		for (TFieldIterator<FProperty> innerIt(structProp->Struct); innerIt; ++innerIt)
		{
			const FProperty* innerProp = *innerIt;
			FString nestedPath = FString::Printf(TEXT("%s.%s"), *PropertyPath, *innerProp->GetDisplayNameText().ToString());
			ExtractDepsFromProperty(innerProp, structPtr, nestedPath, OutDependencies);
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
						OutDependencies.Emplace(ref->GetId(), elementPath, ref->GetUserFacingId());
					}
				}
				else
				{
					for (TFieldIterator<FProperty> innerIt(innerStruct->Struct); innerIt; ++innerIt)
					{
						const FProperty* innerProp = *innerIt;
						FString nestedPath = FString::Printf(TEXT("%s.%s"), *elementPath, *innerProp->GetDisplayNameText().ToString());
						ExtractDepsFromProperty(innerProp, elementPtr, nestedPath, OutDependencies);
					}
				}
			}
			else if (const FSoftObjectProperty* innerSoftObj = CastField<FSoftObjectProperty>(arrayProp->Inner))
			{
				const FSoftObjectPtr* softPtr = static_cast<const FSoftObjectPtr*>(elementPtr);
				if (softPtr && !softPtr->IsNull())
				{
					const ESGReferenceType refType = (innerSoftObj->PropertyClass && innerSoftObj->PropertyClass->IsChildOf(UWorld::StaticClass()))
						? ESGReferenceType::Level : ESGReferenceType::Blueprint;
					OutDependencies.Emplace(softPtr->ToSoftObjectPath(), elementPath,
						innerSoftObj->PropertyClass ? innerSoftObj->PropertyClass->GetFName() : NAME_None, refType);
				}
			}
			else if (const FSoftClassProperty* innerSoftClass = CastField<FSoftClassProperty>(arrayProp->Inner))
			{
				const FSoftObjectPtr* softPtr = static_cast<const FSoftObjectPtr*>(elementPtr);
				if (softPtr && !softPtr->IsNull())
				{
					const ESGReferenceType refType = (innerSoftClass->MetaClass && innerSoftClass->MetaClass->IsChildOf(UWorld::StaticClass()))
						? ESGReferenceType::Level : ESGReferenceType::Blueprint;
					OutDependencies.Emplace(softPtr->ToSoftObjectPath(), elementPath,
						innerSoftClass->MetaClass ? innerSoftClass->MetaClass->GetFName() : NAME_None, refType);
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
						OutDependencies.Emplace(ref->GetId(), valuePath, ref->GetUserFacingId());
					}
				}
				else
				{
					for (TFieldIterator<FProperty> innerIt(valueStruct->Struct); innerIt; ++innerIt)
					{
						const FProperty* innerProp = *innerIt;
						FString nestedPath = FString::Printf(TEXT("%s.%s"), *valuePath, *innerProp->GetDisplayNameText().ToString());
						ExtractDepsFromProperty(innerProp, valuePtr, nestedPath, OutDependencies);
					}
				}
			}
			else if (const FSoftObjectProperty* innerSoftObj = CastField<FSoftObjectProperty>(mapProp->ValueProp))
			{
				const FSoftObjectPtr* softPtr = static_cast<const FSoftObjectPtr*>(valuePtr);
				if (softPtr && !softPtr->IsNull())
				{
					const ESGReferenceType refType = (innerSoftObj->PropertyClass && innerSoftObj->PropertyClass->IsChildOf(UWorld::StaticClass()))
						? ESGReferenceType::Level : ESGReferenceType::Blueprint;
					OutDependencies.Emplace(softPtr->ToSoftObjectPath(), valuePath,
						innerSoftObj->PropertyClass ? innerSoftObj->PropertyClass->GetFName() : NAME_None, refType);
				}
			}
			else if (const FSoftClassProperty* innerSoftClass = CastField<FSoftClassProperty>(mapProp->ValueProp))
			{
				const FSoftObjectPtr* softPtr = static_cast<const FSoftObjectPtr*>(valuePtr);
				if (softPtr && !softPtr->IsNull())
				{
					const ESGReferenceType refType = (innerSoftClass->MetaClass && innerSoftClass->MetaClass->IsChildOf(UWorld::StaticClass()))
						? ESGReferenceType::Level : ESGReferenceType::Blueprint;
					OutDependencies.Emplace(softPtr->ToSoftObjectPath(), valuePath,
						innerSoftClass->MetaClass ? innerSoftClass->MetaClass->GetFName() : NAME_None, refType);
				}
			}
		}
	}
}

void USGDynamicTextAssetReferenceSubsystem::UpdateScanNotification(const FText& StatusText, float Progress)
{
	if (!ScanNotificationItem.IsValid())
	{
		// Create new notification
		FNotificationInfo info(INVTEXT("Scanning Dynamic Text Asset References..."));
		info.bFireAndForget = false;
		info.bUseThrobber = true;
		info.bUseSuccessFailIcons = false;
		info.ExpireDuration = 0.0f;
		info.FadeOutDuration = 0.5f;

		ScanNotificationItem = FSlateNotificationManager::Get().AddNotification(info);
		if (ScanNotificationItem.IsValid())
		{
			ScanNotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
		}
	}

	if (ScanNotificationItem.IsValid())
	{
		// Update the notification text with progress
		int32 percentage = FMath::RoundToInt(Progress * 100.0f);
		FText progressText = FText::Format(
			INVTEXT("{0} ({1}%)"),
			StatusText,
			FText::AsNumber(percentage)
		);
		ScanNotificationItem->SetText(progressText);
	}
}

void USGDynamicTextAssetReferenceSubsystem::CloseScanNotification(bool bSuccess)
{
	if (ScanNotificationItem.IsValid())
	{
		if (bSuccess)
		{
			ScanNotificationItem->SetText(INVTEXT("Dynamic Text Asset reference scan complete"));
			ScanNotificationItem->SetCompletionState(SNotificationItem::CS_Success);
		}
		else
		{
			ScanNotificationItem->SetText(INVTEXT("Dynamic Text Asset reference scan failed"));
			ScanNotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
		}

		ScanNotificationItem->ExpireAndFadeout();
		ScanNotificationItem.Reset();
	}
}

void USGDynamicTextAssetReferenceSubsystem::ClearCacheAndRescan()
{
	// Clear all caches
	PersistentAssetCache.Empty();
	ReferencerCache.Empty();
	bCachePopulated = false;

	// Delete cache files from disk
	IFileManager& fileManager = IFileManager::Get();
	FString cacheFolderPath = GetCacheFolderPath();
	if (fileManager.DirectoryExists(*cacheFolderPath))
	{
		fileManager.DeleteDirectory(*cacheFolderPath, false, true);
		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Deleted reference cache folder: %s"), *cacheFolderPath);
	}

	// Force full rescan
	bForceFullRescan = true;
	RebuildReferenceCacheAsync();
}

void USGDynamicTextAssetReferenceSubsystem::SaveCacheToDisk()
{
	// Organize cache entries by content type
	TMap<ESGReferenceCacheContentType, TArray<const FSGReferenceCacheEntry*>> entriesByType;
	for (const TPair<FString, FSGReferenceCacheEntry>& pair : PersistentAssetCache)
	{
		entriesByType.FindOrAdd(pair.Value.ContentType).Add(&pair.Value);
	}

	// Ensure cache folder exists
	FString cacheFolderPath = GetCacheFolderPath();
	IFileManager::Get().MakeDirectory(*cacheFolderPath, true);

	// Save each content type to its own file
	for (const TPair<ESGReferenceCacheContentType, TArray<const FSGReferenceCacheEntry*>>& typeEntries : entriesByType)
	{
		TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
		rootObject->SetNumberField(TEXT("version"), 1);
		rootObject->SetStringField(TEXT("savedAt"), FDateTime::UtcNow().ToIso8601());

		TArray<TSharedPtr<FJsonValue>> entriesArray;
		for (const FSGReferenceCacheEntry* entry : typeEntries.Value)
		{
			TSharedRef<FJsonObject> entryObject = MakeShared<FJsonObject>();
			entryObject->SetStringField(TEXT("assetPath"), entry->AssetPath);
			entryObject->SetStringField(TEXT("lastModified"), entry->LastModifiedTime.ToIso8601());
			entryObject->SetStringField(TEXT("displayName"), entry->SourceDisplayName);
			entryObject->SetNumberField(TEXT("referenceType"), static_cast<int32>(entry->ReferenceType));

			// Serialize found references
			TSharedRef<FJsonObject> refsObject = MakeShared<FJsonObject>();
			for (const TPair<FSGDynamicTextAssetId, TArray<FString>>& refPair : entry->FoundReferences)
			{
				TArray<TSharedPtr<FJsonValue>> pathsArray;
				for (const FString& path : refPair.Value)
				{
					pathsArray.Add(MakeShared<FJsonValueString>(path));
				}
				refsObject->SetArrayField(refPair.Key.ToString(), pathsArray);
			}
			entryObject->SetObjectField(TEXT("references"), refsObject);

			entriesArray.Add(MakeShared<FJsonValueObject>(entryObject));
		}
		rootObject->SetArrayField(TEXT("entries"), entriesArray);

		// Write to file
		FString jsonString;
		TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&jsonString);
		FJsonSerializer::Serialize(rootObject, writer);

		FString filePath = GetCacheFilePath(typeEntries.Key);
		if (FFileHelper::SaveStringToFile(jsonString, *filePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Saved reference cache: %s (%d entries)"), *filePath, typeEntries.Value.Num());
		}
	}
}

void USGDynamicTextAssetReferenceSubsystem::LoadCacheFromDisk()
{
	PersistentAssetCache.Empty();

	// Load each content type file
	TArray<ESGReferenceCacheContentType> contentTypes = {
		ESGReferenceCacheContentType::GameContent,
		ESGReferenceCacheContentType::PluginContent,
		ESGReferenceCacheContentType::EngineContent,
		ESGReferenceCacheContentType::DynamicTextAssets
	};

	for (ESGReferenceCacheContentType contentType : contentTypes)
	{
		FString filePath = GetCacheFilePath(contentType);
		FString jsonString;

		if (!FFileHelper::LoadFileToString(jsonString, *filePath))
		{
			continue;
		}

		TSharedPtr<FJsonObject> rootObject;
		TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonString);
		if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("Failed to parse cache file: %s"), *filePath);
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* entriesArray;
		if (!rootObject->TryGetArrayField(TEXT("entries"), entriesArray))
		{
			continue;
		}

		int32 loadedCount = 0;
		for (const TSharedPtr<FJsonValue>& entryValue : *entriesArray)
		{
			const TSharedPtr<FJsonObject>* entryObject;
			if (!entryValue->TryGetObject(entryObject))
			{
				continue;
			}

			FSGReferenceCacheEntry entry;
			entry.ContentType = contentType;

			if (!(*entryObject)->TryGetStringField(TEXT("assetPath"), entry.AssetPath))
			{
				continue;
			}

			FString lastModifiedStr;
			if ((*entryObject)->TryGetStringField(TEXT("lastModified"), lastModifiedStr))
			{
				FDateTime::ParseIso8601(*lastModifiedStr, entry.LastModifiedTime);
			}

			(*entryObject)->TryGetStringField(TEXT("displayName"), entry.SourceDisplayName);

			int32 refTypeInt;
			if ((*entryObject)->TryGetNumberField(TEXT("referenceType"), refTypeInt))
			{
				entry.ReferenceType = static_cast<ESGReferenceType>(refTypeInt);
			}

			// Load references
			const TSharedPtr<FJsonObject>* refsObject;
			if ((*entryObject)->TryGetObjectField(TEXT("references"), refsObject))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& refPair : (*refsObject)->Values)
				{
					FSGDynamicTextAssetId id;
					if (!id.ParseString(refPair.Key))
					{
						continue;
					}

					const TArray<TSharedPtr<FJsonValue>>* pathsArray;
					if (refPair.Value->TryGetArray(pathsArray))
					{
						TArray<FString>& paths = entry.FoundReferences.FindOrAdd(id);
						for (const TSharedPtr<FJsonValue>& pathValue : *pathsArray)
						{
							FString path;
							if (pathValue->TryGetString(path))
							{
								paths.Add(path);
							}
						}
					}
				}
			}

			PersistentAssetCache.Add(entry.AssetPath, MoveTemp(entry));
			loadedCount++;
		}

		UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Loaded reference cache: %s (%d entries)"), *filePath, loadedCount);
	}

	// Rebuild the referencer cache from loaded data
	if (PersistentAssetCache.Num() > 0)
	{
		RebuildReferencerCacheFromPersistent();
		bCachePopulated = true;
	}
}

ESGReferenceCacheContentType USGDynamicTextAssetReferenceSubsystem::GetContentTypeForPath(const FString& AssetPath) const
{
	// Check if it's a dynamic text asset file (file system path)
	if (AssetPath.Contains(FSGDynamicTextAssetFileManager::GetDynamicTextAssetsRelativeRootPath())
		|| !FSGDynamicTextAssetFileManager::GetSupportedExtensionForFile(AssetPath).IsEmpty())
	{
		return ESGReferenceCacheContentType::DynamicTextAssets;
	}

	// For package paths (start with /), use mount point logic
	if (AssetPath.StartsWith(TEXT("/")))
	{
		// Game content always starts with /Game/
		if (AssetPath.StartsWith(TEXT("/Game/")) || AssetPath.StartsWith(TEXT("/Game")))
		{
			return ESGReferenceCacheContentType::GameContent;
		}

		// Check if this is from a project plugin by looking at known project plugin mount points
		// Project plugins are mounted at /<PluginName>/ and their content folders are in the project's Plugins directory
		// We need to check if the mount point corresponds to a project plugin vs an engine plugin
		
		// Extract the mount point (first path component after the leading slash)
		FString mountPoint;
		FString remainder;
		if (AssetPath.Mid(1).Split(TEXT("/"), &mountPoint, &remainder))
		{
			// Check if this mount point corresponds to a project plugin
			// Project plugins have their content in: <ProjectDir>/Plugins/<PluginName>/Content/
			FString projectPluginContentPath = FPaths::ProjectPluginsDir() / mountPoint / TEXT("Content");
			if (FPaths::DirectoryExists(projectPluginContentPath))
			{
				return ESGReferenceCacheContentType::PluginContent;
			}
		}

		// Everything else (Engine content, Engine plugins like ControlRig, Niagara, etc.)
		// These are mounted at root level but come from Engine or Engine plugins
		return ESGReferenceCacheContentType::EngineContent;
	}

	// File system paths - check if they're in the project's Plugins folder
	if (AssetPath.Contains(TEXT("/Plugins/")) || AssetPath.Contains(TEXT("\\Plugins\\")))
	{
		// Check if it's a project plugin (path contains project directory)
		FString projectDir = FPaths::ProjectDir();
		if (AssetPath.Contains(projectDir))
		{
			return ESGReferenceCacheContentType::PluginContent;
		}
		return ESGReferenceCacheContentType::EngineContent;
	}

	// Check for Engine paths in file system
	if (AssetPath.Contains(TEXT("/Engine/")) || AssetPath.Contains(TEXT("\\Engine\\")))
	{
		return ESGReferenceCacheContentType::EngineContent;
	}

	// Default to game content for anything else
	return ESGReferenceCacheContentType::GameContent;
}

FString USGDynamicTextAssetReferenceSubsystem::GetCacheFolderPath() const
{
	const USGDynamicTextAssetEditorSettings* settings = USGDynamicTextAssetEditorSettings::Get();
	FString relativePath = settings ? settings->ReferenceCacheFolderPath : TEXT("SGDynamicTextAssets/ReferenceCache");
	return FPaths::ProjectSavedDir() / relativePath;
}

FString USGDynamicTextAssetReferenceSubsystem::GetCacheFilePath(ESGReferenceCacheContentType ContentType) const
{
	FString fileName;
	switch (ContentType)
	{
		case ESGReferenceCacheContentType::GameContent:
		{
			fileName = TEXT("GameContent.json");
			break;
		}
		case ESGReferenceCacheContentType::PluginContent:
		{
			fileName = TEXT("PluginContent.json");
			break;
		}
		case ESGReferenceCacheContentType::EngineContent:
		{
			fileName = TEXT("EngineContent.json");
			break;
		}
		case ESGReferenceCacheContentType::DynamicTextAssets:
		{
			fileName = TEXT("DynamicTextAssets.json");
			break;
		}
		// default not needed - all enum values covered
	}
	return GetCacheFolderPath() / fileName;
}

bool USGDynamicTextAssetReferenceSubsystem::DoesAssetNeedRescan(const FString& AssetPath, const FDateTime& CurrentTimestamp) const
{
	// Always rescan if forced
	if (bForceFullRescan)
	{
		return true;
	}

	// Check if we have a cached entry
	const FSGReferenceCacheEntry* cachedEntry = PersistentAssetCache.Find(AssetPath);
	if (!cachedEntry)
	{
		return true; // New asset, needs scan
	}

	// Compare timestamps
	return CurrentTimestamp > cachedEntry->LastModifiedTime;
}

void USGDynamicTextAssetReferenceSubsystem::RebuildReferencerCacheFromPersistent()
{
	ReferencerCache.Empty();

	for (const TPair<FString, FSGReferenceCacheEntry>& pair : PersistentAssetCache)
	{
		const FSGReferenceCacheEntry& entry = pair.Value;

		for (const TPair<FSGDynamicTextAssetId, TArray<FString>>& refPair : entry.FoundReferences)
		{
			TArray<FSGDynamicTextAssetReferenceEntry>& referencers = ReferencerCache.FindOrAdd(refPair.Key);

			for (const FString& encodedValue : refPair.Value)
			{
				FSGDynamicTextAssetReferenceEntry refEntry;
				refEntry.ReferencedId = refPair.Key;
				// PersistentAssetCache keys for Blueprint/Level assets are bare package paths
				// (e.g. /Game/BP_Foo from AssetData.PackageName). GetAssetByObjectPath requires
				// the full object path (e.g. /Game/BP_Foo.BP_Foo). Reconstruct it by appending
				// the asset name when only a package path is stored.
				FSoftObjectPath reconstructedPath(entry.AssetPath);
				if (reconstructedPath.GetAssetName().IsEmpty() && !entry.SourceDisplayName.IsEmpty())
				{
					reconstructedPath = FSoftObjectPath(entry.AssetPath + TEXT(".") + entry.SourceDisplayName);
				}
				refEntry.SourceAsset = reconstructedPath;
				refEntry.ReferenceType = entry.ReferenceType;
				if (entry.ReferenceType == ESGReferenceType::DynamicTextAsset)
				{
					refEntry.SourceFilePath = entry.AssetPath;
				}

				// Cached strings are encoded as "DisplayName|PropertyPath"
				// Split on the last '|' to extract both parts
				int32 separatorIndex = INDEX_NONE;
				if (encodedValue.FindLastChar(TEXT('|'), separatorIndex))
				{
					refEntry.SourceDisplayName = encodedValue.Left(separatorIndex);
					refEntry.PropertyPath = encodedValue.Mid(separatorIndex + 1);
				}
				else
				{
					// Legacy format or no separator - treat entire string as property path
					refEntry.SourceDisplayName = entry.SourceDisplayName;
					refEntry.PropertyPath = encodedValue;
				}

				referencers.Add(MoveTemp(refEntry));
			}
		}
	}
}
