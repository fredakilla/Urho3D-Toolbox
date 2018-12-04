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

#pragma once


#include "../Common/ToolBoxObject.h"


namespace Urho3D
{

class RenderSurface;
class Scene;


/// Scene manager has activated a new scene.
URHO3D_EVENT(E_SCENEACTIVATED, SceneActivated)
{
    URHO3D_PARAM(P_OLDSCENE, OldScene);            // Scene pointer
    URHO3D_PARAM(P_NEWSCENE, NewScene);            // Scene pointer
}


class URHO3D_API SceneManager : public ToolBoxObject
{
    URHO3D_OBJECT(SceneManager, ToolBoxObject)
    public:
    /// Construct.
    explicit SceneManager(Context* context);
    /// Register object with the engine.
    static void RegisterObject(Context* context);

    /// Creates and returns empty scene. Returns null if scene already exists.
    Scene* CreateScene(const String& name=String::EMPTY);
    /// Returns a previously created scene or null if no scene with specified name was created.
    Scene* GetScene(const String& name);
    /// Returns a previously created scene if it exits or creates a new one.
    Scene* GetOrCreateScene(const String& name);
    /// Unload scene from memory.
    void UnloadScene(Scene* scene);
    /// Unload scene from memory.
    void UnloadScene(const String& name);
    /// Unloads all scenes from memory.
    void UnloadAll();
    /// Unloads all scenes from memory except active one.
    void UnloadAllButActiveScene();
    /// Set specified scene as active. It will start rendering to viewports set up by scene components.
    void SetActiveScene(Scene* scene);
    /// Set specified scene as active. It will start rendering to viewports set up by scene components.
    void SetActiveScene(const String& name);
    /// Get current active scene. Returns null pointer if no scene is active.
    Scene* GetActiveScene() const { return activeScene_; }
    /// Set surface to which active scene should render. If surface is null then scene will render to main window.
    void SetRenderSurface(RenderSurface* surface);

protected:
    /// Creates and sets up viewports for scene rendering.
    void UpdateViewports();

    /// Current loaded scenes.
    Vector<SharedPtr<Scene>> scenes_;
    /// Current active scene.
    WeakPtr<Scene> activeScene_;
    /// Surface for rendering active scene into.
    WeakPtr<RenderSurface> renderSurface_;
    ///
    bool missingMetadataWarned_ = false;
};

}
