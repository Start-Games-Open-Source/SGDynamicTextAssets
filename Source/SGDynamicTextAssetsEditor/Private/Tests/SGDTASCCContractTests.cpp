// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_WORKER

#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"

#include "Management/SGDTAFileManager.h"
#include "Management/SGDTASCCBridge.h"
#include "Management/SGDynamicTextAssetFileInfo.h"
#include "Management/SGDynamicTextAssetRegistry.h"
#include "Core/SGDynamicTextAssetId.h"
#include "Statics/SGDTAConstants.h"

#include "SGDynamicTextAssetUnitTest.h"

/**
 * SCC-contract automation suite.
 *
 * Locks down two provider-independent guarantees the hardened file manager makes:
 *   1. Every mutation still succeeds and attempts no source-control op when SCC is
 *      opted out (bAutoCheckOut=false / bHandleSourceControl=false).
 *   2. Every successful mutation broadcasts ON_FILE_CHANGED with the correct kind,
 *      independent of source-control state.
 *
 * "No provider" is modeled at the contract boundary the epic actually guarantees: a
 * counting fake bridge installed on the public SOURCE_CONTROL_BRIDGE pointer proves the
 * exact bridge call count (zero on opt-out paths). This is deterministic and needs no
 * ISourceControlModule state. Provider-side outcomes (real mark-for-add/delete/checkout
 * against a live server) stay manual and are out of scope here.
 *
 * All tests run in the editor automation context and clean up their own temp files.
 */
namespace SGDTASCCContractTestUtils
{
    /**
     * Test-local ISGDTASourceControlBridge that counts each command and always succeeds.
     * Installed onto FSGDynamicTextAssetFileManager::SOURCE_CONTROL_BRIDGE via FScopedBridge
     * so a test can assert the exact number of bridge calls a mutation makes.
     */
    class FCountingSourceControlBridge final : public ISGDTASourceControlBridge
    {
    public:
        int32 CheckOutCount = 0;
        int32 MarkForAddCount = 0;
        int32 MarkForDeleteCount = 0;

        /** Total of all recorded bridge calls. Zero means the mutation attempted no SCC op. */
        int32 TotalCalls() const
        {
            return CheckOutCount + MarkForAddCount + MarkForDeleteCount;
        }

        // ISGDTASourceControlBridge interface
        virtual bool CheckOutFile(const FString& FilePath) override
        {
            ++CheckOutCount;
            return true;
        }

        virtual bool MarkForAdd(const FString& FilePath) override
        {
            ++MarkForAddCount;
            return true;
        }

        virtual bool MarkForDelete(const FString& FilePath) override
        {
            ++MarkForDeleteCount;
            return true;
        }
        // ~ISGDTASourceControlBridge interface
    };

    /**
     * RAII guard that installs a bridge on the file manager's public SOURCE_CONTROL_BRIDGE
     * pointer and restores the previously registered bridge (the real editor default) on
     * scope exit, so each test leaves the global pointer exactly as it found it.
     */
    class FScopedBridge
    {
    public:
        explicit FScopedBridge(ISGDTASourceControlBridge* NewBridge)
            : PreviousBridge(FSGDynamicTextAssetFileManager::SOURCE_CONTROL_BRIDGE)
        {
            FSGDynamicTextAssetFileManager::SOURCE_CONTROL_BRIDGE = NewBridge;
        }

        ~FScopedBridge()
        {
            FSGDynamicTextAssetFileManager::SOURCE_CONTROL_BRIDGE = PreviousBridge;
        }

        FScopedBridge(const FScopedBridge&) = delete;
        FScopedBridge& operator=(const FScopedBridge&) = delete;

    private:
        ISGDTASourceControlBridge* PreviousBridge = nullptr;
    };

    /** A single recorded ON_FILE_CHANGED event. */
    struct FRecordedChange
    {
        ESGDynamicTextAssetFileChangeKind Kind = ESGDynamicTextAssetFileChangeKind::Created;
        FString FilePath;
        FGuid Guid;
    };

    /**
     * RAII recorder that subscribes to ON_FILE_CHANGED on construction and unsubscribes
     * on destruction, capturing every broadcast as an FRecordedChange. Lets a test assert
     * the kind/path/GUID of each event without leaking a subscription between tests.
     */
    class FFileChangeRecorder
    {
    public:
        FFileChangeRecorder()
        {
            Handle = FSGDynamicTextAssetFileManager::ON_FILE_CHANGED.AddRaw(this, &FFileChangeRecorder::OnFileChanged);
        }

        ~FFileChangeRecorder()
        {
            FSGDynamicTextAssetFileManager::ON_FILE_CHANGED.Remove(Handle);
        }

