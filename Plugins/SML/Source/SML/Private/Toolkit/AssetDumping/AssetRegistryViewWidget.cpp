#include "Toolkit/AssetDumping/AssetRegistryViewWidget.h"
#include "AssetRegistryModule.h"
#include "SatisfactoryModLoader.h"
#include "Toolkit/AssetDumping/AssetTypeSerializerMacros.h"
#define LOCTEXT_NAMESPACE "SML"

bool FSelectedAssetsStruct::ProcessIncludedPathAsset(const FAssetData& AssetData) {
    // Skip assets located in excluded package paths
    if (ExcludedPackagePaths.Contains(AssetData.PackagePath)) {
        return true;
    }
    // Skip assets that have been explicitly excluded by package name
    if (ExcludedPackageNames.Contains(AssetData.PackageName)) {
        return true;
    }
    // Skip assets that we have already included
    if (GatheredAssetPackages.Contains(AssetData.ObjectPath)) {
        return true;
    }
    this->GatheredAssetPackages.Add(AssetData.ObjectPath, AssetData);
    return true;
}

// Here we ignore all kind of exludes, since these assets are forcibly included
bool FSelectedAssetsStruct::ProcessForciblyIncludedAsset(const FAssetData& AssetData) {
    // Skip assets that we have already included
    if (GatheredAssetPackages.Contains(AssetData.ObjectPath)) {
        return true;
    }
    this->GatheredAssetPackages.Add(AssetData.ObjectPath, AssetData);
    return true;
}

void FSelectedAssetsStruct::AddIncludedPackagePath(const FString& PackagePath) {
    this->IncludedPackagePaths.Add(PackagePath);
}

void FSelectedAssetsStruct::AddIncludedPackageName(const FString& PackageName) {
    this->IncludedPackageNames.Add(PackageName);
}

void FSelectedAssetsStruct::AddExcludedPackagePath(const FString& PackagePath) {
    this->ExcludedPackagePaths.Add(*PackagePath);

    // We need to append PackagePath, but also expand it's sub-paths
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.GetRegistry();

    AssetRegistry.EnumerateSubPaths(
        PackagePath,
        [this](const FString& PackageSubPath) {
            this->ExcludedPackagePaths.Add(*PackageSubPath);
            return true;
        },
        true
    );
}

void FSelectedAssetsStruct::AddExcludedPackageName(const FString& PackageName) {
    this->ExcludedPackageNames.Add(*PackageName);
}

void FSelectedAssetsStruct::AddAssetClassWhitelist(FName AssetClass) {
    this->AssetClassesWhitelist.Add(AssetClass);
}

void FSelectedAssetsStruct::GatherAssetsData() {
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.GetRegistry();

    // First retrieve assets by included paths, matching them recursively and applying excludes
    if (IncludedPackagePaths.Num()) {
        FARFilter PackagePathsFilter{};
        PackagePathsFilter.bRecursivePaths = true;
        PackagePathsFilter.PackagePaths.Append(IncludedPackagePaths);
        PackagePathsFilter.ClassNames.Append(AssetClassesWhitelist);
        AssetRegistry.EnumerateAssets(
            PackagePathsFilter,
            [this](const FAssetData& AssetData) {
                return ProcessIncludedPathAsset(AssetData);
            }
        );
    }

    // Then retrieve assets matched by individual package names, these assets take priority over excludes and don't need them
    if (IncludedPackageNames.Num()) {
        FARFilter PackageNamesFilter{};
        PackageNamesFilter.PackageNames.Append(IncludedPackageNames);
        PackageNamesFilter.ClassNames.Append(AssetClassesWhitelist);
        AssetRegistry.EnumerateAssets(
            PackageNamesFilter,
            [this](const FAssetData& AssetData) {
                return ProcessForciblyIncludedAsset(AssetData);
            }
        );
    }
}

FAssetTreeNode::FAssetTreeNode() {
    this->bIsLeafNode = false;
    this->AssetClass = NAME_None;
    this->bChildrenNodesInitialized = false;
    this->bIsOverridingParentState = false;
    this->bIsChecked = false;
}

void FAssetTreeNode::RegenerateChildren() {
    if (bIsLeafNode) {
        return;
    }
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.GetRegistry();

    // Retrieve sub paths without recursing into them, we want to populate them lazily
    TArray<FString> OutSubPaths;
    AssetRegistry.GetSubPaths(Path, OutSubPaths, false);

    // Append sub path nodes first into the resulting array
    for (const FString& AssetSubPath : OutSubPaths) {
        TSharedPtr<FAssetTreeNode> Node = MakeChildNode();
        Node->bIsLeafNode = false;
        Node->NodeName = FPaths::GetCleanFilename(AssetSubPath);
        Node->Path = AssetSubPath;
    }

    // Now attempt to resolve assets at the provided path
    TArray<FAssetData> OutAssetsAtPath;
    AssetRegistry.GetAssetsByPath(*Path, OutAssetsAtPath, false, false);
    for (const FAssetData& AssetData : OutAssetsAtPath) {
        TSharedPtr<FAssetTreeNode> Node = MakeChildNode();
        Node->bIsLeafNode = true;
        Node->NodeName = AssetData.AssetName.ToString();
        Node->Path = AssetData.PackageName.ToString();
        Node->AssetClass = AssetData.AssetClass;
    }
}

