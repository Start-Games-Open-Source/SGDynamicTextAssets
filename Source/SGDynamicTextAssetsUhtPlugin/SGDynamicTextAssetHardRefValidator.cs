// Copyright Start Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace SGDynamicTextAssetsUhtPlugin
{
	[UnrealHeaderTool]
	static class SGDynamicTextAssetHardRefValidator
	{
		[UhtExporter(
			Name = "SGDynamicTextAssetHardRefValidator",
			Description = "Validates ISGDynamicTextAssetProvider implementors have no hard reference UPROPERTYs",
			ModuleName = "SGDynamicTextAssetsRuntime",
			Options = UhtExporterOptions.Default)]
		private static void ValidateHardReferences(IUhtExportFactory factory)
		{
			new Validator(factory).Run();
		}
	}

	internal class Validator
	{
		private const string SkipMetaKey = "SGSkipHardRefValidation";
		private const string ErrorPrefix = "ISGDynamicTextAssetProvider implementors cannot contain hard reference UPROPERTYs. ";

		private readonly IUhtExportFactory _factory;
		private UhtSession Session => _factory.Session;

		public Validator(IUhtExportFactory factory)
		{
			_factory = factory;
		}

		public void Run()
		{
			// Find USGDynamicTextAssetProvider (the UInterface class) in the parsed type hierarchy.
			// We detect by interface implementation, not class hierarchy, so that custom
			// providers that do NOT inherit from USGDynamicTextAsset are also validated.
			UhtClass? providerInterface = FindClassByName("ISGDynamicTextAssetProvider");
			if (providerInterface == null)
			{
				// Module doesn't contain USGDynamicTextAssetProvider  - nothing to validate
				Session.AddMessage(new UhtMessage
				{
					MessageType = UhtMessageType.Info,
					Message = "SGDynamicTextAssetHardRefValidator: ISGDynamicTextAssetProvider not found in parsed types, skipping validation"
				});
				return;
			}

			// Walk ALL modules (not just ours) to find implementors across the project
			int classCount = 0;
			WalkAllTypes(type =>
			{
				if (type is UhtClass classType
					&& classType != providerInterface
					&& (classType.ClassFlags & EClassFlags.Interface) == 0
					&& ClassOrAncestorImplementsInterface(classType, providerInterface))
				{
					classCount++;
					ValidateProperties(classType, providerInterface);
				}
			});

			Session.AddMessage(new UhtMessage
			{
				MessageType = UhtMessageType.Info,
				Message = $"SGDynamicTextAssetHardRefValidator: Scanned {classCount} ISGDynamicTextAssetProvider implementor(s)"
			});
		}

		/// <summary>
		/// Validates all UPROPERTYs on an ISGDynamicTextAssetProvider implementor for hard references.
		/// Only checks properties directly declared on this class (not inherited).
		/// Inherited properties are validated when the declaring class is scanned.
		/// </summary>
		private void ValidateProperties(UhtClass classType, UhtClass providerInterface)
		{
			// Classes can opt out with UCLASS(meta=(SGSkipHardRefValidation))
			if (classType.MetaData.ContainsKey(SkipMetaKey))
			{
				return;
			}

			UhtClass? boundaryClass = FindProviderBoundaryClass(classType, providerInterface);

			foreach (UhtType child in classType.Children)
			{
				if (child is not UhtProperty property)
				{
					continue;
				}

				// Skip properties declared on the provider boundary class or its ancestors.
				// These are provider contract properties (DynamicTextAssetId, UserFacingId, Version, etc.)
				if (boundaryClass != null && property.Outer is UhtClass ownerClass
					&& boundaryClass.IsChildOf(ownerClass))
				{
					continue;
				}

				// Individual properties can opt out with UPROPERTY(meta=(SGSkipHardRefValidation))
				if (property.MetaData.ContainsKey(SkipMetaKey))
				{
					continue;
				}

				CheckPropertyForHardRef(property, property.SourceName);
			}
		}

		/// <summary>
		/// Recursively checks a property for hard reference types.
		/// Emits LogError on the property for each violation found.
		/// Check order matters  - derived types must be checked before base types.
		/// </summary>
		private void CheckPropertyForHardRef(UhtProperty property, string context)
		{
			// ALLOWED: Soft references (check BEFORE hard refs due to inheritance)
			if (property is UhtSoftClassProperty || property is UhtSoftObjectProperty)
			{
				return;
			}

			// ALLOWED: Weak/lazy object pointers
			if (property is UhtWeakObjectPtrProperty || property is UhtLazyObjectPtrProperty)
			{
				return;
			}

			// ALLOWED: All structs (value types, no hard UObject references)
			if (property is UhtStructProperty)
			{
				return;
			}

			// VIOLATION: TSubclassOf<> (UhtClassProperty inherits UhtObjectProperty  - check first!)
			if (property is UhtClassProperty)
			{
				property.LogError(
					ErrorPrefix + $"'{context}' is TSubclassOf<>. Use TSoftClassPtr<> instead.");
				return;
			}

			// ALLOWED: Instanced objects (UPROPERTY(Instanced)) are owned sub-objects
			// serialized inline, not external asset references
			if (property is UhtObjectProperty objectProp
				&& (property.PropertyFlags & EPropertyFlags.InstancedReference) != 0)
			{
				UhtClass? instancedClass = objectProp.Class;
				if (instancedClass != null)
				{
					// Check 1: EditInlineNew is required for instanced sub-objects
					if ((instancedClass.ClassFlags & EClassFlags.EditInlineNew) == 0)
					{
						property.LogError(
							ErrorPrefix + $"Instanced property '{context}' references class {instancedClass.SourceName} which does not have EditInlineNew in its UCLASS. This is treated as a hard reference.");
						return;
					}

					// Check 2: Recursively check the instanced class for nested hard references
					foreach (UhtType child in instancedClass.Children)
					{
						if (child is UhtProperty nestedProp
							&& !nestedProp.MetaData.ContainsKey(SkipMetaKey))
						{
							CheckPropertyForHardRef(nestedProp,
								$"{context} (Instanced {instancedClass.SourceName}) -> {nestedProp.SourceName}");
						}
					}

					// Both checks passed, instanced object is allowed
					return;
				}
			}

			// VIOLATION: TObjectPtr<> or raw UObject*
			if (property is UhtObjectProperty)
			{
				property.LogError(
					ErrorPrefix + $"'{context}' is TObjectPtr<>/UObject*. Use TSoftObjectPtr<> instead.");
				return;
			}

			// CONTAINERS: Recursively check inner types
			if (property is UhtArrayProperty arrayProp)
			{
				CheckPropertyForHardRef(arrayProp.ValueProperty, $"{context} (TArray element)");
				return;
			}

			if (property is UhtMapProperty mapProp)
			{
				CheckPropertyForHardRef(mapProp.KeyProperty, $"{context} (TMap key)");
				CheckPropertyForHardRef(mapProp.ValueProperty, $"{context} (TMap value)");
				return;
			}

			if (property is UhtSetProperty setProp)
			{
				CheckPropertyForHardRef(setProp.ValueProperty, $"{context} (TSet element)");
				return;
			}

			// Everything else (int32, float, FString, FName, enums, delegates, interfaces, etc.) is fine
		}

		/// <summary>
		/// Finds the highest class in the hierarchy that implements ISGDynamicTextAssetProvider.
		/// Properties declared at or above this class are exempt (provider contract properties).
		/// </summary>
		private static UhtClass? FindProviderBoundaryClass(UhtClass classType, UhtClass providerInterface)
		{
			UhtClass? highestProvider = null;
			for (UhtClass? current = classType; current != null; current = current.SuperClass)
			{
				if (ClassImplementsInterface(current, providerInterface))
				{
					highestProvider = current;
				}
			}

			return highestProvider;
		}

		/// <summary>
		/// Checks if a class directly lists the given interface in its Bases.
		/// Replicates UhtClass.ImplementsInterface which is private.
		/// </summary>
		private static bool ClassImplementsInterface(UhtClass classType, UhtClass interfaceClass)
		{
			foreach (UhtStruct baseStruct in classType.Bases)
			{
				if (baseStruct.IsChildOf(interfaceClass))
				{
					return true;
				}
			}

			return false;
		}

		/// <summary>
		/// Checks if a class or any of its ancestors directly implements the given interface.
		/// ClassImplementsInterface only checks direct declarations; this walks the SuperClass chain.
		/// </summary>
		private static bool ClassOrAncestorImplementsInterface(UhtClass classType, UhtClass interfaceClass)
		{
			for (UhtClass? current = classType; current != null; current = current.SuperClass)
			{
				if (ClassImplementsInterface(current, interfaceClass))
				{
					return true;
				}
			}

			return false;
		}

		/// <summary>
		/// Walks every UhtType across all modules and packages, invoking the action on each.
		/// </summary>
		private void WalkAllTypes(Action<UhtType> action)
		{
			foreach (UhtModule module in Session.Modules)
			{
				foreach (UhtPackage package in module.Packages)
				{
					WalkTypeTree(package, action);
				}
			}
		}

		/// <summary>
		/// Recursively walks a UhtType and all its children, invoking the action on each.
		/// </summary>
		private static void WalkTypeTree(UhtType type, Action<UhtType> action)
		{
			action(type);

			foreach (UhtType child in type.Children)
			{
				WalkTypeTree(child, action);
			}
		}

		/// <summary>
		/// Finds a UHT class by name across all parsed packages.
		/// </summary>
		private UhtClass? FindClassByName(string className)
		{
			UhtClass? result = null;

			WalkAllTypes(type =>
			{
				if (result == null && type is UhtClass classType
					&& classType.SourceName == className)
				{
					result = classType;
				}
			});

			return result;
		}
	}
}
