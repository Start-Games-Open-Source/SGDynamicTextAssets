// Copyright Start Games, Inc. All Rights Reserved.

#include "Core/SGDynamicTextAssetValidationResult.h"

void FSGDynamicTextAssetValidationResult::AddEntry(const FSGDynamicTextAssetValidationEntry& Entry)
{
	switch (Entry.Severity)
	{
		case ESGValidationSeverity::Error:
			{
				Errors.Add(Entry);
				break;
			}
		case ESGValidationSeverity::Warning:
			{
				Warnings.Add(Entry);
				break;
			}
		// ESGValidationSeverity::Info
		default:
			{
				Infos.Add(Entry);
				break;
			}
	}
}

void FSGDynamicTextAssetValidationResult::AddEntry(FSGDynamicTextAssetValidationEntry&& Entry)
{
	switch (Entry.Severity)
	{
		case ESGValidationSeverity::Error:
			{
				Errors.Add(MoveTemp(Entry));
				break;
			}
		case ESGValidationSeverity::Warning:
			{
				Warnings.Add(MoveTemp(Entry));
				break;
			}
		// ESGValidationSeverity::Info
		default:
			{
				Infos.Add(MoveTemp(Entry));
				break;
			}
	}
}

void FSGDynamicTextAssetValidationResult::AddError(const FText& InMessage,
	const FString& InPropertyPath,
	const FText& InSuggestedFix)
{
	Errors.Emplace(FSGDynamicTextAssetValidationEntry(ESGValidationSeverity::Error, InMessage, InPropertyPath, InSuggestedFix));
}

void FSGDynamicTextAssetValidationResult::AddWarning(const FText& InMessage,
	const FString& InPropertyPath,
	const FText& InSuggestedFix)
{
	Warnings.Emplace(FSGDynamicTextAssetValidationEntry(ESGValidationSeverity::Warning, InMessage, InPropertyPath, InSuggestedFix));
}

void FSGDynamicTextAssetValidationResult::AddInfo(const FText& InMessage,
	const FString& InPropertyPath,
	const FText& InSuggestedFix)
{
	Infos.Emplace(FSGDynamicTextAssetValidationEntry(ESGValidationSeverity::Info, InMessage, InPropertyPath, InSuggestedFix));
}

void FSGDynamicTextAssetValidationResult::Append(const FSGDynamicTextAssetValidationResult& Other)
{
	Errors.Append(Other.Errors);
	Warnings.Append(Other.Warnings);
	Infos.Append(Other.Infos);
}

void FSGDynamicTextAssetValidationResult::Append(FSGDynamicTextAssetValidationResult&& Other)
{
	Errors.Append(MoveTemp(Other.Errors));
	Warnings.Append(MoveTemp(Other.Warnings));
	Infos.Append(MoveTemp(Other.Infos));
}

void FSGDynamicTextAssetValidationResult::Reset()
{
	Errors.Empty();
	Warnings.Empty();
	Infos.Empty();
}

FString FSGDynamicTextAssetValidationResult::ToFormattedString() const
{
	FString output;

	auto FormatEntries = [&output](const TArray<FSGDynamicTextAssetValidationEntry>& Entries, const TCHAR* Prefix)
	{
		for (const FSGDynamicTextAssetValidationEntry& entry : Entries)
		{
			output += FString::Printf(TEXT("%s %s"), Prefix, *entry.Message.ToString());

			if (!entry.PropertyPath.IsEmpty())
			{
				output += FString::Printf(TEXT(" (Property: %s)"), *entry.PropertyPath);
			}
			output += TEXT("\n");

			if (!entry.SuggestedFix.IsEmpty())
			{
				output += FString::Printf(TEXT("    Fix: %s\n"), *entry.SuggestedFix.ToString());
			}
		}
	};

	FormatEntries(Errors, TEXT("[ERROR]"));
	FormatEntries(Warnings, TEXT("[WARNING]"));
	FormatEntries(Infos, TEXT("[INFO]"));

	return output;
}
