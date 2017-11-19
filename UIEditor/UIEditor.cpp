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
#include <Urho3D/Engine/Application.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/DebugRenderer.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/GraphicsEvents.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/SceneEvents.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/Window.h>

#include <tinyfiledialogs/tinyfiledialogs.h>
#include <IconFontCppHeaders/IconsFontAwesome.h>

#include <unordered_map>

#include <Toolbox/Common/UndoManager.h>
#include <Toolbox/SystemUI/AttributeInspector.h>
#include <Toolbox/SystemUI/SystemUI.h>
#include <Toolbox/SystemUI/Widgets.h>


using namespace std::placeholders;

namespace Urho3D
{

class UIEditor : public Application
{
URHO3D_OBJECT(UIEditor, Application);
public:
    SharedPtr<Scene> scene_;
    WeakPtr<UIElement> selectedElement_;
    WeakPtr<Camera> camera_;
    Undo::Manager undo_;
    String currentFilePath_;
    String currentStyleFilePath_;
    bool showInternal_ = false;
    bool hideResizeHandles_ = false;
    Vector<String> styleNames_;
    String textureSelectorAttribute_;
    int textureWindowScale_ = 1;
    WeakPtr<UIElement> rootElement_;
    AttributeInspector inspector_;
    ImGuiWindowFlags_ rectWindowFlags_ = (ImGuiWindowFlags_)0;
    IntRect rectWindowDeltaAccumulator_;

    explicit UIEditor(Context* context) : Application(context), undo_(context), inspector_(context)
    {
    }

    void Setup() override
    {
        engineParameters_[EP_WINDOW_TITLE] = GetTypeName();
        engineParameters_[EP_HEADLESS] = false;
        engineParameters_[EP_RESOURCE_PREFIX_PATHS] =
            GetSubsystem<FileSystem>()->GetProgramDir() + ";;..;../share/Urho3D/Resources";
        engineParameters_[EP_FULL_SCREEN] = false;
        engineParameters_[EP_WINDOW_HEIGHT] = 1080;
        engineParameters_[EP_WINDOW_WIDTH] = 1920;
        engineParameters_[EP_LOG_LEVEL] = LOG_DEBUG;
        engineParameters_[EP_WINDOW_RESIZABLE] = true;
        engineParameters_[EP_RESOURCE_PATHS] = "CoreData;Data;EditorData";
    }

    void Start() override
    {
        context_->RegisterFactory<SystemUI>();
        context_->RegisterSubsystem(new SystemUI(context_));

        rootElement_ = GetSubsystem<UI>()->GetRoot();
        GetSubsystem<SystemUI>()->AddFont("Fonts/fontawesome-webfont.ttf", 0, {ICON_MIN_FA, ICON_MAX_FA, 0}, true);

        Input* input = GetSubsystem<Input>();
        input->SetMouseMode(MM_FREE);
        input->SetMouseVisible(true);

        // Background color
        scene_ = new Scene(context_);
        scene_->CreateComponent<Octree>();
        scene_->CreateComponent<Zone>()->SetFogColor(Color(0.2f, 0.2f, 0.2f));

        camera_ = scene_->CreateChild("Camera")->CreateComponent<Camera>();
        camera_->SetOrthographic(true);
        camera_->GetNode()->SetPosition({0, 10, 0});
        camera_->GetNode()->LookAt({0, 0, 0});
        GetSubsystem<Renderer>()->SetViewport(0, new Viewport(context_, scene_, camera_));

        // Events
        SubscribeToEvent(E_UPDATE, std::bind(&UIEditor::RenderSystemUI, this));
        SubscribeToEvent(E_DROPFILE, std::bind(&UIEditor::OnFileDrop, this, _2));
        SubscribeToEvent(E_ATTRIBUTEINSPECTORMENU, std::bind(&UIEditor::AttributeMenu, this, _2));
        SubscribeToEvent(E_ATTRIBUTEINSPECTOATTRIBUTE, std::bind(&UIEditor::AttributeCustomize, this, _2));

        undo_.Connect(rootElement_);
        undo_.Connect(&inspector_);

        // UI style
        GetSubsystem<SystemUI>()->ApplyStyleDefault(true, 1.0f);
        ui::GetStyle().WindowRounding = 3;

        // Arguments
        for (const auto& arg: GetArguments())
            LoadFile(arg);
    }

