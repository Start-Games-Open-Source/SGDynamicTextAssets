// Copyright Start Games, Inc. All Rights Reserved.

#include "Utilities/SGDynamicTextAssetSourceControl.h"

#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#include "SourceControlHelpers.h"

bool FSGDynamicTextAssetSourceControl::CheckOutFile(const FString& FilePath)
{
	if (!IsSourceControlEnabled())
	{
		return true;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	const FSourceControlStatePtr FileState = SourceControlProvider.GetState(FilePath, EStateCacheUsage::Use);
	
	if (!FileState.IsValid())
	{
		return false;
	}

	// Already checked out by us
	if (FileState->IsCheckedOut())
	{
		return true;
	}

	// Can't check out if someone else has it
	if (FileState->IsCheckedOutOther())
	{
		return false;
	}

	// Not in source control - nothing to check out
	if (!FileState->IsSourceControlled())
	{
		return true;
	}

	TArray<FString> FilesToCheckOut;
	FilesToCheckOut.Add(FilePath);
	
	return SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), FilesToCheckOut) == ECommandResult::Succeeded;
}

bool FSGDynamicTextAssetSourceControl::RevertFile(const FString& FilePath)
{
	if (!IsSourceControlEnabled())
	{
		return false;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	
	TArray<FString> FilesToRevert;
	FilesToRevert.Add(FilePath);
	
	return SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), FilesToRevert) == ECommandResult::Succeeded;
}

bool FSGDynamicTextAssetSourceControl::MarkForAdd(const FString& FilePath)
{
	if (!IsSourceControlEnabled())
	{
		return true;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	const FSourceControlStatePtr FileState = SourceControlProvider.GetState(FilePath, EStateCacheUsage::Use);
	
	if (FileState.IsValid() && FileState->IsSourceControlled())
	{
		// Already in source control
		return true;
	}

	TArray<FString> FilesToAdd;
	FilesToAdd.Add(FilePath);
	
	return SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), FilesToAdd) == ECommandResult::Succeeded;
}

bool FSGDynamicTextAssetSourceControl::MarkForDelete(const FString& FilePath)
{
	if (!IsSourceControlEnabled())
	{
		return true;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	
	TArray<FString> FilesToDelete;
	FilesToDelete.Add(FilePath);
	
	return SourceControlProvider.Execute(ISourceControlOperation::Create<FDelete>(), FilesToDelete) == ECommandResult::Succeeded;
}

ESGDynamicTextAssetSourceControlStatus FSGDynamicTextAssetSourceControl::GetFileStatus(const FString& FilePath)
{
	if (!IsSourceControlEnabled())
	{
		return ESGDynamicTextAssetSourceControlStatus::Unknown;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr FileState = SourceControlProvider.GetState(FilePath, EStateCacheUsage::Use);
	
	if (!FileState.IsValid())
	{
		return ESGDynamicTextAssetSourceControlStatus::Unknown;
	}
	if (!FileState->IsSourceControlled())
	{
		return ESGDynamicTextAssetSourceControlStatus::NotInSourceControl;
	}
	if (FileState->IsAdded())
	{
		return ESGDynamicTextAssetSourceControlStatus::Added;
	}
	if (FileState->IsCheckedOutOther())
	{
		return ESGDynamicTextAssetSourceControlStatus::CheckedOutByOther;
	}
	if (FileState->IsCheckedOut())
	{
		return ESGDynamicTextAssetSourceControlStatus::CheckedOut;
	}
	if (FileState->IsModified())
	{
		return ESGDynamicTextAssetSourceControlStatus::ModifiedLocally;
	}

	return ESGDynamicTextAssetSourceControlStatus::NotCheckedOut;
}

bool FSGDynamicTextAssetSourceControl::IsCheckedOutByOther(const FString& FilePath, FString& OutUsername)
{
	OutUsername.Empty();

	if (!IsSourceControlEnabled())
	{
		return false;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	const FSourceControlStatePtr FileState = SourceControlProvider.GetState(FilePath, EStateCacheUsage::Use);
	
	if (!FileState.IsValid())
	{
		return false;
	}

	if (FileState->IsCheckedOutOther())
	{
		OutUsername = FileState->GetOtherUserBranchCheckedOuts();
		return true;
	}

	return false;
}

bool FSGDynamicTextAssetSourceControl::IsSourceControlEnabled()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	return SourceControlModule.IsEnabled() && SourceControlModule.GetProvider().IsAvailable();
}

bool FSGDynamicTextAssetSourceControl::CheckOutFiles(const TArray<FString>& FilePaths)
{
	if (!IsSourceControlEnabled() || FilePaths.IsEmpty())
	{
		return true;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	
	TArray<FString> FilesToCheckOut;
	
	for (const FString& FilePath : FilePaths)
	{
		FSourceControlStatePtr FileState = SourceControlProvider.GetState(FilePath, EStateCacheUsage::Use);
		
		if (!FileState.IsValid())
		{
			continue;
		}

		// Skip if already checked out or not in source control
		if (FileState->IsCheckedOut() || !FileState->IsSourceControlled())
		{
			continue;
		}

		// Can't check out if someone else has it
		if (FileState->IsCheckedOutOther())
		{
			return false;
		}

		FilesToCheckOut.Add(FilePath);
	}

	if (FilesToCheckOut.IsEmpty())
	{
		return true;
	}
	
	return SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), FilesToCheckOut) == ECommandResult::Succeeded;
}

bool FSGDynamicTextAssetSourceControl::MarkFilesForAdd(const TArray<FString>& FilePaths)
{
	if (!IsSourceControlEnabled() || FilePaths.IsEmpty())
	{
		return true;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	
	TArray<FString> FilesToAdd;
	
	for (const FString& FilePath : FilePaths)
	{
		FSourceControlStatePtr FileState = SourceControlProvider.GetState(FilePath, EStateCacheUsage::Use);
		
		// Skip if already in source control
		if (FileState.IsValid() && FileState->IsSourceControlled())
		{
			continue;
		}

		FilesToAdd.Add(FilePath);
	}

	if (FilesToAdd.IsEmpty())
	{
		return true;
	}
	
	return SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), FilesToAdd) == ECommandResult::Succeeded;
}
