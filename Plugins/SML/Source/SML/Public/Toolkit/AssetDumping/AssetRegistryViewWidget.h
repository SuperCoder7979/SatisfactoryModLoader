#pragma once
#include "Slate.h"

/** Struct used to gather data about selected assets from the asset tree */
struct SML_API FSelectedAssetsStruct {
private:
    /** Package paths we should include, will also include all sub-paths recursively */
    TArray<FString> IncludedPackagePaths;
    /** Individual assets we should include, should contain full package names */
    TArray<FString> IncludedPackageNames;

    /** Packages with these names will be excluded from the results */
    TSet<FName> ExcludedPackageNames;
    /** Package paths to exclude. Does not provide recursive lookup, you should expand paths beforehand manually */
    TArray<FName> ExcludedPackagePaths;

    /** When not empty, only assets of the specified classes are included into the search result */
    TArray<FName> AssetClassesWhitelist;

    /** Asset packages already gathered */
    TMap<FName, FAssetData> GatheredAssetPackages;

    /** Called by asset registry to process asset being enumerated */
    bool ProcessIncludedPathAsset(const FAssetData& AssetData);
    bool ProcessForciblyIncludedAsset(const FAssetData& AssetData);
public:
    /** Appends included package path and all sub paths recursively */
    void AddIncludedPackagePath(const FString& PackagePath);

    /** Appends excluded package path and all sub paths recursively */
    void AddExcludedPackagePath(const FString& PackagePath);

    /** Adds package name to be included into the asset data. This takes priority over any kind of excludes */
    void AddIncludedPackageName(const FString& PackageName);

    /** Adds package name to be excluded from asset data. This has lower priority than AddIncludedPackageName but higher than AddIncludedPackagePath */
    void AddExcludedPackageName(const FString& PackageName);

    /** Adds asset class into the whitelist */
    void AddAssetClassWhitelist(FName AssetClass);

    /** Performs asset registry lookup using included and excluded asset paths */
    void GatherAssetsData();

    /** Logs active settings for this instance into the game log */
    void LogSettings();

    /** Find unknown asset classes in the asset registry */
    static void FindUnknownAssetClasses(const TArray<FName>& KnownAssetClasses, TArray<FName>& OutUnknownClasses);

    /** Returns the map of gathered assets. Keep in mind you need to use GatherAssetsData before using it */
    FORCEINLINE const TMap<FName, FAssetData>& GetGatheredAssets() const { return GatheredAssetPackages; }
};

struct SML_API FAssetTreeNode : TSharedFromThis<FAssetTreeNode> {
public:
    /** Whenever this node represents a complete asset path, or just a directory */
    bool bIsLeafNode;
    /** Full Path to the represented asset OR directory */
    FString Path;
    /** Last fragment of the path, representing this node's name */
    FString NodeName;
    /** Class of the asset, if this info represents an asset */
    FName AssetClass;

    FAssetTreeNode();
private:
    /** True if this asset and all underlying assets are to be dumped */
    bool bIsChecked;
    /** True if our state has been explicitly overriden by the user and we should be listed as a manual override */
    bool bIsOverridingParentState;

    /** True when children nodes have already been initialized */
    bool bChildrenNodesInitialized;
    TWeakPtr<FAssetTreeNode> ParentNode;
    TArray<TSharedPtr<FAssetTreeNode>> Children;

    void RegenerateChildren();
    TSharedPtr<FAssetTreeNode> MakeChildNode();
public:
    FORCEINLINE bool IsChecked() const { return bIsChecked; }

    /** Updates selection state of the element and all of it's children */
    void UpdateSelectedState(bool bIsChecked, bool bIsSetByParent);

    /** Retrieves a list of children nodes associated with this node */
    void GetChildrenNodes(TArray<TSharedPtr<FAssetTreeNode>>& OutChildrenNodes);

    /** Appends assets referenced by this node to selected assets list */
    void PopulateSelectedAssets(const TSharedPtr<FSelectedAssetsStruct>& SelectedAssets);
};

class SML_API SAssetRegistryViewWidget : public SCompoundWidget {
public:
SLATE_BEGIN_ARGS(SAssetRegistryViewWidget) {
        }

    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Populates selected assets struct with data selected by the user through this widget */
    void PopulateSelectedAssets(const TSharedPtr<FSelectedAssetsStruct>& SelectedAssets) const;

    SAssetRegistryViewWidget();
protected:
    TSharedPtr<FAssetTreeNode> RootNode;
    TArray<TSharedPtr<FAssetTreeNode>> RootAssetPaths;

    TSharedRef<class ITableRow> OnCreateRow(const TSharedPtr<FAssetTreeNode> TreeNode, const TSharedRef<STableViewBase>& Owner) const;
    void GetNodeChildren(const TSharedPtr<FAssetTreeNode> TreeNode, TArray<TSharedPtr<FAssetTreeNode>>& OutChildren) const;
};

class SML_API SAssetTreeNodeRow : public SMultiColumnTableRow<TSharedPtr<FAssetTreeNode>> {
SLATE_BEGIN_ARGS(SAssetTreeNodeRow) {
        }

    SLATE_END_ARGS()

public:
    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedPtr<FAssetTreeNode>& TreeNodeArg);
    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override;
private:
    TSharedPtr<FAssetTreeNode> TreeNode;
};
