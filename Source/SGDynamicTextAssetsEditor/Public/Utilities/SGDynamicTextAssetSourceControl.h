// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SGDynamicTextAssetSourceControl.generated.h"

/**
 * Source control status for dynamic text asset files.
 * Maps to Unreal's internal source control states.
 */
UENUM(BlueprintType)
enum class ESGDynamicTextAssetSourceControlStatus : uint8
{
	/** Status not determined */
	Unknown             UMETA(DisplayName = "Unknown"),

	/** File not tracked by source control */
	NotInSourceControl  UMETA(DisplayName = "Not In Source Control"),

	/** File marked for add */
	Added               UMETA(DisplayName = "Added"),

	/** File checked out by current user */
	CheckedOut          UMETA(DisplayName = "Checked Out"),

	/** File checked out by another user */
	CheckedOutByOther   UMETA(DisplayName = "Checked Out By Other"),

	/** File is tracked but not checked out */
	NotCheckedOut       UMETA(DisplayName = "Not Checked Out"),

	/** File has local modifications */
	ModifiedLocally     UMETA(DisplayName = "Modified Locally")
};

/**
 * Static utility class providing thin wrappers around Unreal's ISourceControlModule.
 * Used for managing source control operations on dynamic text asset files.
 */
class SGDYNAMICTEXTASSETSEDITOR_API FSGDynamicTextAssetSourceControl
{
public:
	/**
	 * Checks out a file from source control.
	 * @param FilePath Full path to the file to check out.
	 * @return True if the checkout was successful or file is already checked out by current user.
	 */
	static bool CheckOutFile(const FString& FilePath);

	/**
	 * Reverts a file in source control, discarding local changes.
	 * @param FilePath Full path to the file to revert.
	 * @return True if the revert was successful.
	 */
	static bool RevertFile(const FString& FilePath);

	/**
	 * Marks a file for add in source control.
	 * @param FilePath Full path to the file to mark for add.
	 * @return True if the file was successfully marked for add.
	 */
	static bool MarkForAdd(const FString& FilePath);

	/**
	 * Marks a file for delete in source control.
	 * @param FilePath Full path to the file to mark for delete.
	 * @return True if the file was successfully marked for delete.
	 */
	static bool MarkForDelete(const FString& FilePath);

	/**
	 * Gets the current source control status of a file.
	 * @param FilePath Full path to the file to check.
	 * @return The source control status of the file.
	 */
	static ESGDynamicTextAssetSourceControlStatus GetFileStatus(const FString& FilePath);

	/**
	 * Checks if a file is checked out by another user.
	 * @param FilePath Full path to the file to check.
	 * @param OutUsername If checked out by another user, contains their username.
	 * @return True if the file is checked out by another user.
	 */
	static bool IsCheckedOutByOther(const FString& FilePath, FString& OutUsername);

	/**
	 * Checks if source control is enabled and available.
	 * @return True if source control is enabled and connected.
	 */
	static bool IsSourceControlEnabled();

	/**
	 * Checks out multiple files from source control.
	 * @param FilePaths Array of full paths to the files to check out.
	 * @return True if all checkouts were successful.
	 */
	static bool CheckOutFiles(const TArray<FString>& FilePaths);

	/**
	 * Marks multiple files for add in source control.
	 * @param FilePaths Array of full paths to the files to mark for add.
	 * @return True if all files were successfully marked for add.
	 */
	static bool MarkFilesForAdd(const TArray<FString>& FilePaths);
};
