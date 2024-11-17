#include <pxr/usd/usdGeom/camera.h>
#include "CameraManipulator.h"
#include "Viewport.h"
#include "Commands.h"
#include "Gui.h"

CameraManipulator::CameraManipulator(const GfVec2i &viewportSize, bool isZUp) : CameraRig(viewportSize, isZUp) {}

void CameraManipulator::OnBeginEdition(Viewport &viewport) {
    if (viewport.IsEditingStageCamera()) {
        _stageCamera = UsdGeomCamera::Get(viewport.GetCurrentStage(), viewport.GetSelectedStageCameraPath());
        BeginEdition(viewport.GetCurrentStage());
    }
}

void CameraManipulator::OnEndEdition(Viewport &viewport) {
    if (viewport.IsEditingStageCamera() && _stageCamera) {
        EndEdition();
    }
}

Manipulator *CameraManipulator::OnUpdate(Viewport &viewport) {
    auto &cameraManipulator = viewport.GetCameraManipulator();
    ImGuiIO &io = ImGui::GetIO();

    /// If the user released key alt, escape camera manipulation
    if (!ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
        return viewport.GetManipulator<MouseHoverManipulator>();
    } else if (ImGui::IsMouseReleased(1) || ImGui::IsMouseReleased(2) || ImGui::IsMouseReleased(0)) {
        SetMovementType(MovementType::None);
    } else if (ImGui::IsMouseClicked(0) && !viewport.IsEditingInternalOrthoCamera()) {
        SetMovementType(MovementType::Orbit);
    } else if (ImGui::IsMouseClicked(2)) {
        SetMovementType(MovementType::Truck);
    } else if (ImGui::IsMouseClicked(1)) {
        SetMovementType(MovementType::Dolly);
    }
    GfCamera &currentCamera = viewport.GetEditableCamera();
    // EDITABLE CAMERA should be the camera used for the viewport
    // After edition:
    // The internal and stage cameras should copy only the position and focus, etc, not frustum
    // or anything related to the viewport size.
    // OR ....
    // We edit the internal camera, but then we need the viewport info and viewport camera
    //
    // The camera manipulator need the viewport camera to compute the motion diffs
    //
    
    
    if (Move(currentCamera, io.MouseDelta.x, io.MouseDelta.y)) {
            
        // TODO: viewport.CommitEditableCamera(currentCamera);
        // and remove below
        if (viewport.IsEditingStageCamera() && _stageCamera) {
            // This is going to fill the undo/redo buffer :S
            _stageCamera.SetFromCamera(currentCamera, viewport.GetCurrentTimeCode());
        }
    }

    return this;
}
