//
// Copyright (c) 2008 - 2018 the Urho3D project, 2017 - 2018 Flock SDK developers & contributors. 
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// Original ProcSky implementation by Justin Miller (carnalis) @ https://github.com/carnalis/ProcSky,
// Additional information can be found on ProcSkyCore.cpp.

#pragma once

/// Turns on a debugging GUI for tweaking ProcSky parameters.
#define PROCSKY_UI

#include "../../Scene/Component.h"
#include "../../Graphics/Camera.h"
#include "../../Scene/Node.h"
#include "../../Graphics/RenderPath.h"
#include "../../Graphics/Viewport.h"
#include "../../Container/Vector.h"
#include "../../Math/Matrix3.h"
#include "../../Math/Vector3.h"

namespace Urho3D
{
#if defined(PROCSKY_UI)
class UIElement;
#endif

class Skybox;
class StringHash;

class ProcSky : public Component
{
    URHO3D_OBJECT(ProcSky, Component);

public:
    /// TODO: Comment on function prototypes.
    ProcSky(Context* context);
    virtual ~ProcSky();
    
    static void RegisterObject(Context* context);
    void OnNodeSet(Node* node);
    
    bool Initialize();
    
    /// Update shader parameters and queue a render.
    void Update();

    inline void SetUpdateWait(float f) { updateWait_ = f; }
    inline float GetUpdateWait() const { return updateWait_; }

    inline void SetUpdateAuto(bool on) { updateAuto_ = on; }
    inline bool GetUpdateAuto() const { return updateAuto_; }

    inline void SetUpdateInterval(float interval) { updateInterval_ = interval; }
    inline float GetUpdateInterval() const { return updateInterval_; }

    void SetRenderSize(unsigned size);
    inline unsigned GetRenderSize() const { return renderSize_; }  

    inline float GetRayleighBrightness() const { return rayleighBrightness_; }
    inline void SetRayleighBrightness(float f) { rayleighBrightness_ = f; } 

    inline Vector3 GetAirAbsorptionProfile() const { return Kr_; }
    inline void SetAirAbsorptionProfile(const Vector3 &v) { Kr_ = v; }

    inline float GetMieBrightness() const  { return mieBrightness_; }
    inline void SetMieBrightness(float f) { mieBrightness_ = f; } 

    inline float GetSpotBrightness() const { return spotBrightness_; }
    inline void SetSpotBrightness(float f) { spotBrightness_ = f; } 

    inline float GetScatteringStrength() const  { return scatterStrength_; }
    inline void SetScatteringStrength(float f) { scatterStrength_ = f; } 

    inline float GetRayleighStrength() const { return rayleighStrength_; }
    inline void SetRayleighStrength(float f) { rayleighStrength_ = f; } 

    inline float GetMieStrength() const { return mieStrength_; }
    inline void SetMieStrength(float f) { mieStrength_ = f; } 

    inline float GetRayleighCollectionPower() const { return rayleighCollectionPower_; }
    inline void SetRayleighCollectionPower(float f) { rayleighCollectionPower_ = f; } 

    inline float GetMieCollectionPower() const { return mieCollectionPower_; }
    inline void SetMieCollectionPower(float f) { mieCollectionPower_ = f; } 

    inline float GetMieDistribution() const { return mieDistribution_; }
    inline void SetMieDistribution(float f) { mieDistribution_ = f; }
    
    inline void SetProjectionCamera(Camera *camera) { camera_ = camera; }
    inline Camera *GetProjectionCamera() const { return camera_; }
    
    /// Atmospheric parameters.
    Vector3 Kr_;
    float rayleighBrightness_;
    float mieBrightness_;
    float spotBrightness_;
    float scatterStrength_;
    float rayleighStrength_;
    float mieStrength_;
    float rayleighCollectionPower_;
    float mieCollectionPower_;
    float mieDistribution_;

protected:    
    /// Set rendering of next frame active/inactive.
    void SetRenderQueued(bool enable);

#if defined(PROCSKY_UI)
    void ToggleUI();
#endif

    /// Camera used for face projections.
    Camera* camera_;
    
    /// Urho3D Skybox with geometry and main TextureCube.
    SharedPtr<Skybox> skybox_;
    
    /// Node used for light direction.
    WeakPtr<Node> lightNode_;
    
    /// Render size of each face.
    unsigned renderSize_;
    
    /// Fixed rotations for each cube face.
    Matrix3 faceRotations_[MAX_CUBEMAP_FACES];
    
    SharedPtr<RenderPath> rPath_;

    bool updateAuto_;
    float updateInterval_;
    float updateWait_;
    bool renderQueued_;

#if defined(PROCSKY_UI)
    Vector3 angVel_;
#endif
};

}

