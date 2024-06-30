#include "CameraRig.h"
#include "Constants.h"
#include "GeometricFunctions.h"
#include <pxr/base/gf/bbox3d.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/transform.h>

PXR_NAMESPACE_USING_DIRECTIVE

///
/// Camera manipulator, this is copying how usdview freeCamera.py works
///

// TODO: Set near and far plane automatically - we need the stage BBox and closest object bbox
//

//
using DistT = float;
using RotationT = GfVec3d;

/// Updates manipulator internal
// similar to _pullFromCameraTransform in usdviewq
// Get internals from the camera transform
static void FromCameraTransform(const GfCamera &camera, const GfMatrix4d &zUpMatrix, GfVec3d &center, RotationT &rotation,
                                DistT &dist) {
    const auto &frustum = camera.GetFrustum();
    const auto &cameraPosition = frustum.GetPosition();
    const auto &cameraAxis = frustum.ComputeViewDirection();
    auto transform = camera.GetTransform() * zUpMatrix;
    transform.Orthonormalize();
    auto camRotation = transform.ExtractRotation();

    // Theta, Phi, Psi
    rotation = -camRotation.Decompose(GfVec3d::YAxis(), GfVec3d::XAxis(), GfVec3d::ZAxis());
    dist = camera.GetFocusDistance();
    center = cameraPosition + dist * cameraAxis;
}

static void ToCameraTransform(GfCamera &camera, const GfMatrix4d &zUpMatrix, const GfVec3d &center, const RotationT &rotation,
                              const DistT &dist) {
    GfMatrix4d trans;
    trans.SetTranslate(GfVec3d::ZAxis() * dist);
    GfMatrix4d roty(1.0);
    GfMatrix4d rotz(1.0);
    GfMatrix4d rotx(1.0);
    roty.SetRotate(GfRotation(GfVec3d::YAxis(), -rotation[0]));
    rotx.SetRotate(GfRotation(GfVec3d::XAxis(), -rotation[1]));
    rotz.SetRotate(GfRotation(GfVec3d::ZAxis(), -rotation[2]));
    GfMatrix4d toCenter;
    toCenter.SetTranslate(center);
    camera.SetTransform(trans * rotz * rotx * roty * zUpMatrix.GetInverse() * toCenter);
    camera.SetFocusDistance(dist);
}

CameraRig::CameraRig(const GfVec2i &viewportSize, bool isZUp)
    : _movementType(MovementType::None), _selectionSize(1.0), _viewportSize(viewportSize) {
    SetZIsUp(isZUp);
}

// TODO: function name is misleading, it doesn't reset the position, it resets the proj mat
void CameraRig::ResetPosition(GfCamera &camera) {
    GfRotation rotf;
    if (camera.GetProjection() == GfCamera::Perspective) {
        camera.SetPerspectiveFromAspectRatioAndFieldOfView(16.0 / 9.0, 60, GfCamera::FOVHorizontal);
        constexpr float focusDistance = 100.f;
        camera.SetFocusDistance(focusDistance);
    } else if (camera.GetProjection() == GfCamera::Orthographic) {
        camera.SetOrthographicFromAspectRatioAndSize(16.0 / 9.0, camera.GetVerticalAperture() * GfCamera::APERTURE_UNIT,
                                                     GfCamera::FOVHorizontal);
    }
}

void CameraRig::SetZIsUp(bool isZUp) { _zUpMatrix = GfMatrix4d().SetRotate(GfRotation(GfVec3d::XAxis(), isZUp ? -90 : 0)); }

