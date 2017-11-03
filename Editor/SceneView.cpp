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

#include "SceneView.h"
#include "EditorEvents.h"
#include <Urho3D/Core/StringUtils.h>
#include <Toolbox/Scene/DebugCameraController.h>
#include <Toolbox/SystemUI/Widgets.h>
#include <ImGui/imgui_internal.h>
#include <ImGuizmo/ImGuizmo.h>
#include <IconFontCppHeaders/IconsFontAwesome.h>


namespace Urho3D
{

SceneView::SceneView(Context* context, StringHash id, const String& afterDockName, ui::DockSlot_ position)
    : Object(context)
    , gizmo_(context)
    , inspector_(context)
    , placeAfter_(afterDockName)
    , placePosition_(position)
    , id_(id)
{
    SetTitle(title_);
    scene_ = SharedPtr<Scene>(new Scene(context));
    scene_->CreateComponent<Octree>();
    view_ = SharedPtr<Texture2D>(new Texture2D(context));
    view_->SetFilterMode(FILTER_ANISOTROPIC);
    viewport_ = SharedPtr<Viewport>(new Viewport(context_, scene_, nullptr));
    CreateEditorObjects();
    SetScreenRect({0, 0, 1024, 768});
    SubscribeToEvent(this, E_EDITORSELECTIONCHANGED, std::bind(&SceneView::OnNodeSelectionChanged, this));
}

SceneView::~SceneView()
{
    renderer_->Remove();
}

void SceneView::SetScreenRect(const IntRect& rect)
{
    if (rect == screenRect_)
        return;
    screenRect_ = rect;
    view_->SetSize(rect.Width(), rect.Height(), Graphics::GetRGBFormat(), TEXTURE_RENDERTARGET);
    viewport_->SetRect(IntRect(IntVector2::ZERO, rect.Size()));
    view_->GetRenderSurface()->SetViewport(0, viewport_);
    gizmo_.SetScreenRect(rect);
}

bool SceneView::RenderWindow()
{
    bool open = true;
    auto& style = ui::GetStyle();

    if (GetInput()->IsMouseVisible())
        lastMousePosition_ = GetInput()->GetMousePosition();

    ui::SetNextDockPos(placeAfter_.CString(), placePosition_, ImGuiCond_FirstUseEver);
    if (ui::BeginDock(uniqueTitle_.CString(), &open, windowFlags_))
    {
        // Focus window when appearing
        if (!wasRendered_)
        {
            ui::SetWindowFocus();
            wasRendered_ = true;
        }

        ImGuizmo::SetDrawlist();
        ui::SetCursorPos(ui::GetCursorPos() - style.WindowPadding);
        ui::Image(view_, ToImGui(screenRect_.Size()));

        if (screenRect_.IsInside(lastMousePosition_) == INSIDE)
        {
            if (!ui::IsWindowFocused() && ui::IsItemHovered() && GetInput()->GetMouseButtonDown(MOUSEB_RIGHT))
                ui::SetWindowFocus();

            if (ui::IsDockActive())
                isActive_ = ui::IsWindowFocused();
            else
                isActive_ = false;
        }
        else
            isActive_ = false;

        camera_->GetComponent<DebugCameraController>()->SetEnabled(isActive_);

        gizmo_.ManipulateSelection(GetCamera());

        // Update scene view rect according to window position
        if (!GetInput()->GetMouseButtonDown(MOUSEB_LEFT))
        {
            auto titlebarHeight = ui::GetCurrentContext()->CurrentWindow->TitleBarHeight();
            auto pos = ui::GetWindowPos();
            pos.y += titlebarHeight;
            auto size = ui::GetWindowSize();
            size.y -= titlebarHeight;
            if (size.x > 0 && size.y > 0)
            {
                IntRect newRect(ToIntVector2(pos), ToIntVector2(pos + size));
                SetScreenRect(newRect);
            }
        }

        if (ui::IsItemHovered())
        {
            // Prevent dragging window when scene view is clicked.
            windowFlags_ = ImGuiWindowFlags_NoMove;

            // Handle object selection.
            if (!gizmo_.IsActive() && GetInput()->GetMouseButtonPress(MOUSEB_LEFT))
            {
                IntVector2 pos = GetInput()->GetMousePosition();
                pos -= screenRect_.Min();

                Ray cameraRay = GetCamera()->GetScreenRay((float)pos.x_ / screenRect_.Width(), (float)pos.y_ / screenRect_.Height());
                // Pick only geometry objects, not eg. zones or lights, only get the first (closest) hit
                PODVector<RayQueryResult> results;

                RayOctreeQuery query(results, cameraRay, RAY_TRIANGLE, M_INFINITY, DRAWABLE_GEOMETRY);
                scene_->GetComponent<Octree>()->RaycastSingle(query);

                if (!results.Size())
                {
                    // When object geometry was not hit by a ray - query for object bounding box.
                    RayOctreeQuery query2(results, cameraRay, RAY_OBB, M_INFINITY, DRAWABLE_GEOMETRY);
                    scene_->GetComponent<Octree>()->RaycastSingle(query2);
                }

                if (results.Size())
                {
                    WeakPtr<Node> clickNode(results[0].drawable_->GetNode());
                    if (!GetInput()->GetKeyDown(KEY_CTRL))
                        UnselectAll();

                    ToggleSelection(clickNode);
                }
                else
                    UnselectAll();
            }
        }
        else
            windowFlags_ = 0;

        const auto tabContextMenuTitle = "SceneView context menu";
        if (ui::IsDockTabHovered() && GetInput()->GetMouseButtonPress(MOUSEB_RIGHT))
            ui::OpenPopup(tabContextMenuTitle);
        if (ui::BeginPopup(tabContextMenuTitle))
        {
            if (ui::MenuItem("Settings"))
            {
                settingsOpen_ = true;
                ReloadPostProcessEffects();
            }

            if (ui::MenuItem("Save"))
                SaveScene();

            ui::EndPopup();
        }
    }
    else
    {
        isActive_ = false;
        wasRendered_ = false;
    }
    ui::EndDock();

    RenderSettingsWindow();

    return open;
}

void SceneView::LoadScene(const String& filePath)
{
    if (filePath.Empty())
        return;

    if (filePath.EndsWith(".xml", false))
    {
        if (scene_->LoadXML(GetCache()->GetResource<XMLFile>(filePath)->GetRoot()))
        {
            path_ = filePath;
            CreateEditorObjects();
        }
        else
            URHO3D_LOGERRORF("Loading scene %s failed", GetFileName(filePath).CString());
    }
    else if (filePath.EndsWith(".json", false))
    {
        if (scene_->LoadJSON(GetCache()->GetResource<JSONFile>(filePath)->GetRoot()))
        {
            path_ = filePath;
            CreateEditorObjects();
        }
        else
            URHO3D_LOGERRORF("Loading scene %s failed", GetFileName(filePath).CString());
    }
    else
        URHO3D_LOGERRORF("Unknown scene file format %s", GetExtension(filePath).CString());
}

bool SceneView::SaveScene(const String& filePath)
{
    auto resourcePath = filePath.Empty() ? path_ : filePath;
    auto fullPath = GetCache()->GetResourceFileName(resourcePath);
    File file(context_, fullPath, FILE_WRITE);
    bool result = false;

    float elapsed = 0;
    if (!saveSceneElapsedTime_)
    {
        elapsed = scene_->GetElapsedTime();
        scene_->SetElapsedTime(0);
    }

    if (fullPath.EndsWith(".xml", false))
        result = scene_->SaveXML(file);
    else if (fullPath.EndsWith(".json", false))
        result = scene_->SaveJSON(file);

    if (!saveSceneElapsedTime_)
        scene_->SetElapsedTime(elapsed);

    if (result)
    {
        if (!filePath.Empty())
            path_ = filePath;
    }
    else
        URHO3D_LOGERRORF("Saving scene to %s failed.", resourcePath.CString());

    return result;
}

void SceneView::CreateEditorObjects()
{
    camera_ = scene_->CreateChild("DebugCamera");
    camera_->SetTemporary(true);
    camera_->CreateComponent<Camera>();
    camera_->CreateComponent<DebugCameraController>();
    scene_->GetOrCreateComponent<DebugRenderer>()->SetView(GetCamera());
    viewport_->SetCamera(GetCamera());
}

void SceneView::Select(Node* node)
{
    if (gizmo_.Select(node))
    {
        using namespace EditorSelectionChanged;
        SendEvent(E_EDITORSELECTIONCHANGED, P_SCENEVIEW, this);
    }
}

void SceneView::Unselect(Node* node)
{
    if (gizmo_.Unselect(node))
    {
        using namespace EditorSelectionChanged;
        SendEvent(E_EDITORSELECTIONCHANGED, P_SCENEVIEW, this);
    }
}

void SceneView::ToggleSelection(Node* node)
{
    gizmo_.ToggleSelection(node);
    using namespace EditorSelectionChanged;
    SendEvent(E_EDITORSELECTIONCHANGED, P_SCENEVIEW, this);
}

void SceneView::UnselectAll()
{
    if (gizmo_.UnselectAll())
    {
        using namespace EditorSelectionChanged;
        SendEvent(E_EDITORSELECTIONCHANGED, P_SCENEVIEW, this);
    }
}

const Vector<WeakPtr<Node>>& SceneView::GetSelection() const
{
    return gizmo_.GetSelection();
}

void SceneView::RenderGizmoButtons()
{
    const auto& style = ui::GetStyle();

    auto drawGizmoOperationButton = [&](GizmoOperation operation, const char* icon, const char* tooltip)
    {
        if (gizmo_.GetOperation() == operation)
            ui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
        else
            ui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_Button]);
        if (ui::ButtonEx(icon, {20, 20}, ImGuiButtonFlags_PressedOnClick))
            gizmo_.SetOperation(operation);
        ui::PopStyleColor();
        ui::SameLine();
        if (ui::IsItemHovered())
            ui::SetTooltip("%s", tooltip);
    };

    auto drawGizmoTransformButton = [&](TransformSpace transformSpace, const char* icon, const char* tooltip)
    {
        if (gizmo_.GetTransformSpace() == transformSpace)
            ui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
        else
            ui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_Button]);
        if (ui::ButtonEx(icon, {20, 20}, ImGuiButtonFlags_PressedOnClick))
            gizmo_.SetTransformSpace(transformSpace);
        ui::PopStyleColor();
        ui::SameLine();
        if (ui::IsItemHovered())
            ui::SetTooltip(tooltip);
    };

    drawGizmoOperationButton(GIZMOOP_TRANSLATE, ICON_FA_ARROWS, "Translate");
    drawGizmoOperationButton(GIZMOOP_ROTATE, ICON_FA_REPEAT, "Rotate");
    drawGizmoOperationButton(GIZMOOP_SCALE, ICON_FA_ARROWS_ALT, "Scale");
    ui::TextUnformatted("|");
    ui::SameLine();
    drawGizmoTransformButton(TS_WORLD, ICON_FA_ARROWS, "World");
    drawGizmoTransformButton(TS_LOCAL, ICON_FA_ARROWS_ALT, "Local");
    ui::TextUnformatted("|");
    ui::SameLine();


    auto light = camera_->GetComponent<Light>();
    if (light->IsEnabled())
        ui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
    else
        ui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_Button]);
    if (ui::Button(ICON_FA_LIGHTBULB_O, {20, 20}))
        light->SetEnabled(!light->IsEnabled());
    ui::PopStyleColor();
    ui::SameLine();
    if (ui::IsItemHovered())
        ui::SetTooltip("Camera Headlight");
}

