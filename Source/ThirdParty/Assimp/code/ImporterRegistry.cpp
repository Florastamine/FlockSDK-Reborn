﻿/*
---------------------------------------------------------------------------
Open Asset Import Library (assimp)
---------------------------------------------------------------------------

Copyright (c) 2006-2017, assimp team


All rights reserved.

Redistribution and use of this software in source and binary forms,
with or without modification, are permitted provided that the following
conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the assimp team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the assimp team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
---------------------------------------------------------------------------
*/

// Modified by Lasse Oorni for Urho3D

/** @file ImporterRegistry.cpp

Central registry for all importers available. Do not edit this file
directly (unless you are adding new loaders), instead use the
corresponding preprocessor flag to selectively disable formats.
*/

#include <vector>
#include "BaseImporter.h"

// ------------------------------------------------------------------------------------------------
// Importers
// (include_new_importers_here)
// ------------------------------------------------------------------------------------------------
#ifndef ASSIMP_BUILD_NO_X_IMPORTER
#   include "XFileImporter.h"
#endif
#ifndef ASSIMP_BUILD_NO_3DS_IMPORTER
#   include "3DSLoader.h"
#endif
#ifndef ASSIMP_BUILD_NO_ASE_IMPORTER
#   include "ASELoader.h"
#endif
#ifndef ASSIMP_BUILD_NO_OBJ_IMPORTER
#   include "ObjFileImporter.h"
#endif
#ifndef ASSIMP_BUILD_NO_SMD_IMPORTER
#   include "SMDLoader.h"
#endif
#ifndef ASSIMP_BUILD_NO_MD5_IMPORTER
#   include "MD5Loader.h"
#endif
#ifndef ASSIMP_BUILD_NO_STL_IMPORTER
#   include "STLLoader.h"
#endif
#ifndef ASSIMP_BUILD_NO_LWO_IMPORTER
#   include "LWOLoader.h"
#endif
#ifndef ASSIMP_BUILD_NO_DXF_IMPORTER
#   include "DXFLoader.h"
#endif
#ifndef ASSIMP_BUILD_NO_RAW_IMPORTER
#   include "RawLoader.h"
#endif
#ifndef ASSIMP_BUILD_NO_SIB_IMPORTER
#   include "SIBImporter.h"
#endif
#ifndef ASSIMP_BUILD_NO_AC_IMPORTER
#   include "ACLoader.h"
#endif
#ifndef ASSIMP_BUILD_NO_BVH_IMPORTER
#   include "BVHLoader.h"
#endif
#ifndef ASSIMP_BUILD_NO_COLLADA_IMPORTER
#   include "ColladaLoader.h"
#endif
#ifndef ASSIMP_BUILD_NO_LWS_IMPORTER
#   include "LWSLoader.h"
#endif
#ifndef ASSIMP_BUILD_NO_OGRE_IMPORTER
#   include "OgreImporter.h"
#endif
#ifndef ASSIMP_BUILD_NO_OPENGEX_IMPORTER
#   include "OpenGEXImporter.h"
#endif
#ifndef ASSIMP_BUILD_NO_BLEND_IMPORTER
#   include "BlenderLoader.h"
#endif
#ifndef ASSIMP_BUILD_NO_FBX_IMPORTER
#   include "FBXImporter.h"
#endif
#ifndef ASSIMP_BUILD_NO_ASSBIN_IMPORTER
#   include "AssbinLoader.h"
#endif
#ifndef ASSIMP_BUILD_NO_GLTF_IMPORTER
#   include "glTFImporter.h"
#   include "glTF2Importer.h"
#endif

namespace Assimp {

// ------------------------------------------------------------------------------------------------
void GetImporterInstanceList(std::vector< BaseImporter* >& out)
{
    // ----------------------------------------------------------------------------
    // Add an instance of each worker class here
    // (register_new_importers_here)
    // ----------------------------------------------------------------------------
    out.reserve(64);
#if (!defined ASSIMP_BUILD_NO_X_IMPORTER)
    out.push_back( new XFileImporter());
#endif
#if (!defined ASSIMP_BUILD_NO_OBJ_IMPORTER)
    out.push_back( new ObjFileImporter());
#endif
#if (!defined ASSIMP_BUILD_NO_3DS_IMPORTER)
    out.push_back( new Discreet3DSImporter());
#endif
#if (!defined ASSIMP_BUILD_NO_ASE_IMPORTER)
  #if (!defined ASSIMP_BUILD_NO_3DS_IMPORTER)
    out.push_back( new ASEImporter());
#  endif    
#endif
#if (!defined ASSIMP_BUILD_NO_SMD_IMPORTER)
    out.push_back( new SMDImporter());
#endif
#if (!defined ASSIMP_BUILD_NO_MD5_IMPORTER)
    out.push_back( new MD5Importer());
#endif
#if (!defined ASSIMP_BUILD_NO_STL_IMPORTER)
    out.push_back( new STLImporter());
#endif
#if (!defined ASSIMP_BUILD_NO_LWO_IMPORTER)
    out.push_back( new LWOImporter());
#endif
#if (!defined ASSIMP_BUILD_NO_DXF_IMPORTER)
    out.push_back( new DXFImporter());
#endif
#if (!defined ASSIMP_BUILD_NO_RAW_IMPORTER)
    out.push_back( new RAWImporter());
#endif
#if (!defined ASSIMP_BUILD_NO_SIB_IMPORTER)
    out.push_back( new SIBImporter());
#endif
#if (!defined ASSIMP_BUILD_NO_AC_IMPORTER)
    out.push_back( new AC3DImporter());
#endif
#if (!defined ASSIMP_BUILD_NO_BVH_IMPORTER)
    out.push_back( new BVHLoader());
#endif
#if (!defined ASSIMP_BUILD_NO_COLLADA_IMPORTER)
    out.push_back( new ColladaLoader());
#endif
#if (!defined ASSIMP_BUILD_NO_LWS_IMPORTER)
    out.push_back( new LWSImporter());
#endif
#if (!defined ASSIMP_BUILD_NO_OGRE_IMPORTER)
    out.push_back( new Ogre::OgreImporter());
#endif
#if (!defined ASSIMP_BUILD_NO_OPENGEX_IMPORTER )
    out.push_back( new OpenGEX::OpenGEXImporter() );
#endif
#if (!defined ASSIMP_BUILD_NO_BLEND_IMPORTER)
    out.push_back( new BlenderImporter());
#endif
#if ( !defined ASSIMP_BUILD_NO_FBX_IMPORTER )
    out.push_back( new FBXImporter() );
#endif
#if ( !defined ASSIMP_BUILD_NO_ASSBIN_IMPORTER )
    out.push_back( new AssbinImporter() );
#endif
#if ( !defined ASSIMP_BUILD_NO_GLTF_IMPORTER )
    out.push_back( new glTFImporter() );
    out.push_back( new glTF2Importer() );
#endif
}

/** will delete all registered importers. */
void DeleteImporterInstanceList(std::vector< BaseImporter* >& deleteList){
	for(size_t i= 0; i<deleteList.size();++i){
		delete deleteList[i];
		deleteList[i]=NULL;
	}//for
}

} // namespace Assimp