    void AttributeMenu(VariantMap& args)
    {
        using namespace AttributeInspectorMenu;

        if (auto selected = GetSelected())
        {
            auto* item = dynamic_cast<Serializable*>(args[P_SERIALIZABLE].GetPtr());
            auto* info = static_cast<AttributeInfo*>(args[P_ATTRIBUTEINFO].GetVoidPtr());

            Variant value = item->GetAttribute(info->name_);
            XMLElement styleAttribute;
            XMLElement styleXml;
            Variant styleVariant;
            GetStyleData(*info, styleXml, styleAttribute, styleVariant);

            if (styleVariant != value)
            {
                if (!styleVariant.IsEmpty())
                {
                    if (ui::MenuItem("Reset to style"))
                    {
                        undo_.TrackState(item, info->name_, styleVariant, value);
                        item->SetAttribute(info->name_, styleVariant);
                        item->ApplyAttributes();
                    }
                }

                if (styleXml.NotNull())
                {
                    if (ui::MenuItem("Save to style"))
                    {
                        if (styleAttribute.IsNull())
                        {
                            styleAttribute = undo_.XMLCreate(styleXml, "attribute");
                            styleAttribute.SetAttribute("name", info->name_);
                            styleAttribute.SetVariantValue(value);
                        }
                        else
                        {
                            undo_.XMLSetVariantValue(styleAttribute, styleAttribute.GetVariantValue(info->type_));
                            undo_.XMLSetVariantValue(styleAttribute, value);
                        }
                    }
                }
            }

            if (styleAttribute.NotNull() && !styleVariant.IsEmpty())
            {
                if (ui::MenuItem("Remove from style"))
                    undo_.XMLRemove(styleAttribute);
            }

            if (info->type_ == VAR_INTRECT && dynamic_cast<BorderImage*>(selected) != nullptr)
            {
                if (ui::MenuItem("Select in UI Texture"))
                    textureSelectorAttribute_ = info->name_;
            }
        }
    }

    void AttributeCustomize(VariantMap& args)
    {
        using namespace AttributeInspectorAttribute;

        if (auto selected = GetSelected())
        {
            auto* item = dynamic_cast<Serializable*>(args[P_SERIALIZABLE].GetPtr());
            auto* info = static_cast<AttributeInfo*>(args[P_ATTRIBUTEINFO].GetVoidPtr());

            Variant value = item->GetAttribute(info->name_);
            XMLElement styleAttribute;
            XMLElement styleXml;
            Variant styleVariant;
            GetStyleData(*info, styleXml, styleAttribute, styleVariant);

            if (!styleVariant.IsEmpty())
            {
                if (styleVariant == value)
                {
                    args[P_COLOR] = Color::GRAY;
                    args[P_TOOLTIP] = "Value inherited from style.";
                }
                else
                {
                    args[P_COLOR] = Color::GREEN;
                    args[P_TOOLTIP] = "Style value was modified.";
                }
            }
        }
    }

