// Copyright Start Games, Inc. All Rights Reserved.

#include "Management/SGDynamicTextAssetCookManifestStringTable.h"

#include "SGDynamicTextAssetLogs.h"

uint32 FSGDynamicTextAssetCookManifestStringTable::AddString(const FString& InString)
{
	if (InString.IsEmpty())
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDynamicTextAssetCookManifestStringTable::AddString: "
				"Empty string passed  - use INVALID_STRING_INDEX for absent fields"));
		return INVALID_STRING_INDEX;
	}

	// Check for existing entry
	const uint32* existingIndex = DeduplicationMap.Find(InString);
	if (existingIndex != nullptr)
	{
		return *existingIndex;
	}

	// Add new entry
	uint32 newIndex = static_cast<uint32>(Strings.Num());
	Strings.Add(InString);
	DeduplicationMap.Add(InString, newIndex);

	return newIndex;
}

const FString& FSGDynamicTextAssetCookManifestStringTable::GetString(uint32 Index) const
{
	if (Index == INVALID_STRING_INDEX)
	{
		static const FString emptyString;
		return emptyString;
	}

	if (Index >= static_cast<uint32>(Strings.Num()))
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
			TEXT("FSGDynamicTextAssetCookManifestStringTable::GetString: "
				"Index %u out of range (table has %d strings)"), Index, Strings.Num());

		static const FString emptyString;
		return emptyString;
	}

	return Strings[Index];
}

void FSGDynamicTextAssetCookManifestStringTable::SerializeTo(FArchive& Ar) const
{
	uint32 count = static_cast<uint32>(Strings.Num());
	Ar << count;

	for (const FString& string : Strings)
	{
		FTCHARToUTF8 utf8Converter(*string);
		int32 utf8Length = utf8Converter.Length();

		if (utf8Length > MAX_uint16)
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
				TEXT("FSGDynamicTextAssetCookManifestStringTable::SerializeTo: "
					"String '%s' UTF-8 length (%d) exceeds uint16 max  - truncating to %d bytes"),
				*string, utf8Length, MAX_uint16);
			utf8Length = MAX_uint16;
		}

		uint16 byteLength = static_cast<uint16>(utf8Length);
		Ar << byteLength;
		Ar.Serialize(const_cast<ANSICHAR*>(utf8Converter.Get()), byteLength);
	}
}

bool FSGDynamicTextAssetCookManifestStringTable::DeserializeFrom(FArchive& Ar)
{
	Reset();

	uint32 count = 0;
	Ar << count;

	// Sanity check to prevent absurd allocations from malformed data
	static constexpr uint32 MAX_REASONABLE_STRING_COUNT = 1000000;
	if (count > MAX_REASONABLE_STRING_COUNT)
	{
		UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
			TEXT("FSGDynamicTextAssetCookManifestStringTable::DeserializeFrom: "
				"String count %u exceeds maximum of %u  - data is likely corrupt"),
			count, MAX_REASONABLE_STRING_COUNT);
		return false;
	}

	Strings.Reserve(count);
	DeduplicationMap.Reserve(count);

	for (uint32 i = 0; i < count; ++i)
	{
		uint16 byteLength = 0;
		Ar << byteLength;

		if (Ar.IsError())
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
				TEXT("FSGDynamicTextAssetCookManifestStringTable::DeserializeFrom: "
					"Archive error reading byte length for string %u of %u"), i, count);
			Reset();
			return false;
		}

		if (byteLength == 0)
		{
			// Zero-length string entry (shouldn't normally occur but handle gracefully)
			Strings.Add(FString());
			continue;
		}

		// Read raw UTF-8 bytes
		TArray<uint8> utf8Bytes;
		utf8Bytes.SetNumUninitialized(byteLength);
		Ar.Serialize(utf8Bytes.GetData(), byteLength);

		if (Ar.IsError())
		{
			UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
				TEXT("FSGDynamicTextAssetCookManifestStringTable::DeserializeFrom: "
					"Archive error reading UTF-8 bytes for string %u of %u (expected %u bytes)"),
				i, count, byteLength);
			Reset();
			return false;
		}

		// Convert UTF-8 bytes to FString
		FUTF8ToTCHAR utf8Converter(
			reinterpret_cast<const ANSICHAR*>(utf8Bytes.GetData()), byteLength);
		FString convertedString(utf8Converter.Length(), utf8Converter.Get());

		Strings.Add(MoveTemp(convertedString));
	}

	// Rebuild deduplication map from loaded strings
	for (int32 i = 0; i < Strings.Num(); ++i)
	{
		DeduplicationMap.Add(Strings[i], static_cast<uint32>(i));
	}

	return true;
}

bool FSGDynamicTextAssetCookManifestStringTable::IsEmpty() const
{
	return Strings.IsEmpty();
}

int32 FSGDynamicTextAssetCookManifestStringTable::Num() const
{
	return Strings.Num();
}

void FSGDynamicTextAssetCookManifestStringTable::Reset()
{
	Strings.Empty();
	DeduplicationMap.Empty();
}
