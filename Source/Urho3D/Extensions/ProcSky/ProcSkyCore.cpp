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
// modified, extended, and modernized by Florastamine https://github.com/Florastamine, including:
// - Code modernization, including the use of lambdas, auto, and nullptr.
// - Lua bindings. (also removed AngelScript bindings)
// - Revised programming style.

#include "ProcSkyCore.h"

#include "../../Core/Context.h"
#include "../../Core/CoreEvents.h"
#include "../../Core/Variant.h"
#include "../../Graphics/Camera.h"
#include "../../Graphics/Graphics.h"
#include "../../Graphics/GraphicsDefs.h"
#include "../../Graphics/Material.h"
#include "../../Graphics/Model.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/RenderPath.h"
#include "../../Graphics/Skybox.h"
#include "../../Graphics/Technique.h"
#include "../../Graphics/Texture.h"
#include "../../Graphics/TextureCube.h"
#include "../../Graphics/View.h"
#include "../../Graphics/Viewport.h"
#include "../../Input/Input.h"
#include "../../Input/InputEvents.h"
#include "../../Math/Matrix4.h"
#include "../../Math/Vector2.h"
#include "../../Math/Vector3.h"
#include "../../Math/Vector4.h"
#include "../../Resource/ResourceCache.h"
#include "../../IO/Log.h"
#include "../../Scene/Scene.h"

#if defined(PROCSKY_UI)
#include "../../Resource/XMLFile.h"
#include "../../UI/UI.h"
#include "../../UI/Slider.h"
#include "../../UI/Text.h"
#include "../../UI/UIEvents.h"
#include "../../UI/Window.h"
#endif