bool SceneView::IsSelected(Node* node) const
{
    return gizmo_.IsSelected(node);
}

void SceneView::OnNodeSelectionChanged()
{
    using namespace EditorSelectionChanged;
    const auto& selection = GetSelection();
    if (selection.Size() == 1)
    {
        const auto& node = selection.Front();
        const auto& components = node->GetComponents();
        if (!components.Empty())
            selectedComponent_ = components.Front();
        else
            selectedComponent_ = nullptr;
    }
    else
        selectedComponent_ = nullptr;
}

void SceneView::RenderInspector()
{
    // TODO: inspector for multi-selection.
    if (GetSelection().Size() == 1)
    {
        auto node = GetSelection().Front();
        PODVector<Serializable*> items;
        items.Push(dynamic_cast<Serializable*>(node.Get()));
        if (!selectedComponent_.Expired())
            items.Push(dynamic_cast<Serializable*>(selectedComponent_.Get()));
        inspector_.RenderAttributes(items);
    }
}

void SceneView::RenderSceneNodeTree(Node* node)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (node == nullptr)
    {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
        node = scene_;
    }

    if (node->IsTemporary())
        return;

    String name = ToString("%s (%d)", (node->GetName().Empty() ? node->GetTypeName() : node->GetName()).CString(), node->GetID());
    bool isSelected = IsSelected(node);

    if (isSelected)
        flags |= ImGuiTreeNodeFlags_Selected;

    auto opened = ui::TreeNodeEx(name.CString(), flags);

    if (ui::IsItemClicked(0))
    {
        if (!GetInput()->GetKeyDown(KEY_CTRL))
            UnselectAll();
        ToggleSelection(node);
    }

    if (opened)
    {
        for (auto& component: node->GetComponents())
        {
            bool selected = selectedComponent_ == component;
            if (ui::Selectable(component->GetTypeName().CString(), selected))
            {
                UnselectAll();
                ToggleSelection(node);
                selectedComponent_ = component;
            }
        }

        for (auto& child: node->GetChildren())
            RenderSceneNodeTree(child);
        ui::TreePop();
    }
}

