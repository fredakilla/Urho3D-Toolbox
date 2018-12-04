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

#include <Urho3D/Core/ProcessUtils.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Core/Context.h>

#include "Project.h"
#include "ImportAssimp.h"
#include "FileSystemEx.h"


namespace Urho3D
{

//-------------------------------------------------------------------------------------------------
// Additionnal stuf
//-------------------------------------------------------------------------------------------------

/// Class which creates a subprocess and returns it's return code and output.
class URHO3D_API Process
{
public:
    /// Construct a process object.
    Process(const String& command, const Vector<String>& args={});
    /// Set current directory subprocess will execute in. If not set defaults to current directory of executing process.
    void SetCurrentDirectory(const String& directory) { subprocessDir_ = directory; }
    /// Execute subprocess and return it's return code.
    int Run();
    /// Get output of subprocess.
    String GetOutput() const { return output_; }

protected:
    /// Path to a directory subprocess will execute in.
    String subprocessDir_ = ".";
    /// Full command to be executed. Contains arguments as well.
    String command_;
    /// Subprocess output.
    String output_;
};

Process::Process(const String& command, const Vector<String>& args)
{
    command_ = "\"" + GetNativePath(command).Replaced("\"", "\\\"") + "\" ";
    for (const auto& arg: args)
    {
        command_ += "\"";
        command_ += arg.Replaced("\"", "\\\"");
        command_ += "\" ";
    }
}

int Process::Run()
{
    char buffer[1024];
    String command;
    if (!subprocessDir_.Empty())
    {
        command = "cd \"";
        command += GetNativePath(AddTrailingSlash(subprocessDir_)).Replaced("\"", "\\\"");
        command += "\"";
#if _WIN32
        command += "&";
#else
        command += ";";
#endif
        command += command_;
    }

    String output;
    FILE* stream = popen(command.Empty() ? command_.CString() : command.CString(), "r");
    while (fgets(buffer, sizeof(buffer), stream) != nullptr)
        output.Append(buffer);

    return pclose(stream);
}





ImportAssimp::ImportAssimp(Context* context)
    : ImportAsset(context)
{
}

bool ImportAssimp::Accepts(const String& path)
{
    String extension = GetExtension(path);
    return extension == ".fbx" || extension == ".blend";
}

bool ImportAssimp::Convert(const String& path)
{
    bool importedAny = false;
    auto* project = GetSubsystem<Project>();
    assert(path.StartsWith(project->GetResourcePath()));

    const auto& cachePath = project->GetCachePath();
    auto resourceName = path.Substring(project->GetResourcePath().Length());
    auto resourceFileName = GetFileName(path);
    auto outputDir = cachePath + AddTrailingSlash(resourceName);
    CreateDirsRecursive(outputDir, context_);

    // Import models
    {
        String outputPath = outputDir + resourceFileName + ".mdl";

        StringVector args{"model", path, outputPath, "-na", "-ns"};
        Process process(GetFileSystem()->GetProgramDir() + "AssetImporter", args);
        if (process.Run() == 0 && GetFileSystem()->FileExists(outputPath))
            importedAny = true;
    }

    // Import animations
    {
        String outputPath = cachePath + resourceName;

        StringVector args{"anim", path, outputPath, "-nm", "-nt", "-nc", "-ns"};
        Process process(GetFileSystem()->GetProgramDir() + "AssetImporter", args);
        if (process.Run() == 0 && GetFileSystem()->FileExists(outputPath))
            importedAny = true;
    }

    return importedAny;
}

}
