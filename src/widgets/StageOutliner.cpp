#include <iostream>

#include <vector>

#include <pxr/usd/pcp/layerStack.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/gprim.h>

#include "Commands.h"
#include "Constants.h"
#include "Gui.h"
#include "ImGuiHelpers.h"
#include "PropertyEditor.h" // for DrawUsdPrimEditTarget
#include "StageOutliner.h"
#include "VtValueEditor.h"

static void ExploreLayerTree(SdfLayerTreeHandle tree, PcpNodeRef node) {
    if (!tree)
        return;
    auto obj = tree->GetLayer()->GetObjectAtPath(node.GetPath());
    if (obj) {
        std::string format;
        format += tree->GetLayer()->GetDisplayName();
        format += " ";
        format += obj->GetPath().GetString();
        if (ImGui::MenuItem(format.c_str())) {
            ExecuteAfterDraw<EditorInspectLayerLocation>(tree->GetLayer(), obj->GetPath());
        }
    }
    for (auto subTree : tree->GetChildTrees()) {
        ExploreLayerTree(subTree, node);
    }
}

static void ExploreComposition(PcpNodeRef root) {
    auto tree = root.GetLayerStack()->GetLayerTree();
    ExploreLayerTree(tree, root);
    TF_FOR_ALL(childNode, root.GetChildrenRange()) { ExploreComposition(*childNode); }
}

static void DrawUsdPrimEditMenuItems(const UsdPrim &prim) {
    if (ImGui::MenuItem("Toggle active")) {
        const bool active = !prim.IsActive();
        ExecuteAfterDraw(&UsdPrim::SetActive, prim, active);
    }
    // TODO: Load and Unload are not in the undo redo :( ... make a command for them
    if (prim.HasAuthoredPayloads() && prim.IsLoaded() && ImGui::MenuItem("Unload")) {
        ExecuteAfterDraw(&UsdPrim::Unload, prim);
    }
    if (prim.HasAuthoredPayloads() && !prim.IsLoaded() && ImGui::MenuItem("Load")) {
        ExecuteAfterDraw(&UsdPrim::Load, prim, UsdLoadWithDescendants);
    }
    if (ImGui::MenuItem("Copy prim path")) {
        ImGui::SetClipboardText(prim.GetPath().GetString().c_str());
    }
    if (ImGui::BeginMenu("Edit layer")) {
        ImGui::SetClipboardText(prim.GetPath().GetString().c_str());
        auto pcpIndex = prim.ComputeExpandedPrimIndex();
        if (pcpIndex.IsValid()) {
            auto rootNode = pcpIndex.GetRootNode();
            ExploreComposition(rootNode);
        }
        ImGui::EndMenu();
    }
}

static ImVec4 GetPrimColor(const UsdPrim &prim) {
    if (!prim.IsActive() || !prim.IsLoaded()) {
        return ImVec4(ColorPrimInactive);
    }
    if (prim.IsInstance()) {
        return ImVec4(ColorPrimInstance);
    }
    const auto hasCompositionArcs = prim.HasAuthoredReferences() || prim.HasAuthoredPayloads() || prim.HasAuthoredInherits() ||
                                    prim.HasAuthoredSpecializes() || prim.HasVariantSets();
    if (hasCompositionArcs) {
        return ImVec4(ColorPrimHasComposition);
    }
    if (prim.IsPrototype() || prim.IsInPrototype() || prim.IsInstanceProxy()) {
        return ImVec4(ColorPrimPrototype);
    }
    return ImVec4(ColorPrimDefault);
}