void SceneView::RenderSettingsWindow()
{
    struct State
    {
        explicit State(SceneView* sceneView)
        {
            strncpy(titleBuffer_, sceneView->GetTitle().CString(), sizeof(titleBuffer_));
        }

        char titleBuffer_[64]{};
    };

    if (settingsOpen_)
    {
        ui::SetNextWindowSize({0, 0}, ImGuiCond_Always);
        if (ui::Begin("Scene Settings", &settingsOpen_))
        {
            State* state = ui::GetUIState<State>(this);
            if (ui::InputText("Title", state->titleBuffer_, IM_ARRAYSIZE(state->titleBuffer_)))
                SetTitle(state->titleBuffer_);
            ui::Checkbox("Save Elapsed Time", &saveSceneElapsedTime_);

            RenderPath* path = viewport_->GetRenderPath();
            for (auto it = effectVariables_.Begin(); it != effectVariables_.End(); it++)
            {
                auto fileName = it->first_;
                for (auto jt = it->second_.Begin(); jt != it->second_.End(); jt++)
                {
                    auto tag = jt->first_;

                    bool enabled = path->IsEnabled(tag);
                    if (ui::Checkbox(tag.CString(), &enabled))
                    {
                        if (enabled)
                        {
                            if (!path->IsAdded(tag))
                            {
                                path->Append(GetCache()->GetResource<XMLFile>(fileName));
                                // Some render paths have multiple tags and appending enables them all. Disable all tags
                                // in added path, later on only selected tag will be enabled.
                                for (auto kt = it->second_.Begin(); kt != it->second_.End(); kt++)
                                    path->SetEnabled(kt->first_, false);
                            }
                        }
                        path->SetEnabled(tag, enabled);
                    }

                    if (enabled)
                    {
                        for (const auto& variable: jt->second_)
                        {
                            const Variant& value = path->GetShaderParameter(variable);
                            switch (value.GetType())
                            {
                            case VAR_FLOAT:
                            {
                                float v = value.GetFloat();
                                if (ui::DragFloat(variable.CString(), &v))
                                    path->SetShaderParameter(variable, v);
                                break;
                            }
                            case VAR_VECTOR2:
                            {
                                Vector2 v = value.GetVector2();
                                if (ui::DragFloat2(variable.CString(), &v.x_))
                                    path->SetShaderParameter(variable, v);
                                break;
                            }
                            case VAR_VECTOR3:
                            {
                                Vector3 v = value.GetVector3();
                                if (ui::DragFloat3(variable.CString(), &v.x_))
                                    path->SetShaderParameter(variable, v);
                                break;
                            }
                            case VAR_VECTOR4:
                            {
                                Vector4 v = value.GetVector4();
                                if (ui::DragFloat4(variable.CString(), &v.x_))
                                    path->SetShaderParameter(variable, v);
                                break;
                            }
                            default:
                                break;
                            }
                        }
                    }
                }
            }
        }
        ui::End();
    }
}

