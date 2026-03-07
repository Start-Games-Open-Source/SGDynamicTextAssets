// Copyright Start Games, Inc. All Rights Reserved.

#include "Core/SGDynamicTextAssetTypeId.h"

const FSGDynamicTextAssetTypeId FSGDynamicTextAssetTypeId::INVALID_TYPE_ID;

FSGDynamicTextAssetTypeId::FSGDynamicTextAssetTypeId(const FGuid& InGuid)
	: Guid(InGuid)
{

}

FSGDynamicTextAssetTypeId::FSGDynamicTextAssetTypeId(FGuid&& InGuid)
	: Guid(InGuid)
{
}

bool FSGDynamicTextAssetTypeId::IsValid() const
{
	return Guid.IsValid();
}

const FGuid& FSGDynamicTextAssetTypeId::GetGuid() const
{
	return Guid;
}

void FSGDynamicTextAssetTypeId::SetGuid(const FGuid& InGuid)
{
#if WITH_EDITOR
	const FGuid prevGuid = Guid;
#endif
	Guid = InGuid;
#if WITH_EDITOR
	OnGuidChanged_Editor.Broadcast(prevGuid, Guid);
#endif
}

bool FSGDynamicTextAssetTypeId::ParseString(const FString& GuidString)
{
	FGuid guid;
	bool result = FGuid::Parse(GuidString, guid);
	SetGuid(guid);
	return result;
}

void FSGDynamicTextAssetTypeId::Invalidate()
{
#if WITH_EDITOR
	const FGuid prevGuid = Guid;
#endif
	Guid.Invalidate();
#if WITH_EDITOR
	OnGuidChanged_Editor.Broadcast(prevGuid, Guid);
#endif
}

FString FSGDynamicTextAssetTypeId::ToString() const
{
	return Guid.ToString(EGuidFormats::DigitsWithHyphens);
}

void FSGDynamicTextAssetTypeId::GenerateNewId()
{
	SetGuid(FGuid::NewGuid());
}

FSGDynamicTextAssetTypeId FSGDynamicTextAssetTypeId::NewGeneratedId()
{
	return FSGDynamicTextAssetTypeId(FGuid::NewGuid());
}

FSGDynamicTextAssetTypeId FSGDynamicTextAssetTypeId::FromGuid(const FGuid& InGuid)
{
	return FSGDynamicTextAssetTypeId(InGuid);
}

FSGDynamicTextAssetTypeId FSGDynamicTextAssetTypeId::FromString(const FString& InString)
{
	FGuid parsedGuid;
	if (FGuid::Parse(InString, parsedGuid))
	{
		return FSGDynamicTextAssetTypeId(parsedGuid);
	}
	return FSGDynamicTextAssetTypeId();
}

void FSGDynamicTextAssetTypeId::operator=(const FGuid& Other)
{
	SetGuid(Other);
}

bool FSGDynamicTextAssetTypeId::operator==(const FGuid& Other) const
{
	return Guid == Other;
}

bool FSGDynamicTextAssetTypeId::operator!=(const FGuid& Other) const
{
	return Guid != Other;
}

bool FSGDynamicTextAssetTypeId::operator==(const FSGDynamicTextAssetTypeId& Other) const
{
	return Guid == Other.Guid;
}

bool FSGDynamicTextAssetTypeId::operator!=(const FSGDynamicTextAssetTypeId& Other) const
{
	return Guid != Other.Guid;
}

bool FSGDynamicTextAssetTypeId::Serialize(FArchive& Ar)
{
	Ar << Guid;
	return true;
}

bool FSGDynamicTextAssetTypeId::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Ar << Guid;
	bOutSuccess = true;
	return true;
}

bool FSGDynamicTextAssetTypeId::ExportTextItem(FString& ValueStr, const FSGDynamicTextAssetTypeId& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	ValueStr += ToString();
	return true;
}

bool FSGDynamicTextAssetTypeId::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	// Read up to 36 characters (GUID with hyphens: 8-4-4-4-12)
	FString guidString;
	const TCHAR* start = Buffer;

	// Consume characters that are valid hex digits or hyphens
	while (*Buffer != TEXT('\0') && ((FChar::IsHexDigit(*Buffer) || *Buffer == TEXT('-'))))
	{
		guidString.AppendChar(*Buffer);
		++Buffer;
	}

	if (guidString.IsEmpty())
	{
		// Restore position on failure
		Buffer = start;
		return false;
	}

	if (ParseString(guidString))
	{
		return true;
	}

	// Restore position on failure
	Buffer = start;
	return false;
}