TSharedPtr<FAssetTreeNode> FAssetTreeNode::MakeChildNode() {
    TSharedPtr<FAssetTreeNode> Node(new FAssetTreeNode);
    Node->ParentNode = SharedThis(this);
    Node->bIsChecked = bIsChecked;
    Children.Add(Node);
    return Node;
}

void FAssetTreeNode::UpdateSelectedState(bool bIsCheckedNew, bool bIsSetByParent) {
    this->bIsChecked = bIsCheckedNew;

    // We reset override state when selected state is updated by parent
    if (bIsSetByParent) {
        this->bIsOverridingParentState = false;
    } else {
        // Otherwise we reset it if it matches parent state or set it to true otherwise
        const TSharedPtr<FAssetTreeNode> ParentNodePinned = ParentNode.Pin();
        const bool bIsParentChecked = ParentNodePinned.IsValid() ? ParentNodePinned->IsChecked() : false;
        // If updated state matches parent state, we should remove override
        if (bIsParentChecked == bIsCheckedNew) {
            this->bIsOverridingParentState = false;
        } else {
            // Otherwise our state differs from the parents one, so we are overriding it
            this->bIsOverridingParentState = true;
        }
    }

    // Propagate state update to children widgets
    for (const TSharedPtr<FAssetTreeNode>& ChildNode : Children) {
        ChildNode->UpdateSelectedState(bIsCheckedNew, true);
    }
}

void FAssetTreeNode::GetChildrenNodes(TArray<TSharedPtr<FAssetTreeNode>>& OutChildrenNodes) {
    if (!bChildrenNodesInitialized) {
        this->bChildrenNodesInitialized = true;
        RegenerateChildren();
    }
    OutChildrenNodes.Append(Children);
}

void FAssetTreeNode::PopulateSelectedAssets(const TSharedPtr<FSelectedAssetsStruct>& SelectedAssets) {
    // We only need to append anything into the list if we are overriding parent state
    if (bIsOverridingParentState) {
        if (bIsChecked) {
            // We are checked, so we represent either an included path or included package name
            if (bIsLeafNode) {
                // We are a leaf node, so we represent an included package name
                SelectedAssets->AddIncludedPackageName(Path);
            } else {
                // We are not a leaf node, we represent a package path to be included
                SelectedAssets->AddIncludedPackagePath(Path);
            }
        } else {
            // We are unchecked, so we represent excluded package name or path
            if (bIsLeafNode) {
                // Leaf node, excluded package nae
                SelectedAssets->AddExcludedPackageName(Path);
            } else {
                // Non-leaf node, excluded sub-path inside of the included path
                SelectedAssets->AddExcludedPackagePath(Path);
            }
        }
    }

    // Dispatch call to children nodes
    for (const TSharedPtr<FAssetTreeNode>& ChildNode : Children) {
        ChildNode->PopulateSelectedAssets(SelectedAssets);
    }
}

SAssetRegistryViewWidget::SAssetRegistryViewWidget() {
    // Create root node and populate root paths using it's children paths
    this->RootNode = MakeShareable(new FAssetTreeNode);
    this->RootNode->bIsLeafNode = false;
    this->RootNode->Path = TEXT("/");
    this->RootNode->GetChildrenNodes(this->RootAssetPaths);
}

void SAssetRegistryViewWidget::Construct(const FArguments& InArgs) {
    ChildSlot[
        SNew(STreeView<TSharedPtr<FAssetTreeNode>>)
        .HeaderRow(
                                                       SNew(SHeaderRow)
                                                       + SHeaderRow::Column(TEXT("ShouldDump"))
                                                         .DefaultLabel(LOCTEXT("AssetDumper_ColumnShouldDump", "Dump"))
                                                         .FixedWidth(50)
                                                         .HAlignCell(HAlign_Center)
                                                         .HAlignHeader(HAlign_Center)
                                                       + SHeaderRow::Column(TEXT("Path"))
                                                       .DefaultLabel(LOCTEXT("AssetDumper_ColumnPath", "Asset Path"))
                                                       + SHeaderRow::Column(TEXT("AssetClass"))
                                                         .DefaultLabel(LOCTEXT("AssetDumper_ColumnAssetClass", "Asset Class"))
                                                         .FixedWidth(100)
                                                         .HAlignCell(HAlign_Left)
                                                         .HAlignHeader(HAlign_Center)
                                                   )
        .SelectionMode(ESelectionMode::None)
        .OnGenerateRow_Raw(this, &SAssetRegistryViewWidget::OnCreateRow)
        .OnGetChildren_Raw(this, &SAssetRegistryViewWidget::GetNodeChildren)
        .TreeItemsSource(&this->RootAssetPaths)
    ];
}

