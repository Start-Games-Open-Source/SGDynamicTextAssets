// Copyright Start Games, Inc. All Rights Reserved.

#include "Utilities/SGDynamicTextAssetEditorStatics.h"

#include "Core/SGDynamicTextAsset.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"
#include "Management/SGDynamicTextAssetFileManager.h"
#include "SGDynamicTextAssetsEditorModule.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Templates/SubclassOf.h"

bool USGDynamicTextAssetEditorStatics::CreateDynamicTextAsset(TSubclassOf<USGDynamicTextAsset> DynamicTextAssetClass, const FString& UserFacingId, FSGDynamicTextAssetId& OutId, FString& OutFilePath)
{
	if (!DynamicTextAssetClass)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("CreateDynamicTextAsset: Inputted NULL DynamicTextAssetClass"));
		return false;
	}

	return FSGDynamicTextAssetFileManager::CreateDynamicTextAssetFile(DynamicTextAssetClass, UserFacingId, FSGDynamicTextAssetFileManager::DEFAULT_TEXT_EXTENSION, OutId, OutFilePath);
}

bool USGDynamicTextAssetEditorStatics::RenameDynamicTextAsset(const FString& SourceFilePath, const FString& NewUserFacingId, FString& OutNewFilePath)
{
	return FSGDynamicTextAssetFileManager::RenameDynamicTextAsset(SourceFilePath, NewUserFacingId, OutNewFilePath);
}

bool USGDynamicTextAssetEditorStatics::DuplicateDynamicTextAsset(const FString& SourceFilePath, const FString& NewUserFacingId, FSGDynamicTextAssetId& OutNewId, FString& OutNewFilePath)
{
	return FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset(SourceFilePath, NewUserFacingId, OutNewId, OutNewFilePath);
}

bool USGDynamicTextAssetEditorStatics::DeleteDynamicTextAsset(const FString& FilePath)
{
	return FSGDynamicTextAssetFileManager::DeleteDynamicTextAssetFile(FilePath);
}

TSharedRef<FWorkspaceItem> USGDynamicTextAssetEditorStatics::GetPluginLocalWorkspaceMenuCategory()
{
	static TSharedRef<FWorkspaceItem> sharedCategory = WorkspaceMenu::GetMenuStructure().GetStructureRoot()->AddGroup(INVTEXT("SG Dynamic Text Assets"),
		INVTEXT("SG Dynamic Text Assets Category for available tools."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"),
		true);

	return sharedCategory;
}

void USGDynamicTextAssetEditorStatics::FrameDelayedKeyboardFocusToWidget(const TSharedPtr<SWidget>& Widget)
{
	if (!Widget.IsValid())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("USGDynamicTextAssetEditorStatics::FrameDelayedKeyboardFocusToWidget: Inputted NULL Widget"));
		return;
	}
	TWeakPtr<SWidget> weakWidget(Widget);
	Widget->RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateLambda(
		[weakWidget](double, float) -> EActiveTimerReturnType
		{
			if (weakWidget.Pin().IsValid())
			{
				FSlateApplication::Get().SetKeyboardFocus(weakWidget.Pin());
			}
			return EActiveTimerReturnType::Stop;
		}));
}
