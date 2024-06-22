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

void DrawHydraBrowser() {

    static std::string selectedSceneIndexName;
    static std::string selectedInputName;
    static SdfPath selectedPrimIndexPath;
    
    if (ImGui::BeginCombo("Select scene index", selectedSceneIndexName.c_str())) {
        for(const auto &name : HdSceneIndexNameRegistry::GetInstance().GetRegisteredNames()) {
            if (ImGui::Selectable(name.c_str())) {
                selectedSceneIndexName = name;
                selectedInputName = "";
            }
        }
        ImGui::EndCombo();
    }
    
    HdSceneIndexBaseRefPtr sceneIndex = HdSceneIndexNameRegistry::GetInstance().GetNamedSceneIndex(selectedSceneIndexName);
    if (sceneIndex) {
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
                            selectedInputName = scene->GetDisplayName();
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
        selectedInputName = "";
    }

    // TODO scene with tree view
    if (!selectedInputName.empty()) {
        HdSceneIndexBaseRefPtr inputIndex = HdSceneIndexNameRegistry::GetInstance().GetNamedSceneIndex(selectedSceneIndexName);
        
        // Draw the paths
        if (inputIndex) {
            // Get all the paths in a vector in depth first
            std::vector<SdfPath> paths;
            std::stack<SdfPath> st;
            st.push(SdfPath::AbsoluteRootPath());
            while(!st.empty()) {
                const SdfPath &current = st.top();
                paths.push_back(current);
                st.pop();
                for (const SdfPath &path: inputIndex->GetChildPrimPaths(current)) {
                    st.push(path);
                }
            }
        
            //ImGui::Text(typeid(inputIndex).name());
            if (ImGui::BeginListBox("Paths")) {
                for (const SdfPath &path:paths) {
                    if (ImGui::Selectable(path.GetText())) {
                        selectedPrimIndexPath = path;
                    }
                }
                ImGui::EndListBox();
            }
            
            // We could have added the parameters directly in the prim tree but to find the actual parameter type
            // we need to Cast it to all the potential hydra class known and, worse case, it could happen
            // for each frame and for all the parameters of the scene. So we just consider the selected parameter
            if (selectedPrimIndexPath!=SdfPath::AbsoluteRootPath()) {
                if (ImGui::BeginTable("##DrawHydraParameter", 2)) {
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("Value");
                    
                    ImGui::TableHeadersRow();
                    
                    HdSceneIndexPrim siPrim = inputIndex->GetPrim(selectedPrimIndexPath);
                    if (siPrim.dataSource) {
                        for (const TfToken &tk: siPrim.dataSource->GetNames()) {
                            ImGui::Selectable(tk.GetText());
                        }
                    }
                    
                    ImGui::EndTable();
                }
        
            }
        }
    }
}
#endif
