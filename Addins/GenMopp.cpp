// ***** BEGIN LICENSE BLOCK *****
//
// Copyright (c) 2006-2011, Havok Tools.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above
//      copyright notice, this list of conditions and the following
//      disclaimer in the documentation and/or other materials provided
//      with the distribution.
//
//    * Neither the name of the Havok Tools
//      project nor the names of its contributors may be used to endorse
//      or promote products derived from this software without specific
//      prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// ***** END LICENSE BLOCK *****

#include "stdafx.h"

#include "hkxcmd.h"

#include "hkxutils.h"

//
// Math and base include
#include <Common/Base/hkBase.h>
#include <Common/Base/System/hkBaseSystem.h>
//#include <Common/Base/Memory/hkThreadMemory.h>
//#include <Common/Base/Memory/Memory/Pool/hkPoolMemory.h>
#include <Common/Base/System/Error/hkDefaultError.h>
#include <Common/Base/Monitor/hkMonitorStream.h>

#include <Common/Base/System/Io/FileSystem/hkFileSystem.h>
#include <Common/Base/Container/LocalArray/hkLocalBuffer.h>
//
#include <Physics/Collide/Shape/Convex/Box/hkpBoxShape.h>
#include <Physics/Collide/Shape/Convex/ConvexTranslate/hkpConvexTranslateShape.h>
#include <Physics/Collide/Shape/Convex/ConvexTransform/hkpConvexTransformShape.h>
#include <Physics/Collide/Shape/Compound/Collection/SimpleMesh/hkpSimpleMeshShape.h>
#include <Physics/Collide/Shape/Compound/Collection/List/hkpListShape.h>
#include <Physics/Collide/Shape/Convex/Capsule/hkpCapsuleShape.h>
#include <Physics/Collide/Shape/Compound/Tree/Mopp/hkpMoppBvTreeShape.h>
#include <Physics/Collide/Shape/Compound/Tree/Mopp/hkpMoppUtility.h>
#include <Physics/Internal/Collide/Mopp/Code/hkpMoppCode.h>

#pragma comment(lib, "hkBase.lib")
#pragma comment(lib, "hkSerialize.lib")
#pragma comment(lib, "hkpInternal.lib")
#pragma comment(lib, "hkpUtilities.lib")
#pragma comment(lib, "hkpCollide.lib")
#pragma comment(lib, "hkpConstraintSolver.lib")

using namespace std;
#if 0
static hkpSimpleMeshShape * ConstructHKMesh( vector<Vector3>& verts, vector<Niflib::Triangle>& tris)
{
	hkpSimpleMeshShape * storageMeshShape = new hkpSimpleMeshShape( 0.01f );
	hkArray<hkVector4> &vertices = storageMeshShape->m_vertices;
	hkArray<hkpSimpleMeshShape::Triangle> &triangles = storageMeshShape->m_triangles;

	triangles.setSize( 0 );
	for (unsigned int i=0;i<tris.size();++i) {
		Niflib::Triangle &tri = tris[i];
		hkpSimpleMeshShape::Triangle hktri;
		hktri.m_a = tri[0];
		hktri.m_b = tri[1];
		hktri.m_c = tri[2];
		triangles.pushBack( hktri );
	}

	vertices.setSize( 0 );
	for (unsigned int i=0;i<verts.size();++i) {
		Niflib::Vector3 &vert = verts[i];
		vertices.pushBack( hkVector4(vert.x, vert.y, vert.z) );
	}
	//storageMeshShape->setRadius(1.0f);
	return storageMeshShape;
}

static hkpSimpleMeshShape * ConstructHKMesh( NiTriBasedGeomRef shape )
{
	NiTriBasedGeomDataRef data = shape->GetData();
	return ConstructHKMesh(data->GetVertices(), data->GetTriangles());
}

static hkpSimpleMeshShape * ConstructHKMesh( bhkPackedNiTriStripsShapeRef shape )
{
	hkPackedNiTriStripsDataRef data = shape->GetData();
	return ConstructHKMesh(data->GetVertices(), data->GetTriangles());
}

static void BuildCollision(NiNodeRef top, vector<NiTriBasedGeomRef>& bodies)
{
	vector<const hkpShape *> shapeList;
	for (vector<NiTriBasedGeomRef>::iterator itr = bodies.begin(); itr != bodies.end(); ++itr)
		shapeList.push_back( ConstructHKMesh(*itr));


	hkpListShape *list = new hkpListShape( &shapeList[0], shapeList.size() );
	hkpMoppCompilerInput mfr;
	mfr.setAbsoluteFitToleranceOfAxisAlignedTriangles( hkVector4( 100.0f, 100.0f, 100.0f ) );
	mfr.setAbsoluteFitToleranceOfTriangles(100.0f);
	mfr.setAbsoluteFitToleranceOfInternalNodes(100.0f);

	hkpMoppCode* code = hkpMoppUtility::buildCode(list, mfr);

	hkpMoppBvTreeShape* moppBvTreeShape = new hkpMoppBvTreeShape(list, code);
	code->removeReference();

	for(unsigned int i=0;i<shapeList.size(); ++i)
		shapeList[i]->removeReference();
	list->removeReference();
	moppBvTreeShape->removeReference();
}

