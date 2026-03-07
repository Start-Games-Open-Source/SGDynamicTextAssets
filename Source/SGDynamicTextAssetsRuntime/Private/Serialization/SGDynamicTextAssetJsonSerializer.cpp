// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/SGDynamicTextAssetJsonSerializer.h"

#include "SGDynamicTextAssetsRuntimeModule.h"
#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Core/SGDynamicTextAssetVersion.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Statics/SGDynamicTextAssetSlateStyles.h"

#if WITH_EDITOR
const FSlateBrush* FSGDynamicTextAssetJsonSerializer::GetIconBrush() const
{
    static const FSlateBrush* icon = FSGDynamicTextAssetSlateStyles::GetBrush(FSGDynamicTextAssetSlateStyles::BRUSH_NAME_JSON);
    return icon;
}
#endif

FString FSGDynamicTextAssetJsonSerializer::GetFileExtension() const
{
    // Using static string to avoid regenerating it everytime its used.
    static const FString extension = ".dta.json";
    return extension;
}

FText FSGDynamicTextAssetJsonSerializer::GetFormatName() const
{
    // Using static text to avoid regenerating it everytime its used.
    static const FText name = INVTEXT("JSON");
    return name;
}

FText FSGDynamicTextAssetJsonSerializer::GetFormatDescription() const
{
#if !UE_BUILD_SHIPPING
    // Using static text to avoid regenerating it everytime its used.
    static const FText description = FText::AsCultureInvariant(TEXT(R"(JSON serialization for dynamic text assets.

Implements ISGDynamicTextAssetSerializer for polymorphic format dispatch
while also providing static utility methods for direct usage with
USGDynamicTextAsset pointers.

Uses Unreal's property reflection system to serialize and deserialize
all UPROPERTY marked fields on USGDynamicTextAsset subclasses.

JSON format uses a metadata wrapper block, for example:
{
  "metadata": {
    "type": "UWeaponData",
    "version": "1.0.0",
    "id": "...",
    "userfacingid": "excalibur"
  },
  "data": { ... properties ... }
}
)"));

    return description;
    // No need for this information to be in shipping builds
#else
    return FText::GetEmpty();
#endif
}

uint32 FSGDynamicTextAssetJsonSerializer::GetSerializerTypeId() const
{
    return TYPE_ID;
}

bool FSGDynamicTextAssetJsonSerializer::SerializeProvider(const ISGDynamicTextAssetProvider* Provider, FString& OutString) const
{
    OutString.Empty();

    if (!Provider)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer: Inputted NULL Provider"));
        return false;
    }

    // Must be a UObject to serialize properties
    const UObject* providerObject = Provider->_getUObject();
    if (!providerObject)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer: Provider is not a valid UObject"));
        return false;
    }

    // Build metadata sub-object containing all identity fields
    TSharedRef<FJsonObject> metadataObject = MakeShared<FJsonObject>();

    // Write Asset Type ID GUID to the type field, fall back to class name if unavailable
    FString typeString;
    if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
    {
        const FSGDynamicTextAssetTypeId typeId = registry->GetTypeIdForClass(providerObject->GetClass());
        if (typeId.IsValid())
        {
            typeString = typeId.ToString();
        }
    }

    if (typeString.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
            TEXT("FSGDynamicTextAssetJsonSerializer::SerializeProvider: No valid Asset Type ID found for class '%s', falling back to class name"),
            *providerObject->GetClass()->GetName());
        typeString = providerObject->GetClass()->GetName();
    }

    metadataObject->SetStringField(KEY_TYPE, typeString);
    metadataObject->SetStringField(KEY_VERSION, Provider->GetVersion().ToString());
    metadataObject->SetStringField(KEY_ID, Provider->GetDynamicTextAssetId().ToString());
    metadataObject->SetStringField(KEY_USER_FACING_ID, Provider->GetUserFacingId());

    // Serialize properties into data block
    TSharedRef<FJsonObject> dataObject = MakeShared<FJsonObject>();

    // Serialize all UPROPERTY fields, delegating filtering and conversion to base class helpers
    for (TFieldIterator<FProperty> propIt(providerObject->GetClass()); propIt; ++propIt)
    {
        FProperty* property = *propIt;

        if (!ShouldSerializeProperty(property))
        {
            continue;
        }

        const void* valuePtr = property->ContainerPtrToValuePtr<void>(providerObject);
        TSharedPtr<FJsonValue> jsonValue = SerializePropertyToValue(property, valuePtr);

        if (jsonValue.IsValid())
        {
            dataObject->SetField(property->GetName(), jsonValue);
        }
        else
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer: Failed to serialize property(%s) on Provider(%s)"),
                *property->GetName(), *GetNameSafe(providerObject));
        }
    }

    // Build root object: metadata wrapper + data block
    TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
    rootObject->SetObjectField(KEY_METADATA, metadataObject);
    rootObject->SetObjectField(KEY_DATA, dataObject);

    // Convert to string with pretty printing
    TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&OutString);
    if (!FJsonSerializer::Serialize(rootObject, writer))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer: Failed to serialize JSON for Provider(%s)"), *GetNameSafe(providerObject));
        return false;
    }

    return true;
}