    void RenderSystemUI()
    {
        if (ui::BeginMainMenuBar())
        {
            if (ui::BeginMenu("File"))
            {
                if (ui::MenuItem(ICON_FA_FILE_TEXT " New"))
                    rootElement_->RemoveAllChildren();

                const char* filters[] = {"*.xml"};
                if (ui::MenuItem(ICON_FA_FOLDER_OPEN " Open"))
                {
                    auto filename = tinyfd_openFileDialog("Open file", ".", 2, filters, "XML files", 0);
                    if (filename)
                        LoadFile(filename);
                }

                if (ui::MenuItem(ICON_FA_FLOPPY_O " Save UI As") && rootElement_->GetNumChildren() > 0)
                {
                    if (auto path = tinyfd_saveFileDialog("Save UI file", ".", 1, filters, "XML files"))
                        SaveFileUI(path);
                }


                if (ui::MenuItem(ICON_FA_FLOPPY_O " Save Style As") && rootElement_->GetNumChildren() > 0 && rootElement_->GetChild(0)->GetDefaultStyle())
                {
                    if (auto path = tinyfd_saveFileDialog("Save Style file", ".", 1, filters, "XML files"))
                        SaveFileStyle(path);
                }

                ui::EndMenu();
            }

            if (ui::Button(ICON_FA_FLOPPY_O))
            {
                if (!currentFilePath_.Empty())
                    SaveFileUI(currentFilePath_);
                if (GetCurrentStyleFile() != nullptr)
                    SaveFileStyle(currentStyleFilePath_);
            }

            if (ui::IsItemHovered())
                ui::SetTooltip("Save current UI and style files.");

            ui::SameLine();

            if (ui::Button(ICON_FA_UNDO))
                undo_.Undo();

            if (ui::IsItemHovered())
                ui::SetTooltip("Undo.");
            ui::SameLine();

            if (ui::Button(ICON_FA_REPEAT))
                undo_.Redo();

            if (ui::IsItemHovered())
                ui::SetTooltip("Redo.");

            ui::SameLine();

            ui::Checkbox("Show Internal", &showInternal_);
            ui::SameLine();

            ui::Checkbox("Hide Resize Handles", &hideResizeHandles_);
            ui::SameLine();

            ui::EndMainMenuBar();
        }

        const auto menuBarHeight = 20.f;
        const auto leftPanelWidth = 300.f;
        const auto leftPanelRight = 400.f;
        const auto panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                ImGuiWindowFlags_NoTitleBar;

        Graphics* graphics = GetSubsystem<Graphics>();
        auto windowHeight = (float)graphics->GetHeight();
        auto windowWidth = (float)graphics->GetWidth();
        IntVector2 rootPos(5, static_cast<int>(5 + menuBarHeight));
        IntVector2 rootSize(0, static_cast<int>(windowHeight) - 20);

        ui::SetNextWindowPos({0.f, menuBarHeight}, ImGuiCond_Always);
        ui::SetNextWindowSize({leftPanelWidth, windowHeight - menuBarHeight});
        if (ui::Begin("ElementTree", nullptr, panelFlags))
        {
            rootPos.x_ += static_cast<int>(ui::GetWindowWidth());
            RenderUiTree(rootElement_);
        }
        ui::End();


        ui::SetNextWindowPos({windowWidth - leftPanelRight, menuBarHeight}, ImGuiCond_Always);
        ui::SetNextWindowSize({leftPanelRight, windowHeight - menuBarHeight});
        if (ui::Begin("AttributeList", nullptr, panelFlags))
        {
            rootSize.x_ = static_cast<int>(windowWidth - rootPos.x_ - ui::GetWindowWidth());
            if (auto selected = GetSelected())
            {
                // Label
                ui::TextUnformatted("Style");
                inspector_.NextColumn();

                // Style name
                auto type_style = GetAppliedStyle();
                ui::TextUnformatted(type_style.CString());

                inspector_.RenderAttributes(selected);
            }
        }
        ui::End();

        rootElement_->SetSize(rootSize);
        rootElement_->SetPosition(rootPos);

        // Background window
        // Used for rendering various lines on top of UrhoUI.
        const auto backgroundTextWindowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
                                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
        ui::SetNextWindowSize(ToImGui(graphics->GetSize()), ImGuiCond_Always);
        ui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ui::PushStyleColor(ImGuiCol_WindowBg, 0);
        if (ui::Begin("Background Window", nullptr, backgroundTextWindowFlags))
        {
            if (auto selected = GetSelected())
            {
                // Render element selection rect, resize handles, and handle element transformations.
                IntRect delta;
                IntRect screenRect(
                    selected->GetScreenPosition(),
                    selected->GetScreenPosition() + selected->GetSize()
                );
                auto flags = ui::TSF_NONE;
                if (hideResizeHandles_)
                    flags |= ui::TSF_HIDEHANDLES;
                if (selected->GetMinSize().x_ == selected->GetMaxSize().x_)
                    flags |= ui::TSF_NOHORIZONTAL;
                if (selected->GetMinSize().y_ == selected->GetMaxSize().y_)
                    flags |= ui::TSF_NOVERTICAL;

                struct State
                {
                    bool resizeActive_ = false;
                    IntVector2 resizeStartPos_;
                    IntVector2 resizeStartSize_;
                };
                State* s = ui::GetUIState<State>();

                if (ui::TransformRect(screenRect, delta, flags))
                {
                    if (!s->resizeActive_)
                    {
                        s->resizeActive_ = true;
                        s->resizeStartPos_ = selected->GetPosition();
                        s->resizeStartSize_ = selected->GetSize();
                    }
                    selected->SetPosition(selected->GetPosition() + delta.Min());
                    selected->SetSize(selected->GetSize() + delta.Size());
                }

                if (s->resizeActive_ && !ui::IsItemActive())
                {
                    s->resizeActive_ = false;
                    undo_.TrackState(selected, "Position", selected->GetPosition(), s->resizeStartPos_);
                    undo_.TrackState(selected, "Size", selected->GetSize(), s->resizeStartSize_);
                }
            }
        }
        ui::End();
        ui::PopStyleColor();
        // Background window end

        auto input = GetSubsystem<Input>();
        if (!ui::IsAnyItemActive() && !ui::IsAnyItemHovered() && !ui::IsAnyWindowHovered() &&
            (input->GetMouseButtonPress(MOUSEB_LEFT) || input->GetMouseButtonPress(MOUSEB_RIGHT)))
        {
            auto pos = input->GetMousePosition();
            auto clicked = GetSubsystem<UI>()->GetElementAt(pos, false);
            if (!clicked && rootElement_->GetCombinedScreenRect().IsInside(pos) == INSIDE && !ui::IsAnyWindowHovered())
                clicked = rootElement_;

            if (clicked)
                SelectItem(clicked);
        }

        if (auto selected = GetSelected())
        {
            if (input->GetKeyPress(KEY_DELETE))
            {
                selected->Remove();
                SelectItem(nullptr);
            }
        }

        // These interactions include root element, therefore GetSelected() is not used here.
        if (selectedElement_)
        {
            if (ui::BeginPopupContextVoid("Element Context Menu", 2))
            {
                if (ui::BeginMenu("Add Child"))
                {
                    const char* uiTypes[] = {"BorderImage", "Button", "CheckBox", "Cursor", "DropDownList", "LineEdit",
                                              "ListView", "Menu", "ProgressBar", "ScrollBar", "ScrollView", "Slider",
                                              "Sprite", "Text", "ToolTip", "UIElement", "View3D", "Window", nullptr
                    };
                    for (auto i = 0; uiTypes[i] != nullptr; i++)
                    {
                        // TODO: element creation with custom styles more usable.
                        if (input->GetKeyDown(KEY_SHIFT))
                        {
                            if (ui::BeginMenu(uiTypes[i]))
                            {
                                for (auto j = 0; j < styleNames_.Size(); j++)
                                {
                                    if (ui::MenuItem(styleNames_[j].CString()))
                                    {
                                        SelectItem(selectedElement_->CreateChild(uiTypes[i]));
                                        selectedElement_->SetStyle(styleNames_[j]);
                                    }
                                }
                                ui::EndMenu();
                            }
                        }
                        else
                        {
                            if (ui::MenuItem(uiTypes[i]))
                            {
                                SelectItem(selectedElement_->CreateChild(uiTypes[i]));
                                selectedElement_->SetStyleAuto();
                            }
                        }
                    }
                    ui::EndMenu();
                }

                if (selectedElement_ != rootElement_)
                {
                    if (ui::MenuItem("Delete Element"))
                    {
                        selectedElement_->Remove();
                        SelectItem(nullptr);
                    }

                    if (ui::MenuItem("Bring To Front"))
                        selectedElement_->BringToFront();
                }
                ui::EndPopup();
            }

            if (!textureSelectorAttribute_.Empty())
            {
                auto selected = DynamicCast<BorderImage>(selectedElement_);
                bool open = selected.NotNull();
                if (open)
                {
                    auto tex = selected->GetTexture();
                    // Texture is better visible this way when zoomed in.
                    tex->SetFilterMode(FILTER_NEAREST);
                    auto padding = ImGui::GetStyle().WindowPadding;
                    ui::SetNextWindowPos(ImVec2(tex->GetWidth() + padding.x * 2, tex->GetHeight() + padding.y * 2),
                                         ImGuiCond_FirstUseEver);
                    if (ui::Begin("Select Rect", &open, rectWindowFlags_))
                    {
                        ui::SliderInt("Zoom", &textureWindowScale_, 1, 5);
                        auto windowPos = ui::GetWindowPos();
                        auto imagePos = ui::GetCursorPos();
                        ui::Image(tex, ImVec2(tex->GetWidth() * textureWindowScale_,
                                              tex->GetHeight() * textureWindowScale_));

                        // Disable dragging of window if mouse is hovering texture.
                        if (ui::IsItemHovered())
                            rectWindowFlags_ = ImGuiWindowFlags_NoMove;
                        else
                            rectWindowFlags_ = (ImGuiWindowFlags_)0;

                        if (!textureSelectorAttribute_.Empty())
                        {
                            IntRect rect = selectedElement_->GetAttribute(textureSelectorAttribute_).GetIntRect();
                            IntRect originalRect = rect;
                            // Upscale selection rect if texture is upscaled.
                            rect *= textureWindowScale_;

                            ui::TransformSelectorFlags flags = ui::TSF_NONE;
                            if (hideResizeHandles_)
                                flags |= ui::TSF_HIDEHANDLES;

                            IntRect screenRect(
                                rect.Min() + ToIntVector2(imagePos) + ToIntVector2(windowPos),
                                IntVector2(rect.right_ - rect.left_, rect.bottom_ - rect.top_)
                            );
                            // Essentially screenRect().Max() += screenRect().Min()
                            screenRect.bottom_ += screenRect.top_;
                            screenRect.right_ += screenRect.left_;
                            IntRect delta;

                            struct State
                            {
                                bool resizeActive_ = false;
                                IntRect resizeStart_;
                            };
                            State* s = ui::GetUIState<State>();

                            if (ui::TransformRect(screenRect, delta, flags))
                            {
                                if (!s->resizeActive_)
                                {
                                    s->resizeActive_ = true;
                                    s->resizeStart_ = originalRect;
                                }
                                // Accumulate delta value. This is required because resizing upscaled rect does not work
                                // with small increments when rect values are integers.
                                rectWindowDeltaAccumulator_ += delta;
                            }

                            if (ui::IsItemActive())
                            {
                                // Downscale and add accumulated delta to the original rect value
                                rect = originalRect + rectWindowDeltaAccumulator_ / textureWindowScale_;

                                // If downscaled rect size changed compared to original value - set attribute and
                                // reset delta accumulator.
                                if (rect != originalRect)
                                {
                                    selectedElement_->SetAttribute(textureSelectorAttribute_, rect);
                                    // Keep remainder in accumulator, otherwise resizing will cause cursor to drift from
                                    // the handle over time.
                                    rectWindowDeltaAccumulator_.left_ %= textureWindowScale_;
                                    rectWindowDeltaAccumulator_.top_ %= textureWindowScale_;
                                    rectWindowDeltaAccumulator_.right_ %= textureWindowScale_;
                                    rectWindowDeltaAccumulator_.bottom_ %= textureWindowScale_;
                                }
                            }
                            else if (s->resizeActive_)
                            {
                                s->resizeActive_ = false;
                                undo_.TrackState(selected, textureSelectorAttribute_,
                                    selectedElement_->GetAttribute(textureSelectorAttribute_), s->resizeStart_);
                            }
                        }
                    }
                    ui::End();
                }

                if (!open)
                    textureSelectorAttribute_.Clear();
            }
        }

        if (!ui::IsAnyItemActive())
        {
            if (input->GetKeyDown(KEY_CTRL))
            {
                if (input->GetKeyPress(KEY_Y) || (input->GetKeyDown(KEY_SHIFT) && input->GetKeyPress(KEY_Z)))
                    undo_.Redo();
                else if (input->GetKeyPress(KEY_Z))
                    undo_.Undo();
            }
        }
    }

