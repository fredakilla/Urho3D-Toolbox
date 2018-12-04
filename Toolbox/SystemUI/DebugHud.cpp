//
// Copyright (c) 2008-2017 the Urho3D project.
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

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/Profiler.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/GraphicsEvents.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/UI/UI.h>
#include "SystemUI.h"
#include "DebugHud.h"

#include <Urho3D/DebugNew.h>


namespace Urho3D
{

static const char* qualityTexts[] =
{
    "Low",
    "Med",
    "High"
};

static const char* shadowQualityTexts[] =
{
    "16bit Low",
    "24bit Low",
    "16bit High",
    "24bit High"
};

static const unsigned FPS_UPDATE_INTERVAL_MS = 500;

DebugHudEx::DebugHudEx(Context* context) :
    Object(context),
    profilerMaxDepth_(M_MAX_UNSIGNED),
    profilerInterval_(1000),
    useRendererStats_(true),
    mode_(DEBUGHUDEX_SHOW_NONE),
    fps_(0)
{
    SetExtents();
    SubscribeToEvent(E_UPDATE, std::bind(&DebugHudEx::RenderUi, this, std::placeholders::_2));
}

DebugHudEx::~DebugHudEx()
{
    UnsubscribeFromAllEvents();
}

void DebugHudEx::SetExtents(const IntVector2& position, IntVector2 size)
{
    if (size == IntVector2::ZERO)
    {
        size = { GetSubsystem<Graphics>()->GetWidth(), GetSubsystem<Graphics>()->GetHeight() };
        if (!HasSubscribedToEvent(E_SCREENMODE))
            SubscribeToEvent(E_SCREENMODE, std::bind(&DebugHudEx::SetExtents, this, IntVector2::ZERO, IntVector2::ZERO));
    }
    else
        UnsubscribeFromEvent(E_SCREENMODE);

    auto bottomRight = position + size;
    extents_ = IntRect(position.x_, position.y_, bottomRight.x_, bottomRight.y_);
}

void DebugHudEx::SetMode(DebugHudModeFlags mode)
{
    mode_ = mode;
}

void DebugHudEx::CycleMode()
{
    switch (mode_.AsInteger())
    {
    case DEBUGHUDEX_SHOW_NONE:
        SetMode(DEBUGHUDEX_SHOW_STATS);
        break;
    case DEBUGHUDEX_SHOW_STATS:
        SetMode(DEBUGHUDEX_SHOW_MODE);
        break;
    case DEBUGHUDEX_SHOW_MODE:
        SetMode(DEBUGHUDEX_SHOW_ALL);
        break;
    case DEBUGHUDEX_SHOW_ALL:
    default:
        SetMode(DEBUGHUDEX_SHOW_NONE);
        break;
    }
}

void DebugHudEx::SetUseRendererStats(bool enable)
{
    useRendererStats_ = enable;
}

void DebugHudEx::Toggle(DebugHudModeFlags mode)
{
    SetMode(GetMode() ^ mode);
}

void DebugHudEx::ToggleAll()
{
    Toggle(DEBUGHUDEX_SHOW_ALL);
}

void DebugHudEx::SetAppStats(const String& label, const Variant& stats)
{
    SetAppStats(label, stats.ToString());
}

void DebugHudEx::SetAppStats(const String& label, const String& stats)
{
    bool newLabel = !appStats_.Contains(label);
    appStats_[label] = stats;
    if (newLabel)
        appStats_.Sort();
}

bool DebugHudEx::ResetAppStats(const String& label)
{
    return appStats_.Erase(label);
}

void DebugHudEx::ClearAppStats()
{
    appStats_.Clear();
}

void DebugHudEx::RenderUi(VariantMap& eventData)
{
    Renderer* renderer = GetSubsystem<Renderer>();
    Graphics* graphics = GetSubsystem<Graphics>();


    ui::SetNextWindowPos(ToImGui(Vector2(extents_.Min())));
    ui::SetNextWindowSize(ToImGui(Vector2(extents_.Size())));
    ui::PushStyleColor(ImGuiCol_WindowBg, 0);
    if (ui::Begin("DebugHud", nullptr, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|
                                       ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoScrollbar))
    {
        if (mode_ & DEBUGHUDEX_SHOW_STATS)
        {
            // Update stats regardless of them being shown.
            if (fpsTimer_.GetMSec(false) > FPS_UPDATE_INTERVAL_MS)
            {
                fps_ = static_cast<unsigned int>(Round(GetSubsystem<Time>()->GetFramesPerSecond()));
                fpsTimer_.Reset();
            }

            String stats;
            unsigned primitives, batches;
            if (!useRendererStats_)
            {
                primitives = graphics->GetNumPrimitives();
                batches = graphics->GetNumBatches();
            }
            else
            {
                primitives = GetSubsystem<Renderer>()->GetNumPrimitives();
                batches = GetSubsystem<Renderer>()->GetNumBatches();
            }

            ui::Text("FPS %d", fps_);
            ui::Text("Triangles %u", primitives);
            ui::Text("Batches %u", batches);
            ui::Text("Views %u", renderer->GetNumViews());
            ui::Text("Lights %u", renderer->GetNumLights(true));
            ui::Text("Shadowmaps %u", renderer->GetNumShadowMaps(true));
            ui::Text("Occluders %u", renderer->GetNumOccluders(true));

            for (HashMap<String, String>::ConstIterator i = appStats_.Begin(); i != appStats_.End(); ++i)
                ui::Text("%s %s", i->first_.CString(), i->second_.CString());
        }

        if (mode_ & DEBUGHUDEX_SHOW_MODE)
        {
            auto& style = ui::GetStyle();
            ui::SetCursorPos({style.WindowPadding.x, ui::GetWindowSize().y - ui::GetStyle().WindowPadding.y - 10});
            ui::Text("Tex:%s | Mat:%s | Spec:%s | Shadows:%s | Size:%i | Quality:%s | Occlusion:%s | Instancing:%s | API:%s",
                qualityTexts[renderer->GetTextureQuality()],
                qualityTexts[renderer->GetMaterialQuality()],
                renderer->GetSpecularLighting() ? "On" : "Off",
                renderer->GetDrawShadows() ? "On" : "Off",
                renderer->GetShadowMapSize(),
                shadowQualityTexts[renderer->GetShadowQuality()],
                renderer->GetMaxOccluderTriangles() > 0 ? "On" : "Off",
                renderer->GetDynamicInstancing() ? "On" : "Off",
                graphics->GetApiName().CString());
        }
    }
    ui::End();
    ui::PopStyleColor();
}

}