bool FSGDynamicTextAssetJsonSerializer::DeserializeProvider(const FString& InString, ISGDynamicTextAssetProvider* OutProvider, bool& bOutMigrated) const
{
    // Initialize migration output flag
    bOutMigrated = false;

    if (!OutProvider)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer: Inputted NULL OutProvider"));
        return false;
    }

    // Must be a UObject to deserialize properties
    UObject* providerObject = OutProvider->_getUObject();
    if (!providerObject)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer: Provider is not a valid UObject"));
        return false;
    }

    if (InString.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer: Inputted EMPTY JsonString"));
        return false;
    }

    // Parse JSON
    TSharedPtr<FJsonObject> rootObject;
    TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(InString);

    if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer: Failed to parse JSON string"));
        return false;
    }

    // Extract metadata sub-object
    const TSharedPtr<FJsonObject>* metadataObjectPtr;
    if (!rootObject->TryGetObjectField(KEY_METADATA, metadataObjectPtr) || !metadataObjectPtr->IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer: JSON missing KEY_METADATA(%s) block"), *KEY_METADATA);
        return false;
    }

    const TSharedPtr<FJsonObject>& metadataObject = *metadataObjectPtr;

    // Validate class type matches — type field may contain a GUID (new format) or class name (legacy)
    FString typeFieldValue;
    if (metadataObject->TryGetStringField(KEY_TYPE, typeFieldValue))
    {
        FSGDynamicTextAssetTypeId fileTypeId = FSGDynamicTextAssetTypeId::FromString(typeFieldValue);
        if (fileTypeId.IsValid())
        {
            // New format: resolve Asset Type ID GUID to class and validate
            if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
            {
                if (const UClass* resolvedClass = registry->ResolveClassForTypeId(fileTypeId))
                {
                    if (resolvedClass != providerObject->GetClass())
                    {
                        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                            TEXT("FSGDynamicTextAssetJsonSerializer: JSON Asset Type ID(%s) resolves to class(%s) but OutProvider is class(%s)"),
                            *typeFieldValue, *resolvedClass->GetName(), *providerObject->GetClass()->GetName());
                        // Continue anyway, might be loading into a parent class
                    }
                }
                else
                {
                    UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                        TEXT("FSGDynamicTextAssetJsonSerializer: Could not resolve Asset Type ID(%s) to a class"),
                        *typeFieldValue);
                }
            }
        }
        else
        {
            // Legacy format: compare class name directly
            if (typeFieldValue != providerObject->GetClass()->GetName())
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                    TEXT("FSGDynamicTextAssetJsonSerializer: JSON typeName(%s) does not match OutProvider(%s)"),
                    *typeFieldValue, *providerObject->GetClass()->GetName());
                // Continue anyway, might be loading into a parent class
            }
        }
    }

    // Extract ID
    FString idString;
    if (metadataObject->TryGetStringField(KEY_ID, idString))
    {
        FSGDynamicTextAssetId id;
        if (id.ParseString(idString))
        {
            OutProvider->SetDynamicTextAssetId(id);
        }
    }

    // Extract UserFacingId
    FString userFacingId;
    if (metadataObject->TryGetStringField(KEY_USER_FACING_ID, userFacingId))
    {
        OutProvider->SetUserFacingId(userFacingId);
    }

    // Extract version
    FSGDynamicTextAssetVersion fileVersion;
    FString versionString;
    if (metadataObject->TryGetStringField(KEY_VERSION, versionString))
    {
        fileVersion = FSGDynamicTextAssetVersion::ParseFromString(versionString);
        OutProvider->SetVersion(fileVersion);
    }

    // Check for version migration
    const FSGDynamicTextAssetVersion currentVersion = OutProvider->GetCurrentVersion();
    if (fileVersion.Major < currentVersion.Major)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
            TEXT("FSGDynamicTextAssetJsonSerializer: Migration required for Provider(%s): file version %s -> class version %s"),
            *OutProvider->GetDynamicTextAssetId().ToString(), *fileVersion.ToString(), *currentVersion.ToString());

        if (!OutProvider->MigrateFromVersion(fileVersion, currentVersion, rootObject))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Error,
                TEXT("FSGDynamicTextAssetJsonSerializer: Migration failed for Provider(%s) from version %s"),
                *OutProvider->GetDynamicTextAssetId().ToString(), *fileVersion.ToString());
            return false;
        }

        // Update version to current class version
        OutProvider->SetVersion(currentVersion);

        bOutMigrated = true;

        UE_LOG(LogSGDynamicTextAssetsRuntime, Log,
            TEXT("FSGDynamicTextAssetJsonSerializer: Migration succeeded for Provider(%s): version now %s"),
            *OutProvider->GetDynamicTextAssetId().ToString(), *OutProvider->GetVersion().ToString());
    }
    else if (fileVersion.Major > currentVersion.Major)
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
            TEXT("FSGDynamicTextAssetJsonSerializer: Provider(%s) has file major version %d which is newer than class major version %d. Loading with best-effort."),
            *OutProvider->GetDynamicTextAssetId().ToString(), fileVersion.Major, currentVersion.Major);
    }

    // Get data block
    const TSharedPtr<FJsonObject>* dataObject;
    if (!rootObject->TryGetObjectField(KEY_DATA, dataObject) || !dataObject->IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer: JSON missing KEY_DATA(%s) field"), *KEY_DATA);
        return false;
    }

    // Deserialize properties, delegating filtering and conversion to base class helpers
    for (TFieldIterator<FProperty> propIt(providerObject->GetClass()); propIt; ++propIt)
    {
        FProperty* property = *propIt;

        if (!ShouldSerializeProperty(property))
        {
            continue;
        }

        // Skip if not in JSON
        if (!(*dataObject)->HasField(property->GetName()))
        {
            continue;
        }

        void* valuePtr = property->ContainerPtrToValuePtr<void>(providerObject);
        TSharedPtr<FJsonValue> jsonValue = (*dataObject)->TryGetField(property->GetName());

        if (jsonValue.IsValid())
        {
            if (!DeserializeValueToProperty(jsonValue, property, valuePtr))
            {
                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("FSGDynamicTextAssetJsonSerializer: Failed to deserialize property(%s) on OutProvider(%s)"),
                    *property->GetName(), *GetNameSafe(providerObject));
            }
        }
    }

    return true;
}