    void OnFileDrop(VariantMap& args)
    {
        LoadFile(args[DropFile::P_FILENAME].GetString());
    }

    String GetResourcePath(String filePath)
    {
        const static Vector<String> dataDirectories = {
            "Materials", "RenderPaths", "Shaders", "Techniques", "Textures", "Fonts", "Models", "Particle", "Scenes",
            "Textures", "Music", "Objects", "PostProcess", "Sounds", "UI"
        };

        for (;filePath.Length();)
        {
            filePath = GetParentPath(filePath);
            for (const auto& dataDirectory: dataDirectories)
            {
                if (GetSubsystem<FileSystem>()->DirExists(filePath + dataDirectory))
                    return filePath;
            }
        }

        return "";
    }

    bool LoadFile(const String& filePath)
    {
        auto cache = GetSubsystem<ResourceCache>();
        String resourceDir;
        if (IsAbsolutePath(filePath))
        {
            if (!currentFilePath_.Empty())
            {
                resourceDir = GetResourcePath(currentFilePath_);
                if (!resourceDir.Empty())
                    cache->RemoveResourceDir(resourceDir);
            }

            resourceDir = GetResourcePath(filePath);
            if (!resourceDir.Empty())
            {
                if (!cache->GetResourceDirs().Contains(resourceDir))
                    cache->AddResourceDir(resourceDir);
            }
        }

        if (filePath.EndsWith(".xml", false))
        {
            SharedPtr<XMLFile> xml(new XMLFile(context_));
            bool loaded = false;
            if (IsAbsolutePath(filePath))
                loaded = xml->LoadFile(filePath);
            else
            {
                auto cacheFile = cache->GetFile(filePath);
                loaded = xml->Load(*cacheFile);
            }

            if (loaded)
            {
                if (xml->GetRoot().GetName() == "elements")
                {
                    // This is a style.
                    rootElement_->SetDefaultStyle(xml);
                    currentStyleFilePath_ = filePath;

                    auto styles = xml->GetRoot().SelectPrepared(XPathQuery("/elements/element"));
                    for (auto i = 0; i < styles.Size(); i++)
                    {
                        auto type = styles[i].GetAttribute("type");
                        if (type.Length() && !styleNames_.Contains(type) &&
                            styles[i].GetAttribute("auto").ToLower() == "false")
                            styleNames_.Push(type);
                    }
                    Sort(styleNames_.Begin(), styleNames_.End());
                    UpdateWindowTitle();
                    return true;
                }
                else if (xml->GetRoot().GetName() == "element")
                {
                    // If element has style file specified - load it
                    auto styleFile = xml->GetRoot().GetAttribute("styleFile");
                    if (!styleFile.Empty())
                    {
                        styleFile = cache->GetResourceFileName(styleFile);
                        if (!currentStyleFilePath_.Empty())
                        {
                            auto styleResourceDir = GetResourcePath(currentStyleFilePath_);
                            if (!styleResourceDir.Empty())
                                cache->RemoveResourceDir(styleResourceDir);
                        }
                        LoadFile(styleFile);
                    }

                    Vector<SharedPtr<UIElement>> children = rootElement_->GetChildren();
                    auto child = rootElement_->CreateChild(xml->GetRoot().GetAttribute("type"));
                    if (child->LoadXML(xml->GetRoot()))
                    {
                        // If style file is not in xml then apply style according to ui types.
                        if (styleFile.Empty())
                            child->SetStyleAuto();

                        // Must be disabled because it interferes with ui element resizing
                        if (auto window = dynamic_cast<Window*>(child))
                        {
                            window->SetMovable(false);
                            window->SetResizable(false);
                        }

                        currentFilePath_ = filePath;
                        UpdateWindowTitle();

                        for (const auto& oldChild : children)
                            oldChild->Remove();

                        undo_.Clear();
                        return true;
                    }
                    else
                        child->Remove();
                }
            }
        }

        cache->RemoveResourceDir(resourceDir);
        tinyfd_messageBox("Error", "Opening XML file failed", "ok", "error", 1);
        return false;
    }