        FFileChangeRecorder(const FFileChangeRecorder&) = delete;
        FFileChangeRecorder& operator=(const FFileChangeRecorder&) = delete;

        /** Number of recorded events. */
        int32 Num() const
        {
            return Events.Num();
        }

        const FRecordedChange& operator[](int32 Index) const
        {
            return Events[Index];
        }

        /** Discards recorded events so a recorder can be reused after fixture setup. */
        void Reset()
        {
            Events.Reset();
        }

        /**
         * Returns the first recorded event whose path matches the given path
         * (case-insensitive, full-path comparison), or nullptr if none matches.
         * Used for order-independent per-path assertions on rename/convert.
         */
        const FRecordedChange* FindByPath(const FString& InPath) const
        {
            for (const FRecordedChange& Event : Events)
            {
                if (FPaths::IsSamePath(Event.FilePath, InPath))
                {
                    return &Event;
                }
            }
            return nullptr;
        }

    private:
        void OnFileChanged(ESGDynamicTextAssetFileChangeKind Kind, const FString& FilePath, const FGuid& Guid)
        {
            FRecordedChange& recorded = Events.AddDefaulted_GetRef();
            recorded.Kind = Kind;
            recorded.FilePath = FilePath;
            recorded.Guid = Guid;
        }

        FDelegateHandle Handle;
        TArray<FRecordedChange> Events;
    };

    /**
     * RAII fixture that creates self-contained temp DTA files through the file manager and
     * deletes every path it touched on scope exit (with broadcasts suppressed so cleanup
     * never perturbs a recorder). Each test owns one fixture so files never collide and
     * never leave residue.
     */
    class FTempFixtures
    {
    public:
        explicit FTempFixtures(const FString& InTestPrefix)
            : TestPrefix(InTestPrefix)
        {
            // Ensure the unit-test provider class is registered so the registry-driven
            // mutations (duplicate resolves class via the registry) can find it. Idempotent.
            if (USGDynamicTextAssetRegistry* registry = USGDynamicTextAssetRegistry::Get())
            {
                registry->RegisterDynamicTextAssetClass(USGDynamicTextAssetUnitTest::StaticClass());
            }
        }

        ~FTempFixtures()
        {
            IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
            for (const FString& path : PathsToCleanup)
            {
                if (platformFile.FileExists(*path))
                {
                    platformFile.DeleteFile(*path);
                }
            }
        }

        FTempFixtures(const FTempFixtures&) = delete;
        FTempFixtures& operator=(const FTempFixtures&) = delete;

        /** Tracks a path for cleanup even if the test did not create it through this fixture. */
        void Track(const FString& Path)
        {
            if (!Path.IsEmpty())
            {
                PathsToCleanup.AddUnique(Path);
            }
        }

        /** Builds a unique UserFacingId for this test run (prefix + a fresh GUID fragment). */
        FString MakeUniqueId(const FString& Suffix) const
        {
            return FString::Printf(TEXT("%s_%s_%s"), *TestPrefix, *Suffix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
        }

        /**
         * Creates a fresh JSON DTA file through the manager and tracks it for cleanup.
         * Returns true on success and fills OutId / OutPath.
         */
        bool CreateJsonFixture(const FString& Suffix, FSGDynamicTextAssetId& OutId, FString& OutPath)
        {
            const FString userFacingId = MakeUniqueId(Suffix);
            const bool bCreated = FSGDynamicTextAssetFileManager::CreateDynamicTextAssetFile(
                USGDynamicTextAssetUnitTest::StaticClass(),
                userFacingId,
                SGDynamicTextAssetConstants::JSON_FILE_EXTENSION,
                OutId,
                OutPath);

            if (bCreated)
            {
                Track(OutPath);
            }
            return bCreated;
        }

    private:
        FString TestPrefix;
        TArray<FString> PathsToCleanup;
    };
}

using namespace SGDTASCCContractTestUtils;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_WriteCreate_NoProvider_SucceedsNoSCCOp,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.WriteCreate.NoProviderSucceedsNoSCCOp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_WriteCreate_NoProvider_SucceedsNoSCCOp::RunTest(const FString& Parameters)
{
    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("WriteCreate"));

    // A brand-new path: with bAutoCheckOut=false the manager must not touch the bridge.
    const FString newPath = FSGDynamicTextAssetFileManager::BuildFilePath(
        USGDynamicTextAssetUnitTest::StaticClass(), fixtures.MakeUniqueId(TEXT("file")), SGDynamicTextAssetConstants::JSON_FILE_EXTENSION);
    fixtures.Track(newPath);