namespace Urho3D
{
ProcSky::ProcSky(Context* context):
    Component(context)
  , renderSize_(64)
  , updateAuto_(true)
  , updateInterval_(0.0f)
  , updateWait_(0)
  , renderQueued_(true)
  , camera_(nullptr)
  , Kr_(Vector3(0.18867780436772762, 0.4978442963618773, 0.6616065586417131))
  , rayleighBrightness_(3.3f)
  , mieBrightness_(0.1f)
  , spotBrightness_(50.0f)
  , scatterStrength_(0.028f)
  , rayleighStrength_(0.139f)
  , mieStrength_(0.264f)
  , rayleighCollectionPower_(0.81f)
  , mieCollectionPower_(0.39f)
  , mieDistribution_(0.63f)
{
    faceRotations_[FACE_POSITIVE_X] = Matrix3(0,0,1,  0,1,0, -1,0,0);
    faceRotations_[FACE_NEGATIVE_X] = Matrix3(0,0,-1, 0,1,0,  1,0,0);
    faceRotations_[FACE_POSITIVE_Y] = Matrix3(1,0,0,  0,0,1,  0,-1,0);
    faceRotations_[FACE_NEGATIVE_Y] = Matrix3(1,0,0,  0,0,-1, 0,1,0);
    faceRotations_[FACE_POSITIVE_Z] = Matrix3(1,0,0,  0,1,0,  0,0,1);
    faceRotations_[FACE_NEGATIVE_Z] = Matrix3(-1,0,0, 0,1,0,  0,0,-1);
}

ProcSky::~ProcSky() {}

void ProcSky::RegisterObject(Context* context)
{
    context->RegisterFactory<ProcSky>();
}

void ProcSky::OnNodeSet(Node* node)
{
    if (!node)
        return;
}

bool ProcSky::Initialize()
{
    if (!camera_)
        return false;
    
    auto *cache = GetSubsystem<ResourceCache>();
    rPath_ = GetSubsystem<Renderer>()->GetViewport(0)->GetRenderPath();
    
    // Creates a light component which will be attached to the sun itself. Users can be able to query
    // the light afterwards (node_->GetChild("PSLight")->GetComponent<Light>()) and manually alters parameters
    // to their needs.
    if (!lightNode_)
    {
        lightNode_ = node_->CreateChild("PSLight");
        
        auto *light = lightNode_->CreateComponent<Light>();
        light->SetLightType(LIGHT_DIRECTIONAL);
        light->SetColor(Color::WHITE);
    }

    // Creates a Skybox object to draw to, or re-use an existing one. Either way, the existing Skybox object will be altered.
    if (!skybox_)
        skybox_ = node_->CreateComponent<Skybox>();
    
    skybox_->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
    SharedPtr<Material> material(new Material(context_));
    material->SetTechnique(0, cache->GetResource<Technique>("Techniques/DiffSkybox.xml"));
    material->SetCullMode(CULL_NONE);
    skybox_->SetMaterial(material);
    SetRenderSize(renderSize_);

    // Shove some of the shader parameters into a VariantMap.
    VariantMap atmoParams;
    atmoParams["Kr"] = Kr_;
    atmoParams["RayleighBrightness"] = rayleighBrightness_;
    atmoParams["MieBrightness"] = mieBrightness_;
    atmoParams["SpotBrightness"] = spotBrightness_;
    atmoParams["ScatterStrength"] = scatterStrength_;
    atmoParams["RayleighStrength"] = rayleighStrength_;
    atmoParams["MieStrength"] = mieStrength_;
    atmoParams["RayleighCollectionPower"] = rayleighCollectionPower_;
    atmoParams["MieCollectionPower"] = mieCollectionPower_;
    atmoParams["MieDistribution"] = mieDistribution_;
    atmoParams["LightDir"] = Vector3::DOWN;
    atmoParams["InvProj"] = camera_->GetProjection().Inverse();

    // Add custom quad commands to render path.
    for (unsigned i = 0; i < MAX_CUBEMAP_FACES; ++i)
    {
        RenderPathCommand cmd;
        cmd.tag_ = "ProcSky";
        cmd.type_ = CMD_QUAD;
        cmd.sortMode_ = SORT_BACKTOFRONT;
        cmd.pass_ = "base";
        cmd.outputs_.Push(MakePair(String("DiffProcSky"), (CubeMapFace)i));
        cmd.textureNames_[0] = "";
        cmd.vertexShaderName_ = "ProcSky";
        cmd.vertexShaderDefines_ = "";
        cmd.pixelShaderName_ = "ProcSky";
        cmd.pixelShaderDefines_ = "";
        cmd.shaderParameters_ = atmoParams;
        cmd.shaderParameters_["InvViewRot"] = faceRotations_[i];
        cmd.enabled_ = true;
        rPath_->AddCommand(cmd);
    }

    // Perform at least one render to avoid empty sky.
    Update();

    SubscribeToEvent(E_UPDATE, [this] (StringHash eventType, VariantMap& eventData) -> void {
        float dt = eventData[Update::P_TIMESTEP].GetFloat();

#if defined(PROCSKY_UI)
        if (!GetSubsystem<UI>()->GetFocusElement())
        {
            auto *input = GetSubsystem<Input>();
            
            angVel_.x_ = (input->GetKeyDown(KEY_UP) ? 1 : (input->GetKeyDown(KEY_DOWN) ? -1 : 0)) * 10.0f * dt;
            angVel_.y_ = (input->GetKeyDown(KEY_LEFT) ? -1 : (input->GetKeyDown(KEY_RIGHT) ? 1 : 0)) * 10.0f * dt;
            SetRenderQueued(true);
        }
#endif

        // If using an interval, queue update when done waiting.
        if (updateAuto_)
        {
            if (updateInterval_ > 0)
            {
                updateWait_ -= dt;
                if (updateWait_ <= 0)
                {
                    updateWait_ = updateInterval_;
                    Update();
                }
            }
            else
            {
                Update();
            }
        }

#if defined(PROCSKY_UI)
        lightNode_->Rotate(Quaternion(angVel_.x_, angVel_.y_, angVel_.z_));
#endif
    });

#if defined(PROCSKY_UI)
    SubscribeToEvent(E_KEYDOWN, [this] (StringHash eventType, VariantMap& eventData) -> void {
        if (GetSubsystem<UI>()->GetFocusElement())
            return;

        switch(eventData[KeyDown::P_KEY].GetInt())
        {
            case KEY_U:
                updateAuto_ = !updateAuto_;
                break;
            
            case KEY_SPACE:
                ToggleUI();
                break;
            
            case KEY_0:
            case KEY_KP_0:
                angVel_ = Vector3::ZERO;
                break;
            
            case KEY_KP_6:
                lightNode_->SetRotation(Quaternion(faceRotations_[FACE_POSITIVE_X]));
                break;
            
            case KEY_KP_4:
                lightNode_->SetRotation(Quaternion(faceRotations_[FACE_NEGATIVE_X]));
                break;
            
            case KEY_KP_7:
                lightNode_->SetRotation(Quaternion(faceRotations_[FACE_POSITIVE_Y]));
                break;
            
            case KEY_KP_1:
                lightNode_->SetRotation(Quaternion(faceRotations_[FACE_NEGATIVE_Y]));
                break;
            
            case KEY_KP_8:
                lightNode_->SetRotation(Quaternion(faceRotations_[FACE_POSITIVE_Z]));
                break;
            
            case KEY_KP_2:
                lightNode_->SetRotation(Quaternion(faceRotations_[FACE_NEGATIVE_Z]));
                break;
        }
    });
#endif

    return true;
}

void ProcSky::Update()
{
    // In the shader code, LightDir is the direction TO the object casting light, not the direction OF the light, so invert the direction.
    rPath_->SetShaderParameter("LightDir", -lightNode_->GetWorldDirection());
    
    /// TODO: Only send changed parameters.
    rPath_->SetShaderParameter("Kr", Kr_);
    rPath_->SetShaderParameter("RayleighBrightness", rayleighBrightness_);
    rPath_->SetShaderParameter("MieBrightness", mieBrightness_);
    rPath_->SetShaderParameter("SpotBrightness", spotBrightness_);
    rPath_->SetShaderParameter("ScatterStrength", scatterStrength_);
    rPath_->SetShaderParameter("RayleighStrength", rayleighStrength_);
    rPath_->SetShaderParameter("MieStrength", mieStrength_);
    rPath_->SetShaderParameter("RayleighCollectionPower", rayleighCollectionPower_);
    rPath_->SetShaderParameter("MieCollectionPower", mieCollectionPower_);
    rPath_->SetShaderParameter("MieDistribution", mieDistribution_);
    rPath_->SetShaderParameter("InvProj", camera_->GetProjection().Inverse());
    SetRenderQueued(true);
}

void ProcSky::SetRenderSize(unsigned size)
{
    size = size >= 16 ? size : 64;
    
    // Create a TextureCube and assign to the ProcSky material.
    SharedPtr<TextureCube> skyboxTexCube(new TextureCube(context_));
    skyboxTexCube->SetName("DiffProcSky");
    skyboxTexCube->SetSize(size, Graphics::GetRGBAFormat(), TEXTURE_RENDERTARGET);
    skyboxTexCube->SetFilterMode(FILTER_BILINEAR);
    skyboxTexCube->SetAddressMode(COORD_U, ADDRESS_CLAMP);
    skyboxTexCube->SetAddressMode(COORD_V, ADDRESS_CLAMP);
    skyboxTexCube->SetAddressMode(COORD_W, ADDRESS_CLAMP);
    GetSubsystem<ResourceCache>()->AddManualResource(skyboxTexCube);

    skybox_->GetMaterial()->SetTexture(TU_DIFFUSE, skyboxTexCube);
    renderSize_ = size;
}

void ProcSky::SetRenderQueued(bool queued)
{
    if (renderQueued_ == queued)
        return;
    
    // When using manual update, be notified after rendering.
    if (!updateAuto_)
    {
        SubscribeToEvent(E_POSTRENDERUPDATE, [this] (StringHash eventType, VariantMap& eventData) {
            if (!updateAuto_)
                SetRenderQueued(false);
        });
    }
    
    rPath_->SetEnabled("ProcSky", queued);
    renderQueued_ = queued;
}

#if defined(PROCSKY_UI)
void ProcSky::ToggleUI()
{
    auto *ui = GetSubsystem<UI>();
    auto *root = ui->GetRoot();
    
    auto *win = static_cast<Window *>(root->GetChild("ProcSkyWindow", true));
    if (win)
    {
        win->Remove();
        return;
    }
    
    root->SetDefaultStyle(GetSubsystem<ResourceCache>()->GetResource<XMLFile>("UI/DefaultStyle.xml"));

    win = new Window(context_);
    win->SetName("ProcSkyWindow");
    win->SetStyleAuto();
    win->SetMovable(true);
    win->SetLayout(LM_VERTICAL, 2, IntRect(2,2,2,2));
    win->SetAlignment(HA_LEFT, VA_TOP);
    win->SetOpacity(0.8f);
    
    UIElement* titleBar(new UIElement(context_));
    titleBar->SetMinSize(0, 16);
    titleBar->SetVerticalAlignment(VA_TOP);
    titleBar->SetLayoutMode(LM_HORIZONTAL);
    Text* windowTitle(new Text(context_));
    windowTitle->SetText("ProcSky Parameters");
    titleBar->AddChild(windowTitle);
    windowTitle->SetStyleAuto();
    
    win->AddChild(titleBar);
    root->AddChild(win);
    
    const auto CreateSlider = [this] (UIElement* parent, const String& label, float* target, float range) -> void {
        UIElement* textContainer(new UIElement(context_));
        parent->AddChild(textContainer);
        textContainer->SetStyleAuto();
        textContainer->SetLayoutMode(LM_HORIZONTAL);
        textContainer->SetMaxSize(2147483647, 16);

        Text* text(new Text(context_));
        textContainer->AddChild(text);
        text->SetStyleAuto();
        text->SetAlignment(HA_LEFT, VA_TOP);
        text->SetText(label);
        text->SetMaxSize(2147483647, 16);

        Text* valueText(new Text(context_));
        textContainer->AddChild(valueText);
        valueText->SetStyleAuto();
        valueText->SetAlignment(HA_RIGHT, VA_TOP);
        valueText->SetText(String(*target));
        valueText->SetMaxSize(2147483647, 16);

        Slider* slider(new Slider(context_));
        parent->AddChild(slider);
        slider->SetStyleAuto();
        slider->SetAlignment(HA_LEFT, VA_TOP);
        slider->SetName(label);
        slider->SetRange(range);
        slider->SetValue(*target);

        slider->SetMaxSize(2147483647, 16);
        // Store target for handler to use.
        slider->SetVar(label, target);
        // Store value label for handler to use.
        slider->SetVar(label+"_value", static_cast<void *>(valueText));
        SubscribeToEvent(slider, E_SLIDERCHANGED, [this] (StringHash eventType, VariantMap& eventData) {
            auto *slider = static_cast<Slider*>(eventData[SliderChanged::P_ELEMENT].GetPtr());
            assert(slider != nullptr);

            float value = eventData[SliderChanged::P_VALUE].GetFloat();
            String sliderName(slider->GetName());
            
            // Get target member from node var and update it.
            auto *target = static_cast<float*>(slider->GetVar(sliderName).GetVoidPtr());
            if (target)
                *target = value;
            
            // Get stored value Text label and update it.
            auto *valueText = static_cast<Text*>(slider->GetVar(sliderName+"_value").GetVoidPtr());
            if (valueText)
                valueText->SetText(String(value));
        });
    };

    CreateSlider(win, "rayleighBrightness", &rayleighBrightness_, 100.0f);
    CreateSlider(win, "mieBrightness", &mieBrightness_, 10.0f);
    CreateSlider(win, "spotBrightness", &spotBrightness_, 200.0f);
    CreateSlider(win, "scatterStrength", &scatterStrength_, 1.0f);
    CreateSlider(win, "rayleighStrength", &rayleighStrength_, 5.0f);
    CreateSlider(win, "mieStrength", &mieStrength_, 2.0f);
    CreateSlider(win, "rayleighCollectionPower", &rayleighCollectionPower_, 10.0f);
    CreateSlider(win, "mieCollectionPower", &mieCollectionPower_, 10.0f);
    CreateSlider(win, "mieDistribution", &mieDistribution_, 10.0f);
    CreateSlider(win, "Kr_red", &Kr_.x_, 1.0f);
    CreateSlider(win, "Kr_green", &Kr_.y_, 1.0f);
    CreateSlider(win, "Kr_blue", &Kr_.z_, 1.0f);
    CreateSlider(win, "updateInterval", &updateInterval_, 10.0f);

    auto *graphics = GetSubsystem<Graphics>();
    IntVector2 scrSize(graphics->GetWidth(), graphics->GetHeight());
    IntVector2 winSize(scrSize);
    winSize.x_ = (int)(0.3f * winSize.x_); winSize.y_ = (int)(0.5f * winSize.y_);
    win->SetSize(winSize);
    win->SetPosition(0, (scrSize.y_-winSize.y_)/2);
}
#endif

}