    bool SaveFileUI(const String& file_path)
    {
        if (file_path.EndsWith(".xml", false))
        {
            XMLFile xml(context_);
            XMLElement root = xml.CreateRoot("element");
            if (rootElement_->GetChild(0)->SaveXML(root))
            {
                // Remove internal UI elements
                auto result = root.SelectPrepared(XPathQuery("//element[@internal=\"true\"]"));
                for (auto el = result.FirstResult(); el.NotNull(); el = el.NextResult())
                    el.GetParent().RemoveChild(el);

                // Remove style="none"
                root.SelectPrepared(XPathQuery("//element[@style=\"none\"]"));
                for (auto el = result.FirstResult(); el.NotNull(); el = el.NextResult())
                    el.RemoveAttribute("style");

                File saveFile(context_, file_path, FILE_WRITE);
                xml.Save(saveFile);

                currentFilePath_ = file_path;
                UpdateWindowTitle();
                return true;
            }
        }

        tinyfd_messageBox("Error", "Saving UI file failed", "ok", "error", 1);
        return false;
    }

    bool SaveFileStyle(const String& file_path)
    {
        if (file_path.EndsWith(".xml", false))
        {
            auto styleFile = GetCurrentStyleFile();
            if (styleFile == nullptr)
                return false;

            File saveFile(context_, file_path, FILE_WRITE);
            styleFile->Save(saveFile);
            saveFile.Close();

            // Remove all attributes with empty value. Empty value is used to "fake" removal, because current xml class
            // does not allow removing and reinserting xml elements, they must be recreated. Removal has to be done on
            // reopened and re-read xml file so that it does not break undo functionality of currently edited file.
            SharedPtr<XMLFile> xml(new XMLFile(context_));
            xml->LoadFile(file_path);
            auto result = xml->GetRoot().SelectPrepared(XPathQuery("//attribute[@type=\"None\"]"));
            for (auto attribute = result.FirstResult(); attribute.NotNull(); attribute.NextResult())
                attribute.GetParent().RemoveChild(attribute);
            xml->SaveFile(file_path);

            currentStyleFilePath_ = file_path;
            UpdateWindowTitle();
            return true;
        }

        tinyfd_messageBox("Error", "Saving UI file failed", "ok", "error", 1);
        return false;
    }

