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

#include "Editor.h"
#include "EditorEvents.h"
#include "EditorConstants.h"
#include "SceneView.h"
#include <ImGui/imgui_internal.h>
#include <IconFontCppHeaders/IconsFontAwesome.h>
#include <Toolbox/Toolbox.h>
#include <Toolbox/ImGuiDock.h>


URHO3D_DEFINE_APPLICATION_MAIN(Editor);


namespace Urho3D
{

Editor::Editor(Context* context)
    : Application(context)
    , inspector_(context)
{
}

void Editor::Setup()
{
    engineParameters_[EP_WINDOW_TITLE] = GetTypeName();
    engineParameters_[EP_HEADLESS] = false;
    engineParameters_[EP_RESOURCE_PREFIX_PATHS] =
        context_->GetFileSystem()->GetProgramDir() + ";;..;../share/Urho3D/Resources";
    engineParameters_[EP_FULL_SCREEN] = false;
    engineParameters_[EP_WINDOW_HEIGHT] = 1080;
    engineParameters_[EP_WINDOW_WIDTH] = 1920;
    engineParameters_[EP_LOG_LEVEL] = LOG_DEBUG;
    engineParameters_[EP_WINDOW_RESIZABLE] = true;

    LoadConfig();
}

void Editor::Start()
{
    GetInput()->SetMouseMode(MM_ABSOLUTE);
    GetInput()->SetMouseVisible(true);
    RegisterToolboxTypes(context_);
    context_->RegisterFactory<Editor>();
    context_->RegisterSubsystem(this);

    GetSystemUI()->ApplyStyleDefault(true, 1.0f);
    GetSystemUI()->AddFont("Fonts/fontawesome-webfont.ttf", 0, {ICON_MIN_FA, ICON_MAX_FA, 0}, true);
    ui::GetStyle().WindowRounding = 3;

    // Dummy scene required for textures to render
    scene_ = new Scene(context_);
    scene_->CreateComponent<Octree>();
    GetRenderer()->SetViewport(0, new Viewport(context_, scene_, scene_->CreateChild()->GetOrCreateComponent<Camera>()));

    inspector_.SetEnabled(true);

    SubscribeToEvent(E_UPDATE, std::bind(&Editor::OnUpdate, this, _2));

    CreateNewScene();
}

void Editor::Stop()
{
    SaveConfig();
    ui::ShutdownDock();
}

void Editor::SaveConfig()
{
    SharedPtr<XMLFile> xml(new XMLFile(context_));
    XMLElement root = xml->CreateRoot("editor");
    auto window = root.CreateChild("window");
    window.SetAttribute("width", ToString("%d", GetGraphics()->GetWidth()));
    window.SetAttribute("height", ToString("%d", GetGraphics()->GetHeight()));
    window.SetAttribute("x", ToString("%d", GetGraphics()->GetWindowPosition().x_));
    window.SetAttribute("y", ToString("%d", GetGraphics()->GetWindowPosition().y_));

    ui::SaveDock(root.CreateChild("docks"));
    xml->SaveFile("EditorConfig.xml");
}

void Editor::LoadConfig()
{
    SharedPtr<XMLFile> xml(new XMLFile(context_));
    if (xml->LoadFile("EditorConfig.xml"))
    {
        auto root = xml->GetRoot();
        if (root.NotNull())
        {
            auto window = root.GetChild("window");
            if (window.NotNull())
            {
                engineParameters_[EP_WINDOW_WIDTH] = ToInt(window.GetAttribute("width"));
                engineParameters_[EP_WINDOW_HEIGHT] = ToInt(window.GetAttribute("height"));
                engineParameters_[EP_WINDOW_POSITION_X] = ToInt(window.GetAttribute("x"));
                engineParameters_[EP_WINDOW_POSITION_Y] = ToInt(window.GetAttribute("y"));
            }

            ui::LoadDock(root.GetChild("docks"));
        }
    }
    else
        URHO3D_LOGWARNINGF("Loading EditorConfig.xml failed");
}

void Editor::OnUpdate(VariantMap& args)
{
    RenderMenuBar();

    ui::RootDock({0, 20}, ui::GetIO().DisplaySize - ImVec2(0, 20));

    if (ui::BeginDock("Hierarchy"))
    {
        if (!lastActiveView_.Expired())
            RenderSceneNodeTree(lastActiveView_->scene_);
    }
    ui::EndDock();

    activeView_ = nullptr;
    for (auto it = sceneViews_.Begin(); it != sceneViews_.End();)
    {
        auto& view = *it;
        if (view->RenderWindow())
        {
            ++it;
            if (view->isActive_)
                lastActiveView_ = activeView_ = view;
        }
        else
            it = sceneViews_.Erase(it);
    }

    if (!lastActiveView_.Expired())
        inspector_.SetSerializable(lastActiveView_->GetSelectedSerializable());
    else
        inspector_.SetSerializable(nullptr);
}

void Editor::RenderMenuBar()
{
    if (ui::BeginMainMenuBar())
    {
        if (ui::BeginMenu("File"))
        {
            if (ui::MenuItem("New Scene"))
                CreateNewScene();

            ui::Separator();

            if (ui::MenuItem("Exit"))
                engine_->Exit();

            ui::EndMenu();
        }

        if (!lastActiveView_.Expired())
        {
            lastActiveView_->RenderGizmoButtons();
            SendEvent(E_EDITORTOOLBARBUTTONS);
        }

        ui::EndMainMenuBar();
    }
}

void Editor::RenderSceneNodeTree(Node* node)
{
    if (node->HasTag(InternalEditorElementTag))
        return;

    String name = ToString("%s (%d)", (node->GetName().Empty() ? node->GetTypeName() : node->GetName()).CString(), node->GetID());
    bool isSelected = lastActiveView_->IsSelected(node);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (isSelected)
        flags |= ImGuiTreeNodeFlags_Selected;
    if (node == lastActiveView_->scene_)
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

    auto opened = ui::TreeNodeEx(name.CString(), flags);

    if (ui::IsItemClicked(0))
    {
        if (!GetInput()->GetKeyDown(KEY_CTRL))
            lastActiveView_->UnselectAll();
        lastActiveView_->ToggleSelection(node);
    }

    if (opened)
    {
        for (auto& component: node->GetComponents())
        {
            bool selected = inspector_.GetSerializable() == component;
            if (ui::Selectable(component->GetTypeName().CString(), selected))
            {
                lastActiveView_->UnselectAll();
                lastActiveView_->ToggleSelection(node);
                lastActiveView_->Select(component);
            }
        }

        for (auto& child: node->GetChildren())
            RenderSceneNodeTree(child);
        ui::TreePop();
    }
}

void Editor::CreateNewScene(const String& path)
{
    SharedPtr<SceneView> sceneView(new SceneView(context_));

    for (auto index = 1; ; index++)
    {
        auto newTitle = ToString("Scene#%d", index);
        if (GetSceneView(newTitle) == nullptr)
        {
            sceneView->title_ = newTitle;
            break;
        }
    }

    // In order to render scene to a texture we must add a dummy node to scene rendered to a screen, which has material
    // pointing to scene texture. This object must also be visible to main camera.
    auto node = scene_->CreateChild();
    node->SetPosition(Vector3::FORWARD);
    StaticModel* model = node->CreateComponent<StaticModel>();
    model->SetModel(GetCache()->GetResource<Model>("Models/Plane.mdl"));
    SharedPtr<Material> material(new Material(context_));
    material->SetTechnique(0, GetCache()->GetResource<Technique>("Techniques/DiffUnlit.xml"));
    material->SetTexture(TU_DIFFUSE, sceneView->view_);
    material->SetDepthBias(BiasParameters(-0.001f, 0.0f));
    model->SetMaterial(material);

    sceneView->SetScreenRect({0, 0, 1024, 768});
    sceneView->SetRendererNode(node);
    sceneViews_.Push(sceneView);

    sceneView->LoadScene("Data/Scenes/SceneLoadExample.xml");

    // Fills up hierarchy window
    if (lastActiveView_.Null())
        lastActiveView_ = sceneView;
}

bool Editor::IsActive(Scene* scene)
{
    if (scene == nullptr || activeView_.Null())
        return false;

    return activeView_->scene_ == scene && activeView_->isActive_;
}

SceneView* Editor::GetSceneView(const String& title)
{
    for (auto& view: sceneViews_)
    {
        if (view->title_ == title)
            return view;
    }
    return nullptr;
}

}