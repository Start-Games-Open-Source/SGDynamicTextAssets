// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/SGDynamicTextAssetIdentityCustomization.h"

#include "Core/SGDynamicTextAsset.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor/SSGDynamicTextAssetIdentityBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "SGDynamicTextAssetEditorLogs.h"
#include "Statics/SGDynamicTextAssetStatics.h"

TSharedRef<IDetailCustomization> FSGDynamicTextAssetIdentityCustomization::MakeInstance()
{
	return MakeShareable(new FSGDynamicTextAssetIdentityCustomization());
}

void FSGDynamicTextAssetIdentityCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Get the objects being edited
	TArray<TWeakObjectPtr<UObject>> objects;
	DetailBuilder.GetObjectsBeingCustomized(objects);

	if (!objects.IsValidIndex(0))
	{
		return;
	}
	TWeakObjectPtr<UObject> weakObject = objects[0];
	if (!weakObject.IsValid())
	{
		return;
	}

	TScriptInterface<ISGDynamicTextAssetProvider> provider(weakObject.Get());
	if (!provider.GetObject())
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("Failed to retrieve dynamic text asset from weakObject(%s)"), *GetNameSafe(weakObject.Get()));
		return;
	}

	provider->GetOnDynamicTextAssetIdChanged().AddSP(this, &FSGDynamicTextAssetIdentityCustomization::RefreshId);
	provider->GetOnUserFacingIdChanged().AddSP(this, &FSGDynamicTextAssetIdentityCustomization::RefreshUserFacingId);
	provider->GetOnVersionChanged().AddSP(this, &FSGDynamicTextAssetIdentityCustomization::RefreshVersion);

	// Get the Identity category and move it to the top (sort order 0)
	IDetailCategoryBuilder& identityCategory = DetailBuilder.EditCategory(
		TEXT("Identity"),
		FText::GetEmpty(),
		ECategoryPriority::Important);

	// Hide standard properties and spawn shared identity block
	TSharedPtr<IPropertyHandle> idHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USGDynamicTextAsset, DynamicTextAssetId));
	TSharedPtr<IPropertyHandle> userFacingIdHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USGDynamicTextAsset, UserFacingId));
	TSharedPtr<IPropertyHandle> versionHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USGDynamicTextAsset, Version));

	if (idHandle.IsValid() && idHandle->IsValidHandle())
	{
		DetailBuilder.HideProperty(idHandle);
	}
	if (userFacingIdHandle.IsValid() && userFacingIdHandle->IsValidHandle())
	{
		DetailBuilder.HideProperty(userFacingIdHandle);
	}
	if (versionHandle.IsValid() && versionHandle->IsValidHandle())
	{
		DetailBuilder.HideProperty(versionHandle);
	}
	FText fileFormatVersion = FText::GetEmpty();
	if (TSharedPtr<ISGDynamicTextAssetSerializer> serializer = USGDynamicTextAssetStatics::FindSerializerForDynamicTextAssetId(provider->GetDynamicTextAssetId()))
	{
		fileFormatVersion = serializer->GetFileFormatVersion().ToText();
	}

	identityCategory.AddCustomRow(INVTEXT("Identity Properties"))
		.WholeRowContent()
		.HAlign(HAlign_Fill)
		[
			SAssignNew(IdentityBlock, SSGDynamicTextAssetIdentityBlock)
			.DynamicTextAssetId(GetIdText(provider))
			.UserFacingId(GetUserFacingIdText(provider))
			.Version(GetVersionText(provider))
			.FileFormatVersion(fileFormatVersion)
		];
}

TSharedRef<SWidget> FSGDynamicTextAssetIdentityCustomization::CreateCopyButton(FOnClicked OnCopyClicked, const FText& Tooltip)
{
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.OnClicked(OnCopyClicked)
		.ToolTipText(Tooltip)
		.ContentPadding(0.0f)
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush("GenericCommands.Copy"))
			.ColorAndOpacity(FSlateColor::UseForeground())
		];
}

void FSGDynamicTextAssetIdentityCustomization::RefreshId(TScriptInterface<ISGDynamicTextAssetProvider> InDynamicTextAsset)
{
	if (!InDynamicTextAsset)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDynamicTextAssetIdentityCustomization::RefreshId: Inputted NULL InDynamicTextAsset"));
		return;
	}
	if (!IdentityBlock.IsValid())
	{
		return;
	}
	IdentityBlock->SetIdentityProperties(GetIdText(InDynamicTextAsset), GetUserFacingIdText(InDynamicTextAsset), GetVersionText(InDynamicTextAsset));
}

void FSGDynamicTextAssetIdentityCustomization::RefreshUserFacingId(TScriptInterface<ISGDynamicTextAssetProvider> InDynamicTextAsset)
{
	if (!InDynamicTextAsset)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDynamicTextAssetIdentityCustomization::RefreshId: Inputted NULL InDynamicTextAsset"));
		return;
	}
	if (!IdentityBlock.IsValid())
	{
		return;
	}
	IdentityBlock->SetIdentityProperties(GetIdText(InDynamicTextAsset), GetUserFacingIdText(InDynamicTextAsset), GetVersionText(InDynamicTextAsset));
}

void FSGDynamicTextAssetIdentityCustomization::RefreshVersion(TScriptInterface<ISGDynamicTextAssetProvider> InDynamicTextAsset)
{
	if (!InDynamicTextAsset)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDynamicTextAssetIdentityCustomization::RefreshId: Inputted NULL InDynamicTextAsset"));
		return;
	}
	if (!IdentityBlock.IsValid())
	{
		return;
	}
	IdentityBlock->SetIdentityProperties(GetIdText(InDynamicTextAsset), GetUserFacingIdText(InDynamicTextAsset), GetVersionText(InDynamicTextAsset));
}

FText FSGDynamicTextAssetIdentityCustomization::GetIdText(const TScriptInterface<ISGDynamicTextAssetProvider>& InDynamicTextAsset) const
{
	return FText::FromString(InDynamicTextAsset->GetDynamicTextAssetId().ToString());
}

FText FSGDynamicTextAssetIdentityCustomization::GetUserFacingIdText(const TScriptInterface<ISGDynamicTextAssetProvider>& InDynamicTextAsset) const
{
	return FText::FromString(InDynamicTextAsset->GetUserFacingId());
}

FText FSGDynamicTextAssetIdentityCustomization::GetVersionText(const TScriptInterface<ISGDynamicTextAssetProvider>& InDynamicTextAsset) const
{
	return InDynamicTextAsset->GetVersion().ToText();
}