    void RenderUiTree(UIElement* element)
    {
        auto& name = element->GetName();
        auto& type = element->GetTypeName();
        auto tooltip = "Type: " + type;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        bool is_internal = element->IsInternal();
        if (is_internal && !showInternal_)
            return;
        else
            flags |= ImGuiTreeNodeFlags_DefaultOpen;

        if (showInternal_)
            tooltip += String("\nInternal: ") + (is_internal ? "true" : "false");

        if (element == selectedElement_)
            flags |= ImGuiTreeNodeFlags_Selected;

        if (ui::TreeNodeEx(element, flags, "%s", name.Length() ? name.CString() : type.CString()))
        {
            if (ui::IsItemHovered())
                ui::SetTooltip("%s", tooltip.CString());

            if (ui::IsItemHovered() && ui::IsMouseClicked(0))
                SelectItem(element);

            for (const auto& child: element->GetChildren())
                RenderUiTree(child);
            ui::TreePop();
        }
    }

    String GetAppliedStyle(UIElement* element = nullptr)
    {
        if (element == nullptr)
            element = selectedElement_;

        if (element == nullptr)
            return "";

        auto applied_style = selectedElement_->GetAppliedStyle();
        if (applied_style.Empty())
            applied_style = selectedElement_->GetTypeName();
        return applied_style;
    }

