// Copyright Start Games, Inc. All Rights Reserved.

#include "Serialization/SGDynamicTextAssetJsonSerializer.h"

#include "SGDynamicTextAssetLogs.h"
#include "Core/SGDynamicTextAsset.h"
#include "Core/SGDynamicTextAssetTypeId.h"
#include "Core/SGDynamicTextAssetVersion.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Internationalization/Regex.h"
#include "Statics/SGDynamicTextAssetSlateStyles.h"
#include "UObject/UnrealType.h"

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

JSON format uses a metadata wrapper block(sgFileInformation), for example:
{
  "sgFileInformation": {
    "type": "UWeaponData",
    "version": "1.0.0",
    "id": "...",
    "userfacingid": "excalibur",
    "fileFormatVersion": "1.5.2"
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

FSGDynamicTextAssetVersion FSGDynamicTextAssetJsonSerializer::GetFileFormatVersion() const
{
    static const FSGDynamicTextAssetVersion version(2, 0, 0);
    return version;
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
    metadataObject->SetStringField(KEY_FILE_FORMAT_VERSION, GetFileFormatVersion().ToString());

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

        // Handle instanced object properties.
        // CPF_InstancedReference is on the FObjectProperty itself (single) or on
        // the container's inner/element/value property (TArray, TSet, TMap).

        // Single instanced object: UPROPERTY(Instanced) TObjectPtr<UMyClass>
        if (IsInstancedObjectProperty(property))
        {
            if (const FObjectProperty* objectProp = CastField<FObjectProperty>(property))
            {
                const UObject* subObject = objectProp->GetObjectPropertyValue(valuePtr);
                TSharedPtr<FJsonValue> jsonValue = SerializeInstancedObjectToValue(objectProp, subObject);
                if (jsonValue.IsValid())
                {
                    dataObject->SetField(property->GetName(), jsonValue);
                }
                continue;
            }
        }

        // Array of instanced objects: UPROPERTY(Instanced) TArray<TObjectPtr<UMyClass>>
        if (const FArrayProperty* arrayProp = CastField<FArrayProperty>(property))
        {
            if (arrayProp->Inner && IsInstancedObjectProperty(arrayProp->Inner))
            {
                if (const FObjectProperty* innerObjProp = CastField<FObjectProperty>(arrayProp->Inner))
                {
                    FScriptArrayHelper arrayHelper(arrayProp, valuePtr);
                    TArray<TSharedPtr<FJsonValue>> jsonArray;
                    jsonArray.Reserve(arrayHelper.Num());

                    for (int32 i = 0; i < arrayHelper.Num(); ++i)
                    {
                        const UObject* element = innerObjProp->GetObjectPropertyValue(arrayHelper.GetRawPtr(i));
                        jsonArray.Add(SerializeInstancedObjectToValue(innerObjProp, element));
                    }

                    dataObject->SetField(property->GetName(), MakeShared<FJsonValueArray>(jsonArray));
                    continue;
                }
            }
        }

        // TSet of instanced objects: UPROPERTY(Instanced) TSet<TObjectPtr<UMyClass>>
        if (const FSetProperty* setProp = CastField<FSetProperty>(property))
        {
            if (setProp->ElementProp && IsInstancedObjectProperty(setProp->ElementProp))
            {
                if (const FObjectProperty* elemObjProp = CastField<FObjectProperty>(setProp->ElementProp))
                {
                    FScriptSetHelper setHelper(setProp, valuePtr);
                    TArray<TSharedPtr<FJsonValue>> jsonArray;
                    jsonArray.Reserve(setHelper.Num());

                    for (int32 i = 0; i < setHelper.GetMaxIndex(); ++i)
                    {
                        if (setHelper.IsValidIndex(i))
                        {
                            const UObject* element = elemObjProp->GetObjectPropertyValue(setHelper.GetElementPtr(i));
                            jsonArray.Add(SerializeInstancedObjectToValue(elemObjProp, element));
                        }
                    }

                    dataObject->SetField(property->GetName(), MakeShared<FJsonValueArray>(jsonArray));
                    continue;
                }
            }
        }

        // TMap with instanced object values: UPROPERTY(Instanced) TMap<FString, TObjectPtr<UMyClass>>
        if (const FMapProperty* mapProp = CastField<FMapProperty>(property))
        {
            if (mapProp->ValueProp && IsInstancedObjectProperty(mapProp->ValueProp))
            {
                if (const FObjectProperty* valueObjProp = CastField<FObjectProperty>(mapProp->ValueProp))
                {
                    FScriptMapHelper mapHelper(mapProp, valuePtr);
                    TSharedRef<FJsonObject> mapObject = MakeShared<FJsonObject>();

                    for (int32 i = 0; i < mapHelper.Num(); ++i)
                    {
                        if (mapHelper.IsValidIndex(i))
                        {
                            FString keyString;
                            mapProp->KeyProp->ExportTextItem_Direct(keyString, mapHelper.GetKeyPtr(i), nullptr, nullptr, PPF_None);

                            const UObject* valueObj = valueObjProp->GetObjectPropertyValue(mapHelper.GetValuePtr(i));
                            mapObject->SetField(keyString, SerializeInstancedObjectToValue(valueObjProp, valueObj));
                        }
                    }

                    dataObject->SetField(property->GetName(), MakeShared<FJsonValueObject>(mapObject));
                    continue;
                }
            }
        }

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

    // Extract and serialize asset bundle metadata from soft reference properties
    FSGDynamicTextAssetBundleData bundleData;
    bundleData.ExtractFromObject(providerObject);

    TSharedPtr<FJsonObject> bundlesObject;
    if (bundleData.HasBundles())
    {
        bundlesObject = MakeShared<FJsonObject>();

        for (const FSGDynamicTextAssetBundle& bundle : bundleData.Bundles)
        {
            TArray<TSharedPtr<FJsonValue>> entryArray;
            entryArray.Reserve(bundle.Entries.Num());

            for (const FSGDynamicTextAssetBundleEntry& entry : bundle.Entries)
            {
                TSharedRef<FJsonObject> entryObj = MakeShared<FJsonObject>();
                entryObj->SetStringField(TEXT("property"), entry.PropertyName.ToString());
                entryObj->SetStringField(TEXT("path"), entry.AssetPath.ToString());
                entryArray.Add(MakeShared<FJsonValueObject>(entryObj));
            }

            bundlesObject->SetField(bundle.BundleName.ToString(), MakeShared<FJsonValueArray>(entryArray));
        }
    }

    // Build root object: metadata wrapper + data block + optional bundles
    TSharedRef<FJsonObject> rootObject = MakeShared<FJsonObject>();
    rootObject->SetObjectField(KEY_FILE_INFORMATION, metadataObject);
    rootObject->SetObjectField(KEY_DATA, dataObject);

    if (bundlesObject.IsValid())
    {
        rootObject->SetObjectField(KEY_SGDT_ASSET_BUNDLES, bundlesObject.ToSharedRef());
    }

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
    if (!rootObject->TryGetObjectField(KEY_FILE_INFORMATION, metadataObjectPtr))
    {
        rootObject->TryGetObjectField(KEY_METADATA_LEGACY, metadataObjectPtr);
    }
    if (!metadataObjectPtr || !metadataObjectPtr->IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer: JSON missing '%s' (or legacy '%s') block"), *KEY_FILE_INFORMATION, *KEY_METADATA_LEGACY);
        return false;
    }

    const TSharedPtr<FJsonObject>& metadataObject = *metadataObjectPtr;

    // Validate class type matches  - type field may contain a GUID (new format) or class name (legacy)
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

    // Extract file format version (missing = 1.0.0 for pre-format-version files)
    FSGDynamicTextAssetVersion fileFormatVersion(1, 0, 0);
    FString formatVersionString;
    if (metadataObject->TryGetStringField(KEY_FILE_FORMAT_VERSION, formatVersionString))
    {
        fileFormatVersion = FSGDynamicTextAssetVersion::ParseFromString(formatVersionString);
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Verbose,
        TEXT("FSGDynamicTextAssetJsonSerializer: File format version: %s (serializer current: %s)"),
        *fileFormatVersion.ToString(), *GetFileFormatVersion().ToString());

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

        if (!jsonValue.IsValid())
        {
            continue;
        }

        // Handle instanced object properties.
        // CPF_InstancedReference is on the FObjectProperty itself (single) or on
        // the container's inner/element/value property (TArray, TSet, TMap).

        // Single instanced object
        if (IsInstancedObjectProperty(property))
        {
            if (const FObjectProperty* objectProp = CastField<FObjectProperty>(property))
            {
                if (!DeserializeValueToInstancedObject(jsonValue, objectProp, valuePtr, providerObject))
                {
                    UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                        TEXT("FSGDynamicTextAssetJsonSerializer: Failed to deserialize instanced property(%s) on OutProvider(%s)"),
                        *property->GetName(), *GetNameSafe(providerObject));
                }
                continue;
            }
        }

        // Array of instanced objects
        if (const FArrayProperty* arrayProp = CastField<FArrayProperty>(property))
        {
            if (arrayProp->Inner && IsInstancedObjectProperty(arrayProp->Inner))
            {
                if (const FObjectProperty* innerObjProp = CastField<FObjectProperty>(arrayProp->Inner))
                {
                    const TArray<TSharedPtr<FJsonValue>>* jsonArray = nullptr;
                    if (jsonValue->TryGetArray(jsonArray) && jsonArray)
                    {
                        FScriptArrayHelper arrayHelper(arrayProp, valuePtr);
                        arrayHelper.Resize(jsonArray->Num());

                        for (int32 i = 0; i < jsonArray->Num(); ++i)
                        {
                            if (!DeserializeValueToInstancedObject((*jsonArray)[i], innerObjProp, arrayHelper.GetRawPtr(i), providerObject))
                            {
                                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                                    TEXT("FSGDynamicTextAssetJsonSerializer: Failed to deserialize instanced array element [%d] of property(%s) on OutProvider(%s)"),
                                    i, *property->GetName(), *GetNameSafe(providerObject));
                            }
                        }
                    }
                    continue;
                }
            }
        }

        // TSet of instanced objects
        if (const FSetProperty* setProp = CastField<FSetProperty>(property))
        {
            if (setProp->ElementProp && IsInstancedObjectProperty(setProp->ElementProp))
            {
                if (const FObjectProperty* elemObjProp = CastField<FObjectProperty>(setProp->ElementProp))
                {
                    const TArray<TSharedPtr<FJsonValue>>* jsonArray = nullptr;
                    if (jsonValue->TryGetArray(jsonArray) && jsonArray)
                    {
                        FScriptSetHelper setHelper(setProp, valuePtr);
                        setHelper.EmptyElements();

                        for (int32 i = 0; i < jsonArray->Num(); ++i)
                        {
                            const int32 newIndex = setHelper.AddDefaultValue_Invalid_NeedsRehash();
                            if (!DeserializeValueToInstancedObject((*jsonArray)[i], elemObjProp, setHelper.GetElementPtr(newIndex), providerObject))
                            {
                                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                                    TEXT("FSGDynamicTextAssetJsonSerializer: Failed to deserialize instanced set element [%d] of property(%s) on OutProvider(%s)"),
                                    i, *property->GetName(), *GetNameSafe(providerObject));
                            }
                        }

                        setHelper.Rehash();
                    }
                    continue;
                }
            }
        }

        // TMap with instanced object values
        if (const FMapProperty* mapProp = CastField<FMapProperty>(property))
        {
            if (mapProp->ValueProp && IsInstancedObjectProperty(mapProp->ValueProp))
            {
                if (const FObjectProperty* valueObjProp = CastField<FObjectProperty>(mapProp->ValueProp))
                {
                    const TSharedPtr<FJsonObject>* mapObjectPtr = nullptr;
                    if (jsonValue->TryGetObject(mapObjectPtr) && mapObjectPtr && mapObjectPtr->IsValid())
                    {
                        FScriptMapHelper mapHelper(mapProp, valuePtr);
                        mapHelper.EmptyValues();

                        for (const TPair<FString, TSharedPtr<FJsonValue>>& pair : (*mapObjectPtr)->Values)
                        {
                            const int32 newIndex = mapHelper.AddDefaultValue_Invalid_NeedsRehash();
                            uint8* keyPtr = mapHelper.GetKeyPtr(newIndex);
                            mapProp->KeyProp->ImportText_Direct(*pair.Key, keyPtr, nullptr, PPF_None);

                            uint8* valPtr = mapHelper.GetValuePtr(newIndex);
                            if (!DeserializeValueToInstancedObject(pair.Value, valueObjProp, valPtr, providerObject))
                            {
                                UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
                                    TEXT("FSGDynamicTextAssetJsonSerializer: Failed to deserialize instanced map value for key '%s' of property(%s)"),
                                    *pair.Key, *property->GetName());
                            }
                        }

                        mapHelper.Rehash();
                    }
                    continue;
                }
            }
        }

        if (!DeserializeValueToProperty(jsonValue, property, valuePtr))
        {
            UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("FSGDynamicTextAssetJsonSerializer: Failed to deserialize property(%s) on OutProvider(%s)"),
                *property->GetName(), *GetNameSafe(providerObject));
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
    if (!rootObject->TryGetObjectField(KEY_FILE_INFORMATION, metadataObjectPtr))
    {
        rootObject->TryGetObjectField(KEY_METADATA_LEGACY, metadataObjectPtr);
    }
    if (!metadataObjectPtr || !metadataObjectPtr->IsValid())
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required block '%s' (or legacy '%s')"), *KEY_FILE_INFORMATION, *KEY_METADATA_LEGACY);
        return false;
    }

    // Check type inside metadata
    if (!(*metadataObjectPtr)->HasField(KEY_TYPE))
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required field KEY_TYPE(%s) inside metadata"), *KEY_TYPE);
        return false;
    }

    // If fileFormatVersion is present, validate it is a parseable version string
    FString formatVersionStr;
    if ((*metadataObjectPtr)->TryGetStringField(KEY_FILE_FORMAT_VERSION, formatVersionStr))
    {
        FSGDynamicTextAssetVersion parsedVersion = FSGDynamicTextAssetVersion::ParseFromString(formatVersionStr);
        if (!parsedVersion.IsValid())
        {
            OutErrorMessage = FString::Printf(
                TEXT("Field KEY_FILE_FORMAT_VERSION(%s) has invalid version string: %s"),
                *KEY_FILE_FORMAT_VERSION, *formatVersionStr);
            return false;
        }
    }

    // Check data at root
    if (!rootObject->HasField(KEY_DATA))
    {
        OutErrorMessage = FString::Printf(TEXT("Missing required field KEY_DATA(%s)"), *KEY_DATA);
        return false;
    }

    return true;
}

