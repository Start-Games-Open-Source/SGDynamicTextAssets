// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SGDynamicTextAssetValidationResult.generated.h"

/**
 * Severity level for a validation entry.
 */
UENUM(BlueprintType)
enum class ESGValidationSeverity : uint8
{
    /** Informational note, does not prevent saving */
    Info        UMETA(DisplayName = "Info"),

    /** Non-critical issue, does not prevent saving */
    Warning     UMETA(DisplayName = "Warning"),

    /** Critical issue, prevents saving */
    Error       UMETA(DisplayName = "Error")
};

/**
 * A single validation entry produced during dynamic text asset validation.
 *
 * Contains the message, severity, source property path, and an
 * optional fix suggestion. Entries are collected inside
 * FSGDynamicTextAssetValidationResult.
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetValidationEntry
{
    GENERATED_BODY()
public:

    /** Default constructor. Severity defaults to Error. */
    FSGDynamicTextAssetValidationEntry() = default;

    /**
     * Constructs an entry with severity and message.
     *
     * @param InSeverity The severity level of this entry
     * @param InMessage Human-readable description of the issue
     */
    FSGDynamicTextAssetValidationEntry(ESGValidationSeverity&& InSeverity,
                                 const FText& InMessage)
        : Message(InMessage)
        , Severity(InSeverity)
    { }

    /**
     * Constructs an entry with severity, message, and property path.
     *
     * @param InSeverity The severity level of this entry
     * @param InMessage Human-readable description of the issue
     * @param InPropertyPath The property that caused the issue
     */
    FSGDynamicTextAssetValidationEntry(ESGValidationSeverity&& InSeverity,
                                 const FText& InMessage,
                                 const FString& InPropertyPath)
        : Message(InMessage)
        , PropertyPath(InPropertyPath)
        , Severity(InSeverity)
    { }

    /**
     * Constructs an entry with all fields.
     *
     * @param InSeverity The severity level of this entry
     * @param InMessage Human-readable description of the issue
     * @param InPropertyPath The property that caused the issue
     * @param InSuggestedFix Optional suggestion for how to fix the issue
     */
    FSGDynamicTextAssetValidationEntry(ESGValidationSeverity&& InSeverity,
                                 const FText& InMessage,
                                 const FString& InPropertyPath,
                                 const FText& InSuggestedFix)
        : Message(InMessage)
        , PropertyPath(InPropertyPath)
        , SuggestedFix(InSuggestedFix)
        , Severity(InSeverity)
    { }

    /** Human-readable description of the validation issue */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText Message;

    /** The property path that caused the issue (e.g. "Damage", "WeaponRefs[0]") */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString PropertyPath;

    /** Optional suggestion for how to fix the issue */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText SuggestedFix;

    /** Severity level of this validation entry */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ESGValidationSeverity Severity = ESGValidationSeverity::Error;

    /** Returns true if this entry represents a blocking error */
    bool IsError() const { return Severity == ESGValidationSeverity::Error; }

    /** Returns true if this entry represents a warning */
    bool IsWarning() const { return Severity == ESGValidationSeverity::Warning; }

    /** Returns true if this entry represents an informational note */
    bool IsInfo() const { return Severity == ESGValidationSeverity::Info; }
};

