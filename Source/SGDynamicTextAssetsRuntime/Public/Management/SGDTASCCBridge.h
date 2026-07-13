// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Internal source-control command seam for the dynamic text asset file manager.
 *
 * Declared by the Runtime module and implemented once by the Editor module,
 * which registers its instance as a static pointer on FSGDynamicTextAssetFileManager
 * (see SGDTAEditorModule.cpp StartupModule/ShutdownModule). This inverts the module
 * dependency: the Runtime module reaches source control through this abstraction
 * without linking the Editor-only source-control wrapper, so the cooked build never
 * pulls in editor code. In cooked/runtime builds, and any time the Editor module is
 * not loaded, the registered pointer is null and every call site treats it as a
 * graceful exit (checkout is simply skipped).
 *
 * This is intended for internal usage, NOT a developer extension point but we do allow it(at your own peril!) 
 * It is implemented by a single editor-module class and is never implemented by DTA provider type. 
 * The only interface a developer implements to add UObject support remains ISGDynamicTextAssetProvider. 
 * The bridge operates on file paths only and knows nothing about DTA provider types.
 */
class ISGDTASourceControlBridge
{
public:
    virtual ~ISGDTASourceControlBridge() = default;

    /**
     * Checks out a file from source control before it is overwritten.
     *
     * @param FilePath Absolute path to the file to check out.
     * @return True if the file is checked out (or was already checked out by this user).
     * False on failure, in which case the caller logs a warning and proceeds.
     */
    virtual bool CheckOutFile(const FString& FilePath) = 0;

    /**
     * Marks a newly created file for add in source control.
     *
     * @param FilePath Absolute path to the file to mark for add.
     * @return True if the file was successfully marked for add.
     * False on failure, in which case the caller logs a warning and proceeds.
     */
    virtual bool MarkForAdd(const FString& FilePath) = 0;

    /**
     * Marks a file for delete in source control before it is removed from disk.
     *
     * @param FilePath Absolute path to the file to mark for delete.
     * @return True if the file was successfully marked for delete.
     * False on failure, in which case the caller logs a warning and proceeds.
     */
    virtual bool MarkForDelete(const FString& FilePath) = 0;
};
