// Copyright Start Games, Inc. All Rights Reserved.

#include "Editor/SGDTADetailCustomization.h"

#include "Core/SGDynamicTextAsset.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor/SSGDTAIdentityBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "SGDTAEditorLogs.h"
#include "Statics/SGDynamicTextAssetStatics.h"
#include "UObject/UnrealType.h"

TSharedRef<IDetailCustomization> FSGDTADetailCustomization::MakeInstance()
{
	return MakeShareable(new FSGDTADetailCustomization());
}

void FSGDTADetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
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

	provider->GetOnDynamicTextAssetIdChanged().AddSP(this, &FSGDTADetailCustomization::RefreshId);
	provider->GetOnUserFacingIdChanged().AddSP(this, &FSGDTADetailCustomization::RefreshUserFacingId);
	provider->GetOnVersionChanged().AddSP(this, &FSGDTADetailCustomization::RefreshVersion);

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

	// Create a top-level "File Information" category directly after Identity
	IDetailCategoryBuilder& fileInfoCategory = DetailBuilder.EditCategory(
		TEXT("File Information"),
		FText::GetEmpty(),
		ECategoryPriority::Important);

	// Add AssetBundleExtenderOverride first for guaranteed top position
	TSharedPtr<IPropertyHandle> extenderHandle =
		DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USGDynamicTextAsset, AssetBundleExtenderOverride));
	DetailBuilder.HideProperty(extenderHandle);
	fileInfoCategory.AddProperty(extenderHandle);

	// Collect DTA|File Information properties from subclasses, then hide the entire
	// DTA category. AddExternalObjectProperty creates independent rows in File Information
	// that are not affected by HideCategory("DTA").
	const FName extenderOverrideName = GET_MEMBER_NAME_CHECKED(USGDynamicTextAsset, AssetBundleExtenderOverride);
	UClass* editedClass = weakObject.IsValid() ? weakObject->GetClass() : DetailBuilder.GetBaseClass();
	TArray<UObject*> editedObjects;
	if (weakObject.IsValid())
	{
		editedObjects.Add(weakObject.Get());
	}

	for (TFieldIterator<FProperty> propIt(editedClass); propIt; ++propIt)
	{
		if (propIt->GetFName() == extenderOverrideName)
		{
			continue;
		}

		const FString category = propIt->GetMetaData(TEXT("Category"));
		if (category.StartsWith(TEXT("SGDTA|File Information")) && editedObjects.Num() > 0)
		{
			fileInfoCategory.AddExternalObjectProperty(editedObjects, propIt->GetFName());
		}
	}

	// Hide the entire DTA category. The AssetBundleExtenderOverride was already moved
	// via AddProperty (handle-based, unaffected by category hide). Subclass properties
	// were moved via AddExternalObjectProperty (independent rows, unaffected by category hide).
	DetailBuilder.HideCategory(TEXT("SGDTA"));
}

TSharedRef<SWidget> FSGDTADetailCustomization::CreateCopyButton(FOnClicked OnCopyClicked, const FText& Tooltip)
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

void FSGDTADetailCustomization::RefreshId(TScriptInterface<ISGDynamicTextAssetProvider> InDynamicTextAsset)
{
	if (!InDynamicTextAsset)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDTADetailCustomization::RefreshId: Inputted NULL InDynamicTextAsset"));
		return;
	}
	if (!IdentityBlock.IsValid())
	{
		return;
	}
	IdentityBlock->SetIdentityProperties(GetIdText(InDynamicTextAsset), GetUserFacingIdText(InDynamicTextAsset), GetVersionText(InDynamicTextAsset));
}

void FSGDTADetailCustomization::RefreshUserFacingId(TScriptInterface<ISGDynamicTextAssetProvider> InDynamicTextAsset)
{
	if (!InDynamicTextAsset)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDTADetailCustomization::RefreshId: Inputted NULL InDynamicTextAsset"));
		return;
	}
	if (!IdentityBlock.IsValid())
	{
		return;
	}
	IdentityBlock->SetIdentityProperties(GetIdText(InDynamicTextAsset), GetUserFacingIdText(InDynamicTextAsset), GetVersionText(InDynamicTextAsset));
}

void FSGDTADetailCustomization::RefreshVersion(TScriptInterface<ISGDynamicTextAssetProvider> InDynamicTextAsset)
{
	if (!InDynamicTextAsset)
	{
		UE_LOG(LogSGDynamicTextAssetsEditor, Error, TEXT("FSGDTADetailCustomization::RefreshId: Inputted NULL InDynamicTextAsset"));
		return;
	}
	if (!IdentityBlock.IsValid())
	{
		return;
	}
	IdentityBlock->SetIdentityProperties(GetIdText(InDynamicTextAsset), GetUserFacingIdText(InDynamicTextAsset), GetVersionText(InDynamicTextAsset));
}

FText FSGDTADetailCustomization::GetIdText(const TScriptInterface<ISGDynamicTextAssetProvider>& InDynamicTextAsset) const
{
	return FText::FromString(InDynamicTextAsset->GetDynamicTextAssetId().ToString());
}

FText FSGDTADetailCustomization::GetUserFacingIdText(const TScriptInterface<ISGDynamicTextAssetProvider>& InDynamicTextAsset) const
{
	return FText::FromString(InDynamicTextAsset->GetUserFacingId());
}

FText FSGDTADetailCustomization::GetVersionText(const TScriptInterface<ISGDynamicTextAssetProvider>& InDynamicTextAsset) const
{
	return InDynamicTextAsset->GetVersion().ToText();
}