///// Recursive function to draw a prim and its descendants
// static void DrawPrimTreeNode(const UsdPrim &prim, Selection &selectedPaths) {
//    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
//    const auto &children = prim.GetFilteredChildren(UsdTraverseInstanceProxies(UsdPrimAllPrimsPredicate));
//    if (children.empty()) {
//        flags |= ImGuiTreeNodeFlags_Leaf;
//    }
//    if (IsSelected(selectedPaths, prim.GetPath())) {
//        flags |= ImGuiTreeNodeFlags_Selected;
//    }
//
//    bool unfolded = true;
//    {
//        ScopedStyleColor primColor(ImGuiCol_Text, GetPrimColor(prim));
//        unfolded = ImGui::TreeNodeEx(prim.GetName().GetText(), flags);
//        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
//            SetSelected(selectedPaths, prim.GetPath());
//        }
//        {
//            ScopedStyleColor popupColor(ImGuiCol_Text, ImVec4(ColorPrimDefault));
//            if (ImGui::BeginPopupContextItem()) {
//                DrawUsdPrimEditMenuItems(prim);
//                ImGui::EndPopup();
//            }
//        }
//        ImGui::NextColumn();
//
//        // Get visibility parameter.
//        // Is it really useful ???
//        UsdGeomImageable imageable(prim);
//        const char* icon = "";
//        if (imageable) {
//            VtValue visible;
//            imageable.GetVisibilityAttr().Get(&visible);
//            icon = visible == TfToken("invisible") ? ICON_FA_EYE_SLASH : ICON_FA_EYE;
//        }
//
//        ImGui::Text("%s %s", icon, prim.GetTypeName().GetText());
//    }
//
//    ImGui::NextColumn(); // Back to the first column
//
//    if (unfolded) {
//        if (prim.IsActive()) {
//            for (const auto &child : children) {
//                DrawPrimTreeNode(child, selectedPaths);
//            }
//        }
//        ImGui::TreePop();
//    }
//}

static void DrawVisibilityButton(const UsdPrim &prim) {
    constexpr const char *inheritedIcon = ICON_FA_QUESTION_CIRCLE;
    UsdGeomImageable imageable(prim);
    if (imageable) {
        // 4 possible states:
        // 1. no edit on the attribute -> default inherited
        // 2. edit inherited on the attribute -
        // 3. edit visible on the attribute
        // 4. edit invisible on the attribute
        VtValue visibleValue;
        auto attr = imageable.GetVisibilityAttr();
        if (attr.HasAuthoredValue()) {
            attr.Get(&visibleValue);

            const TfToken &visibilityToken = visibleValue.Get<TfToken>();
            const bool invisible = visibilityToken == UsdGeomTokens->visible;
            // const char *icon =  invisible ? ICON_FA_EYE_SLASH : ICON_FA_EYE;

            const char *icon = visibilityToken == UsdGeomTokens->invisible
                                   ? ICON_FA_EYE_SLASH
                                   : (visibilityToken == UsdGeomTokens->visible ? ICON_FA_EYE : inheritedIcon);
            ScopedStyleColor buttonColor(ImGuiCol_Text, ImVec4(1.0, 1.0, 1.0, 1.0));
            if (ImGui::SmallButton(icon)) {
                UsdTimeCode tc = UsdTimeCode::Default();
                if (visibilityToken == UsdGeomTokens->inherited) {
                    ExecuteAfterDraw<AttributeSet>(attr, VtValue(UsdGeomTokens->visible), tc);
                } else if (visibilityToken == UsdGeomTokens->visible) {
                    ExecuteAfterDraw<AttributeSet>(attr, VtValue(UsdGeomTokens->invisible), tc);
                } else if (visibilityToken == UsdGeomTokens->invisible) {
                    ExecuteAfterDraw(&UsdPrim::RemoveProperty, prim, attr.GetName());
                }
            }
        } else {
            ScopedStyleColor buttonColor(ImGuiCol_Text, ImVec4(ColorPrimInactive));
            if (ImGui::SmallButton(inheritedIcon)) {
                UsdTimeCode tc = UsdTimeCode::Default();
                ExecuteAfterDraw<AttributeSet>(attr, VtValue(UsdGeomTokens->inherited), tc);
            }
        }
    }
}

