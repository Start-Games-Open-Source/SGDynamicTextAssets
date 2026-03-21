// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SGDynamicTextAssetId.h"
#include "Framework/Docking/WorkspaceItem.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "SGDynamicTextAssetEditorStatics.generated.h"

class SWidget;
class USGDynamicTextAsset;

/**
 * Editor-only Blueprint function library for dynamic text asset file operations.
 *
 * Provides Blueprint-callable wrappers for creating, renaming, duplicating,
 * and deleting dynamic text asset files. Only available in editor contexts
 * (Editor Utility Widgets, Editor Utility Blueprints, Blutilities).
 */
UCLASS(ClassGroup = "Start Games")
class SGDYNAMICTEXTASSETSEDITOR_API USGDynamicTextAssetEditorStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Creates a new dynamic text asset file.
	 *
	 * @param DynamicTextAssetClass The class of dynamic text asset to create
	 * @param UserFacingId The human-readable name for the new dynamic text asset
	 * @param OutId Output for the generated ID
	 * @param OutFilePath Output for the created file path
	 * @return True if the file was created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Editor|File Operations", meta = (DevelopmentOnly))
	static bool CreateDynamicTextAsset(TSubclassOf<USGDynamicTextAsset> DynamicTextAssetClass, const FString& UserFacingId, FSGDynamicTextAssetId& OutId, FString& OutFilePath);

	/**
	 * Renames a dynamic text asset by moving the file to a new path.
	 * The ID remains unchanged.
	 *
	 * @param SourceFilePath Absolute path to the source file
	 * @param NewUserFacingId The new human-readable name
	 * @param OutNewFilePath Output for the new file path after rename
	 * @return True if the rename succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Editor|File Operations", meta = (DevelopmentOnly))
	static bool RenameDynamicTextAsset(const FString& SourceFilePath, const FString& NewUserFacingId, FString& OutNewFilePath);

	/**
	 * Duplicates an existing dynamic text asset with a new name and ID.
	 *
	 * @param SourceFilePath Absolute path to the source file
	 * @param NewUserFacingId The human-readable name for the duplicate
	 * @param OutNewId Output for the generated ID of the duplicate
	 * @param OutNewFilePath Output for the created file path
	 * @return True if the duplication succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Editor|File Operations", meta = (DevelopmentOnly))
	static bool DuplicateDynamicTextAsset(const FString& SourceFilePath, const FString& NewUserFacingId, FSGDynamicTextAssetId& OutNewId, FString& OutNewFilePath);

	/**
	 * Deletes a dynamic text asset file.
	 *
	 * @param FilePath Absolute path to the file to delete
	 * @return True if the file was deleted or didn't exist
	 */
	UFUNCTION(BlueprintCallable, Category = "SG Dynamic Text Assets|Editor|File Operations", meta = (DevelopmentOnly))
	static bool DeleteDynamicTextAsset(const FString& FilePath);

	/** Creates the local general use plugin workspace menu category. */
	static TSharedRef<FWorkspaceItem> GetPluginLocalWorkspaceMenuCategory();

	/**
	 * Utility function that sets keyboard focus to the inputted widget on next tick.
	 * Useful for after the widget is fully laid out.
	 */
	static void FrameDelayedKeyboardFocusToWidget(const TSharedPtr<SWidget>& Widget);
};
