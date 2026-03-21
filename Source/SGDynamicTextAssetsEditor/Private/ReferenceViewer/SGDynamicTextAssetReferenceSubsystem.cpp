// Copyright Start Games, Inc. All Rights Reserved.

#include "ReferenceViewer/SGDynamicTextAssetReferenceSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Core/ISGDynamicTextAssetProvider.h"
#include "Core/SGDynamicTextAssetRef.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "Management/SGDynamicTextAssetFileInfo.h"
#include "Serialization/SGDynamicTextAssetJsonSerializer.h"
#include "Components/ActorComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Editor.h"
#include "UObject/Package.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Settings/SGDynamicTextAssetEditorSettings.h"
#include "SGDynamicTextAssetEditorLogs.h"
#include "SGDynamicTextAssetScanSubsystem.h"
#include "Utilities/SGDynamicTextAssetScanPriorities.h"

void USGDynamicTextAssetReferenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Ensure the scan subsystem is initialized before we try to register phases with it
	Collection.InitializeDependency<USGDynamicTextAssetScanSubsystem>();

	bCachePopulated = false;
	bForceFullRescan = false;

	// Load persistent cache from disk
	LoadCacheFromDisk();

	// Register scan phases with the scan subsystem
	RegisterScanPhases();

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
		// If an async scan is already running, don't duplicate work with a blocking scan.
		// Return whatever partial data is available; the scan will update the cache when done.
		if (!bForceRescan && IsScanningInProgress())
		{
			UE_LOG(LogSGDynamicTextAssetsEditor, Verbose,
				TEXT("FindReferencers: async scan in progress, returning partial results for %s"), *Id.ToString());
		}
		else
		{
			RebuildReferenceCache();
		}
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

	// Extract file info to instantiate the correct type
	FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(filePath);
	if (!fileInfo.bIsValid)
	{
		return;
	}

	// Find the UClass using FindFirstObject which handles class lookup by name correctly
	UClass* dataObjectClass = FindFirstObject<UClass>(*fileInfo.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);

	// Verify it's a valid dynamic text asset class
	if (dataObjectClass && !dataObjectClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
	{
		dataObjectClass = nullptr;
	}

	if (!dataObjectClass)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("FindDependencies: Could not find class '%s' for ID %s"), *fileInfo.ClassName, *Id.ToString());
		return;
	}

	// Skip abstract classes  - they cannot be instantiated
	if (dataObjectClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning, TEXT("FindDependencies: Class '%s' is abstract, cannot instantiate for ID %s"), *fileInfo.ClassName, *Id.ToString());
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
		// If an async scan is already running, don't duplicate work with a blocking scan
		if (!IsScanningInProgress())
		{
			RebuildReferenceCache();
		}
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
	FScopedSlowTask slowTask(3.0f, INVTEXT("DTA Reference Cache: Scanning for Dynamic Text Asset references..."));
	slowTask.MakeDialog();

	// Each Scan* method writes to PersistentAssetCache (via Process* functions)
	slowTask.EnterProgressFrame(1.0f, INVTEXT("DTA Reference Cache: Scanning Blueprint assets..."));
	ScanBlueprintAssets();

	slowTask.EnterProgressFrame(1.0f, INVTEXT("DTA Reference Cache: Scanning Level assets..."));
	ScanLevelAssets();

	slowTask.EnterProgressFrame(1.0f, INVTEXT("DTA Reference Cache: Scanning Dynamic Text Asset files..."));
	ScanDynamicTextAssetFiles();

	// Build the in-memory ReferencerCache from PersistentAssetCache and save to disk
	RebuildReferencerCacheFromPersistent();
	SaveCacheToDisk();

	bCachePopulated = true;
	bForceFullRescan = false;

	int32 totalRefs = 0;
	for (const TPair<FSGDynamicTextAssetId, TArray<FSGDynamicTextAssetReferenceEntry>>& pair : ReferencerCache)
	{
		totalRefs += pair.Value.Num();
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Reference cache built: %d dynamic text assets with %d total references, %d cached assets"),
		ReferencerCache.Num(), totalRefs, PersistentAssetCache.Num());
}

