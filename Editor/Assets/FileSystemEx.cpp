#include "FileSystemEx.h"
#include <Urho3D/Core/Context.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Resource/ResourceCache.h>

namespace Urho3D
{

bool CreateDirsRecursive(const String& directoryIn, Context* context)
{
    String directory = AddTrailingSlash(GetInternalPath(directoryIn));

    if (context->GetSubsystem<FileSystem>()->DirExists(directory))
        return true;

    if (context->GetSubsystem<FileSystem>()->FileExists(directory))
        return false;

    String parentPath = directory;

    Vector<String> paths;

    paths.Push(directory);

    while (true)
    {
        parentPath = GetParentPath(parentPath);

        if (!parentPath.Length())
            break;

        paths.Push(parentPath);
    }

    if (!paths.Size())
        return false;

    for (auto i = (int) (paths.Size() - 1); i >= 0; i--)
    {
        const String& pathName = paths[i];

        if (context->GetSubsystem<FileSystem>()->FileExists(pathName))
            return false;

        if (context->GetSubsystem<FileSystem>()->DirExists(pathName))
            continue;

        if (!context->GetSubsystem<FileSystem>()->CreateDir(pathName))
            return false;

        // double check
        if (!context->GetSubsystem<FileSystem>()->DirExists(pathName))
            return false;

    }

    return true;
}

bool RemoveDir(const String& directoryIn, bool recursive, Context* context)
{
    String directory = AddTrailingSlash(directoryIn);

    if (!context->GetSubsystem<FileSystem>()->DirExists(directory))
        return false;

    Vector<String> results;

    // ensure empty if not recursive
    if (!recursive)
    {
        context->GetSubsystem<FileSystem>()->ScanDir(results, directory, "*", SCAN_DIRS | SCAN_FILES | SCAN_HIDDEN, true );
        while (results.Remove(".")) {}
        while (results.Remove("..")) {}

        if (results.Size())
            return false;

#ifdef WIN32
        return RemoveDirectoryW(GetWideNativePath(directory).CString()) != 0;
#else
        return remove(GetNativePath(directory).CString()) == 0;
#endif
    }

    // delete all files at this level
    context->GetSubsystem<FileSystem>()->ScanDir(results, directory, "*", SCAN_FILES | SCAN_HIDDEN, false );
    for (unsigned i = 0; i < results.Size(); i++)
    {
        if (!context->GetSubsystem<FileSystem>()->Delete(directory + results[i]))
            return false;
    }
    results.Clear();

    // recurse into subfolders
    context->GetSubsystem<FileSystem>()->ScanDir(results, directory, "*", SCAN_DIRS, false );
    for (unsigned i = 0; i < results.Size(); i++)
    {
        if (results[i] == "." || results[i] == "..")
            continue;

        if (!RemoveDir(directory + results[i], true, context))
            return false;
    }

    return RemoveDir(directory, false, context);
}


bool CopyDir(const String& directoryIn, const String& directoryOut, Context* context)
{
    if (context->GetSubsystem<FileSystem>()->FileExists(directoryOut))
        return false;

    Vector<String> results;
    context->GetSubsystem<FileSystem>()->ScanDir(results, directoryIn, "*", SCAN_FILES, true );

    for (unsigned i = 0; i < results.Size(); i++)
    {
        String srcFile = directoryIn + "/" + results[i];
        String dstFile = directoryOut + "/" + results[i];

        String dstPath = GetPath(dstFile);

        if (!CreateDirsRecursive(dstPath, context))
            return false;

        //LOGINFOF("SRC: %s DST: %s", srcFile.CString(), dstFile.CString());
        if (!context->GetSubsystem<FileSystem>()->Copy(srcFile, dstFile))
            return false;
    }

    return true;
}

bool Exists(const String& pathName, Context* context)
{
    return context->GetSubsystem<FileSystem>()->FileExists(pathName)
            || context->GetSubsystem<FileSystem>()->DirExists(pathName);
}


bool RenameResource(String source, String destination, Context* context)
{
    if (!context->GetSubsystem<ResourceCache>()->GetPackageFiles().Empty())
    {
        URHO3D_LOGERROR("Renaming resources not supported while packages are in use.");
        return false;
    }

    if (!IsAbsolutePath(source) || !IsAbsolutePath(destination))
    {
        URHO3D_LOGERROR("Renaming resources requires absolute paths.");
        return false;
    }

    auto* fileSystem = context->GetSubsystem<FileSystem>();

    if (!fileSystem->FileExists(source) && !fileSystem->DirExists(source))
    {
        URHO3D_LOGERROR("Source path does not exist.");
        return false;
    }

    if (fileSystem->FileExists(destination) || fileSystem->DirExists(destination))
    {
        URHO3D_LOGERROR("Destination path already exists.");
        return false;
    }

    //@@using namespace ResourceRenamed;

    // Ensure parent path exists
    if (!CreateDirsRecursive(GetPath(destination), context))
        return false;

    if (!fileSystem->Rename(source, destination))
    {
        URHO3D_LOGERRORF("Renaming '%s' to '%s' failed.", source.CString(), destination.CString());
        return false;
    }

    String resourceName;
    String destinationName;
    for (const auto& dir : context->GetSubsystem<ResourceCache>()->GetResourceDirs())
    {
        if (source.StartsWith(dir))
            resourceName = source.Substring(dir.Length());
        if (destination.StartsWith(dir))
            destinationName = destination.Substring(dir.Length());
    }

    if (resourceName.Empty())
    {
        URHO3D_LOGERRORF("'%s' does not exist in resource path.", source.CString());
        return false;
    }

    // Update loaded resource information
    for (auto& groupPair : context->GetSubsystem<ResourceCache>()->GetAllResources())
    {
        bool movedAny = false;
        auto resourcesCopy = groupPair.second_.resources_;
        for (auto& resourcePair : resourcesCopy)
        {
            SharedPtr<Resource> resource = resourcePair.second_;
            if (resource->GetName().StartsWith(resourceName))
            {
                //@@ if (autoReloadResources_)
                //@@ {
                //@@     ignoreResourceAutoReload_.EmplaceBack(destinationName);
                //@@     ignoreResourceAutoReload_.EmplaceBack(resourceName);
                //@@ }
                //@@
                //@@ groupPair.second_.resources_.Erase(resource->GetNameHash());
                //@@ resource->SetName(destinationName);
                //@@ groupPair.second_.resources_[resource->GetNameHash()] = resource;
                //@@ movedAny = true;
                //@@
                //@@ using namespace ResourceRenamed;
                //@@ SendEvent(E_RESOURCERENAMED, P_FROM, resourceName, P_TO, destinationName);
                assert(0);
            }
        }
        //@@if (movedAny)
        //@@    UpdateResourceGroup(groupPair.first_);
        assert(0);
    }

    return true;
}

}
