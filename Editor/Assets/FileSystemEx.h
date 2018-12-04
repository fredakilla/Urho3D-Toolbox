#pragma once

namespace Urho3D
{

class Context;
class String;

bool CreateDirsRecursive(const String& directoryIn, Context* context);
bool RemoveDir(const String& directoryIn, bool recursive, Context* context);
bool CopyDir(const String& directoryIn, const String& directoryOut, Context* context);
bool Exists(const String& pathName, Context* context);
bool RenameResource(String source, String destination, Context* context);

}