void USGDynamicTextAssetReferenceSubsystem::RebuildReferenceCacheAsync()
{
	// Delegate to the scan subsystem which manages the ticker infrastructure
	if (USGDynamicTextAssetScanSubsystem* scanSubsystem = GEditor->GetEditorSubsystem<USGDynamicTextAssetScanSubsystem>())
	{
		scanSubsystem->StartScan(bForceFullRescan);
	}
}

bool USGDynamicTextAssetReferenceSubsystem::IsScanningInProgress() const
{
	if (const USGDynamicTextAssetScanSubsystem* scanSubsystem = GEditor->GetEditorSubsystem<USGDynamicTextAssetScanSubsystem>())
	{
		return scanSubsystem->IsScanningInProgress();
	}
	return false;
}

bool USGDynamicTextAssetReferenceSubsystem::IsCacheReady() const
{
	return bCachePopulated && !IsScanningInProgress();
}

void USGDynamicTextAssetReferenceSubsystem::RegisterScanPhases()
{
	USGDynamicTextAssetScanSubsystem* scanSubsystem = GEditor->GetEditorSubsystem<USGDynamicTextAssetScanSubsystem>();
	if (!scanSubsystem)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Warning,
			TEXT("RegisterScanPhases: ScanSubsystem not available, reference phases not registered"));
		return;
	}

	// Immediately rebuild ReferencerCache from existing PersistentAssetCache
	// so cached results are available before the scan finishes
	RebuildReferencerCacheFromPersistent();

	// Blueprint reference scan phase
	{
		FSGDynamicTextAssetScanPhase phase;
		phase.PhaseId = FName(TEXT("References.Blueprints"));
		phase.DisplayName = INVTEXT("Blueprints");
		phase.Priority = SGDynamicTextAssetScanPriorities::REFERENCE_BLUEPRINTS;
		phase.SetupPhase = [this]() { SetupBlueprintScanPhase(); };
		phase.ProcessOneItem = [this]() { return ProcessOneBlueprintItem(); };
		phase.OnPhaseComplete = [this]() { OnBlueprintPhaseComplete(); };
		phase.GetRemainingCount = [this]() { return PendingBlueprintAssets.Num(); };
		scanSubsystem->RegisterScanPhase(phase);
	}

	// Level reference scan phase
	{
		FSGDynamicTextAssetScanPhase phase;
		phase.PhaseId = FName(TEXT("References.Levels"));
		phase.DisplayName = INVTEXT("Levels");
		phase.Priority = SGDynamicTextAssetScanPriorities::REFERENCE_LEVELS;
		phase.SetupPhase = [this]() { SetupLevelScanPhase(); };
		phase.ProcessOneItem = [this]() { return ProcessOneLevelItem(); };
		phase.OnPhaseComplete = [this]() { OnLevelPhaseComplete(); };
		phase.GetRemainingCount = [this]() { return PendingLevelAssets.Num(); };
		scanSubsystem->RegisterScanPhase(phase);
	}

	// DTA file reference scan phase
	{
		FSGDynamicTextAssetScanPhase phase;
		phase.PhaseId = FName(TEXT("References.DTAFiles"));
		phase.DisplayName = INVTEXT("DTA References");
		phase.Priority = SGDynamicTextAssetScanPriorities::REFERENCE_DTA_FILES;
		phase.SetupPhase = [this]() { SetupDTAReferenceScanPhase(); };
		phase.ProcessOneItem = [this]() { return ProcessOneDTAReferenceItem(); };
		phase.OnPhaseComplete = [this]() { OnDTAReferencePhaseComplete(); };
		phase.GetRemainingCount = [this]() { return PendingDynamicTextAssetFiles.Num(); };
		scanSubsystem->RegisterScanPhase(phase);
	}
}

