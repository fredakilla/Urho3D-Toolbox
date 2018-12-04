#include "ToolBoxObject.h"
#include <Urho3D/Urho3DAll.h>
#include "SystemUI/SystemUI.h"

namespace Urho3D
{

ToolBoxObject::ToolBoxObject(Context* context) :
    Object(context)
{
}

Engine* ToolBoxObject::GetEngine() const
{
    return context_->GetSubsystem<Engine>();
}

Time* ToolBoxObject::GetTime() const
{
    return context_->GetSubsystem<Time>();
}

WorkQueue* ToolBoxObject::GetWorkQueue() const
{
    return context_->GetSubsystem<WorkQueue>();
}

FileSystem* ToolBoxObject::GetFileSystem() const
{
    return context_->GetSubsystem<FileSystem>();
}

#if URHO3D_LOGGING
Log* ToolBoxObject::GetLog() const
{
    return context_->GetSubsystem<Log>();
}
#endif

ResourceCache* ToolBoxObject::GetCache() const
{
    return context_->GetSubsystem<ResourceCache>();
}

Localization* ToolBoxObject::GetLocalization() const
{
    return context_->GetSubsystem<Localization>();
}

#if URHO3D_NETWORK
Network* ToolBoxObject::GetNetwork() const
{
    return context_->GetSubsystem<Network>();
}
#endif

Input* ToolBoxObject::GetInput() const
{
    return context_->GetSubsystem<Input>();
}

Audio* ToolBoxObject::GetAudio() const
{
    return context_->GetSubsystem<Audio>();
}

UI* ToolBoxObject::GetUI() const
{
    return context_->GetSubsystem<UI>();
}

SystemUI* ToolBoxObject::GetSystemUI() const
{
    return context_->GetSubsystem<SystemUI>();
}

Graphics* ToolBoxObject::GetGraphics() const
{
    return context_->GetSubsystem<Graphics>();
}

Renderer* ToolBoxObject::GetRenderer() const
{
    return context_->GetSubsystem<Renderer>();
}

#if URHO3D_TASKS
Tasks* ToolBoxObject::GetTasks() const
{
    return context_->GetSubsystem<Log>();
}
#endif


}
