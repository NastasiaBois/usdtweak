#include <iostream>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/variantSets.h>
#include <pxr/usd/usdGeom/gprim.h>
#include "Gui.h"
#include "PropertyEditor.h"
#include "ValueEditor.h"
#include "Constants.h"
#include "Commands.h"

PXR_NAMESPACE_USING_DIRECTIVE

/// Select and draw the appropriate editor depending on the type, metada and so on.
/// Returns the modified value or VtValue
static VtValue DrawAttributeValue(const std::string &label, UsdAttribute &attribute, const VtValue &value) {
    // If the attribute is a TfToken, it might have an "allowedTokens" metadata
    // We assume that the attribute is a token if it has allowedToken, but that might not hold true
    VtValue allowedTokens;
    attribute.GetMetadata(TfToken("allowedTokens"), &allowedTokens);
    if (!allowedTokens.IsEmpty()) {
        return DrawTfToken(label, value, allowedTokens);
    }
    //
    if (attribute.GetRoleName() == TfToken("Color")) {
        // TODO: color values can be "dragged" they should be stored between
        // BeginEdition/EndEdition
        // It needs some refactoring to know when the widgets starts and stop edition
        return DrawColorValue(label, value);
    }
    return DrawVtValue(label, value);
}

void DrawUsdAttribute(UsdAttribute &attribute, UsdTimeCode currentTime) {
    std::string attributeLabel = attribute.GetNamespace().GetString() +
                                 (attribute.GetNamespace() == TfToken() ? std::string() : std::string(":")) +
                                 attribute.GetBaseName().GetString();
    VtValue value;
    if (attribute.Get(&value, currentTime)) {
        // TODO: have a decent layout. It looks like PushItemWidth can be useful to that end
        // ImGui::PushItemWidth(ImGui::GetWindowWidth()/2.f);
        // ImGui::SetNextItemWidth(ImGui::GetWindowWidth()/3.f);
        VtValue modified = DrawAttributeValue(attributeLabel, attribute, value);
        if (!modified.IsEmpty()) {
            ExecuteAfterDraw<AttributeSet>(attribute, modified,
                                           attribute.GetNumTimeSamples() ? currentTime : UsdTimeCode::Default());
        }
        auto attributeTypeName = attribute.GetTypeName();
        auto attributeRoleName = attribute.GetRoleName();
        ImGui::SameLine();
        ImGui::Text("%s(%s)", attributeRoleName.GetString().c_str(), attributeTypeName.GetAsToken().GetString().c_str());
    } else {
        ImVec4 attributeNameColor = attribute.IsAuthored() ? ImVec4(AttributeAuthoredColor) : ImVec4(AttributeUnauthoredColor);
        ImGui::TextColored(attributeNameColor, "'%s'", attributeLabel.c_str());
        if (attribute.HasAuthoredConnections()) {
            SdfPathVector sources;
            attribute.GetConnections(&sources);
            for (auto &connection : sources) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(AttributeConnectionColor), "-> %s", connection.GetString().c_str());
            }
        } else {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4({0.5, 0.5, 0.5, 0.5}), "no value");
        }
    }
}

void DrawUsdRelationship(UsdRelationship &relationship) {
    std::string relationshipName = relationship.GetNamespace().GetString() +
                                   (relationship.GetNamespace() == TfToken() ? std::string() : std::string(":")) +
                                   relationship.GetBaseName().GetString();
    ImVec4 attributeNameColor = relationship.IsAuthored() ? ImVec4(AttributeAuthoredColor) : ImVec4(AttributeUnauthoredColor);
    ImGui::TextColored(ImVec4(attributeNameColor), "'%s'", relationshipName.c_str());
    SdfPathVector targets;
    // relationship.GetTargets(&targets); TODO: what is the difference wiht GetForwardedTargets
    relationship.GetForwardedTargets(&targets);
    if (targets.size()) {
        ImGui::SameLine();
        for (const auto &path : targets) {
            ImGui::TextColored(ImVec4(AttributeRelationshipColor), "-> %s", path.GetString().c_str());
        }
    }
}

/// Specialization for DrawPropertyMiniButton, between UsdAttribute and UsdRelashionship
template <typename UsdPropertyT> const char *SmallButtonLabel();
template <> const char *SmallButtonLabel<UsdAttribute>() { return "(a)"; };
template <> const char *SmallButtonLabel<UsdRelationship>() { return "(r)"; };

template <typename UsdPropertyT> void DrawMenuClearAuthoredValues(UsdPropertyT &property){};
template <> void DrawMenuClearAuthoredValues(UsdAttribute &attribute) {
    if (ImGui::MenuItem("Clear Authored values")) {
        ExecuteAfterDraw(&UsdAttribute::Clear, attribute);
    }
}

template <typename UsdPropertyT> void DrawMenuRemoveProperty(UsdPropertyT &property){};
template <> void DrawMenuRemoveProperty(UsdAttribute &attribute) {
    if (ImGui::MenuItem("Remove edit")) {
        ExecuteAfterDraw(&UsdPrim::RemoveProperty, attribute.GetPrim(), attribute.GetName());
    }
}

template <typename UsdPropertyT> void DrawMenuSetKey(UsdPropertyT &property, UsdTimeCode currentTime){};
template <> void DrawMenuSetKey(UsdAttribute &attribute, UsdTimeCode currentTime) {
    if (ImGui::MenuItem("Set key")) {
        VtValue value;
        attribute.Get(&value, currentTime);
        ExecuteAfterDraw<AttributeSet>(attribute, value, currentTime);
    }
}