static void DrawPrimTreeRow(const UsdPrim &prim, Selection &selectedPaths) {
    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowItemOverlap; // for testing worse case scenario add | ImGuiTreeNodeFlags_DefaultOpen;
    // Another way ???
    const auto &children = prim.GetFilteredChildren(UsdTraverseInstanceProxies(UsdPrimAllPrimsPredicate));
    if (children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (IsSelected(selectedPaths, prim.GetPath())) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    bool unfolded = true;
    {
        ScopedStyleColor primColor(ImGuiCol_Text, GetPrimColor(prim));
        const ImGuiID pathHash = ToImGuiID(prim.GetPath().GetHash());
        // std::cout << pathHash << std::endl;
        unfolded = ImGui::TreeNodeBehavior(pathHash, flags, prim.GetName().GetText());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            SetSelected(selectedPaths, prim.GetPath());
        }
        {
            ScopedStyleColor popupColor(ImGuiCol_Text, ImVec4(ColorPrimDefault));
            if (ImGui::BeginPopupContextItem()) {
                DrawUsdPrimEditMenuItems(prim);
                ImGui::EndPopup();
            }
        }

        // Visibility
        ImGui::TableSetColumnIndex(1);
        DrawVisibilityButton(prim);

        // Type
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%s", prim.GetTypeName().GetText());
    }
    if (unfolded) {
        ImGui::TreePop();
    }
}

static void DrawStageTreeRow(const UsdStageRefPtr &stage, Selection &selectedPaths) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGuiTreeNodeFlags nodeflags = ImGuiTreeNodeFlags_OpenOnArrow;
    std::string stageDisplayName(stage->GetRootLayer()->GetDisplayName());
    auto unfolded = ImGui::TreeNodeBehavior(ToImGuiID(SdfPath::AbsoluteRootPath().GetHash()), nodeflags, stageDisplayName.c_str());

    ImGui::TableSetColumnIndex(2);
    ImGui::SmallButton(ICON_FA_PEN);
    if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft)) {
        UsdPrim &selected =
            IsSelectionEmpty(selectedPaths) ? stage->GetPseudoRoot() : stage->GetPrimAtPath(GetSelectedPath(selectedPaths));
        DrawUsdPrimEditTarget(selected);
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::Text("%s", stage->GetEditTarget().GetLayer()->GetDisplayName().c_str());
    if (unfolded) {
        ImGui::TreePop();
    }
}

/// This function should be called only when the Selection has changed
/// It modifies the internal imgui tree graph state.
static void OpenSelectedPaths(Selection &selectedPaths) {
    ImGuiContext &g = *GImGui;
    ImGuiWindow *window = g.CurrentWindow;
    ImGuiStorage *storage = window->DC.StateStorage;
    for (const auto &path : GetSelectedPaths(selectedPaths)) {
        for (const auto &element : path.GetParentPath().GetPrefixes()) {
            ImGuiID id = ToImGuiID(element.GetHash()); // This has changed with the optim one
            storage->SetInt(id, true);
        }
    }
}

// Traverse the stage skipping the paths closed by the tree ui.
static void TraverseOpenedPaths(UsdStageRefPtr stage, std::vector<SdfPath> &paths) {
    if (!stage)
        return;
    ImGuiContext &g = *GImGui;
    ImGuiWindow *window = g.CurrentWindow;
    ImGuiStorage *storage = window->DC.StateStorage;
    paths.clear();
    const SdfPath &rootPath = SdfPath::AbsoluteRootPath();
    const bool rootPathIsOpen = storage->GetInt(ToImGuiID(rootPath.GetHash()), 0) != 0;
    if (rootPathIsOpen) {
        auto range = UsdPrimRange::Stage(stage, UsdTraverseInstanceProxies(UsdPrimAllPrimsPredicate));
        for (auto iter = range.begin(); iter != range.end(); ++iter) {
            const auto path = iter->GetPath();
            const ImGuiID pathHash = ToImGuiID(path.GetHash());
            const bool isOpen = storage->GetInt(pathHash, 0) != 0;
            if (!isOpen) {
                iter.PruneChildren();
            }
            paths.push_back(path);
        }
    }
}

