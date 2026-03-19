# Commandlets

[Back to Table of Contents](../TableOfContents.md)

## Cook Commandlet

`USGDynamicTextAssetCookCommandlet` converts `.dta.json` source files to compressed `.dta.bin` binary files for packaged builds.

### Usage

```bash
UnrealEditor-Cmd.exe <ProjectPath> -run=SGDynamicTextAssetCook [options]
```

### Options

| Flag | Description |
|------|-------------|
| `-output=Path` | Override the output directory. Default: `Content/SGDynamicTextAssetsCooked/` |
| `-class=Name` | Only process files for a specific class (e.g., `-class=UWeaponData`) |
| `-validate` | Validation-only mode: checks all files but does not produce binary output |
| `-clean` | Clean-only mode: deletes all cooked files and the directory without cooking. Mutually exclusive with `-validate` |
| `-noclean` | Skips pre-cook directory cleaning. Use for build machines that reuse cooked output for faster iterative builds |

### Examples

```bash
# Cook all dynamic text assets with default settings
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook

# Validate all dynamic text assets (CI/CD check, no binary output)
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -validate

# Cook to a custom output directory
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -output=Saved/Cooked/DynamicTextAssets

# Cook only weapon dynamic text assets
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -class=UWeaponData

# Clean cooked directory without cooking (CI/CD cleanup step)
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -clean

# Cook without pre-clean (iterative build machine reuse)
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -noclean
```

### Output

```
{OutputDirectory}/
  {GUID1}.dta.bin
  {GUID2}.dta.bin
  ...
  dta_manifest.bin
```

### Return Codes

- **0**: Success
- **1**: Failure (validation errors or cook failures)

### Notes

- The editor packaging flow (File > Package Project) uses a cook delegate that calls the same `FSGDynamicTextAssetCookUtils` functions automatically. The commandlet is for CI/CD pipelines and manual use.
- Compression method is controlled by `USGDynamicTextAssetSettingsAsset::DefaultCompressionMethod`.
- The manifest includes a SHA-1 integrity hash over the entries array.

## Format Version Commandlet

`USGDynamicTextAssetFormatVersionCommandlet` validates and migrates file format versions across all DTA files. Use this when updating the plugin to a version that changes the serializer file format structure.

### Usage

```bash
UnrealEditor-Cmd.exe <ProjectPath> -run=SGDynamicTextAssetFormatVersion [options]
```

### Options

| Flag | Description |
|------|-------------|
| `-validate` | Scan all DTA files and report version mismatches. Minor mismatches are warnings, major mismatches are errors. Does not modify any files. Default mode if no flag specified. |
| `-migrate` | Scan all DTA files, run `MigrateFileFormat()` and `UpdateFileFormatVersion()` on any files with outdated format versions, and re-save them. |

### Examples

```bash
# Validate all files against current serializer versions
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetFormatVersion -validate

# Migrate all outdated files to current format versions
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetFormatVersion -migrate
```

### Return Codes

- **0**: Success (validate found no major mismatches, or migrate completed without failures)
- **1**: Failure (validate found major mismatches, or migrate had failures)

### Static API

The commandlet exposes static utility functions that can be called from editor code:

```cpp
// Scan all files and return per-file validation results
static void ValidateAllFiles(TArray<FSGFormatVersionValidationResult>& OutResults);

// Migrate all outdated files
static void MigrateAllFiles(TArray<FSGFormatVersionMigrationResult>& OutResults);

// Migrate a single file
static bool MigrateSingleFile(const FString& FilePath, FString& OutError);
```

### Editor Integration

On editor startup, the plugin automatically scans project files via the async scan subsystem. If a major format version mismatch is detected, a dialog prompts the user to run migration. This uses the same static API as the commandlet.

---

## CI/CD Integration

For CI/CD pipelines, combine the commandlet with your build process:

```bash
# Step 1: Validate file format versions
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetFormatVersion -validate
if %ERRORLEVEL% NEQ 0 exit /b 1

# Step 2: Validate dynamic text asset data (catches data errors early)
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -validate
if %ERRORLEVEL% NEQ 0 exit /b 1

# Step 3: Cook dynamic text assets
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook
if %ERRORLEVEL% NEQ 0 exit /b 1

# Step 3: Proceed with normal UE build/cook/package
```

For build machines that reuse cooked output across runs, use `-noclean` to skip pre-cook cleaning:

```bash
# Iterative build: skip pre-clean for faster cooks
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -noclean
```

To clean up cooked files after a build pipeline completes (e.g., disk cleanup):

```bash
# Post-build cleanup
UnrealEditor-Cmd.exe MyProject.uproject -run=SGDynamicTextAssetCook -clean
```

The validation step is optional but recommended as it catches data errors before the full build process.

[Back to Table of Contents](../TableOfContents.md)
