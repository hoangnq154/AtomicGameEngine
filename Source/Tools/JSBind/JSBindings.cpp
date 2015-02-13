// Copyright (c) 2014-2015, THUNDERBEAST GAMES LLC All rights reserved
// Please see LICENSE.md in repository root for license information
// https://github.com/AtomicGameEngine/AtomicGameEngine

#include <Atomic/Atomic.h>
#include <Atomic/IO/Log.h>
#include <Atomic/Core/ProcessUtils.h>
#include <Atomic/Resource/ResourceCache.h>
#include <Atomic/Resource/JSONFile.h>
#include "JSBind.h"
#include "JSBModule.h"
#include "JSBindings.h"
#include "JSBClass.h"
#include "JSBTypeScript.h"
#include "JSBDoc.h"

JSBindings* JSBindings::instance_ = NULL;

void JSBindings::ParseHeaders()
{
    for (unsigned i = 0; i < modules_.Size(); i++)
    {
        modules_[i]->ParseHeaders();
    }

    for (unsigned i = 0; i < modules_.Size(); i++)
    {
        modules_[i]->PreprocessHeaders();
    }

    for (unsigned i = 0; i < modules_.Size(); i++)
    {
        modules_[i]->VisitHeaders();
    }


    JSBClass::Preprocess();

    JSBClass::Process();

    //JSBClass::DumpAllClasses();

    EmitJSModules(JSBind::ROOT_FOLDER + "/Source/Atomic/Javascript/Modules");

    JSBTypeScript* ts = new JSBTypeScript();
    ts->Emit(JSBind::ROOT_FOLDER + "/Bin/Atomic.d.ts");

    JSBDoc* jsdoc = new JSBDoc();
    jsdoc->Emit(JSBind::ROOT_FOLDER + "/Bin/Atomic.js");
}

void JSBindings::EmitJSModules(const String& rootpath)
{
    File file(JSBind::context_);
    file.Open(rootpath + "/JSModules.cpp", FILE_WRITE);

    String source = "// This file was autogenerated by JSBind, changes will be lost\n\n";
	source += "#include \"Precompiled.h\"\n";
    source += "#include <Duktape/duktape.h>\n";    
    source += "#include \"../../Javascript/JSVM.h\"\n";
    source += "#include \"../../Javascript/JSAPI.h\"\n";

    source += "\n\nnamespace Atomic\n{\n";

    for (unsigned i = 0; i < modules_.Size(); i++)
    {
        JSBModule* module = modules_.At(i);
        source.AppendWithFormat("\nextern void jsb_preinit_%s (JSVM* vm);", module->name_.ToLower().CString());
        source.AppendWithFormat("\nextern void jsb_init_%s (JSVM* vm);", module->name_.ToLower().CString());
    }

    source += "\n\nstatic void jsb_modules_setup_prototypes(JSVM* vm)\n{\n";

    source += "   // It is important that these are in order so the prototypes are created properly\n";
    source += "   // This isn't trivial as modules can have dependencies, so do it here\n\n";

    JSBClass::WriteProtoTypeSetup(source);

    source += "\n}\n";

    source += "\n\nvoid jsb_modules_preinit(JSVM* vm)\n{";

    for (unsigned i = 0; i < modules_.Size(); i++)
    {
        JSBModule* module = modules_.At(i);
        source.AppendWithFormat("\n   jsb_preinit_%s(vm);", module->name_.ToLower().CString());
    }

    source += "\n}\n\n";

    source += "\n\nvoid jsb_modules_init(JSVM* vm)\n{";

    source += "\n\n   jsb_modules_preinit(vm);\n";

    source += "\n\n   jsb_modules_setup_prototypes(vm);\n";

    for (unsigned i = 0; i < modules_.Size(); i++)
    {
        JSBModule* module = modules_.At(i);
        source.AppendWithFormat("\n   jsb_init_%s(vm);", module->name_.ToLower().CString());
    }

    source += "\n}\n\n";



    // end Atomic namespace
    source += "\n}\n";

    file.Write(source.CString(), source.Length());

    file.Close();

    for (unsigned i = 0; i < modules_.Size(); i++)
    {
        String filepath = rootpath + "/JSModule" + modules_.At(i)->name_ + ".cpp";
        modules_.At(i)->EmitSource(filepath);
    }


}

void JSBindings::RegisterEnum(JSBEnum* jenum)
{

    if (enums_.Contains(jenum->name_))
    {
        LOGERRORF("Enum name collision: %s", jenum->name_.CString());
        ErrorExit();
    }

    for (unsigned i = 0; i < jenum->values_.Size(); i++)
    {
        if (enumNames_.Contains(jenum->values_[i]))
        {
            LOGERRORF("Enum value collision: %s", jenum->values_[i].CString());
            ErrorExit();
        }
        else
            enumNames_[jenum->values_[i]] = jenum->values_[i];

    }

    enums_[jenum->name_] = jenum;

}


void JSBindings::RegisterClass(const String &classname, const String &_rename)
{
    String rename = _rename;
    if (!rename.Length())
        rename = classname;

    if (classes_.Contains(rename))
    {
        LOGERRORF("Class name collision: %s", classname.CString());
        ErrorExit();
    }

    classes_[classname] = new JSBClass(rename, classname);
}

void JSBindings::Initialize()
{
    ResourceCache* cache = JSBind::context_->GetSubsystem<ResourceCache>();

    JSONFile* jsonFile = cache->GetResource<JSONFile>("modules/Modules.json");

    JSONValue json = jsonFile->GetRoot();
    JSONValue modules = json.GetChild("modules");

    for (unsigned i = 0; i < modules.GetSize(); i++)
    {
        String moduleName = "modules/" + modules.GetString(i) + ".json";

        JSBModule* jsbModule = new JSBModule(this);
        jsbModule->Load(moduleName);
        modules_.Push(jsbModule);
    }
}

JSBModule* JSBindings::GetModuleByName(const String& name)
{
    for (unsigned i = 0; i < modules_.Size(); i++)
        if (modules_[i]->name_ == name)
            return modules_[i];

    return NULL;
}