// Correctly indent the tree nodes using the path
// It allocates a SdfPathVector which might not be optimal, but is called only on the visible paths, that should mitigate the
// allocation
struct TreeIndenter {
    TreeIndenter(const SdfPath &path) {
        path.GetPrefixes(&prefixes);
        for (int i = 0; i < prefixes.size(); ++i) {
            ImGui::TreePushOverrideID(ToImGuiID(prefixes[i].GetHash()));
        }
    }
    ~TreeIndenter() {
        for (int i = 0; i < prefixes.size(); ++i) {
            ImGui::TreePop();
        }
    }
    SdfPathVector prefixes;
};

static void FocusedOnFirstSelectedPath(const Selection &selectedPaths, const std::vector<SdfPath> &paths,
                                       ImGuiListClipper &clipper) {
    const auto &selectedPath = GetSelectedPath(selectedPaths);
    // linear search! it happens only when the selection has changed. But we might want to maintain a map instead
    // if the scenes are big.
    for (int i = 0; i < paths.size(); ++i) {
        if (paths[i] == selectedPath) {
            // scroll only if the item is not visible
            if (i < clipper.DisplayStart || i > clipper.DisplayEnd) {
                ImGui::SetScrollY(clipper.ItemsHeight * i + 1);
            }
            return;
        }
    }
}

/// Draw the hierarchy of the stage
void DrawStageOutliner(UsdStageRefPtr stage, Selection &selectedPaths) {
    if (!stage)
        return;

    constexpr unsigned int textBufferSize = 512;
    static char buf[textBufferSize];
    bool addprimclicked = false;
    auto rootPrim = stage->GetPseudoRoot();
    auto layer = stage->GetSessionLayer();

    static SelectionHash lastSelectionHash = 0;

    const auto cursorPos = ImGui::GetCursorPos();
    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit | /*ImGuiTableFlags_RowBg |*/ ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##DrawStageOutliner", 3, tableFlags)) {
        ImGui::TableSetupScrollFreeze(3, 1); // Freeze the root node of the tree (the layer)
        ImGui::TableSetupColumn("Hierarchy");
        ImGui::TableSetupColumn("V", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("Type");

        // Unfold the selected path
        const bool selectionHasChanged = UpdateSelectionHash(selectedPaths, lastSelectionHash);
        if (selectionHasChanged) {            // We could use the imgui id as well instead of a static ??
            OpenSelectedPaths(selectedPaths); // Also we could have a UsdTweakFrame which contains all the changes that happened
                                              // between the last frame and the new one
        }

        // Find all the opened paths
        std::vector<SdfPath> paths;
        paths.reserve(1024);
        TraverseOpenedPaths(stage, paths); // This must be inside the table to get the correct treenode hash table

        // Draw the tree root node, the layer
        DrawStageTreeRow(stage, selectedPaths);

        // Display only the visible paths with a clipper
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(paths.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                ImGui::PushID(row);
                const SdfPath &path = paths[row];
                const auto &prim = stage->GetPrimAtPath(path);
                TreeIndenter indenter(path);
                DrawPrimTreeRow(prim, selectedPaths);
                ImGui::PopID();
            }
        }
        if (selectionHasChanged) {
            // This function can only be called in this context and after the clipper.Step()
            FocusedOnFirstSelectedPath(selectedPaths, paths, clipper);
        }
        ImGui::EndTable();
        
        // Debug info
        ImGui::SetCursorPos(ImVec2(500 + cursorPos.x, 25 + cursorPos.y));
        ImGui::Text("%d paths", paths.size());
    }
}