static void BuildCollision(NiNodeRef top, vector<bhkRigidBodyRef>& bodies)
{
	for (vector<bhkRigidBodyRef>::iterator itr = bodies.begin(); itr != bodies.end(); ++itr)
	{
		bhkRigidBodyRef body = (*itr);
		bhkShapeRef shape = body->GetShape();
		hkpShapeCollection * list = NULL;
		
		// Mopp already in place
		bhkMoppBvTreeShapeRef mopp;
		if (shape->IsDerivedType(bhkMoppBvTreeShape::TYPE))
		{
			if ( mopp = DynamicCast<bhkMoppBvTreeShape>(shape) )
			{
				if ( bhkPackedNiTriStripsShapeRef mesh = DynamicCast<bhkPackedNiTriStripsShape>( mopp->GetShape() ) )
				{
					list = ConstructHKMesh(mesh);
				}
			}
		}
		else if ( shape->IsDerivedType(bhkPackedNiTriStripsShape::TYPE) )
		{
			bhkPackedNiTriStripsShapeRef mesh = DynamicCast<bhkPackedNiTriStripsShape>( shape );
			mopp = new bhkMoppBvTreeShape();
			mopp->SetMaterial( HAV_MAT_WOOD );
			body->SetShape( mopp );
			mopp->SetShape( mesh );
			list = ConstructHKMesh(mesh);
		}
		else if ( shape->IsDerivedType(bhkNiTriStripsShape::TYPE) )
		{
			bhkNiTriStripsShapeRef mesh = DynamicCast<bhkNiTriStripsShape>( shape );
			
			bhkPackedNiTriStripsShapeRef packedMesh = new bhkPackedNiTriStripsShape();
			hkPackedNiTriStripsDataRef packedData = new hkPackedNiTriStripsData();

			vector<OblivionSubShape> shapes;
			vector<Vector3> verts, norms;
			vector<Triangle> tris;
			for (int i=0;i<mesh->GetNumStripsData(); ++i){
				int toff = tris.size();
				NiTriStripsDataRef data = mesh->GetStripsData(i);

				vector<Vector3> v = data->GetVertices();
				verts.reserve( verts.size() + v.size() );
				for (vector<Vector3>::iterator vi=v.begin(); vi != v.end(); ++vi )
					verts.push_back( *vi / 7.0 );

				vector<Vector3> n = data->GetNormals();
				norms.reserve( norms.size() + n.size() );
				for (vector<Vector3>::iterator ni=n.begin(); ni != n.end(); ++ni )
					norms.push_back(*ni);

				vector<Triangle> t = data->GetTriangles();
				tris.reserve( tris.size() + t.size() );
				for (vector<Triangle>::iterator ti=t.begin(); ti != t.end(); ++ti )
					tris.push_back( Triangle(ti->v1 + toff, ti->v2 + toff, ti->v3 + toff) );

				OblivionSubShape subshape;
				subshape.material = mesh->GetMaterial();
				if ( i < (int)mesh->GetNumDataLayers() )
					subshape.layer = mesh->GetOblivionLayer( i );
				else
					subshape.layer = OL_STATIC;

				shapes.push_back( subshape );
			}
			packedData->SetNumFaces( tris.size() );
			packedData->SetVertices( verts );
			packedData->SetTriangles( tris );
			//packedData->SetNormals( norms );
			packedMesh->SetData( packedData );
			packedMesh->SetSubShapes( shapes );

			list = ConstructHKMesh(verts, tris);

			mopp = new bhkMoppBvTreeShape();
			mopp->SetMaterial( mesh->GetMaterial() );
			body->SetShape( mopp );
			mopp->SetShape( packedMesh );
		}

		if (!mopp || NULL == list)
			continue;

		//////////////////////////////////////////////////////////////////////////

		hkpMoppCompilerInput mfr;
		mfr.setAbsoluteFitToleranceOfAxisAlignedTriangles( hkVector4( 0.1f, 0.1f, 0.1f ) );
		//mfr.setAbsoluteFitToleranceOfTriangles(0.1f);
		//mfr.setAbsoluteFitToleranceOfInternalNodes(0.0001f);

		hkpMoppCode* code = hkpMoppUtility::buildCode(list, mfr);

		vector<Niflib::byte> moppcode;
		moppcode.resize( code->m_data.getSize() );
		for (int i=0; i<code->m_data.getSize(); ++i )
			moppcode[i] = code->m_data[i];
		mopp->SetMoppCode( moppcode );
		mopp->SetMoppOrigin( Vector3(code->m_info.m_offset(0), code->m_info.m_offset(1), code->m_info.m_offset(2) ));
		mopp->SetMoppScale( code->m_info.getScale() );
	
		code->removeReference();
		list->removeReference();
	}
}