void SceneView::LoadProject(XMLElement scene)
{
    ReloadDataForSettings();

    id_ = StringHash(ToUInt(scene.GetAttribute("id"), 16));
    SetTitle(scene.GetAttribute("title"));
    LoadScene(scene.GetAttribute("path"));

    auto camera = scene.GetChild("camera");
    if (camera.NotNull())
    {
        if (auto position = camera.GetChild("position"))
            camera_->SetPosition(position.GetVariant().GetVector3());
        if (auto rotation = camera.GetChild("rotation"))
            camera_->SetRotation(rotation.GetVariant().GetQuaternion());
        if (auto light = camera.GetChild("light"))
            camera_->GetComponent<Light>()->SetEnabled(light.GetVariant().GetBool());
    }

    if (auto saveElapsedTime = scene.GetChild("saveElapsedTime"))
        saveSceneElapsedTime_ = saveElapsedTime.GetVariant().GetBool();

    RenderPath* path = viewport_->GetRenderPath();
    for (auto postprocess = scene.GetChild("postprocess"); postprocess.NotNull();
        postprocess = postprocess.GetNext("postprocess"))
    {
        auto effectPath = postprocess.GetAttribute("path");
        auto tagName = postprocess.GetAttribute("tag");

        if (!path->IsAdded(tagName))
        {
            path->Append(GetCache()->GetResource<XMLFile>(effectPath));
            if (effectVariables_.Contains(tagName))
            {
                // Some render paths have multiple tags and appending enables them all. Disable all tags
                // in added path, later on only selected tag will be enabled.
                for (const auto& tag2: effectVariables_[tagName].Keys())
                    path->SetEnabled(tag2, false);
            }
        }

        path->SetEnabled(tagName, true);

        for (auto child = postprocess.GetChild(); child.NotNull(); child = child.GetNext())
            path->SetShaderParameter(child.GetName(), child.GetVariant());
    }
}

