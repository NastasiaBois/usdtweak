#include "ViewportCameras.h"
#include "Constants.h"
#include "Commands.h"
#include "Gui.h"
#include "ImGuiHelpers.h"
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/base/gf/rotation.h>

static const std::string PerspectiveStr("Persp");
static const std::string TopStr("Top");
static const std::string BottomStr("Bottom");

static void DrawViewportCameraEditor(GfCamera &camera, const UsdStageRefPtr &stage) {
    float focal = camera.GetFocalLength();
    ImGui::InputFloat("Focal length", &focal);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        camera.SetFocalLength(focal);
    }
    GfRange1f clippingRange = camera.GetClippingRange();
    ImGui::InputFloat2("Clipping range", (float *)&clippingRange);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        camera.SetClippingRange(clippingRange);
    }

    if (ImGui::Button("New camera")) {
        // Find the next camera path
        std::string cameraPath = UsdGeomCameraDefaultPrefix;
        for (int cameraNumber = 1; stage->GetPrimAtPath(SdfPath(cameraPath)); cameraNumber++) {
            cameraPath = std::string(UsdGeomCameraDefaultPrefix) + std::to_string(cameraNumber);
        }
        if (stage) {
            // It's not worth creating a command, just use a function
            std::function<void()> duplicateCamera = [camera, cameraPath, stage]() {
                UsdGeomCamera newGeomCamera = UsdGeomCamera::Define(stage, SdfPath(cameraPath));
                newGeomCamera.SetFromCamera(camera, UsdTimeCode::Default());
            };
            ExecuteAfterDraw<UsdFunctionCall>(stage, duplicateCamera);
        }
    }
}

static void DrawUsdGeomCameraEditor(const UsdGeomCamera &usdGeomCamera, UsdTimeCode keyframeTimeCode) {
    auto camera = usdGeomCamera.GetCamera(keyframeTimeCode);
    float focal = camera.GetFocalLength();
    ImGui::InputFloat("Focal length", &focal);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        auto attr = usdGeomCamera.GetFocalLengthAttr(); // this is not ideal as
        VtValue value(focal);
        ExecuteAfterDraw<AttributeSet>(attr, value, keyframeTimeCode);
    }
    GfRange1f clippingRange = camera.GetClippingRange();
    ImGui::InputFloat2("Clipping range", (float *)&clippingRange);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        auto attr = usdGeomCamera.GetClippingRangeAttr();
        VtValue value(GfVec2f(clippingRange.GetMin(), clippingRange.GetMax()));
        ExecuteAfterDraw<AttributeSet>(attr, value, keyframeTimeCode);
    }

    if (ImGui::Button("Duplicate camera")) {
        // TODO: We probably want to duplicate this camera prim using the same parent
        // as the movement of the camera can be set on the parents
        // so basically copy the whole prim as a sibling, find the next available name
    }
}

ViewportCameras::OwnedCameras::OwnedCameras () {
    GfMatrix4d mat;
    _topCamera.SetProjection(GfCamera::Orthographic);
    _topCamera.SetClippingRange({0.1, 20000.f});
    mat.SetTransform(GfRotation({1.0, 0.0, 0.0}, -90.f), {0.0, 10000.f, 0.0});
    _topCamera.SetTransform(mat);
    
    _bottomCamera.SetProjection(GfCamera::Orthographic);
    _bottomCamera.SetClippingRange({0.1, 20000.f});
    mat.SetTransform(GfRotation({1.0, 0.0, 0.0}, 90.f), {0.0, -10000.f, 0.0});
    _bottomCamera.SetTransform(mat);
}

std::unordered_map<std::string, ViewportCameras::OwnedCameras> ViewportCameras::_viewportCamerasPerStage = {};

ViewportCameras::ViewportCameras() {
    // Create the no stage camera
    if (_viewportCamerasPerStage.find("") == _viewportCamerasPerStage.end()) {
        _viewportCamerasPerStage[""] = OwnedCameras(); // No stage
    }
    // No stage configuration
    _perStageConfiguration[""] = CameraConfiguration();
    _currentConfig = &_perStageConfiguration[""];
    _ownedCameras = &_viewportCamerasPerStage[""];
    // Default to perpective camera
    _renderCamera = &_ownedCameras->_perspectiveCamera;
}

void ViewportCameras::DrawCameraEditor(const UsdStageRefPtr &stage, UsdTimeCode tc) {
    ScopedStyleColor defaultStyle(DefaultColorStyle);
    if (IsUsingStageCamera()) {
        DrawUsdGeomCameraEditor(UsdGeomCamera::Get(stage, _currentConfig->_stageCameraPath), tc);
    } else {
        DrawViewportCameraEditor(*_renderCamera, stage);
    }
}

// This could be UseStageCamera
void ViewportCameras::UseStageCamera(const UsdStageRefPtr &stage, const SdfPath &cameraPath) {
    _currentConfig->_renderCameraType = StageCamera;
    _currentConfig->_stageCameraPath = cameraPath;
    _renderCamera = &_stageCamera;
}

