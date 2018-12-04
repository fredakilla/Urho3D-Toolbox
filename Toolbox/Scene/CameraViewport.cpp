//
// Copyright (c) 2018 Rokas Kupstys
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

#include <Urho3D/Core/Context.h>
#include "../Core/StringUtils.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/RenderPath.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/XMLFile.h"
#include "../Scene/Node.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneEvents.h"

#include "SceneMetadata.h"
#include "CameraViewport.h"


namespace Urho3D
{

static ResourceRef defaultRenderPath{XMLFile::GetTypeStatic(), "RenderPaths/Forward.xml"};

CameraViewport::CameraViewport(Context* context)
    : Component(context)
    , viewport_(new Viewport(context))
    , rect_(fullScreenViewport)
    , renderPath_(defaultRenderPath)
    , screenRect_{0, 0, GetSubsystem<Graphics>()->GetWidth(), GetSubsystem<Graphics>()->GetHeight()}
{
}

void CameraViewport::SetNormalizedRect(const Rect& rect)
{
    rect_ = rect;
    IntRect screenRect = GetScreenRect();
    IntRect viewportRect(static_cast<int>(rect.Left() * screenRect.Left()), static_cast<int>(rect.Top() * screenRect.Top()),
        static_cast<int>(rect.Right() * screenRect.Right()), static_cast<int>(rect.Bottom() * screenRect.Bottom()));
    viewport_->SetRect(viewportRect);

    using namespace CameraViewportResized;
    VariantMap args{};
    args[P_VIEWPORT] = GetViewport();
    args[P_CAMERA] = GetViewport()->GetCamera();
    args[P_SIZE] = viewportRect;
    args[P_SIZENORM] = rect;
    SendEvent(E_CAMERAVIEWPORTRESIZED, args);
}

void CameraViewport::RegisterObject(Context* context)
{
    context->RegisterFactory<CameraViewport>("Scene");
}

void CameraViewport::OnNodeSet(Node* node)
{
    if (node == nullptr)
        viewport_->SetCamera(nullptr);
    else
    {
        SubscribeToEvent(node, E_COMPONENTADDED, [this](StringHash, VariantMap& args) {
            using namespace ComponentAdded;
            if (Component* component = static_cast<Component*>(args[P_COMPONENT].GetPtr()))
            {
                if (Camera* camera = component->Cast<Camera>())
                {
                    viewport_->SetCamera(camera);
                    camera->SetViewMask(camera->GetViewMask() & ~(1U << 31));   // Do not render last layer.
                }
            }
        });
        SubscribeToEvent(node, E_COMPONENTREMOVED, [this](StringHash, VariantMap& args) {
            using namespace ComponentRemoved;
            if (Component* component = static_cast<Component*>(args[P_COMPONENT].GetPtr()))
            {
                if (component->GetType() == Camera::GetTypeStatic())
                    viewport_->SetCamera(nullptr);
            }
        });

        if (Camera* camera = node->GetComponent<Camera>())
            viewport_->SetCamera(camera);
    }
}

void CameraViewport::OnSceneSet(Scene* scene)
{
    if (scene)
    {
        if (SceneMetadata* manager = scene->GetOrCreateComponent<SceneMetadata>())
            manager->RegisterComponent(this);
    }
    else
    {
        if (Scene* oldScene = GetScene())
        {
            if (SceneMetadata* manager = oldScene->GetComponent<SceneMetadata>())
                manager->UnregisterComponent(this);
        }
    }
    viewport_->SetScene(scene);
}

IntRect CameraViewport::GetScreenRect() const
{
    return screenRect_;
}

const Vector<AttributeInfo>* CameraViewport::GetAttributes() const
{
    if (attributesDirty_)
        const_cast<CameraViewport*>(this)->RebuildAttributes();
    return &attributes_;
}

template<typename T>
AttributeInfo& CameraViewport::RegisterAttribute(const AttributeInfo& attr)
{
    attributes_.Push(attr);
    return attributes_.Back();
}

void CameraViewport::RebuildAttributes()
{
    auto* context = this;
    // Normal attributes.
    URHO3D_ACCESSOR_ATTRIBUTE("Viewport", GetNormalizedRect, SetNormalizedRect, Rect, fullScreenViewport, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("RenderPath", GetLastRenderPath, SetRenderPath, ResourceRef, defaultRenderPath, AM_DEFAULT);

    // PostProcess effects are special. One file may contain multiple effects that can be enabled or disabled.
    {
        effects_.Clear();
        for (const auto& dir: GetSubsystem<ResourceCache>()->GetResourceDirs())
        {
            Vector<String> effects;
            String resourcePath = "PostProcess/";
            String scanDir = AddTrailingSlash(dir) + resourcePath;
            GetSubsystem<FileSystem>()->ScanDir(effects, scanDir, "*.xml", SCAN_FILES, false);

            for (const auto& effectFileName: effects)
            {
                auto effectPath = resourcePath + effectFileName;
                auto* effect = GetSubsystem<ResourceCache>()->GetResource<XMLFile>(effectPath);

                auto root = effect->GetRoot();
                String tag;
                for (auto command = root.GetChild("command"); command.NotNull(); command = command.GetNext("command"))
                {
                    tag = command.GetAttribute("tag");

                    if (tag.Empty())
                    {
                        URHO3D_LOGWARNING("Invalid PostProcess effect with empty tag");
                        continue;
                    }

                    if (effects_.Find(tag) != effects_.End())
                        continue;

                    effects_[tag] = resourcePath + effectFileName;
                }
            }
        }

        StringVector tags = effects_.Keys();
        Sort(tags.Begin(), tags.End());

        for (auto& effect : effects_)
        {
            auto getter = [this, &effect](const CameraViewport&, Variant& value) {
                value = viewport_->GetRenderPath()->IsEnabled(effect.first_);
            };

            auto setter = [this, &effect](const CameraViewport&, const Variant& value) {
                RenderPath* path = viewport_->GetRenderPath();
                if (!path->IsAdded(effect.first_))
                    path->Append(GetSubsystem<ResourceCache>()->GetResource<XMLFile>(effect.second_));
                path->SetEnabled(effect.first_, value.GetBool());
            };
            URHO3D_CUSTOM_ATTRIBUTE(effect.first_.CString(), getter, setter, bool, false, AM_DEFAULT);
        }
    }

    attributesDirty_ = false;
}

RenderPath* CameraViewport::RebuildRenderPath()
{
    if (viewport_.Null())
        return nullptr;

    SharedPtr<RenderPath> oldRenderPath(viewport_->GetRenderPath());

    if (XMLFile* renderPathFile = GetSubsystem<ResourceCache>()->GetResource<XMLFile>(renderPath_.name_))
    {
        viewport_->SetRenderPath(renderPathFile);
        RenderPath* newRenderPath = viewport_->GetRenderPath();

        for (const auto& effect : effects_)
        {
            if (oldRenderPath->IsEnabled(effect.first_))
            {
                if (!newRenderPath->IsAdded(effect.first_))
                    newRenderPath->Append(GetSubsystem<ResourceCache>()->GetResource<XMLFile>(effect.second_));
                newRenderPath->SetEnabled(effect.first_, true);
            }
        }

        return newRenderPath;
    }

    return nullptr;
}

void CameraViewport::SetRenderPath(const ResourceRef& renderPathResource)
{
    if (viewport_.Null())
        return;

    if (!renderPathResource.name_.Empty() && renderPathResource.type_ != XMLFile::GetTypeStatic())
    {
        URHO3D_LOGWARNINGF("Incorrect RenderPath file '%s' type.", renderPathResource.name_.CString());
        return;
    }

    SharedPtr<RenderPath> oldRenderPath(viewport_->GetRenderPath());

    const String& renderPathFileName = renderPathResource.name_.Empty() ? defaultRenderPath.name_ : renderPathResource.name_;
    if (XMLFile* renderPathFile = GetSubsystem<ResourceCache>()->GetResource<XMLFile>(renderPathFileName))
    {
        viewport_->SetRenderPath(renderPathFile);
        if (!viewport_->GetRenderPath())
        //@@if (!viewport_->SetRenderPath(renderPathFile))
        {
            URHO3D_LOGERRORF("Loading renderpath from %s failed. File probably is not a renderpath.",
                renderPathFileName.CString());
            return;
        }
        RenderPath* newRenderPath = viewport_->GetRenderPath();

        for (const auto& effect : effects_)
        {
            if (oldRenderPath->IsEnabled(effect.first_))
            {
                if (!newRenderPath->IsAdded(effect.first_))
                    newRenderPath->Append(GetSubsystem<ResourceCache>()->GetResource<XMLFile>(effect.second_));
                newRenderPath->SetEnabled(effect.first_, true);
            }
        }

        renderPath_.name_ = renderPathFileName;
    }
    else
    {
        URHO3D_LOGERRORF("Loading renderpath from %s failed. File is missing or you have no permissions to read it.",
                         renderPathFileName.CString());
    }
}

void CameraViewport::UpdateViewport()
{
    SetNormalizedRect(GetNormalizedRect());
}

}
