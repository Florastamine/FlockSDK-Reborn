//
// Copyright (c) 2008 - 2018 the Urho3D project, 2017 - 2018 Flock SDK developers & contributors. 
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

#include "../Core/Object.h"

namespace Urho3D
{

class LuaFunction;
class LuaScriptInstance;

/// Lua script event invoker.
class LuaScriptEventInvoker : public Object
{
    URHO3D_OBJECT(LuaScriptEventInvoker, Object);

public:
    /// Construct.
    explicit LuaScriptEventInvoker(Context* context);
    /// Construct from LuaScriptInstance.
    explicit LuaScriptEventInvoker(LuaScriptInstance* instance);
    /// Destruct.
    ~LuaScriptEventInvoker() override;

    /// Add a scripted event handler.
    void AddEventHandler(Object* sender, const StringHash& eventType, LuaFunction* function);

private:
    /// Handle script event in Lua script.
    void HandleLuaScriptEvent(StringHash eventType, VariantMap& eventData);

    /// Lua script instance.
    WeakPtr<LuaScriptInstance> instance_;
};

}