void SAssetRegistryViewWidget::PopulateSelectedAssets(const TSharedPtr<FSelectedAssetsStruct>& SelectedAssets) const {
    RootNode->PopulateSelectedAssets(SelectedAssets);
}

void SAssetRegistryViewWidget::GetNodeChildren(const TSharedPtr<FAssetTreeNode> TreeNode, TArray<TSharedPtr<FAssetTreeNode>>& OutChildren) const {
    TreeNode->GetChildrenNodes(OutChildren);
}

void SAssetTreeNodeRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedPtr<FAssetTreeNode>& TreeNodeArg) {
    this->TreeNode = TreeNodeArg;
    FSuperRowType::Construct(FTableRowArgs(), OwnerTable);
}

TSharedRef<SWidget> SAssetTreeNodeRow::GenerateWidgetForColumn(const FName& InColumnName) {
    if (InColumnName == TEXT("Path")) {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()[
                SNew(SExpanderArrow, SharedThis(this))
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f)[
                SNew(STextBlock)
                .Text(FText::FromString(TreeNode->NodeName))
            ];
    }
    if (InColumnName == TEXT("AssetClass")) {
        if (TreeNode->bIsLeafNode) {
            return SNew(STextBlock)
                .Text(FText::FromString(TreeNode->AssetClass.ToString()));
        }
        return SNullWidget::NullWidget;
    }
    return SNew(SCheckBox)
        .IsChecked_Lambda(
                              [this]() {
                                  return TreeNode->IsChecked() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                              }
                          )
        .OnCheckStateChanged_Lambda(
                              [this](ECheckBoxState NewState) {
                                  const bool bIsChecked = NewState == ECheckBoxState::Checked;
                                  TreeNode->UpdateSelectedState(bIsChecked, false);
                              }
                          );
}

TSharedRef<ITableRow> SAssetRegistryViewWidget::OnCreateRow(const TSharedPtr<FAssetTreeNode> TreeNode, const TSharedRef<STableViewBase>& Owner) const {
    return SNew(SAssetTreeNodeRow, Owner, TreeNode);
}

#define CHECK_AND_LOG_PARAM(ParamName, TitleText) \
    if (ParamName.Num()) { \
        UE_LOG(LogSatisfactoryModLoader, Display, TitleText); \
        for (const decltype(ParamName)::ElementType& Element : ParamName) { \
            UE_LOG(LogSatisfactoryModLoader, Display, TEXT("- %s"), *FAssetTypeSerializerMacros_Internals::ValueToString<decltype(ParamName)::ElementType>(Element)); \
        } \
    }

void FSelectedAssetsStruct::LogSettings() {
    UE_LOG(LogSatisfactoryModLoader, Display, TEXT("================= BEGIN SETTINGS FOR ASSET GATHERER ================"));

    CHECK_AND_LOG_PARAM(AssetClassesWhitelist, TEXT("Whitelisted Asset Classes: "));
    CHECK_AND_LOG_PARAM(IncludedPackagePaths, TEXT("Included Package Paths: "));
    CHECK_AND_LOG_PARAM(IncludedPackageNames, TEXT("Force Included Packages: "));
    CHECK_AND_LOG_PARAM(ExcludedPackagePaths, TEXT("Excluded Package Paths: "));
    CHECK_AND_LOG_PARAM(ExcludedPackageNames, TEXT("Excluded Packages: "));

    UE_LOG(LogSatisfactoryModLoader, Display, TEXT("================== END SETTINGS FOR ASSET GATHERER ================="));
}

void FSelectedAssetsStruct::FindUnknownAssetClasses(const TArray<FName>& KnownAssetClasses, TArray<FName>& OutUnknownClasses) {
    TSet<FName> KnownAssetClassesSet;
    KnownAssetClassesSet.Append(KnownAssetClasses);
    TSet<FName> UnknownAssetClassesSet;

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.GetRegistry();
    AssetRegistry.EnumerateAllAssets(
        [&](const FAssetData& AssetData) {
            if (!KnownAssetClassesSet.Contains(AssetData.AssetClass)) {
                UnknownAssetClassesSet.Add(AssetData.AssetClass);
            }
            return true;
        }
    );
    OutUnknownClasses = UnknownAssetClassesSet.Array();
}