bool FSGDynamicTextAssetJsonSerializer::ExtractMetadata(const FString& InString, FSGDynamicTextAssetFileMetadata& OutMetadata) const
{
    OutMetadata = FSGDynamicTextAssetFileMetadata();
    OutMetadata.SerializerTypeId = TYPE_ID;

    TSharedPtr<FJsonObject> rootObject;
    TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(InString);

    if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
    {
        return false;
    }

    // Extract from metadata sub-object
    const TSharedPtr<FJsonObject>* metadataObjectPtr;
    if (!rootObject->TryGetObjectField(KEY_FILE_INFORMATION, metadataObjectPtr))
    {
        rootObject->TryGetObjectField(KEY_METADATA_LEGACY, metadataObjectPtr);
    }
    if (!metadataObjectPtr || !metadataObjectPtr->IsValid())
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
            OutMetadata.AssetTypeId = parsedTypeId;

            if (const USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
            {
                if (const UClass* resolvedClass = registry->ResolveClassForTypeId(parsedTypeId))
                {
                    OutMetadata.ClassName = resolvedClass->GetName();
                }
            }
        }
        else
        {
            OutMetadata.ClassName = typeFieldValue;
        }
    }

    FString idString;
    if (metadataObject->TryGetStringField(KEY_ID, idString))
    {
        OutMetadata.Id.ParseString(idString);
    }

    FString versionString;
    if (metadataObject->TryGetStringField(KEY_VERSION, versionString))
    {
        OutMetadata.Version = FSGDynamicTextAssetVersion::ParseFromString(versionString);
    }

    metadataObject->TryGetStringField(KEY_USER_FACING_ID, OutMetadata.UserFacingId);

    // Extract file format version (missing = 1.0.0 for pre-format-version files)
    FString formatVersionString;
    if (metadataObject->TryGetStringField(KEY_FILE_FORMAT_VERSION, formatVersionString))
    {
        OutMetadata.FileFormatVersion = FSGDynamicTextAssetVersion::ParseFromString(formatVersionString);
    }

    OutMetadata.bIsValid = OutMetadata.AssetTypeId.IsValid() || !OutMetadata.ClassName.IsEmpty();
    return OutMetadata.bIsValid;
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
    if (!rootObject->TryGetObjectField(KEY_FILE_INFORMATION, metadataObjectPtr))
    {
        rootObject->TryGetObjectField(KEY_METADATA_LEGACY, metadataObjectPtr);
    }
    if (!metadataObjectPtr || !metadataObjectPtr->IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Error, TEXT("FSGDynamicTextAssetJsonSerializer::UpdateFieldsInPlace: Missing file information block"));
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
        TEXT("{\n    \"%s\": {\n        \"%s\": \"%s\",\n        \"%s\": \"1.0.0\",\n        \"%s\": \"%s\",\n        \"%s\": \"%s\",\n        \"%s\": \"%s\"\n    },\n    \"%s\": {}\n}"),
        *KEY_FILE_INFORMATION,
        *KEY_TYPE,
        *typeString,
        *KEY_VERSION,
        *KEY_ID,
        *Id.ToString(),
        *KEY_USER_FACING_ID,
        *UserFacingId,
        *KEY_FILE_FORMAT_VERSION,
        *GetFileFormatVersion().ToString(),
        *KEY_DATA
    );
}