void SceneView::SaveProject(XMLElement scene) const
{
    scene.SetAttribute("id", id_.ToString().CString());
    scene.SetAttribute("title", title_);
    scene.SetAttribute("path", path_);

    auto camera = scene.CreateChild("camera");
    camera.CreateChild("position").SetVariant(camera_->GetPosition());
    camera.CreateChild("rotation").SetVariant(camera_->GetRotation());
    camera.CreateChild("light").SetVariant(camera_->GetComponent<Light>()->IsEnabled());

    scene.CreateChild("saveElapsedTime").SetVariant(saveSceneElapsedTime_);

    RenderPath* path = viewport_->GetRenderPath();
    for (auto it = effectVariables_.Begin(); it != effectVariables_.End(); it++)
    {
        auto fileName = it->first_;
        for (auto jt = it->second_.Begin(); jt != it->second_.End(); jt++)
        {
            auto tag = jt->first_;
            if (!path->IsEnabled(tag))
                continue;

            auto postprocess = scene.CreateChild("postprocess");
            postprocess.SetAttribute("path", fileName);
            postprocess.SetAttribute("tag", tag);
            for (const auto& variable: jt->second_)
            {
                auto var = postprocess.CreateChild(variable);
                var.SetVariant(path->GetShaderParameter(variable));
            }
        }
    }
}

void SceneView::SetTitle(const String& title)
{
    title_ = title;
    uniqueTitle_ = ToString("%s##%s", title.CString(), id_.ToString().CString());
}

void SceneView::ClearCachedPaths()
{
    path_.Clear();
}

Node* SceneView::GetRendererNode()
{
    renderer_ = context_->CreateObject<Node>();
    renderer_->SetPosition(Vector3::FORWARD);
    StaticModel* model = renderer_->CreateComponent<StaticModel>();
    model->SetModel(GetCache()->GetResource<Model>("Models/Plane.mdl"));
    SharedPtr<Material> material(new Material(context_));
    material->SetTechnique(0, GetCache()->GetResource<Technique>("Techniques/DiffUnlit.xml"));
    material->SetTexture(TU_DIFFUSE, view_);
    material->SetDepthBias(BiasParameters(-0.001f, 0.0f));
    model->SetMaterial(material);
    return renderer_;
}

void SceneView::ReloadDataForSettings()
{
    ReloadPostProcessEffects();
}

void SceneView::ReloadPostProcessEffects()
{
    for (const auto& dir: GetCache()->GetResourceDirs())
    {
        Vector<String> effects;
        GetFileSystem()->ScanDir(effects, AddTrailingSlash(dir) + "PostProcess", "*.xml", SCAN_FILES, false);

        for (const auto& effectFileName: effects)
        {
            auto fullFileName = "PostProcess/" + effectFileName;
            XMLFile* effect = GetCache()->GetResource<XMLFile>(fullFileName);

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

                for (auto parameter = command.GetChild("parameter"); parameter.NotNull();
                    parameter = parameter.GetNext("parameter"))
                {
                    String name = parameter.GetAttribute("name");
                    String valueString = parameter.GetAttribute("value");

                    if (name.Empty() || valueString.Empty())
                    {
                        URHO3D_LOGWARNINGF("Invalid PostProcess effect tagged as %s", tag.CString());
                        continue;
                    }

                    auto& variables = effectVariables_[fullFileName][tag];
                    if (!variables.Contains(name))
                        variables.Push(name);
                }

                // Just in case there were no parameters this will still create empty parameter list. Keys of this map are
                // used for determining existence of effect.
                effectVariables_[tag];
            }
        }
    }
}

}
