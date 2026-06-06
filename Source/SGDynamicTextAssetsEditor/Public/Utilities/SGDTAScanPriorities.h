// Copyright Start Games, Inc. All Rights Reserved.

#pragma once

/**
 * Centralized priority constants for DTA scan phases.
 * Lower values run first. Phases with the same priority have no guaranteed order.
 *
 * All scan phases register with USGDynamicTextAssetScanSubsystem using these
 * priorities to control execution order within the shared ticker infrastructure.
 */
namespace SGDynamicTextAssetScanPriorities
{
	// Extender discovery runs earliest. Scans for USGDTAAssetBundleExtender
	// subclasses via GetDerivedClasses and registers any new ones in the manifest.
	// Must complete before other phases that may need extender resolution.
	static constexpr int32 EXTENDER_DISCOVERY = 5;

	// Project info (format versions) runs first since it is lightweight
	// (file-info-only extraction) and the major version upgrade check
	// needs the result as early as possible.
	static constexpr int32 PROJECT_INFO = 10;

	// Blueprint reference scanning. Loads CDOs and scans UPROPERTY fields
	// for FSGDynamicTextAssetRef references.
	static constexpr int32 REFERENCE_BLUEPRINTS = 100;

	// Level reference scanning. Loads worlds and iterates actors and their
	// components for FSGDynamicTextAssetRef references.
	static constexpr int32 REFERENCE_LEVELS = 200;

	// DTA file reference scanning. Full deserialization into transient
	// instances to discover cross-references between DTA files.
	// Runs last since it is the heaviest operation.
	static constexpr int32 REFERENCE_DTA_FILES = 300;
}
