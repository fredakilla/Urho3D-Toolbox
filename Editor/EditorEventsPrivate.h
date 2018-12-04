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


#include <Urho3D/Core/Object.h>


namespace Urho3D
{

/// Event sent when editor successfully saves a resource.
URHO3D_EVENT(E_EDITORRESOURCESAVED, EditorResourceSaved)
{
}

/// Event sent right before reloading user components.
URHO3D_EVENT(E_EDITORUSERCODERELOADSTART, EditorUserCodeReloadStart)
{
}

/// Event sent right after reloading user components.
URHO3D_EVENT(E_EDITORUSERCODERELOADEND, EditorUserCodeReloadEnd)
{
}

/// Event sent when editor is about to load a new project.
URHO3D_EVENT(E_EDITORPROJECTLOADINGSTART, EditorProjectLoadingStart)
{
}

/// Resource renamed
URHO3D_EVENT(E_RESOURCERENAMED, ResourceRenamed)
{
    URHO3D_PARAM(P_FROM, From);                            // String
    URHO3D_PARAM(P_TO, To);                                // String
}


}