void USGDynamicTextAssetReferenceSubsystem::SetupBlueprintScanPhase()
{
	PendingBlueprintAssets.Empty();

	const USGDynamicTextAssetEditorSettings* settings = USGDynamicTextAssetEditorSettings::Get();
	const bool bScanEngine = settings ? settings->bScanEngineContent : false;
	const bool bScanPlugins = settings ? settings->bScanPluginContent : true;

	if (IAssetRegistry* assetRegistry = IAssetRegistry::Get())
	{
		TArray<FAssetData> allBlueprints;
		const FTopLevelAssetPath blueprintClassPath = UBlueprint::StaticClass()->GetClassPathName();
		assetRegistry->GetAssetsByClass(blueprintClassPath, allBlueprints, true);

		for (const FAssetData& assetData : allBlueprints)
		{
			FString packagePath = assetData.PackageName.ToString();
			ESGReferenceCacheContentType contentType = GetContentTypeForPath(packagePath);

			if (contentType == ESGReferenceCacheContentType::EngineContent && !bScanEngine)
			{
				continue;
			}
			if (contentType == ESGReferenceCacheContentType::PluginContent && !bScanPlugins)
			{
				continue;
			}

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
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Blueprint scan phase: %d assets to scan"), PendingBlueprintAssets.Num());
}

bool USGDynamicTextAssetReferenceSubsystem::ProcessOneBlueprintItem()
{
	if (PendingBlueprintAssets.Num() == 0)
	{
		return false;
	}

	FAssetData assetData = PendingBlueprintAssets.Pop(EAllowShrinking::No);
	ProcessBlueprintAsset(assetData);
	return PendingBlueprintAssets.Num() > 0;
}

void USGDynamicTextAssetReferenceSubsystem::OnBlueprintPhaseComplete()
{
	UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Blueprint scan phase complete"));
}

void USGDynamicTextAssetReferenceSubsystem::SetupLevelScanPhase()
{
	PendingLevelAssets.Empty();

	const USGDynamicTextAssetEditorSettings* settings = USGDynamicTextAssetEditorSettings::Get();
	const bool bScanEngine = settings ? settings->bScanEngineContent : false;
	const bool bScanPlugins = settings ? settings->bScanPluginContent : true;

	if (IAssetRegistry* assetRegistry = IAssetRegistry::Get())
	{
		TArray<FAssetData> allLevels;
		const FTopLevelAssetPath worldClassPath = UWorld::StaticClass()->GetClassPathName();
		assetRegistry->GetAssetsByClass(worldClassPath, allLevels, true);

		for (const FAssetData& assetData : allLevels)
		{
			FString packagePath = assetData.PackageName.ToString();
			ESGReferenceCacheContentType contentType = GetContentTypeForPath(packagePath);

			if (contentType == ESGReferenceCacheContentType::EngineContent && !bScanEngine)
			{
				continue;
			}
			if (contentType == ESGReferenceCacheContentType::PluginContent && !bScanPlugins)
			{
				continue;
			}

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

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("Level scan phase: %d assets to scan"), PendingLevelAssets.Num());
}

bool USGDynamicTextAssetReferenceSubsystem::ProcessOneLevelItem()
{
	if (PendingLevelAssets.Num() == 0)
	{
		return false;
	}

	FAssetData assetData = PendingLevelAssets.Pop(EAllowShrinking::No);
	ProcessLevelAsset(assetData);
	return PendingLevelAssets.Num() > 0;
}

void USGDynamicTextAssetReferenceSubsystem::OnLevelPhaseComplete()
{
	UE_LOG(LogSGDynamicTextAssetsEditor, Verbose, TEXT("Level scan phase complete"));
}

void USGDynamicTextAssetReferenceSubsystem::SetupDTAReferenceScanPhase()
{
	PendingDynamicTextAssetFiles.Empty();

	// Use the file list discovered by the scan subsystem, filtered by timestamp
	if (const USGDynamicTextAssetScanSubsystem* scanSubsystem = GEditor->GetEditorSubsystem<USGDynamicTextAssetScanSubsystem>())
	{
		for (const FString& filePath : scanSubsystem->GetDiscoveredDTAFiles())
		{
			FDateTime fileTimestamp = IFileManager::Get().GetTimeStamp(*filePath);
			if (DoesAssetNeedRescan(filePath, fileTimestamp))
			{
				PendingDynamicTextAssetFiles.Add(filePath);
			}
		}
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("DTA reference scan phase: %d files to scan"), PendingDynamicTextAssetFiles.Num());
}

bool USGDynamicTextAssetReferenceSubsystem::ProcessOneDTAReferenceItem()
{
	if (PendingDynamicTextAssetFiles.Num() == 0)
	{
		return false;
	}

	FString filePath = PendingDynamicTextAssetFiles.Pop(EAllowShrinking::No);
	ProcessDynamicTextAssetFile(filePath);
	return PendingDynamicTextAssetFiles.Num() > 0;
}

void USGDynamicTextAssetReferenceSubsystem::OnDTAReferencePhaseComplete()
{
	bCachePopulated = true;
	bForceFullRescan = false;

	RebuildReferencerCacheFromPersistent();
	SaveCacheToDisk();

	int32 totalRefs = 0;
	for (const TPair<FSGDynamicTextAssetId, TArray<FSGDynamicTextAssetReferenceEntry>>& pair : ReferencerCache)
	{
		totalRefs += pair.Value.Num();
	}

	UE_LOG(LogSGDynamicTextAssetsEditor, Log, TEXT("DTA reference scan phase complete: %d dynamic text assets with %d total references, %d cached assets"),
		ReferencerCache.Num(), totalRefs, PersistentAssetCache.Num());

	OnReferenceScanComplete.Broadcast();
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

	// Extract file info for display
	FSGDynamicTextAssetFileInfo fileInfo = FSGDynamicTextAssetFileManager::ExtractFileInfoFromFile(FilePath);
	if (!fileInfo.bIsValid)
	{
		return;
	}

	// Use FindFirstObject which handles class lookup by name correctly
	UClass* dataObjectClass = FindFirstObject<UClass>(*fileInfo.ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);

	// Verify it's a valid dynamic text asset class
	if (!dataObjectClass || !dataObjectClass->ImplementsInterface(USGDynamicTextAssetProvider::StaticClass()))
	{
		return;
	}

	// Skip abstract classes  - they cannot be instantiated
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
	const FString displayName = fileInfo.UserFacingId.IsEmpty()
		? FSGDynamicTextAssetFileManager::ExtractUserFacingIdFromPath(FilePath)
		: fileInfo.UserFacingId;

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
		ProcessBlueprintAsset(assetData);
	}
}

void USGDynamicTextAssetReferenceSubsystem::ScanDynamicTextAssetFiles()
{
	TArray<FString> allFiles;
	FSGDynamicTextAssetFileManager::FindAllDynamicTextAssetFiles(allFiles);

	for (const FString& filePath : allFiles)
	{
		ProcessDynamicTextAssetFile(filePath);
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
		ProcessLevelAsset(assetData);
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

		for (FScriptMapHelper::FIterator itr = mapHelper.CreateIterator(); itr; ++itr)
		{
			// Check map values (keys are unlikely to be FSGDynamicTextAssetRef but check both)
			const void* valuePtr = mapHelper.GetValuePtr(itr.GetInternalIndex());
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

		for (FScriptMapHelper::FIterator itr = mapHelper.CreateIterator(); itr; ++itr)
		{
			const void* valuePtr = mapHelper.GetValuePtr(itr.GetInternalIndex());
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
	return settings->GetCacheRootFolder() / settings->ReferenceCacheFolderPath;
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