/**
 * Container for dynamic text asset validation results.
 *
 * Holds arrays of errors, warnings, and informational entries.
 * Provides convenience methods for adding entries, querying state,
 * and building formatted messages for display.
 *
 * Supports move semantics for efficient transfer of validation data.
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGDynamicTextAssetValidationResult
{
    GENERATED_BODY()
public:

    /** Default constructor */
    FSGDynamicTextAssetValidationResult() = default;

    /** Move constructor */
    FSGDynamicTextAssetValidationResult(FSGDynamicTextAssetValidationResult&& Other) noexcept
        : Errors(MoveTemp(Other.Errors))
        , Warnings(MoveTemp(Other.Warnings))
        , Infos(MoveTemp(Other.Infos))
    { }

    /** Move assignment operator */
    FSGDynamicTextAssetValidationResult& operator=(FSGDynamicTextAssetValidationResult&& Other) noexcept
    {
        if (this != &Other)
        {
            Errors = MoveTemp(Other.Errors);
            Warnings = MoveTemp(Other.Warnings);
            Infos = MoveTemp(Other.Infos);
        }
        return *this;
    }

    /** Copy constructor */
    FSGDynamicTextAssetValidationResult(const FSGDynamicTextAssetValidationResult&) = default;

    /** Copy assignment operator */
    FSGDynamicTextAssetValidationResult& operator=(const FSGDynamicTextAssetValidationResult&) = default;

    /**
     * Adds an entry, routing it to the appropriate array by severity.
     *
     * @param Entry The validation entry to add
     */
    void AddEntry(const FSGDynamicTextAssetValidationEntry& Entry);

    /** Adds an entry via move semantics. */
    void AddEntry(FSGDynamicTextAssetValidationEntry&& Entry);

    /**
     * Adds an error entry.
     *
     * @param InMessage Human-readable description of the error
     * @param InPropertyPath The property that caused the error
     * @param InSuggestedFix Optional suggestion for how to fix the error
     */
    void AddError(const FText& InMessage,
                  const FString& InPropertyPath = FString(),
                  const FText& InSuggestedFix = FText());

    /**
     * Adds a warning entry.
     *
     * @param InMessage Human-readable description of the warning
     * @param InPropertyPath The property that caused the warning
     * @param InSuggestedFix Optional suggestion for how to fix the warning
     */
    void AddWarning(const FText& InMessage,
                    const FString& InPropertyPath = FString(),
                    const FText& InSuggestedFix = FText());

    /**
     * Adds an informational entry.
     *
     * @param InMessage Human-readable description of the note
     * @param InPropertyPath The property related to the note
     * @param InSuggestedFix Optional suggestion
     */
    void AddInfo(const FText& InMessage,
                 const FString& InPropertyPath = FString(),
                 const FText& InSuggestedFix = FText());

    /**
     * Appends all entries from another result into this one.
     *
     * @param Other The validation result to merge from
     */
    void Append(const FSGDynamicTextAssetValidationResult& Other);

    /** Appends all entries from another result via move semantics. */
    void Append(FSGDynamicTextAssetValidationResult&& Other);

    /** Returns true if there are any error-severity entries */
    bool HasErrors() const { return !Errors.IsEmpty(); }

    /** Returns true if there are any warning-severity entries */
    bool HasWarnings() const { return !Warnings.IsEmpty(); }

    /** Returns true if there are any informational entries */
    bool HasInfos() const { return !Infos.IsEmpty(); }

    /** Returns true if there are no entries of any severity */
    bool IsEmpty() const { return Errors.IsEmpty() && Warnings.IsEmpty() && Infos.IsEmpty(); }

    /** Returns true if validation passed (no errors). Warnings and infos do not cause failure. */
    bool IsValid() const { return !HasErrors(); }

    /** Returns the total number of entries across all severities */
    int32 GetTotalCount() const { return Errors.Num() + Warnings.Num() + Infos.Num(); }

    /** Removes all entries */
    void Reset();

    /**
     * Builds a formatted string of all entries for display.
     * Each entry is prefixed with its severity and includes
     * property path and suggested fix when available.
     */
    FString ToFormattedString() const;

    /** All error-severity entries (blocking issues) */
    UPROPERTY(BlueprintReadOnly)
    TArray<FSGDynamicTextAssetValidationEntry> Errors;

    /** All warning-severity entries (non-blocking issues) */
    UPROPERTY(BlueprintReadOnly)
    TArray<FSGDynamicTextAssetValidationEntry> Warnings;

    /** All informational entries */
    UPROPERTY(BlueprintReadOnly)
    TArray<FSGDynamicTextAssetValidationEntry> Infos;
};