// Property mini button, should work with UsdProperty, UsdAttribute and UsdRelationShip
template <typename UsdPropertyT>
void DrawPropertyMiniButton(UsdPropertyT &property, const UsdEditTarget &editTarget, UsdTimeCode currentTime) {
    ImVec4 propertyColor = property.IsAuthoredAt(editTarget) ? ImVec4({0.0, 1.0, 0.0, 1.0}) : ImVec4({0.0, 0.7, 0.0, 1.0});
    ImGui::PushStyleColor(ImGuiCol_Text, propertyColor);
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    ImGui::SmallButton(SmallButtonLabel<UsdPropertyT>());
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    if (ImGui::BeginPopupContextItem()) {
        if (property.IsAuthoredAt(editTarget)) {
            DrawMenuClearAuthoredValues(property);
            DrawMenuRemoveProperty(property);
        }
        DrawMenuSetKey(property, currentTime);
        ImGui::EndPopup();
    }
}

void DrawVariantSetsCombos(const UsdPrim &prim) {
    auto variantSets = prim.GetVariantSets();
    for (auto variantSetName : variantSets.GetNames()) {
        auto variantSet = variantSets.GetVariantSet(variantSetName);
        ImVec4 variantColor =
            variantSet.HasAuthoredVariantSelection() ? ImVec4({0.0, 1.0, 0.0, 1.0}) : ImVec4({0.0, 0.7, 0.0, 1.0});
        ImGui::PushStyleColor(ImGuiCol_Text, variantColor);
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
        ImGui::SmallButton("(v)");
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Remove edit")) {
                ExecuteAfterDraw(&UsdVariantSet::ClearVariantSelection, variantSet);
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();

        if (ImGui::BeginCombo(variantSetName.c_str(), variantSet.GetVariantSelection().c_str())) {
            for (auto variant : variantSet.GetVariantNames()) {
                if (ImGui::Selectable(variant.c_str(), false)) {
                    ExecuteAfterDraw(&UsdVariantSet::SetVariantSelection, variantSet, variant);
                }
            }
            ImGui::EndCombo();
        }
    }
}

void DrawUsdPrimProperties(UsdPrim &prim, UsdTimeCode currentTime) {
    if (prim) {
        auto editTarget = prim.GetStage()->GetEditTarget();
        ImGui::Text("Edit target: %s", editTarget.GetLayer()->GetDisplayName().c_str());
        ImGui::Text("%s %s", prim.GetTypeName().GetString().c_str(), prim.GetPrimPath().GetString().c_str());

        // Draw variant sets
        if (prim.HasVariantSets()) {
            ImGui::Separator();
            ImGui::Text("VariantSets selection");
            DrawVariantSetsCombos(prim);
        }
        ImGui::Separator();

        DrawXformsCommon(prim, currentTime);

        int miniButtonId = 0;

        // Draw attributes
        for (auto &attribute : prim.GetAttributes()) {
            ImGui::PushID(miniButtonId++);
            DrawPropertyMiniButton(attribute, editTarget, currentTime);
            ImGui::PopID();
            ImGui::SameLine();
            DrawUsdAttribute(attribute, currentTime);
        }

        // Draw relations
        for (auto &relation : prim.GetRelationships()) {
            ImGui::PushID(miniButtonId++);
            DrawPropertyMiniButton(relation, editTarget, currentTime);
            ImGui::PopID();
            ImGui::SameLine();
            DrawUsdRelationship(relation);
        }
    }
}

void DrawXformsCommon(UsdPrim &prim, UsdTimeCode currentTime) {
    UsdGeomXformCommonAPI xformAPI(prim);

    if (xformAPI) {
        // For testing
        if (ImGui::Button("Create transform")) {
            auto ops = xformAPI.CreateXformOps(UsdGeomXformCommonAPI::OpTranslate, UsdGeomXformCommonAPI::OpRotate,
                                    UsdGeomXformCommonAPI::OpPivot, UsdGeomXformCommonAPI::OpScale);
            ops.translateOp.Set(GfVec3d(0.0, 0.0, 0.0));
            ops.scaleOp.Set(GfVec3d(1.0, 1.0, 1.0));
        }

        GfVec3d translation;
        GfVec3f scalev;
        GfVec3f pivot;
        GfVec3f rotation;
        UsdGeomXformCommonAPI::RotationOrder rotOrder;
        xformAPI.GetXformVectors(&translation, &rotation, &scalev, &pivot, &rotOrder, currentTime);

        ImGui::Text("Transform API");

        GfVec3f translationf(translation[0], translation[1], translation[2]);
        ImGui::InputFloat3("Translation", translationf.data(), DecimalPrecision);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            translation[0] = translationf[0]; // TODO: InputDouble3 instead, we don't want to loose values
            translation[1] = translationf[1];
            translation[2] = translationf[2];
            ExecuteAfterDraw(&UsdGeomXformCommonAPI::SetTranslate, xformAPI, translation, currentTime);
        }
        ImGui::InputFloat3("Rotation", rotation.data(), DecimalPrecision);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            ExecuteAfterDraw(&UsdGeomXformCommonAPI::SetRotate, xformAPI, rotation, rotOrder, currentTime);
        }
        ImGui::InputFloat3("Scale", scalev.data(), DecimalPrecision);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            ExecuteAfterDraw(&UsdGeomXformCommonAPI::SetScale, xformAPI, scalev, currentTime);
        }
        ImGui::InputFloat3("Pivot", pivot.data(), DecimalPrecision);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            ExecuteAfterDraw(&UsdGeomXformCommonAPI::SetPivot, xformAPI, pivot, currentTime);
        }

    }
}
