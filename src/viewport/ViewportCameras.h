#pragma once
#include <unordered_map>
#include <string>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/camera.h>

PXR_NAMESPACE_USING_DIRECTIVE
//
// Manage and store the cameras owned by a viewport.
// A camera can be a stage camera or an internal viewport camera.
// There are 2 kinds of internal cameras: perspective and ortho
//
// This class also provide a UI to select the current camera.
//
class ViewportCameras {
public:
    ViewportCameras();

    // The update function is called before every rendered frame.
    // This updates the internals of this structure following the changes that
    // happened between the previous frame and now
    void Update(const UsdStageRefPtr &stage, UsdTimeCode tc);
    
    // Change the camera aspect ratio
    void SetCameraAspectRatio(int width, int height);
    
    //
    // UI helpers
    //
    void DrawCameraList(const UsdStageRefPtr &stage);
    void DrawCameraEditor(const UsdStageRefPtr &stage, UsdTimeCode tc);
    
    // Accessors
    std::string GetCurrentCameraName() const;
    
    // Returning an editable camera which can be modified externaly by manipulators or any functions,
    // in the viewport. This will likely be reset at each frame. This is not thread safe and should be used only
    // in the main render loop.
    GfCamera & GetEditableCamera() { return *_renderCamera; }
    
    // Same as above but const
    const GfCamera & GetCurrentCamera() const { return *_renderCamera; }
       
    //
    const SdfPath & GetStageCameraPath() const;

    // Used in the manipulators for editing stage
    // TODO IsEditingStageCamera()
    inline bool IsUsingStageCamera () const {
        return _currentConfig->_renderCameraType == StageCamera;
    }
    
    inline bool IsUsingInternalOrthoCamera () const {
        return _currentConfig->_renderCameraType != StageCamera
            && _currentConfig->_renderCameraType != ViewportPerspective;
    }

private:
    inline bool IsPerspective() const { return _currentConfig->_renderCameraType == ViewportPerspective;}
    inline bool IsTop() const { return _currentConfig->_renderCameraType == ViewportTop;}
    inline bool IsBottom() const { return _currentConfig->_renderCameraType == ViewportBottom;}
    
    typedef enum CameraType {ViewportPerspective, ViewportTop, ViewportBottom, StageCamera} CameraType;
    
    // Set a new Stage camera path
    void UseStageCamera(const UsdStageRefPtr &stage, const SdfPath &cameraPath);
    
    // Use an internal camera
    void UseInternalCamera(const UsdStageRefPtr &stage, CameraType cameraType);
    
    // Points to a valid camera, stage or perspective that we share with the rest of the application
    GfCamera *_renderCamera;
    // A copy of the current stage camera used for editing
    GfCamera _stageCamera;
    // Keep track of the current stage to know when the stage has changed
    UsdStageRefPtr _currentStage;
    
    // Common to all stages, the perpective and ortho cams
    struct OwnedCameras {
        OwnedCameras ();
        // Persp camera
        GfCamera _perspectiveCamera;
        // Ortho cameras
        GfCamera _topCamera;
        GfCamera _bottomCamera;
    };
    
    // We could also copy instead of referencing the cam ...
    OwnedCameras *_ownedCameras;
    
    // Internal viewport cameras
    static std::unordered_map<std::string, OwnedCameras> _viewportCamerasPerStage;

    struct CameraConfiguration {
        SdfPath _stageCameraPath = SdfPath::EmptyPath();
        // we keep track of the render camera type
        CameraType _renderCameraType = CameraType::ViewportPerspective;
    };
    // Per viewport we want to select particular camera
    // We want to keep the camera configuration per stage on each viewport
    std::unordered_map<std::string, CameraConfiguration> _perStageConfiguration;
    
    CameraConfiguration *_currentConfig = nullptr;

};