void ViewportCameras::UseInternalCamera(const UsdStageRefPtr &stage, CameraType cameraType) {

    switch (cameraType) {
        case ViewportPerspective:
            _renderCamera = &_ownedCameras->_perspectiveCamera;
            break;
        case ViewportTop:
            _renderCamera = &_ownedCameras->_topCamera;
            break;
        case ViewportBottom:
            _renderCamera = &_ownedCameras->_bottomCamera;
            break;
        default:
            assert("ViewportCameras: unknown camera type" && false);
            return;
    }
    _currentConfig->_stageCameraPath = SdfPath::EmptyPath();
    _currentConfig->_renderCameraType = cameraType;
}

// This 'Update' function should update the internal data of this class.
// Many things can changed between 2 consecutive frames,
//   - the stage can be different
//   - the timecode can be different
//   - the stage camera might have been deleted
//   - the user might have modified the current camera
// This function should handle all those cases
void ViewportCameras::Update(const UsdStageRefPtr &stage, UsdTimeCode tc) {
    
    if (stage != _currentStage) {
        // The stage has changed, update the current configuration and the stage
        _currentConfig = & _perStageConfiguration[stage->GetRootLayer()->GetIdentifier()];
        _currentStage = stage;
        _ownedCameras = &_viewportCamerasPerStage[stage->GetRootLayer()->GetIdentifier()];
    }
    
    // TODO may be we should just check if the TC have changed
    // and in that case update the internals ?
    // Although the camera can be modified outside in the parameter editor,
    // so we certainly need the get the most up to date value
    if (IsUsingStageCamera()) {
        const auto stageCameraPrim = UsdGeomCamera::Get(stage, _currentConfig->_stageCameraPath);
        if (stageCameraPrim) {
            _stageCamera = stageCameraPrim.GetCamera(tc);
            UseStageCamera(stage, _currentConfig->_stageCameraPath); // passing the camera path is a bit redundant
        } else {
            // This happens when the camera has been deleted
            // There is always a Persp camera so we revert to this one
            UseInternalCamera(stage, ViewportPerspective);
        }
    } else { // Using internal camera
        UseInternalCamera(stage, _currentConfig->_renderCameraType);
    }
}

const SdfPath & ViewportCameras::GetStageCameraPath() const {
    if (IsUsingStageCamera()) {
        return _currentConfig->_stageCameraPath;
    }
    return SdfPath::EmptyPath();
}


void ViewportCameras::SetCameraAspectRatio(int width, int height) {
    if (GetCurrentCamera().GetProjection() == GfCamera::Perspective) {
        GetEditableCamera().SetPerspectiveFromAspectRatioAndFieldOfView(double(width) / double(height),
                                                                        _renderCamera->GetFieldOfView(GfCamera::FOVVertical),
                                                                        GfCamera::FOVVertical);
    } else { // assuming ortho
        GetEditableCamera().SetOrthographicFromAspectRatioAndSize(double(width) / double(height),
                                                                  _renderCamera->GetVerticalAperture() * GfCamera::APERTURE_UNIT,
                                                                  GfCamera::FOVVertical);
    }
}

std::string ViewportCameras::GetCurrentCameraName() const {
    if (IsPerspective()) {
        return PerspectiveStr;
    } else if (IsTop()) {
        return TopStr;
    } else if (IsBottom()) {
        return BottomStr;
    } else {
        return _currentConfig->_stageCameraPath.GetName();
    }
}

// Draw the list of cameras available for the stage
void ViewportCameras::DrawCameraList(const UsdStageRefPtr &stage) {
    ScopedStyleColor defaultStyle(DefaultColorStyle);
#if ENABLE_MULTIPLE_VIEWPORTS
    if (ImGui::Button("Persp")) {
        UseInternalCamera(stage, ViewportPerspective);
    }
    ImGui::SameLine();
    ImGui::Button("Front");
    ImGui::SameLine();
    ImGui::Button("Back");
    ImGui::SameLine();
    ImGui::Button("Left");
    ImGui::SameLine();
    ImGui::Button("Right");
    ImGui::SameLine();
    if (ImGui::Button("Top")) {
        UseInternalCamera(stage, ViewportTop);
    }
    ImGui::SameLine();
    if (ImGui::Button("Bottom")) {
        UseInternalCamera(stage, ViewportBottom);
    }
#endif

    if (ImGui::BeginListBox("##CameraList")) {
#if !ENABLE_MULTIPLE_VIEWPORTS
        if (ImGui::Selectable(PerspectiveStr.c_str(), IsPerspective())) {
            UseInternalCamera(stage, ViewportPerspective);
        }
#endif
        if (stage) {
            UsdPrimRange range = stage->Traverse();
            for (const auto &prim : range) {
                if (prim.IsA<UsdGeomCamera>()) {
                    ImGui::PushID(prim.GetPath().GetString().c_str());
                    const bool isSelected = _currentConfig->_renderCameraType == StageCamera && (prim.GetPath() == _currentConfig->_stageCameraPath);
                    if (ImGui::Selectable(prim.GetName().data(), isSelected)) {
                        UseStageCamera(stage, prim.GetPath());
                    }
                    if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > 2) {
                        ImGui::SetTooltip("%s", prim.GetPath().GetString().c_str());
                    }
                    ImGui::PopID();
                }
            }
        }
        ImGui::EndListBox();
    }
}
   
