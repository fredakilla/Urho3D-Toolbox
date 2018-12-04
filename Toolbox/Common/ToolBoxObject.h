#pragma once

#include <Urho3D/Core/Object.h>

namespace Urho3D
{

class Context;
class EventHandler;
class Engine;
class Time;
class WorkQueue;
#if URHO3D_PROFILING
class Profiler;
#endif
class FileSystem;
#if URHO3D_LOGGING
class Log;
#endif
class ResourceCache;
class Localization;
#if URHO3D_NETWORK
class Network;
#endif
class Input;
class Audio;
class UI;
class SystemUI;
class Graphics;
class Renderer;
#if URHO3D_TASKS
class Tasks;
#endif


class ToolBoxObject : public Object
{
public:

    ToolBoxObject(Context* context);

    /// Return engine subsystem.
    Engine* GetEngine() const;
    /// Return time subsystem.
    Time* GetTime() const;
    /// Return work queue subsystem.
    WorkQueue* GetWorkQueue() const;
    /// Return file system subsystem.
    FileSystem* GetFileSystem() const;
#if URHO3D_LOGGING
    /// Return logging subsystem.
    Log* GetLog() const;
#endif
    /// Return resource cache subsystem.
    ResourceCache* GetCache() const;
    /// Return localization subsystem.
    Localization* GetLocalization() const;
#if URHO3D_NETWORK
    /// Return network subsystem.
    Network* GetNetwork() const;
#endif
    /// Return input subsystem.
    Input* GetInput() const;
    /// Return audio subsystem.
    Audio* GetAudio() const;
    /// Return UI subsystem.
    UI* GetUI() const;
    /// Return system ui subsystem.
    SystemUI* GetSystemUI() const;
    /// Return graphics subsystem.
    Graphics* GetGraphics() const;
    /// Return renderer subsystem.
    Renderer* GetRenderer() const;
#if URHO3D_TASKS
    /// Return tasks subsystem.
    Tasks* GetTasks() const;
#endif


};

}
