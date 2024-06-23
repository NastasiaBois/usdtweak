#include "Constants.h"
#include "ImGuiHelpers.h"
#include "Gui.h"
#include "HydraBrowser.h"

#include <stack>
#include <iostream>
#include <pxr/pxr.h> // for PXR_VERSION

#if PXR_VERSION < 2302
void DrawHydraBrowser() {
    ImGui::Text("Hydra browser is not supported in this version of USD ");
}
#else

#include <pxr/imaging/hd/filteringSceneIndex.h>
PXR_NAMESPACE_USING_DIRECTIVE
#define HydraBrowserSeed 5343934
#define IdOf ToImGuiID<HydraBrowserSeed, size_t>

inline void DrawSceneIndexSelector(std::string &selectedSceneIndexName, std::string &selectedInputName) {
    if (ImGui::BeginCombo("Select scene index", selectedSceneIndexName.c_str())) {
        for(const auto &name : HdSceneIndexNameRegistry::GetInstance().GetRegisteredNames()) {
            if (ImGui::Selectable(name.c_str())) {
                selectedSceneIndexName = name;
                selectedInputName = "";
            }
        }
        ImGui::EndCombo();
    }
}

static void DrawSceneIndexFilterSelector(std::string &selectedSceneIndexName, HdSceneIndexBasePtr &inputIndex) {
    HdSceneIndexBaseRefPtr sceneIndex = HdSceneIndexNameRegistry::GetInstance().GetNamedSceneIndex(selectedSceneIndexName);
    
    if (sceneIndex) {
        const std::string selectedInputName = inputIndex ? inputIndex->GetDisplayName() : "";
        if (ImGui::BeginCombo("Select filter", selectedInputName.c_str())) {
            HdFilteringSceneIndexBaseRefPtr currentSceneIndex =
                    TfDynamic_cast<HdFilteringSceneIndexBaseRefPtr>(sceneIndex);
            if (currentSceneIndex) {
                const std::vector<HdSceneIndexBaseRefPtr> &inputScenes = currentSceneIndex->GetInputScenes();
                std::stack<HdSceneIndexBaseRefPtr> st;
                std::vector<int> level;
                for(const auto &scene:inputScenes) {
                    st.push(scene);
                    level.push_back(1);
                }
                while (!st.empty()){
                    HdSceneIndexBaseRefPtr scene = st.top();
                    st.pop();
                    level.back()--;
                    if (scene) {
                        std::string label;
                        for (int i=0; i<level.size(); ++i) {
                            label += "  ";
                        }
                        label += scene->GetDisplayName();
                        if (ImGui::Selectable(label.c_str(), selectedInputName == scene->GetDisplayName())) {
                            inputIndex = scene;
                        }
                        
                        HdFilteringSceneIndexBaseRefPtr filteringIndex =
                                TfDynamic_cast<HdFilteringSceneIndexBaseRefPtr>(scene);
                        if (filteringIndex) {
                            const std::vector<HdSceneIndexBaseRefPtr> &inputs = filteringIndex->GetInputScenes();
                            if (!inputs.empty()) {
                                level.push_back(0);
                                for (const auto &input:inputs) {
                                    st.push(input);
                                    level.back()++;
                                }
                            }
                        }
                    }
                    if (level.back()==0) {
                        level.resize(level.size()-1);
                    }
                }
            }
            ImGui::EndCombo();
        }
    } else {
        selectedSceneIndexName = "";
        inputIndex.Reset();
    }
    return inputIndex;
}