bool FSGDynamicTextAssetJsonSerializer::ExtractSGDTAssetBundles(const FString& InString, FSGDynamicTextAssetBundleData& OutBundleData) const
{
    OutBundleData.Reset();

    if (InString.IsEmpty())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("ExtractSGDTAssetBundles(JSON): InString is empty"));
        return false;
    }

    TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(InString);
    TSharedPtr<FJsonObject> rootObject;
    if (!FJsonSerializer::Deserialize(reader, rootObject) || !rootObject.IsValid())
    {
        UE_LOG(LogSGDynamicTextAssetsRuntime, Warning, TEXT("ExtractSGDTAssetBundles(JSON): Failed to parse JSON"));
        return false;
    }

    const TSharedPtr<FJsonObject>* bundlesObject = nullptr;
    if (!rootObject->TryGetObjectField(KEY_SGDT_ASSET_BUNDLES, bundlesObject) || !bundlesObject || !bundlesObject->IsValid())
    {
        return false;
    }

    for (const TPair<FString, TSharedPtr<FJsonValue>>& bundlePair : (*bundlesObject)->Values)
    {
        const FName bundleName = FName(*bundlePair.Key);

        const TArray<TSharedPtr<FJsonValue>>* entryArray = nullptr;
        if (!bundlePair.Value.IsValid() || bundlePair.Value->Type != EJson::Array)
        {
            continue;
        }

        entryArray = &bundlePair.Value->AsArray();

        for (const TSharedPtr<FJsonValue>& entryValue : *entryArray)
        {
            if (!entryValue.IsValid() || entryValue->Type != EJson::Object)
            {
                continue;
            }

            const TSharedPtr<FJsonObject>& entryObj = entryValue->AsObject();
            FString propertyName;
            FString pathString;

            if (entryObj->TryGetStringField(TEXT("property"), propertyName) &&
                entryObj->TryGetStringField(TEXT("path"), pathString))
            {
                FSGDynamicTextAssetBundle* targetBundle = nullptr;
                for (FSGDynamicTextAssetBundle& bundle : OutBundleData.Bundles)
                {
                    if (bundle.BundleName == bundleName)
                    {
                        targetBundle = &bundle;
                        break;
                    }
                }

                if (!targetBundle)
                {
                    FSGDynamicTextAssetBundle& newBundle = OutBundleData.Bundles.AddDefaulted_GetRef();
                    newBundle.BundleName = bundleName;
                    targetBundle = &newBundle;
                }

                targetBundle->Entries.Emplace(FSoftObjectPath(pathString), FName(*propertyName));
            }
        }
    }

    return OutBundleData.HasBundles();
}