bool FSGDynamicTextAssetJsonSerializer::ValidateStructure(const FString& InString, FString& OutErrorMessage) const
{
    if (InString.IsEmpty())
    {
        OutErrorMessage = TEXT("Inputted EMPTY JsonString");
        return false;
    }

    TSharedPtr<FJsonObject> rootObject;
    TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(InString);

    if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
    {
        OutErrorMessage = TEXT("Failed to parse JSON");
        return false;
    }

    // Check for metadata wrapper
    const TSharedPtr<FJsonObject>* metadataObjectPtr;
    if (!rootObject->TryGetObjectField(KEY_METADATA, metadataObjectPtr) || !metadataObjectPtr->IsValid())
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required block KEY_METADATA(%s)"), *KEY_METADATA);
        return false;
    }

    // Check type inside metadata
    if (!(*metadataObjectPtr)->HasField(KEY_TYPE))
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required field KEY_TYPE(%s) inside metadata"), *KEY_TYPE);
        return false;
    }

    // Check data at root
    if (!rootObject->HasField(KEY_DATA))
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required field KEY_DATA(%s)"), *KEY_DATA);
        return false;
    }

    return true;
}

bool FSGDynamicTextAssetJsonSerializer::ExtractMetadata(const FString& InString, FSGDynamicTextAssetId& OutId, FString& OutClassName, FString& OutUserFacingId, FString& OutVersion, FSGDynamicTextAssetTypeId& OutAssetTypeId) const
{
    OutAssetTypeId.Invalidate();
    OutClassName.Empty();

    TSharedPtr<FJsonObject> rootObject;
    TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(InString);

    if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
    {
        return false;
    }

    // Extract from metadata sub-object
    const TSharedPtr<FJsonObject>* metadataObjectPtr;
    if (!rootObject->TryGetObjectField(KEY_METADATA, metadataObjectPtr) || !metadataObjectPtr->IsValid())
    {
        return false;
    }

    const TSharedPtr<FJsonObject>& metadataObject = *metadataObjectPtr;

    // Type field may contain a GUID (new format) or class name (legacy)
    FString typeFieldValue;
    if (metadataObject->TryGetStringField(KEY_TYPE, typeFieldValue))
    {
        FSGDynamicTextAssetTypeId parsedTypeId = FSGDynamicTextAssetTypeId::FromString(typeFieldValue);
        if (parsedTypeId.IsValid())
        {
            // New format: store TypeId and resolve class name from registry
            OutAssetTypeId = parsedTypeId;

            if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
            {
                if (const UClass* resolvedClass = registry->ResolveClassForTypeId(parsedTypeId))
                {
                    OutClassName = resolvedClass->GetName();
                }
            }
        }
        else
        {
            // Legacy format: treat value as class name directly
            OutClassName = typeFieldValue;
        }
    }

    FString idString;
    if (metadataObject->TryGetStringField(KEY_ID, idString))
    {
        OutId.ParseString(idString);
    }

    metadataObject->TryGetStringField(KEY_VERSION, OutVersion);
    metadataObject->TryGetStringField(KEY_USER_FACING_ID, OutUserFacingId);

    return OutAssetTypeId.IsValid() || !OutClassName.IsEmpty();
}

