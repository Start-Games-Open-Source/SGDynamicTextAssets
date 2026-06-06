// Copyright Start Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Core/SGDynamicTextAssetVersion.h"

/**
 * Test: Default constructor produces 1.0.0 (Major defaults to 1 per UPROPERTY ClampMin)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetVersion_DefaultConstructor_ProducesDefaultValues,
	"SGDynamicTextAssets.Runtime.Core.Version.DefaultConstructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetVersion_DefaultConstructor_ProducesDefaultValues::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetVersion version;

	TestEqual(TEXT("Default Major should be 1"), version.Major, 1);
	TestEqual(TEXT("Default Minor should be 0"), version.Minor, 0);
	TestEqual(TEXT("Default Patch should be 0"), version.Patch, 0);

	return true;
}

/**
 * Test: Parameterized constructor stores correct values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetVersion_ParameterizedConstructor_StoresCorrectValues,
	"SGDynamicTextAssets.Runtime.Core.Version.ParameterizedConstructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetVersion_ParameterizedConstructor_StoresCorrectValues::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetVersion version(2, 5, 10);

	TestEqual(TEXT("Major should be 2"), version.Major, 2);
	TestEqual(TEXT("Minor should be 5"), version.Minor, 5);
	TestEqual(TEXT("Patch should be 10"), version.Patch, 10);

	return true;
}

/**
 * Test: Negative values are clamped to minimum (Major=1, Minor/Patch=0)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetVersion_NegativeValues_AreClamped,
	"SGDynamicTextAssets.Runtime.Core.Version.NegativeValuesClamped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetVersion_NegativeValues_AreClamped::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetVersion version(-5, -10, -20);

	TestEqual(TEXT("Negative Major should clamp to 1"), version.Major, 1);
	TestEqual(TEXT("Negative Minor should clamp to 0"), version.Minor, 0);
	TestEqual(TEXT("Negative Patch should clamp to 0"), version.Patch, 0);

	return true;
}

/**
 * Test: ToString formats as "Major.Minor.Patch"
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetVersion_ToString_FormatsCorrectly,
	"SGDynamicTextAssets.Runtime.Core.Version.ToString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetVersion_ToString_FormatsCorrectly::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetVersion version(1, 2, 3);

	TestEqual(TEXT("ToString should format as Major.Minor.Patch"), version.ToString(), TEXT("1.2.3"));

	FSGDynamicTextAssetVersion largeVersion(10, 25, 100);
	TestEqual(TEXT("Large version ToString"), largeVersion.ToString(), TEXT("10.25.100"));

	return true;
}

/**
 * Test: ParseString correctly parses valid version strings
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetVersion_ParseString_ValidInput,
	"SGDynamicTextAssets.Runtime.Core.Version.ParseStringValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetVersion_ParseString_ValidInput::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetVersion version = FSGDynamicTextAssetVersion::ParseFromString(TEXT("1.2.3"));

	TestEqual(TEXT("Parsed Major"), version.Major, 1);
	TestEqual(TEXT("Parsed Minor"), version.Minor, 2);
	TestEqual(TEXT("Parsed Patch"), version.Patch, 3);

	FSGDynamicTextAssetVersion largeVersion = FSGDynamicTextAssetVersion::ParseFromString(TEXT("10.25.100"));
	TestEqual(TEXT("Parsed large Major"), largeVersion.Major, 10);
	TestEqual(TEXT("Parsed large Minor"), largeVersion.Minor, 25);
	TestEqual(TEXT("Parsed large Patch"), largeVersion.Patch, 100);

	return true;
}

/**
 * Test: ParseString handles invalid/malformed strings gracefully
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetVersion_ParseString_InvalidInput,
	"SGDynamicTextAssets.Runtime.Core.Version.ParseStringInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetVersion_ParseString_InvalidInput::RunTest(const FString& Parameters)
{
	// Empty string - FCString::Atoi returns 0 for invalid input
	FSGDynamicTextAssetVersion emptyVersion = FSGDynamicTextAssetVersion::ParseFromString(TEXT(""));
	TestEqual(TEXT("Empty string Major"), emptyVersion.Major, 1);
	TestEqual(TEXT("Empty string Minor"), emptyVersion.Minor, 0);
	TestEqual(TEXT("Empty string Patch"), emptyVersion.Patch, 0);

	// Non-numeric string
	FSGDynamicTextAssetVersion abcVersion = FSGDynamicTextAssetVersion::ParseFromString(TEXT("abc"));
	TestEqual(TEXT("abc Major"), abcVersion.Major, 0);

	// Partial version string (only 2 parts)
	FSGDynamicTextAssetVersion partialVersion = FSGDynamicTextAssetVersion::ParseFromString(TEXT("1.2"));
	TestEqual(TEXT("Partial Major"), partialVersion.Major, 1);
	TestEqual(TEXT("Partial Minor"), partialVersion.Minor, 2);
	TestEqual(TEXT("Partial Patch defaults to 0"), partialVersion.Patch, 0);

	return true;
}

/**
 * Test: Equality operators (==, !=)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetVersion_EqualityOperators,
	"SGDynamicTextAssets.Runtime.Core.Version.EqualityOperators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetVersion_EqualityOperators::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetVersion v1(1, 2, 3);
	FSGDynamicTextAssetVersion v2(1, 2, 3);
	FSGDynamicTextAssetVersion v3(1, 2, 4);
	FSGDynamicTextAssetVersion v4(2, 2, 3);

	TestTrue(TEXT("Same versions should be equal"), v1 == v2);
	TestFalse(TEXT("Same versions should not be unequal"), v1 != v2);

	TestFalse(TEXT("Different patch should not be equal"), v1 == v3);
	TestTrue(TEXT("Different patch should be unequal"), v1 != v3);

	TestFalse(TEXT("Different major should not be equal"), v1 == v4);
	TestTrue(TEXT("Different major should be unequal"), v1 != v4);

	return true;
}

/**
 * Test: Less-than operator
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetVersion_LessThanOperator,
	"SGDynamicTextAssets.Runtime.Core.Version.LessThanOperator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetVersion_LessThanOperator::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetVersion v1_0_0(1, 0, 0);
	FSGDynamicTextAssetVersion v1_0_1(1, 0, 1);
	FSGDynamicTextAssetVersion v1_1_0(1, 1, 0);
	FSGDynamicTextAssetVersion v2_0_0(2, 0, 0);

	// Patch comparison
	TestTrue(TEXT("1.0.0 < 1.0.1"), v1_0_0 < v1_0_1);
	TestFalse(TEXT("1.0.1 < 1.0.0"), v1_0_1 < v1_0_0);

	// Minor comparison
	TestTrue(TEXT("1.0.0 < 1.1.0"), v1_0_0 < v1_1_0);
	TestFalse(TEXT("1.1.0 < 1.0.0"), v1_1_0 < v1_0_0);

	// Major comparison
	TestTrue(TEXT("1.0.0 < 2.0.0"), v1_0_0 < v2_0_0);
	TestFalse(TEXT("2.0.0 < 1.0.0"), v2_0_0 < v1_0_0);

	// Equal versions
	FSGDynamicTextAssetVersion v1_0_0_copy(1, 0, 0);
	TestFalse(TEXT("Equal versions: 1.0.0 < 1.0.0"), v1_0_0 < v1_0_0_copy);

	return true;
}

/**
 * Test: Greater-than and equal operators
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetVersion_GreaterAndEqualOperators,
	"SGDynamicTextAssets.Runtime.Core.Version.GreaterAndEqualOperators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetVersion_GreaterAndEqualOperators::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetVersion v1_0_0(1, 0, 0);
	FSGDynamicTextAssetVersion v1_0_1(1, 0, 1);
	FSGDynamicTextAssetVersion v1_0_0_copy(1, 0, 0);

	// Greater-than
	TestTrue(TEXT("1.0.1 > 1.0.0"), v1_0_1 > v1_0_0);
	TestFalse(TEXT("1.0.0 > 1.0.1"), v1_0_0 > v1_0_1);
	TestFalse(TEXT("1.0.0 > 1.0.0"), v1_0_0 > v1_0_0_copy);

	// Less-than-or-equal
	TestTrue(TEXT("1.0.0 <= 1.0.1"), v1_0_0 <= v1_0_1);
	TestTrue(TEXT("1.0.0 <= 1.0.0"), v1_0_0 <= v1_0_0_copy);
	TestFalse(TEXT("1.0.1 <= 1.0.0"), v1_0_1 <= v1_0_0);

	// Greater-than-or-equal
	TestTrue(TEXT("1.0.1 >= 1.0.0"), v1_0_1 >= v1_0_0);
	TestTrue(TEXT("1.0.0 >= 1.0.0"), v1_0_0 >= v1_0_0_copy);
	TestFalse(TEXT("1.0.0 >= 1.0.1"), v1_0_0 >= v1_0_1);

	return true;
}

/**
 * Test: IsCompatibleWith - same major version is compatible
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetVersion_IsCompatibleWith_SameMajor,
	"SGDynamicTextAssets.Runtime.Core.Version.IsCompatibleWithSameMajor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetVersion_IsCompatibleWith_SameMajor::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetVersion v1_0_0(1, 0, 0);
	FSGDynamicTextAssetVersion v1_5_10(1, 5, 10);
	FSGDynamicTextAssetVersion v1_99_99(1, 99, 99);

	TestTrue(TEXT("1.0.0 compatible with 1.5.10"), v1_0_0.IsCompatibleWith(v1_5_10));
	TestTrue(TEXT("1.5.10 compatible with 1.0.0"), v1_5_10.IsCompatibleWith(v1_0_0));
	TestTrue(TEXT("1.0.0 compatible with 1.99.99"), v1_0_0.IsCompatibleWith(v1_99_99));
	TestTrue(TEXT("Version compatible with itself"), v1_0_0.IsCompatibleWith(v1_0_0));

	return true;
}

/**
 * Test: IsCompatibleWith - different major version is incompatible
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSGDynamicTextAssetVersion_IsCompatibleWith_DifferentMajor,
	"SGDynamicTextAssets.Runtime.Core.Version.IsCompatibleWithDifferentMajor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSGDynamicTextAssetVersion_IsCompatibleWith_DifferentMajor::RunTest(const FString& Parameters)
{
	FSGDynamicTextAssetVersion v1_0_0(1, 0, 0);
	FSGDynamicTextAssetVersion v2_0_0(2, 0, 0);
	FSGDynamicTextAssetVersion v3_5_10(3, 5, 10);

	TestFalse(TEXT("1.0.0 not compatible with 2.0.0"), v1_0_0.IsCompatibleWith(v2_0_0));
	TestFalse(TEXT("2.0.0 not compatible with 1.0.0"), v2_0_0.IsCompatibleWith(v1_0_0));
	TestFalse(TEXT("1.0.0 not compatible with 3.5.10"), v1_0_0.IsCompatibleWith(v3_5_10));

	return true;
}