bool FSGDynamicTextAssetJsonSerializer::UpdateFileFormatVersion(FString& InOutFileContents,
    const FSGDynamicTextAssetVersion& NewVersion) const
{
    // Match "fileFormatVersion": "X.Y.Z" with flexible whitespace
    const FRegexPattern pattern(TEXT("\"fileFormatVersion\"\\s*:\\s*\"[0-9]+\\.[0-9]+\\.[0-9]+\""));
    FRegexMatcher matcher(pattern, InOutFileContents);

    if (matcher.FindNext())
    {
        const FString replacement = FString::Printf(TEXT("\"fileFormatVersion\": \"%s\""), *NewVersion.ToString());
        const int32 matchBegin = matcher.GetMatchBeginning();
        const int32 matchEnd = matcher.GetMatchEnding();

        InOutFileContents = InOutFileContents.Left(matchBegin) + replacement + InOutFileContents.Mid(matchEnd);
        return true;
    }

    // Field not found - insert it into the file information block
    // Find the closing brace of the sgFileInformation (or metadata) object
    // by locating the block key and then finding the first "}" after it
    const FRegexPattern blockPattern(TEXT("\"(?:sgFileInformation|metadata)\"\\s*:\\s*\\{"));
    FRegexMatcher blockMatcher(blockPattern, InOutFileContents);

    if (blockMatcher.FindNext())
    {
        // Find the first "}" that closes this block
        int32 braceDepth = 1;
        int32 searchPos = blockMatcher.GetMatchEnding();
        int32 closingBrace = INDEX_NONE;

        for (int32 i = searchPos; i < InOutFileContents.Len(); ++i)
        {
            if (InOutFileContents[i] == TEXT('{'))
            {
                braceDepth++;
            }
            else if (InOutFileContents[i] == TEXT('}'))
            {
                braceDepth--;
                if (braceDepth == 0)
                {
                    closingBrace = i;
                    break;
                }
            }
        }

        if (closingBrace != INDEX_NONE)
        {
            // Insert before the closing brace with a comma separator
            const FString insertion = FString::Printf(TEXT(",\n\t\t\"%s\": \"%s\"\n\t"),
                *KEY_FILE_FORMAT_VERSION, *NewVersion.ToString());

            // Find the last non-whitespace character before the closing brace to place the comma correctly
            int32 lastContent = closingBrace - 1;
            while (lastContent >= 0 && FChar::IsWhitespace(InOutFileContents[lastContent]))
            {
                lastContent--;
            }

            InOutFileContents = InOutFileContents.Left(lastContent + 1) + insertion + InOutFileContents.Mid(closingBrace);
            return true;
        }
    }

    UE_LOG(LogSGDynamicTextAssetsRuntime, Warning,
        TEXT("FSGDynamicTextAssetJsonSerializer::UpdateFileFormatVersion: Could not find or insert fileFormatVersion field"));
    return false;
}

bool FSGDynamicTextAssetJsonSerializer::MigrateFileFormat(FString& InOutFileContents,
    const FSGDynamicTextAssetVersion& CurrentFormatVersion,
    const FSGDynamicTextAssetVersion& TargetFormatVersion) const
{
    if (CurrentFormatVersion == TargetFormatVersion)
    {
        return true;
    }

    // Migration from 1.x to 2.x: rename "metadata" key to "sgFileInformation"
    if (CurrentFormatVersion.Major < 2 && TargetFormatVersion.Major >= 2)
    {
        const FRegexPattern pattern(TEXT("\"metadata\"(\\s*:)"));
        FRegexMatcher matcher(pattern, InOutFileContents);

        if (matcher.FindNext())
        {
            const FString suffix = matcher.GetCaptureGroup(1);
            const FString replacement = FString::Printf(TEXT("\"%s\"%s"), *KEY_FILE_INFORMATION, *suffix);
            const int32 matchBegin = matcher.GetMatchBeginning();
            const int32 matchEnd = matcher.GetMatchEnding();

            InOutFileContents = InOutFileContents.Left(matchBegin) + replacement + InOutFileContents.Mid(matchEnd);
        }
    }

    return true;
}
