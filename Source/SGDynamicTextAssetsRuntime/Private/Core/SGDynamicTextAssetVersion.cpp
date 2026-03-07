// Copyright Start Games, Inc. All Rights Reserved.

#include "Core/SGDynamicTextAssetVersion.h"

bool FSGDynamicTextAssetVersion::IsValid() const
{
	return Major >= 1 && Minor >= 0 && Patch >= 0;
}

FString FSGDynamicTextAssetVersion::ToString() const
{
	return FString::Printf(TEXT("%d.%d.%d"), Major, Minor, Patch);
}

FSGDynamicTextAssetVersion FSGDynamicTextAssetVersion::ParseFromString(const FString& VersionString)
{
	FSGDynamicTextAssetVersion result;
	TArray<FString> parts;
	VersionString.ParseIntoArray(parts, TEXT("."));

	// Major
	if (parts.IsValidIndex(0))
	{
		result.Major = FCString::Atoi(*parts[0]);
	}
	// Minor
	if (parts.IsValidIndex(1))
	{
		result.Minor = FCString::Atoi(*parts[1]);
	}
	// Patch
	if (parts.IsValidIndex(2))
	{
		result.Patch = FCString::Atoi(*parts[2]);
	}

	return result;
}

bool FSGDynamicTextAssetVersion::ParseString(const FString& VersionString)
{
	TArray<FString> parts;
	VersionString.ParseIntoArray(parts, TEXT("."));

	if (parts.Num() != 3)
	{
		return false;
	}

	const int32 parsedMajor = FCString::Atoi(*parts[0]);
	const int32 parsedMinor = FCString::Atoi(*parts[1]);
	const int32 parsedPatch = FCString::Atoi(*parts[2]);

	if (parsedMajor < 1)
	{
		return false;
	}

	Major = parsedMajor;
	Minor = parsedMinor;
	Patch = parsedPatch;
	return true;
}

bool FSGDynamicTextAssetVersion::IsCompatibleWith(const FSGDynamicTextAssetVersion& Other) const
{
	return Major == Other.Major;
}

bool FSGDynamicTextAssetVersion::operator==(const FSGDynamicTextAssetVersion& Other) const
{
	return Major == Other.Major && Minor == Other.Minor && Patch == Other.Patch;
}

bool FSGDynamicTextAssetVersion::operator!=(const FSGDynamicTextAssetVersion& Other) const
{
	return !(*this == Other);
}

bool FSGDynamicTextAssetVersion::operator<(const FSGDynamicTextAssetVersion& Other) const
{
	if (Major != Other.Major)
	{
		return Major < Other.Major;
	}
	if (Minor != Other.Minor)
	{
		return Minor < Other.Minor;
	}
	return Patch < Other.Patch;
}

bool FSGDynamicTextAssetVersion::operator>(const FSGDynamicTextAssetVersion& Other) const
{
	return Other < *this;
}

bool FSGDynamicTextAssetVersion::operator<=(const FSGDynamicTextAssetVersion& Other) const
{
	return !(Other < *this);
}

bool FSGDynamicTextAssetVersion::operator>=(const FSGDynamicTextAssetVersion& Other) const
{
	return !(*this < Other);
}

bool FSGDynamicTextAssetVersion::Serialize(FArchive& Ar)
{
	Ar << Major;
	Ar << Minor;
	Ar << Patch;
	return true;
}

bool FSGDynamicTextAssetVersion::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Serialize(Ar);
	bOutSuccess = true;
	return true;
}

bool FSGDynamicTextAssetVersion::ExportTextItem(FString& ValueStr, const FSGDynamicTextAssetVersion& DefaultValue,
                                          UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	ValueStr += ToString();
	return true;
}

bool FSGDynamicTextAssetVersion::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent,
	FOutputDevice* ErrorText)
{
    FString versionString;
	const TCHAR* start = Buffer;

	// Add digits and dots only
	while (*Buffer != TEXT('\0') && ((FChar::IsDigit(*Buffer)) || *Buffer != TEXT('.')))
	{
		versionString.AppendChar(*Buffer);
		++Buffer;
	}

	if (versionString.IsEmpty())
	{
		// Restore postion on fail
		Buffer = start;
		return false;
	}

	if (ParseString(versionString))
	{
		return true;
	}

	// Restore postion on fail
	Buffer = start;
	return false;
}