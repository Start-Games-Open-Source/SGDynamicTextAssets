// Copyright Start Games, Inc. All Rights Reserved.

#include "Core/SGDTASerializerFormat.h"

#include "Management/SGDTAFileManager.h"
#include "Serialization/SGDTASerializer.h"

const FSGDTASerializerFormat FSGDTASerializerFormat::INVALID;

FSGDTASerializerFormat::FSGDTASerializerFormat(const uint32& InTypeId)
	: TypeId(InTypeId)
{
}

bool FSGDTASerializerFormat::IsValid() const
{
	return TypeId != INVALID.TypeId;
}

uint32 FSGDTASerializerFormat::GetTypeId() const
{
	return TypeId;
}

void FSGDTASerializerFormat::SetTypeId(const uint32& InTypeId)
{
	TypeId = InTypeId;
}

FString FSGDTASerializerFormat::ToString() const
{
	return FString::FromInt(static_cast<int32>(TypeId));
}

FSGDTASerializerFormat FSGDTASerializerFormat::FromTypeId(uint32 InTypeId)
{
	return FSGDTASerializerFormat(InTypeId);
}

FSGDTASerializerFormat FSGDTASerializerFormat::FromExtension(const FString& Extension)
{
	return FSGDynamicTextAssetFileManager::GetFormatForExtension(Extension);
}

FText FSGDTASerializerFormat::GetFormatName() const
{
	if (TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFormat(*this))
	{
		return serializer->GetFormatName();
	}
	return INVTEXT("Invalid");
}

FString FSGDTASerializerFormat::GetFileExtension() const
{
	if (TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFormat(*this))
	{
		return serializer->GetFileExtension();
	}
	return FString();
}

FText FSGDTASerializerFormat::GetFormatDescription() const
{
	if (TSharedPtr<ISGDynamicTextAssetSerializer> serializer = FSGDynamicTextAssetFileManager::FindSerializerForFormat(*this))
	{
		return serializer->GetFormatDescription();
	}
	return INVTEXT("Invalid");
}

TSharedPtr<ISGDynamicTextAssetSerializer> FSGDTASerializerFormat::GetSerializer() const
{
	return FSGDynamicTextAssetFileManager::FindSerializerForFormat(*this);
}

FSGDTASerializerFormat::operator unsigned int() const
{
	return TypeId;
}

void FSGDTASerializerFormat::operator=(const uint32& Other)
{
	TypeId = Other;
}

bool FSGDTASerializerFormat::operator==(const FSGDTASerializerFormat& Other) const
{
	return TypeId == Other.TypeId;
}

bool FSGDTASerializerFormat::operator!=(const FSGDTASerializerFormat& Other) const
{
	return TypeId != Other.TypeId;
}

bool FSGDTASerializerFormat::operator==(const uint32& Other) const
{
	return TypeId == Other;
}

bool FSGDTASerializerFormat::operator!=(const uint32& Other) const
{
	return TypeId != Other;
}

bool FSGDTASerializerFormat::Serialize(FArchive& Ar)
{
	Ar << TypeId;
	return true;
}

bool FSGDTASerializerFormat::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Ar << TypeId;
	bOutSuccess = true;
	return true;
}

bool FSGDTASerializerFormat::ExportTextItem(FString& ValueStr, const FSGDTASerializerFormat& DefaultValue,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	ValueStr += ToString();
	return true;
}

bool FSGDTASerializerFormat::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags,
	UObject* Parent, FOutputDevice* ErrorText)
{
	FString digitString;
	const TCHAR* start = Buffer;

	// Consume decimal digit characters
	while (*Buffer != TEXT('\0') && FChar::IsDigit(*Buffer))
	{
		digitString.AppendChar(*Buffer);
		++Buffer;
	}

	if (digitString.IsEmpty())
	{
		// Restore position on failure
		Buffer = start;
		return false;
	}

	TypeId = static_cast<uint32>(FCString::Atoi(*digitString));
	return true;
}