/// Frame a bounding box.
void CameraRig::FrameBoundingBox(GfCamera &camera, const GfBBox3d &bbox) {
    if (bbox.GetVolume() == 0) {
        return;
    }

    if (camera.GetProjection() == GfCamera::Perspective) {
        RotationT rotation;
        GfVec3d center;
        
        FromCameraTransform(camera, _zUpMatrix, center, rotation, _dist);
        
        center = bbox.ComputeCentroid();
        
        auto bboxRange = bbox.ComputeAlignedRange();
        auto rect = bboxRange.GetMax() - bboxRange.GetMin();
        _selectionSize = std::max(rect[0], rect[1]) * 2; // This reset the selection size
        auto fov = camera.GetFieldOfView(GfCamera::FOVHorizontal);
        auto lengthToFit = _selectionSize * 0.5;
        _dist = lengthToFit / atan(fov * 0.5 * (PI_F / 180.f));
        ToCameraTransform(camera, _zUpMatrix, center, rotation, _dist);
    } else { // Assuming Ortho case
        // Move the viewpoint to the center of the bounding box
        // TODO: We should make sure that the camera viewpoint ends up outside of all bounding boxes
        // Unfortunately we don't have this information here, we know only the selected bbox,
        // so by default we put the camera far away from it (hence the -1000) and with clipping plane centered around 0.
        // This is not great and that could cause visible issues on larges scenes. We should get the whole scene bounding box
        // so that we can correctly position and set clipping plane of internal cameras.
        // This could be done when the user call "Fit camera"
        const GfFrustum frustum = camera.GetFrustum();
        GfMatrix4d mat = camera.GetTransform();
        mat.SetTranslateOnly(bbox.ComputeCentroid() - 1000.f*frustum.ComputeViewDirection());
        camera.SetTransform(mat);
        
        // Compute framing
        const GfRange3d bboxRange = bbox.ComputeAlignedRange();
        const GfVec3d frameSize = bboxRange.GetSize();
        const float maxSize = fmax(frameSize[0], fmax(frameSize[1], frameSize[2]));
        const float aspectRatio = camera.GetAspectRatio();
        camera.SetOrthographicFromAspectRatioAndSize(aspectRatio, maxSize, aspectRatio > 1.f ? GfCamera::FOVVertical : GfCamera::FOVHorizontal);
    }
}

// Updates the transform matrix of a GfCamera depending on the movement
bool CameraRig::Move(GfCamera &camera, double deltaX, double deltaY) {
    RotationT rotation;
    GfVec3d center;
    if (deltaX == 0 && deltaY == 0)
        return false;
    FromCameraTransform(camera, _zUpMatrix, center, rotation, _dist);

    if (_movementType == MovementType::Orbit) { // TUMBLE
        rotation[0] += deltaX;
        rotation[1] += deltaY;
        ToCameraTransform(camera, _zUpMatrix, center, rotation, _dist);
    } else if (_movementType == MovementType::Truck) {
        auto frustum = camera.GetFrustum();
        auto up = frustum.ComputeUpVector();
        auto cameraAxis = frustum.ComputeViewDirection();
        auto right = GfCross(cameraAxis, up);
        // Pixel to world - usdview behavior
        double pixelToWorld = 1.0;
        if (camera.GetProjection() == GfCamera::Orthographic) {
            pixelToWorld = camera.GetVerticalAperture() * GfCamera::APERTURE_UNIT / static_cast<double>(_viewportSize[1]);
        } else {
            const GfRange2d &window = frustum.GetWindow();
            pixelToWorld = window.GetSize()[1] * _dist / static_cast<double>(_viewportSize[1]);
        }
        center += -deltaX * right * pixelToWorld + deltaY * up * pixelToWorld;
        ToCameraTransform(camera, _zUpMatrix, center, rotation, _dist);
    } else if (_movementType == MovementType::Dolly) { // Not really a dolly in the orthographic case
        auto scaleFactor = 1.0 + -0.002 * (deltaX + deltaY);
        if (camera.GetProjection() == GfCamera::Orthographic) {
            auto value = camera.GetVerticalAperture() * GfCamera::APERTURE_UNIT * (scaleFactor);
            camera.SetOrthographicFromAspectRatioAndSize(camera.GetAspectRatio(), value, GfCamera::FOVVertical);
        } else {
            if (scaleFactor > 1.0 && _dist < 2.0) {
                const auto selBasedIncr = _selectionSize / 25.0;
                scaleFactor -= 1.0;
                _dist += std::min(selBasedIncr, scaleFactor);
            } else {
                _dist *= scaleFactor;
            }
            ToCameraTransform(camera, _zUpMatrix, center, rotation, _dist);
        }
    } else {
        return false; // NO updates
    }
    return true;
}
