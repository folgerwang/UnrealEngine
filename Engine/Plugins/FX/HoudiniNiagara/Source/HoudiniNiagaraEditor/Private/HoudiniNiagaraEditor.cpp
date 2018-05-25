/*
* Copyright (c) <2018> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/

#include "HoudiniNiagaraEditor.h"
#include "HoudiniCSVAssetActions.h"
#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "FHoudiniNiagaraModule"

void FHoudiniNiagaraEditorModule::StartupModule()
{
    // Register the Houdini CSV Type Actions
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked< FAssetToolsModule >("AssetTools").Get();

    TSharedRef< IAssetTypeActions > HCSVAction = MakeShareable( new FHoudiniCSVAssetActions() );
    AssetTools.RegisterAssetTypeActions( HCSVAction );
    AssetTypeActions.Add( HCSVAction );
}

void FHoudiniNiagaraEditorModule::ShutdownModule()
{
    // Unregister asset type actions we have previously registered.
    if ( FModuleManager::Get().IsModuleLoaded("AssetTools") )
    {
		IAssetTools & AssetTools = FModuleManager::GetModuleChecked< FAssetToolsModule >("AssetTools").Get();

		for ( int32 Index = 0; Index < AssetTypeActions.Num(); ++Index )
			AssetTools.UnregisterAssetTypeActions( AssetTypeActions[Index].ToSharedRef() );

		AssetTypeActions.Empty();
    }
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FHoudiniNiagaraEditorModule, HoudiniNiagara)
