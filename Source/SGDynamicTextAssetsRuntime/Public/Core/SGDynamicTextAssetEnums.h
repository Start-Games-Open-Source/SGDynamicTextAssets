// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SGDynamicTextAssetEnums.generated.h"

/**
 * Result of a dynamic text asset load operation.
 */
UENUM(BlueprintType)
enum class ESGLoadResult : uint8
{
    /** Dynamic text asset loaded successfully */
    Success              UMETA(DisplayName = "Success"),
    
    /** Dynamic text asset was already in memory */
    AlreadyLoaded        UMETA(DisplayName = "Already Loaded"),
    
    /** Local file does not exist */
    FileNotFound         UMETA(DisplayName = "File Not Found"),
    
    /** JSON parsing or property population failed */
    DeserializationFailed UMETA(DisplayName = "Deserialization Failed"),
    
    /** Dynamic text asset failed validation */
    ValidationFailed     UMETA(DisplayName = "Validation Failed"),
    
    /** Server indicated this object should not exist */
    ServerDeletedObject  UMETA(DisplayName = "Server Deleted Object"),
    
    /** Server request failed */
    ServerFetchFailed    UMETA(DisplayName = "Server Fetch Failed"),
    
    /** Operation timed out */
    Timeout              UMETA(DisplayName = "Timeout")
};

/**
 * Compression method for binary serialization.
 */
UENUM(BlueprintType)
enum class ESGCompressionMethod : uint8
{
    /** No compression */
    None    UMETA(DisplayName = "None"),
    
    /** Unreal's default Zlib compression */
    Zlib    UMETA(DisplayName = "Zlib"),
    
    /** Gzip compression */
    Gzip    UMETA(DisplayName = "Gzip"),
    
    /** LZ4 fast compression */
    LZ4     UMETA(DisplayName = "LZ4")
};

/**
 * Type of reference to a dynamic text asset.
 */
UENUM(BlueprintType)
enum class ESGReferenceType : uint8
{
    /** Reference from a Blueprint class */
    Blueprint   UMETA(DisplayName = "Blueprint"),

    /** Reference from another dynamic text asset */
    DynamicTextAsset  UMETA(DisplayName = "Dynamic Text Asset"),

    /** Reference from a level actor */
    Level       UMETA(DisplayName = "Level"),

    /** Reference from other UObject type */
    Other       UMETA(DisplayName = "Other")
};