    String GetBaseName(const String& full_path)
    {
        auto parts = full_path.Split('/');
        return parts.At(parts.Size() - 1);
    }

    void UpdateWindowTitle()
    {
        String window_name = "UrhoUIEditor";
        if (!currentFilePath_.Empty())
            window_name += " - " + GetBaseName(currentFilePath_);
        if (!currentStyleFilePath_.Empty())
            window_name += " - " + GetBaseName(currentStyleFilePath_);
        GetSubsystem<Graphics>()->SetWindowTitle(window_name);
    }

    void SelectItem(UIElement* current)
    {
        if (current == nullptr)
            textureSelectorAttribute_.Clear();

        selectedElement_ = current;
    }

    UIElement* GetSelected() const
    {
        // Can not select root widget
        if (selectedElement_ == GetSubsystem<UI>()->GetRoot())
            return nullptr;

        return selectedElement_;
    }

    void GetStyleData(const AttributeInfo& info, XMLElement& style, XMLElement& attribute, Variant& value)
    {
        auto styleFile = selectedElement_->GetDefaultStyle();
        if (styleFile == nullptr)
            return;

        static XPathQuery xpAttribute("attribute[@name=$name]", "name:String");
        static XPathQuery xpStyle("/elements/element[@type=$type]", "type:String");

        value = Variant();
        xpAttribute.SetVariable("name", info.name_);

        auto styleName = GetAppliedStyle();

        do
        {
            // Get current style
            xpStyle.SetVariable("type", styleName);
            style = styleFile->GetRoot().SelectSinglePrepared(xpStyle);
            // Look for attribute in current style
            attribute = style.SelectSinglePrepared(xpAttribute);
            // Go up in style hierarchy
            styleName = style.GetAttribute("Style");
        } while (attribute.IsNull() && !styleName.Empty() && !style.IsNull());


        if (!attribute.IsNull() && attribute.GetAttribute("type") != "None")
            value = GetVariantFromXML(attribute, info);
    }

    Variant GetVariantFromXML(const XMLElement& attribute, const AttributeInfo& info) const
    {
        Variant value = attribute.GetVariantValue(info.enumNames_ ? VAR_STRING : info.type_);
        if (info.enumNames_)
        {
            for (auto i = 0; info.enumNames_[i]; i++)
            {
                if (value.GetString() == info.enumNames_[i])
                {
                    value = i;
                    break;
                }
            }
        }
        return value;
    }

    XMLFile* GetCurrentStyleFile()
    {
        if (rootElement_->GetNumChildren() > 0)
            return rootElement_->GetChild(0)->GetDefaultStyle();

        return nullptr;
    }
};

}

URHO3D_DEFINE_APPLICATION_MAIN(Urho3D::UIEditor);
