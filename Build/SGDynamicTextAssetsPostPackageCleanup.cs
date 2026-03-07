// Copyright Start Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

/// <summary>
/// Post-package cleanup handler for the SGDynamicTextAssets plugin.
/// Deletes cooked .dta.bin files, dta_manifest.bin, and _TypeManifests/ subdirectory from
/// the project's Content directory after packaging completes. This runs after pak staging,
/// so all cooked files are already archived into the pak before cleanup.
///
/// Activated by setting CustomDeployment=SGDynamicTextAssetsCleanup in
/// [/Script/WindowsTargetPlatform.WindowsTargetSettings] in DefaultEngine.ini.
///
/// Controlled by bDeleteCookedAssetsAfterPackaging in [SGDynamicTextAssets] section of DefaultGame.ini.
/// Defaults to true if the key is absent.
///
/// Limitation: Only fires on Windows packaging (WinPlatform is the only platform that reads
/// the CustomDeployment INI key). Other platforms will need platform-specific support added
/// when their SDKs are available.
/// </summary>
[CustomDeploymentHandler("SGDynamicTextAssetsCleanup")]
public class SGDynamicTextAssetsPostPackageCleanup : CustomDeploymentHandler
{
	private static readonly string COOKED_DIRECTORY_NAME = "SGDynamicTextAssetsCooked";
	private static readonly string BINARY_EXTENSION = ".dta.bin";
	private static readonly string MANIFEST_FILENAME = "dta_manifest.bin";
	private static readonly string TYPE_MANIFESTS_DIRECTORY = "_TypeManifests";

	private static ILogger Logger => Log.Logger;

	public SGDynamicTextAssetsPostPackageCleanup(Platform AutomationPlatform)
		: base(AutomationPlatform)
	{
	}

	public override bool SupportsPlatform(UnrealTargetPlatform Platform)
	{
		// Currently only Windows reads the CustomDeployment INI key,
		// but the cleanup logic itself is platform-agnostic
		return true;
	}

	public override void PostPackage(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		if (!ShouldCleanup(Params))
		{
			Logger.LogInformation("SGDynamicTextAssets: Post-package cleanup disabled via DefaultGame.ini setting");
			return;
		}

		string cookedDir = GetCookedDirectoryPath(Params);
		if (!Directory.Exists(cookedDir))
		{
			Logger.LogInformation("SGDynamicTextAssets: No cooked directory found, nothing to clean: {CookedDir}", cookedDir);
			return;
		}

		Logger.LogInformation("SGDynamicTextAssets: Post-package cleanup starting: {CookedDir}", cookedDir);

		int deletedCount = 0;

		// Delete .dta.bin files
		string[] binFiles = Directory.GetFiles(cookedDir, "*" + BINARY_EXTENSION);
		foreach (string binFile in binFiles)
		{
			try
			{
				ClearReadOnly(binFile);
				File.Delete(binFile);
				Logger.LogInformation("SGDynamicTextAssets:   Deleted: {FileName}", Path.GetFileName(binFile));
				deletedCount++;
			}
			catch (Exception ex)
			{
				Logger.LogWarning("SGDynamicTextAssets: Failed to delete {BinFile}: {Message}", binFile, ex.Message);
			}
		}

		// Delete manifest
		string manifestPath = Path.Combine(cookedDir, MANIFEST_FILENAME);
		if (File.Exists(manifestPath))
		{
			try
			{
				ClearReadOnly(manifestPath);
				File.Delete(manifestPath);
				Logger.LogInformation("SGDynamicTextAssets:   Deleted manifest: {ManifestFilename}", MANIFEST_FILENAME);
				deletedCount++;
			}
			catch (Exception ex)
			{
				Logger.LogWarning("SGDynamicTextAssets: Failed to delete manifest {ManifestPath}: {Message}", manifestPath, ex.Message);
			}
		}

		// Delete _TypeManifests/ subdirectory and its contents
		string typeManifestsDir = Path.Combine(cookedDir, TYPE_MANIFESTS_DIRECTORY);
		if (Directory.Exists(typeManifestsDir))
		{
			string[] typeManifestFiles = Directory.GetFiles(typeManifestsDir, "*.json");
			foreach (string typeManifestFile in typeManifestFiles)
			{
				try
				{
					ClearReadOnly(typeManifestFile);
					File.Delete(typeManifestFile);
					Logger.LogInformation("SGDynamicTextAssets:   Deleted type manifest: {FileName}", Path.GetFileName(typeManifestFile));
					deletedCount++;
				}
				catch (Exception ex)
				{
					Logger.LogWarning("SGDynamicTextAssets: Failed to delete type manifest {File}: {Message}", typeManifestFile, ex.Message);
				}
			}

			try
			{
				Directory.Delete(typeManifestsDir);
				Logger.LogInformation("SGDynamicTextAssets:   Removed {TypeManifestsDir}/ directory", TYPE_MANIFESTS_DIRECTORY);
			}
			catch (Exception ex)
			{
				Logger.LogWarning("SGDynamicTextAssets: Failed to remove {TypeManifestsDir}/ directory: {Message}", TYPE_MANIFESTS_DIRECTORY, ex.Message);
			}
		}

		// Remove the cooked directory if empty
		try
		{
			if (Directory.Exists(cookedDir) && !Directory.EnumerateFileSystemEntries(cookedDir).Any())
			{
				Directory.Delete(cookedDir);
				Logger.LogInformation("SGDynamicTextAssets:   Removed empty cooked directory");
			}
		}
		catch (Exception ex)
		{
			Logger.LogWarning("SGDynamicTextAssets: Failed to remove directory {CookedDir}: {Message}", cookedDir, ex.Message);
		}

		Logger.LogInformation("SGDynamicTextAssets: Post-package cleanup complete - {DeletedCount} file(s) deleted", deletedCount);
	}

	/// <summary>
	/// Clears the read-only attribute on a file if set.
	/// C# File.Delete() throws on read-only files, unlike Unreal's IFileManager::Delete().
	/// </summary>
	private static void ClearReadOnly(string filePath)
	{
		FileAttributes attrs = File.GetAttributes(filePath);
		if ((attrs & FileAttributes.ReadOnly) != 0)
		{
			File.SetAttributes(filePath, attrs & ~FileAttributes.ReadOnly);
		}
	}

	/// <summary>
	/// Reads DefaultGame.ini to determine if post-package cleanup is enabled.
	/// Defaults to true if the key is absent.
	/// </summary>
	private static bool ShouldCleanup(ProjectParams Params)
	{
		ConfigHierarchy gameIni = ConfigCache.ReadHierarchy(
			ConfigHierarchyType.Game,
			DirectoryReference.FromFile(Params.RawProjectPath),
			Params.ClientTargetPlatformInstances.First().PlatformType);

		if (gameIni.GetBool("SGDynamicTextAssets", "bDeleteCookedAssetsAfterPackaging", out bool value))
		{
			return value;
		}

		// Default to true if key is not found
		return true;
	}

	/// <summary>
	/// Builds the absolute path to the cooked dynamic text assets directory.
	/// </summary>
	private static string GetCookedDirectoryPath(ProjectParams Params)
	{
		string projectDir = DirectoryReference.FromFile(Params.RawProjectPath).FullName;
		return Path.Combine(projectDir, "Content", COOKED_DIRECTORY_NAME);
	}
}
