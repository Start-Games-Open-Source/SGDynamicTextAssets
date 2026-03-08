// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/SGDynamicTextAssetId.h"
#include "Core/SGDynamicTextAssetVersion.h"

#include "ISGDynamicTextAssetServerInterface.generated.h"

class FJsonObject;

/**
 * Response data for a single dynamic text asset from the server.
 * Returned as part of a bulk fetch or single exists check.
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGServerDynamicTextAssetResponse
{
	GENERATED_BODY()

public:

	FSGServerDynamicTextAssetResponse() = default;

	/** ID of the dynamic text asset this response is for */
	UPROPERTY(BlueprintReadOnly)
	FSGDynamicTextAssetId Id;

	/**
	 * The data from the server(IE: JSON, XLM, etc).
	 * Empty string indicates the object does not exist on the server
	 * (should be deleted locally if configured to do so).
	 */
	UPROPERTY(BlueprintReadOnly)
	FString Data;

	/** Server-side version of this dynamic text asset */
	UPROPERTY(BlueprintReadOnly)
	FSGDynamicTextAssetVersion ServerVersion;
};

/**
 * Local dynamic text asset info sent to the server during a fetch request.
 * Allows the server to compare versions and only return changed data.
 */
USTRUCT(BlueprintType)
struct SGDYNAMICTEXTASSETSRUNTIME_API FSGServerDynamicTextAssetRequest
{
	GENERATED_BODY()

public:

	FSGServerDynamicTextAssetRequest() = default;

	FSGServerDynamicTextAssetRequest(const FSGDynamicTextAssetId& InId,
		const FSGDynamicTextAssetVersion& InLocalVersion)
		: Id(InId)
		, LocalVersion(InLocalVersion)
	{ }

	/** ID of the local dynamic text asset */
	UPROPERTY(BlueprintReadOnly)
	FSGDynamicTextAssetId Id;

	/** Local version for server-side comparison */
	UPROPERTY(BlueprintReadOnly)
	FSGDynamicTextAssetVersion LocalVersion;
};

/** Delegate broadcast when a bulk fetch operation completes */
DECLARE_DELEGATE_TwoParams(FOnServerFetchComplete, bool /*bSuccess*/, const TArray<FSGServerDynamicTextAssetResponse>& /*Updates*/);

/** Delegate broadcast when a single exists check completes */
DECLARE_DELEGATE_ThreeParams(FOnServerExistsComplete, bool /*bSuccess*/, bool /*bExists*/, const FString& /*UpdatedJson*/);

/** Delegate broadcast when a type manifest override fetch completes */
DECLARE_DELEGATE_TwoParams(FOnServerTypeOverridesComplete, bool /*bSuccess*/, TSharedPtr<FJsonObject> /*OverrideData*/);

/**
 * Abstract interface for server-side dynamic text asset integration.
 *
 * Games implement this interface to connect their specific backend
 * to the SGDynamicTextAssets system. This enables:
 * - Hot-fixing data without client updates
 * - A/B testing with server-authoritative data
 * - Server-side data override for live games
 *
 * The server interface is only used at runtime (not in editor).
 * Register an implementation with USGDynamicTextAssetSubsystem to enable
 * server integration.
 *
 * @see USGDynamicTextAssetSubsystem
 * @see USGDynamicTextAssetSettings::IsServerOverrideEnabled
 */
UCLASS(Abstract, ClassGroup = "Start Games")
class SGDYNAMICTEXTASSETSRUNTIME_API USGDynamicTextAssetServerInterface : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Fetches all dynamic text assets from the server that need updating.
	 *
	 * The server compares the provided local IDs and versions against
	 * its own data and returns only objects that have changed or are new.
	 * Objects not returned are assumed to be up-to-date.
	 *
	 * Responses with empty Data indicate the server wants the object
	 * removed from the client.
	 *
	 * @param LocalObjects Array of local dynamic text asset IDs with their current versions
	 * @param OnComplete Callback invoked when the fetch completes (game thread)
	 */
	virtual void FetchAllDynamicTextAssets(
		const TArray<FSGServerDynamicTextAssetRequest>& LocalObjects,
		FOnServerFetchComplete OnComplete)
	PURE_VIRTUAL(USGDynamicTextAssetServerInterface::FetchAllDynamicTextAssets,);

	/**
	 * Checks whether a specific dynamic text asset exists on the server.
	 * Optionally returns updated Data if the server version differs.
	 *
	 * @param Id The dynamic text asset ID to check
	 * @param OnComplete Callback invoked with the result (game thread)
	 */
	virtual void CheckDynamicTextAssetExists(
		const FSGDynamicTextAssetId& Id,
		FOnServerExistsComplete OnComplete)
	PURE_VIRTUAL(USGDynamicTextAssetServerInterface::CheckDynamicTextAssetExists,);

	/**
	 * Fetches type manifest overrides from the server.
	 *
	 * The server returns a JSON object containing type overrides keyed by
	 * root type ID. These overrides can add new type entries, remap existing
	 * entries to different classes, or disable types by providing an empty className.
	 *
	 * Expected response format:
	 * {
	 *   "manifests": {
	 *     "<rootTypeId>": { "types": [{ "typeId": "...", "className": "...", "parentTypeId": "..." }] },
	 *     ...
	 *   }
	 * }
	 *
	 * @param OnComplete Callback invoked with the result (game thread).
	 *                   bSuccess=true with null OverrideData means no overrides available.
	 */
	virtual void FetchTypeManifestOverrides(FOnServerTypeOverridesComplete OnComplete)
	PURE_VIRTUAL(USGDynamicTextAssetServerInterface::FetchTypeManifestOverrides,);
};