    const bool bWrote = FSGDynamicTextAssetFileManager::WriteRawFileContents(
        newPath, TEXT("{}"), /*bAutoCheckOut*/ false, /*bBroadcastChange*/ true);

    TestTrue(TEXT("Write to a new path with bAutoCheckOut=false should succeed"), bWrote);
    TestEqual(TEXT("Write with opt-out should attempt zero SCC ops"), bridge.TotalCalls(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_WriteModify_NoProvider_SucceedsNoSCCOp,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.WriteModify.NoProviderSucceedsNoSCCOp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_WriteModify_NoProvider_SucceedsNoSCCOp::RunTest(const FString& Parameters)
{
    // The fixture create serializes default content for the type-id-less unit-test class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("WriteModify"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    // Overwrite an existing file with bAutoCheckOut=false: the checkout branch must not run.
    const bool bWrote = FSGDynamicTextAssetFileManager::WriteRawFileContents(
        path, TEXT("{\"changed\":true}"), /*bAutoCheckOut*/ false, /*bBroadcastChange*/ true);

    TestTrue(TEXT("Overwrite of an existing file with bAutoCheckOut=false should succeed"), bWrote);
    TestEqual(TEXT("Overwrite with opt-out should attempt zero SCC ops"), bridge.TotalCalls(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Create_NoProvider_SucceedsNoSCCOp,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Create.NoProviderSucceedsNoSCCOp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Create_NoProvider_SucceedsNoSCCOp::RunTest(const FString& Parameters)
{
    // Create serializes default content for the type-id-less unit-test class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("Create"));

    FSGDynamicTextAssetId id;
    FString path;
    const bool bCreated = fixtures.CreateJsonFixture(TEXT("file"), id, path);

    TestTrue(TEXT("CreateDynamicTextAssetFile should succeed"), bCreated);
    // Create never marks a new file for add (that is a default-flag concern handled elsewhere);
    // it never calls the bridge regardless of flags.
    TestEqual(TEXT("Create should attempt zero SCC ops"), bridge.TotalCalls(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Delete_NoProvider_SucceedsNoSCCOp,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Delete.NoProviderSucceedsNoSCCOp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Delete_NoProvider_SucceedsNoSCCOp::RunTest(const FString& Parameters)
{
    // The fixture create serializes default content; delete itself does not serialize.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("Delete"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    const bool bDeleted = FSGDynamicTextAssetFileManager::DeleteDynamicTextAssetFile(path, /*bBroadcastChange*/ true);

    TestTrue(TEXT("DeleteDynamicTextAssetFile should succeed"), bDeleted);
    // Delete never routes through the bridge.
    TestEqual(TEXT("Delete should attempt zero SCC ops"), bridge.TotalCalls(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Rename_NoProvider_SucceedsNoSCCOp,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Rename.NoProviderSucceedsNoSCCOp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Rename_NoProvider_SucceedsNoSCCOp::RunTest(const FString& Parameters)
{
    // Both the fixture create and the rename re-write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("Rename"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FString newPath;
    const bool bRenamed = FSGDynamicTextAssetFileManager::RenameDynamicTextAsset(
        path, fixtures.MakeUniqueId(TEXT("dst")), newPath, /*bHandleSourceControl*/ false);
    fixtures.Track(newPath);

    TestTrue(TEXT("Rename with bHandleSourceControl=false should succeed"), bRenamed);
    TestEqual(TEXT("Rename with opt-out should attempt zero SCC ops"), bridge.TotalCalls(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Duplicate_NoProvider_SucceedsNoSCCOp,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Duplicate.NoProviderSucceedsNoSCCOp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Duplicate_NoProvider_SucceedsNoSCCOp::RunTest(const FString& Parameters)
{
    // Both the fixture create and the duplicate write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("Duplicate"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FSGDynamicTextAssetId newId;
    FString newPath;
    const bool bDuplicated = FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset(
        path, fixtures.MakeUniqueId(TEXT("dup")), newId, newPath, /*bHandleSourceControl*/ false);
    fixtures.Track(newPath);

    TestTrue(TEXT("Duplicate with bHandleSourceControl=false should succeed"), bDuplicated);
    TestEqual(TEXT("Duplicate with opt-out should attempt zero SCC ops"), bridge.TotalCalls(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Convert_NoProvider_SucceedsNoSCCOp,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Convert.NoProviderSucceedsNoSCCOp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Convert_NoProvider_SucceedsNoSCCOp::RunTest(const FString& Parameters)
{
    // Both the fixture create and the convert target write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("Convert"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    // JSON -> XML. The XML serializer is registered by the runtime module at startup
    // (SGDTARuntimeModule.cpp StartupModule), so this target is reliable in automation.
    FString newPath;
    const bool bConverted = FSGDynamicTextAssetFileManager::ConvertFileFormat(
        path, SGDynamicTextAssetConstants::XML_FILE_EXTENSION, newPath, /*bHandleSourceControl*/ false);
    fixtures.Track(newPath);

    TestTrue(TEXT("Convert JSON->XML with bHandleSourceControl=false should succeed"), bConverted);
    TestEqual(TEXT("Convert with opt-out should attempt zero SCC ops"), bridge.TotalCalls(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_WriteCreate_NoProvider_StillBroadcasts,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.WriteCreate.NoProviderStillBroadcasts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_WriteCreate_NoProvider_StillBroadcasts::RunTest(const FString& Parameters)
{
    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("WriteCreateBcast"));

    const FString newPath = FSGDynamicTextAssetFileManager::BuildFilePath(
        USGDynamicTextAssetUnitTest::StaticClass(), fixtures.MakeUniqueId(TEXT("file")), SGDynamicTextAssetConstants::JSON_FILE_EXTENSION);
    fixtures.Track(newPath);

    FFileChangeRecorder recorder;
    FSGDynamicTextAssetFileManager::WriteRawFileContents(newPath, TEXT("{}"), false, /*bBroadcastChange*/ true);

    TestEqual(TEXT("A new-path write should broadcast exactly once"), recorder.Num(), 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_WriteModify_NoProvider_StillBroadcasts,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.WriteModify.NoProviderStillBroadcasts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_WriteModify_NoProvider_StillBroadcasts::RunTest(const FString& Parameters)
{
    // The fixture create serializes default content for the type-id-less unit-test class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("WriteModifyBcast"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FFileChangeRecorder recorder;
    FSGDynamicTextAssetFileManager::WriteRawFileContents(path, TEXT("{\"changed\":true}"), false, /*bBroadcastChange*/ true);

    TestEqual(TEXT("An overwrite should broadcast exactly once"), recorder.Num(), 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Create_NoProvider_StillBroadcasts,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Create.NoProviderStillBroadcasts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Create_NoProvider_StillBroadcasts::RunTest(const FString& Parameters)
{
    // Create serializes default content for the type-id-less unit-test class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("CreateBcast"));

    FFileChangeRecorder recorder;
    FSGDynamicTextAssetId id;
    FString path;
    const bool bCreated = fixtures.CreateJsonFixture(TEXT("file"), id, path);

    TestTrue(TEXT("Create should succeed"), bCreated);
    TestEqual(TEXT("Create should broadcast exactly once"), recorder.Num(), 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Delete_NoProvider_StillBroadcasts,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Delete.NoProviderStillBroadcasts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Delete_NoProvider_StillBroadcasts::RunTest(const FString& Parameters)
{
    // The fixture create serializes default content; delete itself does not serialize.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("DeleteBcast"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FFileChangeRecorder recorder;
    FSGDynamicTextAssetFileManager::DeleteDynamicTextAssetFile(path, /*bBroadcastChange*/ true);

    TestEqual(TEXT("Delete should broadcast exactly once"), recorder.Num(), 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Rename_NoProvider_StillBroadcasts,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Rename.NoProviderStillBroadcasts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Rename_NoProvider_StillBroadcasts::RunTest(const FString& Parameters)
{
    // Both the fixture create and the rename re-write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("RenameBcast"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FFileChangeRecorder recorder;
    FString newPath;
    FSGDynamicTextAssetFileManager::RenameDynamicTextAsset(path, fixtures.MakeUniqueId(TEXT("dst")), newPath, false);
    fixtures.Track(newPath);

    // Rename emits one event per path; exact-count specifics are covered by the double-broadcast test.
    TestTrue(TEXT("Rename should broadcast at least once"), recorder.Num() > 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Duplicate_NoProvider_StillBroadcasts,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Duplicate.NoProviderStillBroadcasts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Duplicate_NoProvider_StillBroadcasts::RunTest(const FString& Parameters)
{
    // Both the fixture create and the duplicate write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("DuplicateBcast"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FFileChangeRecorder recorder;
    FSGDynamicTextAssetId newId;
    FString newPath;
    FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset(path, fixtures.MakeUniqueId(TEXT("dup")), newId, newPath, false);
    fixtures.Track(newPath);

    TestEqual(TEXT("Duplicate should broadcast exactly once"), recorder.Num(), 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Convert_NoProvider_StillBroadcasts,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Convert.NoProviderStillBroadcasts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Convert_NoProvider_StillBroadcasts::RunTest(const FString& Parameters)
{
    // Both the fixture create and the convert target write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("ConvertBcast"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FFileChangeRecorder recorder;
    FString newPath;
    FSGDynamicTextAssetFileManager::ConvertFileFormat(path, SGDynamicTextAssetConstants::XML_FILE_EXTENSION, newPath, false);
    fixtures.Track(newPath);

    // Convert is a move; it emits one event per path.
    TestTrue(TEXT("Convert should broadcast at least once"), recorder.Num() > 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_WriteExisting_Broadcasts_Modified,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Kind.WriteExistingModified",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_WriteExisting_Broadcasts_Modified::RunTest(const FString& Parameters)
{
    // The fixture create serializes default content for the type-id-less unit-test class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("KindWriteExisting"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FFileChangeRecorder recorder;
    FSGDynamicTextAssetFileManager::WriteRawFileContents(path, TEXT("{\"changed\":true}"), false, true);

    if (!TestEqual(TEXT("Overwrite should broadcast once"), recorder.Num(), 1))
    {
        return false;
    }
    TestTrue(TEXT("Overwriting an existing file should broadcast Modified"),
        recorder[0].Kind == ESGDynamicTextAssetFileChangeKind::Modified);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_WriteNew_Broadcasts_Created,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Kind.WriteNewCreated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_WriteNew_Broadcasts_Created::RunTest(const FString& Parameters)
{
    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("KindWriteNew"));

    const FString newPath = FSGDynamicTextAssetFileManager::BuildFilePath(
        USGDynamicTextAssetUnitTest::StaticClass(), fixtures.MakeUniqueId(TEXT("file")), SGDynamicTextAssetConstants::JSON_FILE_EXTENSION);
    fixtures.Track(newPath);

    FFileChangeRecorder recorder;
    FSGDynamicTextAssetFileManager::WriteRawFileContents(newPath, TEXT("{}"), false, true);

    if (!TestEqual(TEXT("New-path write should broadcast once"), recorder.Num(), 1))
    {
        return false;
    }
    TestTrue(TEXT("Writing a new file should broadcast Created"),
        recorder[0].Kind == ESGDynamicTextAssetFileChangeKind::Created);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Create_Broadcasts_Created,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Kind.CreateCreated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Create_Broadcasts_Created::RunTest(const FString& Parameters)
{
    // Create serializes default content for the type-id-less unit-test class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("KindCreate"));

    FFileChangeRecorder recorder;
    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Create should succeed"), fixtures.CreateJsonFixture(TEXT("file"), id, path)))
    {
        return false;
    }

    if (!TestEqual(TEXT("Create should broadcast once"), recorder.Num(), 1))
    {
        return false;
    }
    TestTrue(TEXT("Create should broadcast Created"), recorder[0].Kind == ESGDynamicTextAssetFileChangeKind::Created);
    TestEqual(TEXT("Create broadcast should carry the new file's GUID"), recorder[0].Guid, id.GetGuid());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Duplicate_Broadcasts_Created,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Kind.DuplicateCreated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Duplicate_Broadcasts_Created::RunTest(const FString& Parameters)
{
    // Both the fixture create and the duplicate write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("KindDuplicate"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FFileChangeRecorder recorder;
    FSGDynamicTextAssetId newId;
    FString newPath;
    const bool bDuplicated = FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset(
        path, fixtures.MakeUniqueId(TEXT("dup")), newId, newPath, false);
    fixtures.Track(newPath);

    if (!TestTrue(TEXT("Duplicate should succeed"), bDuplicated))
    {
        return false;
    }
    if (!TestEqual(TEXT("Duplicate should broadcast once"), recorder.Num(), 1))
    {
        return false;
    }
    TestTrue(TEXT("Duplicate should broadcast Created"), recorder[0].Kind == ESGDynamicTextAssetFileChangeKind::Created);
    TestEqual(TEXT("Duplicate broadcast should carry the duplicate's new GUID"), recorder[0].Guid, newId.GetGuid());
    TestNotEqual(TEXT("Duplicate GUID should differ from the source GUID"), recorder[0].Guid, id.GetGuid());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Delete_Broadcasts_Deleted,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Kind.DeleteDeleted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Delete_Broadcasts_Deleted::RunTest(const FString& Parameters)
{
    // The fixture create serializes default content; delete itself does not serialize.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("KindDelete"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FFileChangeRecorder recorder;
    FSGDynamicTextAssetFileManager::DeleteDynamicTextAssetFile(path, true);

    if (!TestEqual(TEXT("Delete should broadcast once"), recorder.Num(), 1))
    {
        return false;
    }
    TestTrue(TEXT("Delete should broadcast Deleted"), recorder[0].Kind == ESGDynamicTextAssetFileChangeKind::Deleted);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Rename_Broadcasts_Renamed,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Kind.RenameRenamed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Rename_Broadcasts_Renamed::RunTest(const FString& Parameters)
{
    // Both the fixture create and the rename re-write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("KindRename"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FFileChangeRecorder recorder;
    FString newPath;
    const bool bRenamed = FSGDynamicTextAssetFileManager::RenameDynamicTextAsset(
        path, fixtures.MakeUniqueId(TEXT("dst")), newPath, false);
    fixtures.Track(newPath);

    if (!TestTrue(TEXT("Rename should succeed"), bRenamed))
    {
        return false;
    }
    // Every event from a rename must be Renamed.
    bool bAllRenamed = recorder.Num() > 0;
    for (int32 i = 0; i < recorder.Num(); ++i)
    {
        bAllRenamed &= (recorder[i].Kind == ESGDynamicTextAssetFileChangeKind::Renamed);
    }
    TestTrue(TEXT("Every rename event should be Renamed"), bAllRenamed);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Convert_Broadcasts_Renamed,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Kind.ConvertRenamed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Convert_Broadcasts_Renamed::RunTest(const FString& Parameters)
{
    // Both the fixture create and the convert target write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("KindConvert"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FFileChangeRecorder recorder;
    FString newPath;
    const bool bConverted = FSGDynamicTextAssetFileManager::ConvertFileFormat(
        path, SGDynamicTextAssetConstants::XML_FILE_EXTENSION, newPath, false);
    fixtures.Track(newPath);

    if (!TestTrue(TEXT("Convert should succeed"), bConverted))
    {
        return false;
    }
    bool bAllRenamed = recorder.Num() > 0;
    for (int32 i = 0; i < recorder.Num(); ++i)
    {
        bAllRenamed &= (recorder[i].Kind == ESGDynamicTextAssetFileChangeKind::Renamed);
    }
    TestTrue(TEXT("Every convert event should be Renamed"), bAllRenamed);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Rename_Broadcasts_BothPaths_WithGuid,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Rename.BroadcastsBothPathsWithGuid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Rename_Broadcasts_BothPaths_WithGuid::RunTest(const FString& Parameters)
{
    // Both the fixture create and the rename re-write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("RenameBoth"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FFileChangeRecorder recorder;
    FString newPath;
    const bool bRenamed = FSGDynamicTextAssetFileManager::RenameDynamicTextAsset(
        path, fixtures.MakeUniqueId(TEXT("dst")), newPath, false);
    fixtures.Track(newPath);

    if (!TestTrue(TEXT("Rename should succeed"), bRenamed))
    {
        return false;
    }
    if (!TestEqual(TEXT("Rename should broadcast exactly two events (old + new path)"), recorder.Num(), 2))
    {
        return false;
    }

    const FRecordedChange* oldEvent = recorder.FindByPath(path);
    const FRecordedChange* newEvent = recorder.FindByPath(newPath);

    if (!TestNotNull(TEXT("Rename should broadcast for the old path"), oldEvent) ||
        !TestNotNull(TEXT("Rename should broadcast for the new path"), newEvent))
    {
        return false;
    }

    TestTrue(TEXT("Old-path rename event should be Renamed"), oldEvent->Kind == ESGDynamicTextAssetFileChangeKind::Renamed);
    TestTrue(TEXT("New-path rename event should be Renamed"), newEvent->Kind == ESGDynamicTextAssetFileChangeKind::Renamed);
    TestEqual(TEXT("Old-path event should carry the preserved source GUID"), oldEvent->Guid, id.GetGuid());
    TestEqual(TEXT("New-path event should carry the preserved source GUID"), newEvent->Guid, id.GetGuid());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Convert_Broadcasts_BothPaths_WithGuid,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Convert.BroadcastsBothPathsWithGuid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Convert_Broadcasts_BothPaths_WithGuid::RunTest(const FString& Parameters)
{
    // Both the fixture create and the convert target write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("ConvertBoth"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FFileChangeRecorder recorder;
    FString newPath;
    const bool bConverted = FSGDynamicTextAssetFileManager::ConvertFileFormat(
        path, SGDynamicTextAssetConstants::XML_FILE_EXTENSION, newPath, false);
    fixtures.Track(newPath);

    if (!TestTrue(TEXT("Convert should succeed"), bConverted))
    {
        return false;
    }
    if (!TestEqual(TEXT("Convert should broadcast exactly two events (old + new path)"), recorder.Num(), 2))
    {
        return false;
    }

    const FRecordedChange* oldEvent = recorder.FindByPath(path);
    const FRecordedChange* newEvent = recorder.FindByPath(newPath);

    if (!TestNotNull(TEXT("Convert should broadcast for the old path"), oldEvent) ||
        !TestNotNull(TEXT("Convert should broadcast for the new path"), newEvent))
    {
        return false;
    }

    TestTrue(TEXT("Old-path convert event should be Renamed"), oldEvent->Kind == ESGDynamicTextAssetFileChangeKind::Renamed);
    TestTrue(TEXT("New-path convert event should be Renamed"), newEvent->Kind == ESGDynamicTextAssetFileChangeKind::Renamed);
    TestEqual(TEXT("Old-path event should carry the GUID preserved through the round-trip"), oldEvent->Guid, id.GetGuid());
    TestEqual(TEXT("New-path event should carry the GUID preserved through the round-trip"), newEvent->Guid, id.GetGuid());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_FailedRename_EmitsNoBroadcast,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.FailedRename.EmitsNoBroadcast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_FailedRename_EmitsNoBroadcast::RunTest(const FString& Parameters)
{
    // The two fixture creates (source + destination collision) each serialize default content
    // for the type-id-less unit-test class. The rename itself fails at the "File already exists"
    // guard before any re-serialize, but the up-front creates still emit this warning.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("FailedRename"));

    // Source to rename.
    FSGDynamicTextAssetId srcId;
    FString srcPath;
    if (!TestTrue(TEXT("Source fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), srcId, srcPath)))
    {
        return false;
    }

    // Destination already occupied by another file: the rename must fail at the
    // "File already exists" guard before any write/delete/broadcast.
    const FString destUserFacingId = fixtures.MakeUniqueId(TEXT("dst"));
    FSGDynamicTextAssetId destId;
    FString destPath;
    const bool bDestCreated = FSGDynamicTextAssetFileManager::CreateDynamicTextAssetFile(
        USGDynamicTextAssetUnitTest::StaticClass(), destUserFacingId, SGDynamicTextAssetConstants::JSON_FILE_EXTENSION, destId, destPath);
    fixtures.Track(destPath);
    if (!TestTrue(TEXT("Destination collision fixture create should succeed"), bDestCreated))
    {
        return false;
    }

    AddExpectedError(TEXT("File already exists"), EAutomationExpectedErrorFlags::Contains, 1);

    FFileChangeRecorder recorder;
    FString outPath;
    const bool bRenamed = FSGDynamicTextAssetFileManager::RenameDynamicTextAsset(srcPath, destUserFacingId, outPath, false);

    TestFalse(TEXT("Renaming onto an existing destination should fail"), bRenamed);
    TestEqual(TEXT("A failed rename should emit zero broadcasts"), recorder.Num(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_WriteExisting_DefaultFlags_ChecksOutOnce,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.DefaultFlags.WriteExistingChecksOutOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_WriteExisting_DefaultFlags_ChecksOutOnce::RunTest(const FString& Parameters)
{
    // The fixture create serializes default content for the type-id-less unit-test class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("DefWriteExisting"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    // Default bAutoCheckOut=true over an existing file: exactly one checkout, no add/delete.
    const bool bWrote = FSGDynamicTextAssetFileManager::WriteRawFileContents(path, TEXT("{\"changed\":true}"));

    TestTrue(TEXT("Default-flag overwrite should succeed"), bWrote);
    TestEqual(TEXT("Default-flag overwrite should check out exactly once"), bridge.CheckOutCount, 1);
    TestEqual(TEXT("Default-flag overwrite should not mark for add"), bridge.MarkForAddCount, 0);
    TestEqual(TEXT("Default-flag overwrite should not mark for delete"), bridge.MarkForDeleteCount, 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Rename_DefaultFlags_DeletesAndAddsOnce,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.DefaultFlags.RenameDeletesAndAddsOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Rename_DefaultFlags_DeletesAndAddsOnce::RunTest(const FString& Parameters)
{
    // Both the fixture create and the rename re-write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("DefRename"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FString newPath;
    const bool bRenamed = FSGDynamicTextAssetFileManager::RenameDynamicTextAsset(path, fixtures.MakeUniqueId(TEXT("dst")), newPath);
    fixtures.Track(newPath);

    TestTrue(TEXT("Default-flag rename should succeed"), bRenamed);
    TestEqual(TEXT("Default-flag rename should mark old path for delete once"), bridge.MarkForDeleteCount, 1);
    TestEqual(TEXT("Default-flag rename should mark new path for add once"), bridge.MarkForAddCount, 1);
    TestEqual(TEXT("Default-flag rename should not check out (the new file is brand new)"), bridge.CheckOutCount, 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Duplicate_DefaultFlags_AddsOnce,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.DefaultFlags.DuplicateAddsOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Duplicate_DefaultFlags_AddsOnce::RunTest(const FString& Parameters)
{
    // Both the fixture create and the duplicate write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("DefDuplicate"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FSGDynamicTextAssetId newId;
    FString newPath;
    const bool bDuplicated = FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset(path, fixtures.MakeUniqueId(TEXT("dup")), newId, newPath);
    fixtures.Track(newPath);

    TestTrue(TEXT("Default-flag duplicate should succeed"), bDuplicated);
    TestEqual(TEXT("Default-flag duplicate should mark new path for add once"), bridge.MarkForAddCount, 1);
    TestEqual(TEXT("Default-flag duplicate should not mark for delete"), bridge.MarkForDeleteCount, 0);
    TestEqual(TEXT("Default-flag duplicate should not check out (the new file is brand new)"), bridge.CheckOutCount, 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Convert_DefaultFlags_DeletesAndAddsOnce,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.DefaultFlags.ConvertDeletesAndAddsOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Convert_DefaultFlags_DeletesAndAddsOnce::RunTest(const FString& Parameters)
{
    // Both the fixture create and the convert target write serialize the type-id-less class.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);
    FTempFixtures fixtures(TEXT("DefConvert"));

    FSGDynamicTextAssetId id;
    FString path;
    if (!TestTrue(TEXT("Fixture create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, path)))
    {
        return false;
    }

    FString newPath;
    const bool bConverted = FSGDynamicTextAssetFileManager::ConvertFileFormat(path, SGDynamicTextAssetConstants::XML_FILE_EXTENSION, newPath);
    fixtures.Track(newPath);

    TestTrue(TEXT("Default-flag convert should succeed"), bConverted);
    TestEqual(TEXT("Default-flag convert should mark old path for delete once"), bridge.MarkForDeleteCount, 1);
    TestEqual(TEXT("Default-flag convert should mark new path for add once"), bridge.MarkForAddCount, 1);
    // The inner write targets a brand-new path, so the bAutoCheckOut=true inner write does not check out.
    TestEqual(TEXT("Default-flag convert should not check out (the target file is brand new)"), bridge.CheckOutCount, 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSGDTASCC_Suite_LeavesNoResidue,
    "SGDynamicTextAssets.Runtime.Management.FileManager.SCCContract.Suite.LeavesNoResidue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDTASCC_Suite_LeavesNoResidue::RunTest(const FString& Parameters)
{
    // This test runs create + duplicate + rename + convert, each of which serializes the
    // type-id-less unit-test class. Match-any covers the combined emissions across all mutations.
    AddExpectedMessage(TEXT("No valid Asset Type ID found for class"), EAutomationExpectedMessageFlags::Contains, 0);

    FCountingSourceControlBridge bridge;
    FScopedBridge scopedBridge(&bridge);

    IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();

    // Run a create + rename + duplicate + convert sequence inside its own scope so the
    // fixture's RAII cleanup runs before we assert nothing is left behind.
    FString createdPath;
    FString renamedPath;
    FString duplicatePath;
    FString convertedPath;
    {
        FTempFixtures fixtures(TEXT("Residue"));

        FSGDynamicTextAssetId id;
        if (!TestTrue(TEXT("Create should succeed"), fixtures.CreateJsonFixture(TEXT("src"), id, createdPath)))
        {
            return false;
        }

        // Duplicate the source first (needs the source to still exist), then rename the source.
        FSGDynamicTextAssetId dupId;
        FSGDynamicTextAssetFileManager::DuplicateDynamicTextAsset(createdPath, fixtures.MakeUniqueId(TEXT("dup")), dupId, duplicatePath, false);
        fixtures.Track(duplicatePath);

        FSGDynamicTextAssetFileManager::RenameDynamicTextAsset(createdPath, fixtures.MakeUniqueId(TEXT("dst")), renamedPath, false);
        fixtures.Track(renamedPath);

        // Convert the renamed file JSON -> XML.
        FSGDynamicTextAssetFileManager::ConvertFileFormat(renamedPath, SGDynamicTextAssetConstants::XML_FILE_EXTENSION, convertedPath, false);
        fixtures.Track(convertedPath);
    }

    // After the fixture's destructor ran, none of the tracked files should remain.
    TestFalse(TEXT("Created/renamed-away source should not remain"), platformFile.FileExists(*createdPath));
    TestFalse(TEXT("Renamed file (converted away) should not remain"), platformFile.FileExists(*renamedPath));
    TestFalse(TEXT("Duplicate file should be cleaned up"), platformFile.FileExists(*duplicatePath));
    TestFalse(TEXT("Converted file should be cleaned up"), platformFile.FileExists(*convertedPath));

    return true;
}

#endif // WITH_AUTOMATION_WORKER