static void DrawSceneIndexTreeView(HdSceneIndexBasePtr inputIndex, const std::string &selectedInputName, SdfPath &selectedPrimIndexPath){
    if (inputIndex) {
        // Get all the opened paths in a vector
        ImGuiWindow *currentWindow = ImGui::GetCurrentWindow();
        ImVec2 tableOuterSize(currentWindow->Size[0]/2, currentWindow->Size[1] - 100); // TODO: set the correct size
        constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit | /*ImGuiTableFlags_RowBg |*/ ImGuiTableFlags_ScrollY;
        constexpr ImGuiTreeNodeFlags rowFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowItemOverlap;
        if (ImGui::BeginTable("##DrawSceneIndexHierarchy", 2, tableFlags, tableOuterSize)) {
            ImGui::TableSetupColumn("Hierarchy");
            ImGui::TableSetupColumn("Type");
            
            ImGuiContext &g = *GImGui;
            ImGuiWindow *window = g.CurrentWindow;
            ImGuiStorage *storage = window->DC.StateStorage;

            std::vector<SdfPath> paths;
            std::stack<SdfPath> st;
            st.push(SdfPath::AbsoluteRootPath());
            while(!st.empty()) {
                const SdfPath &current = st.top();
                const ImGuiID pathHash = IdOf(current.GetHash());
                const bool isOpen = storage->GetInt(pathHash, 0) != 0;
                paths.push_back(current);
                st.pop();
                if (isOpen) {
                    for (const SdfPath &path: inputIndex->GetChildPrimPaths(current)) {
                        st.push(path);
                    }
                }
            }

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(paths.size()));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                    ImGui::PushID(row);
                    const SdfPath &path = paths[row];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    bool unfolded = true;
                    {
                        // TODO remove triangle when leaf
                        // TODO draw node type
                        TreeIndenter<HydraBrowserSeed, SdfPath> indenter(path);
                        const ImGuiID pathHash = IdOf(GetHash(path));
                        unfolded = ImGui::TreeNodeBehavior(pathHash, rowFlags, path.GetName().c_str());
                        if (!ImGui::IsItemToggledOpen() && ImGui::IsItemClicked()) {
                            selectedPrimIndexPath = path;
                            if (ImGui::IsMouseDoubleClicked(0)) {
                                selectedPrimIndexPath = path;
                                ImGui::ClearActiveID(); // see https://github.com/ocornut/imgui/issues/6690
                            }
                        }
                    }
                    ImGui::PopID();
                    if (unfolded) {
                        ImGui::TreePop();
                    }
                }
            }
            ImGui::EndTable();
        }
    }
}

static void DrawPrimParameters(HdSceneIndexBasePtr inputIndex, const SdfPath &selectedPrimIndexPath) {
    if (inputIndex && selectedPrimIndexPath!=SdfPath::AbsoluteRootPath()) {
        // We could have added the parameters directly in the prim tree but to find the actual parameter type
        // we need to Cast it to all the potential hydra class known and, worse case, it could happen
        // for each frame and for all the parameters of the scene. So we just consider the selected parameter
        if (ImGui::BeginTable("##DrawHydraParameter", 2)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Value");
            
            ImGui::TableHeadersRow();

            HdSceneIndexPrim siPrim = inputIndex->GetPrim(selectedPrimIndexPath);
            if (siPrim.dataSource) {
                for (const TfToken &tk: siPrim.dataSource->GetNames()) {
                    // TODO tree of parameters depending on the data source class
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Selectable(tk.GetText());
                    // TODO : draw value
                }
            }
            ImGui::EndTable();
        }
    }
}

void DrawHydraBrowser() {
    static std::string selectedSceneIndexName;
    static std::string selectedInputName;
    static SdfPath selectedPrimIndexPath;
    static HdSceneIndexBasePtr selectedFilter;
    DrawSceneIndexSelector(selectedSceneIndexName, selectedInputName);
    DrawSceneIndexFilterSelector(selectedSceneIndexName, selectedFilter);
    DrawSceneIndexTreeView(selectedFilter, selectedInputName, selectedPrimIndexPath);
    ImGui::SameLine(); // TODO separator between treeview and params
    DrawPrimParameters(selectedFilter, selectedPrimIndexPath);
}
#endif
