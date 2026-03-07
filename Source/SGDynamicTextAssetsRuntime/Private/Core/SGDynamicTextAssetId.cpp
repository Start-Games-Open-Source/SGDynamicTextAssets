// Copyright Start Games, Inc. All Rights Reserved.

#include "Core/SGDynamicTextAssetId.h"

FSGDynamicTextAssetId::FSGDynamicTextAssetId(const FGuid& InGuid)
	: Guid(InGuid)
{

}

FSGDynamicTextAssetId::FSGDynamicTextAssetId(FGuid&& InGuid)
	: Guid(InGuid)
{
}

bool FSGDynamicTextAssetId::IsValid() const
{
	return Guid.IsValid();
}

const FGuid& FSGDynamicTextAssetId::GetGuid() const
{
	return Guid;
}

void FSGDynamicTextAssetId::SetGuid(const FGuid& InGuid)
{
#if WITH_EDITOR
	const FGuid prevGuid = Guid;
#endif
	Guid = InGuid;
#if WITH_EDITOR
	OnGuidChanged_Editor.Broadcast(prevGuid, Guid);
#endif
}

bool FSGDynamicTextAssetId::ParseString(const FString& GuidString)
{
	FGuid guid;
	bool result = FGuid::Parse(GuidString, guid);
	SetGuid(guid);
	return result;
}

void FSGDynamicTextAssetId::Invalidate()
{
#if WITH_EDITOR
	const FGuid prevGuid = Guid;
#endif
	Guid.Invalidate();
#if WITH_EDITOR
	OnGuidChanged_Editor.Broadcast(prevGuid, Guid);
#endif
}

FString FSGDynamicTextAssetId::ToString() const
{
	return Guid.ToString(EGuidFormats::DigitsWithHyphens);
}

void FSGDynamicTextAssetId::GenerateNewId()
{
	SetGuid(FGuid::NewGuid());
}

FSGDynamicTextAssetId FSGDynamicTextAssetId::NewGeneratedId()
{
	return FSGDynamicTextAssetId(FGuid::NewGuid());
}

FSGDynamicTextAssetId FSGDynamicTextAssetId::FromGuid(const FGuid& InGuid)
{
	return FSGDynamicTextAssetId(InGuid);
}

FSGDynamicTextAssetId FSGDynamicTextAssetId::FromString(const FString& InString)
{
	FGuid parsedGuid;
	if (FGuid::Parse(InString, parsedGuid))
	{
		return FSGDynamicTextAssetId(parsedGuid);
	}
	return FSGDynamicTextAssetId();
}

void FSGDynamicTextAssetId::operator=(const FGuid& Other)
{
	SetGuid(Other);
}

bool FSGDynamicTextAssetId::operator==(const FGuid& Other) const
{
	return Guid == Other;
}

bool FSGDynamicTextAssetId::operator!=(const FGuid& Other) const
{
	return Guid != Other;
}

bool FSGDynamicTextAssetId::operator==(const FSGDynamicTextAssetId& Other) const
{
	return Guid == Other.Guid;
}

bool FSGDynamicTextAssetId::operator!=(const FSGDynamicTextAssetId& Other) const
{
	return Guid != Other.Guid;
}

bool FSGDynamicTextAssetId::Serialize(FArchive& Ar)
{
	Ar << Guid;
	return true;
}

bool FSGDynamicTextAssetId::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Ar << Guid;
	bOutSuccess = true;
	return true;
}

bool FSGDynamicTextAssetId::ExportTextItem(FString& ValueStr, const FSGDynamicTextAssetId& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	ValueStr += ToString();
	return true;
}

bool FSGDynamicTextAssetId::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
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