static void HK_CALL errorReport(const char* msg, void*)
{
	//printf("%s", msg);
}


static hkThreadMemory* threadMemory = NULL;
static char* stackBuffer = NULL;
static void InitializeHavok()
{
	// Initialize the base system including our memory system
	hkPoolMemory* memoryManager = new hkPoolMemory();
	threadMemory = new hkThreadMemory(memoryManager, 16);
	hkBaseSystem::init( memoryManager, threadMemory, errorReport );
	memoryManager->removeReference();

	// We now initialize the stack area to 100k (fast temporary memory to be used by the engine).
	{
		int stackSize = 0x100000;
		stackBuffer = hkAllocate<char>( stackSize, HK_MEMORY_CLASS_BASE);
		hkThreadMemory::getInstance().setStackArea( stackBuffer, stackSize);
	}
}

static void CloseHavok()
{
	// Deallocate stack area
	if (threadMemory)
	{
		threadMemory->setStackArea(0, 0);
		hkDeallocate(stackBuffer);

		threadMemory->removeReference();
		threadMemory = NULL;
		stackBuffer = NULL;
	}

	// Quit base system
	hkBaseSystem::quit();
}


static void HelpString(hkxcmd::HelpType type){
   switch (type)
   {
   case hkxcmd::htShort: cout << "GenMopp - Generate Mopp for specified meshes."; break;
   case hkxcmd::htLong:  
      {
         char fullName[MAX_PATH], exeName[MAX_PATH];
         GetModuleFileName(NULL, fullName, MAX_PATH);
         _splitpath(fullName, NULL, NULL, exeName, NULL);
         cout << "Usage: " << exeName << " GenMopp [-opts[modifiers]] <Mesh List>" << endl 
            << "  Generate Mopp for specified meshes." << endl
            << endl
            << "<Switches>" << endl
            << "  i <path>          Input File" << endl
            << "  o <path>          Output File - Defaults to input file with '-out' appended" << endl
            << "  v x.x.x.x         Nif Version to write as - Defaults to input version" << endl
            << endl
            ;
      }
      break;
   }
}


static bool ExecuteCmd(hkxcmdLine &cmdLine)
{
   string current_file = cmdLine.current_file;
   string outfile = cmdLine.outfile;
   list<string> meshlist;
   unsigned outver = cmdLine.outver;
   int argc = cmdLine.argc;
   char **argv = cmdLine.argv;

   list<hkxcmd *> plugins;

   for (int i = 0; i < argc; i++)
   {
      char *arg = argv[i];
      if (arg == NULL)
         continue;
      if (arg[0] == '-' || arg[0] == '/')
      {
         fputs( "ERROR: Unknown argument specified \"", stderr );
         fputs( arg, stderr );
         fputs( "\".\n", stderr );
      }
      else
      {
		  meshlist.push_back( string(arg) );
      }
   }
   if (current_file.empty()){
      hkxcmd::PrintHelp();
      return false;
   }
   if (outfile.empty()) {
      char path[MAX_PATH], drive[MAX_PATH], dir[MAX_PATH], fname[MAX_PATH], ext[MAX_PATH];
      _splitpath(current_file.c_str(), drive, dir, fname, ext);
      strcat(fname, "-out");
      _makepath(path, drive, dir, fname, ext);
      outfile = path;
   }

   unsigned int ver = GetNifVersion( current_file );
   if ( ver == VER_UNSUPPORTED ) cout << "unsupported...";
   else if ( ver == VER_INVALID ) cout << "invalid...";
   else 
   {
      if (!IsSupportedVersion(outver))
         outver = ver;

      // Finally alter block tree
      vector<NiObjectRef> blocks = ReadNifList( current_file );
      NiObjectRef root = blocks[0];

	  NiNodeRef top = DynamicCast<NiNode>(root);

	  InitializeHavok();
	  BuildCollision(top, DynamicCast<bhkRigidBody>(blocks));
	  //BuildCollision(top, DynamicCast<NiTriBasedGeom>(blocks));
	  CloseHavok();

      WriteNifTree(outfile, root, Niflib::NifInfo(cmdLine.outver, cmdLine.uoutver, cmdLine.uoutver));
      return true;
   }
   return true;
}

REGISTER_COMMAND(GenMopp, HelpString, ExecuteCmd);
#endif