bool FSGDynamicTextAssetJsonSerializer::UpdateFieldsInPlace(FString& InOutContents, const TMap<FString, FString>& FieldUpdates) const
{
    if (InOutContents.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer::UpdateFieldsInPlace: Inputted EMPTY InOutContents"));
        return false;
    }

    if (FieldUpdates.IsEmpty())
    {
        return false;
    }

    // Parse the JSON string
    TSharedPtr<FJsonObject> rootObject;
    TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(InOutContents);
    if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer::UpdateFieldsInPlace: Failed to parse JSON"));
        return false;
    }

    // Get metadata sub-object
    const TSharedPtr<FJsonObject>* metadataObjectPtr;
    if (!rootObject->TryGetObjectField(KEY_METADATA, metadataObjectPtr) || !metadataObjectPtr->IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer::UpdateFieldsInPlace: Missing metadata block"));
        return false;
    }

    TSharedPtr<FJsonObject> metadataObject = *metadataObjectPtr;

    // Apply each field update to the metadata block
    bool bAnyUpdated = false;
    for (const TPair<FString, FString>& update : FieldUpdates)
    {
        if (metadataObject->HasField(update.Key))
        {
            metadataObject->SetStringField(update.Key, update.Value);
            bAnyUpdated = true;
        }
    }

    if (!bAnyUpdated)
    {
        return false;
    }

    // Re-serialize back to string
    InOutContents.Empty();
    TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&InOutContents);
    if (!FJsonSerializer::Serialize(rootObject.ToSharedRef(), writer))
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer::UpdateFieldsInPlace: Failed to re-serialize JSON after field update"));
        return false;
    }

    return true;
}

FString FSGDynamicTextAssetJsonSerializer::GetDefaultFileContent(const UClass* DynamicTextAssetClass, const FSGDynamicTextAssetId& Id, const FString& UserFacingId) const
{
    // Write Asset Type ID GUID to the type field, fall back to class name if unavailable
    FString typeString;
    if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
    {
        const FSGDynamicTextAssetTypeId typeId = registry->GetTypeIdForClass(DynamicTextAssetClass);
        if (typeId.IsValid())
        {
            typeString = typeId.ToString();
        }
    }

    if (typeString.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
            TEXT("FSGDynamicTextAssetJsonSerializer::GetDefaultFileContent: No valid Asset Type ID found for class '%s', falling back to class name"),
            *GetNameSafe(DynamicTextAssetClass));
        typeString = GetNameSafe(DynamicTextAssetClass);
    }

    return FString::Printf(
        TEXT("{\n    \"%s\": {\n        \"%s\": \"%s\",\n        \"%s\": \"1.0.0\",\n        \"%s\": \"%s\",\n        \"%s\": \"%s\"\n    },\n    \"%s\": {}\n}"),
        *KEY_METADATA,
        *KEY_TYPE,
        *typeString,
        *KEY_VERSION,
        *KEY_ID,
        *Id.ToString(),
        *KEY_USER_FACING_ID,
        *UserFacingId,
        *KEY_DATA
    );
}
