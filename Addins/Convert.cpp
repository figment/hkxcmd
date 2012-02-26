#include "stdafx.h"

#include "hkxcmd.h"
#include "hkxutils.h"
#include "log.h"
#include <cstdio>
#include <sys/stat.h>

#include <Common/Base/hkBase.h>
#include <Common/Base/Memory/System/Util/hkMemoryInitUtil.h>
#include <Common/Base/Memory/Allocator/Malloc/hkMallocAllocator.h>
#include <Common/Base/System/Io/IStream/hkIStream.h>
#include <Common/Base/Reflection/Registry/hkDynamicClassNameRegistry.h>
#include <Common/Base/Reflection/Registry/hkDefaultClassNameRegistry.h>
#include <Common/Compat/Deprecated/Packfile/Binary/hkBinaryPackfileReader.h>
#include <Common/Compat/Deprecated/Packfile/Xml/hkXmlPackfileReader.h>

// Scene
#include <Common/SceneData/Scene/hkxScene.h>
#include <Common/Serialize/Util/hkRootLevelContainer.h>
#include <Common/Serialize/Util/hkNativePackfileUtils.h>
#include <Common/Serialize/Util/hkLoader.h>
#include <Common/Serialize/Version/hkVersionPatchManager.h>

// Physics
#include <Physics/Dynamics/Entity/hkpRigidBody.h>
#include <Physics/Collide/Shape/Convex/Box/hkpBoxShape.h>
#include <Physics/Utilities/Dynamics/Inertia/hkpInertiaTensorComputer.h>

// Animation
#include <Animation/Animation/Rig/hkaSkeleton.h>
#include <Animation/Animation/hkaAnimationContainer.h>
#include <Animation/Animation/Mapper/hkaSkeletonMapper.h>
#include <Animation/Animation/Playback/Control/Default/hkaDefaultAnimationControl.h>
#include <Animation/Animation/Playback/hkaAnimatedSkeleton.h>
#include <Animation/Animation/Rig/hkaPose.h>
#include <Animation/Ragdoll/Controller/PoweredConstraint/hkaRagdollPoweredConstraintController.h>
#include <Animation/Ragdoll/Controller/RigidBody/hkaRagdollRigidBodyController.h>
#include <Animation/Ragdoll/Utils/hkaRagdollUtils.h>

// Serialize
#include <Common/Serialize/Util/hkSerializeUtil.h>

#pragma comment (lib, "hkBase.lib")
#pragma comment (lib, "hkSerialize.lib")
#pragma comment (lib, "hkSceneData.lib")
#pragma comment (lib, "hkInternal.lib")
#pragma comment (lib, "hkGeometryUtilities.lib")
#pragma comment (lib, "hkVisualize.lib")
#pragma comment (lib, "hkCompat.lib")
#pragma comment (lib, "hkpCollide.lib")
#pragma comment (lib, "hkpConstraintSolver.lib")
#pragma comment (lib, "hkpDynamics.lib")
#pragma comment (lib, "hkpInternal.lib")
#pragma comment (lib, "hkpUtilities.lib")
#pragma comment (lib, "hkpVehicle.lib")
#pragma comment (lib, "hkaAnimation.lib")
#pragma comment (lib, "hkaRagdoll.lib")
#pragma comment (lib, "hkaInternal.lib")
#pragma comment (lib, "hkgBridge.lib")

#define RETURN_FAIL_IF(COND, MSG) \
	HK_MULTILINE_MACRO_BEGIN \
	if(COND) { HK_ERROR(0x53a6a026, MSG); return 1; } \
	HK_MULTILINE_MACRO_END

using namespace std;

static void HelpString(hkxcmd::HelpType type){
	switch (type)
	{
	case hkxcmd::htShort: Log::Info("Convert - Read/write with no modifications but with different format"); break;
	case hkxcmd::htLong:  
		{
			char fullName[MAX_PATH], exeName[MAX_PATH];
			GetModuleFileName(NULL, fullName, MAX_PATH);
			_splitpath(fullName, NULL, NULL, exeName, NULL);
			Log::Info("Usage: %s convert [-opts[modifiers]] [infile] [outfile]", exeName);
				Log::Info("  Simply read and write the file back out with specified format.");
				Log::Info("");
				Log::Info("<Switches>");
				Log::Info(" -i <path>          Input File or directory");
				Log::Info(" -o <path>          Output File - Defaults to input file with '-out' appended");
				Log::Info("");
            Log::Info(" -v:<flags>     Havok Save Options");
            Log::Info("    DEFAULT     Save as Default Format (MSVC Win32 Packed)");
            Log::Info("    XML         Save as Packed Binary Xml Format");
            Log::Info("    WIN32       Save as Win32 Format");
            Log::Info("    AMD64       Save as AMD64 Format");
            Log::Info("    XBOX        Save as XBOX Format");
            Log::Info("    XBOX360     Save as XBOX360 Format");
            Log::Info("    TAGFILE     Save as TagFile Format");
            Log::Info("    TAGXML      Save as TagFile XML Format");
            Log::Info("");
            Log::Info(" -f <flags>         Havok saving flags (Defaults:  SAVE_TEXT_FORMAT|SAVE_TEXT_NUMBERS)");
				Log::Info("     SAVE_DEFAULT           = All flags default to OFF, enable whichever are needed");
				Log::Info("     SAVE_TEXT_FORMAT       = Use text (usually XML) format, default is binary format if available.");
				Log::Info("     SAVE_SERIALIZE_IGNORED_MEMBERS = Write members which are usually ignored.");
				Log::Info("     SAVE_WRITE_ATTRIBUTES  = Include extended attributes in metadata, default is to write minimum metadata.");
				Log::Info("     SAVE_CONCISE           = Doesn't provide any extra information which would make the file easier to interpret. ");
				Log::Info("                              E.g. additionally write hex floats as text comments.");
				Log::Info("     SAVE_TEXT_NUMBERS      = Floating point numbers output as text, not as binary.  ");
				Log::Info("                              Makes them easily readable/editable, but values may not be exact.");
				Log::Info("");
            ;
		}
		break;
	}
}

namespace
{
   enum hkPackFormat
   {
      HKPF_XML,
      HKPF_DEFAULT,
      HKPF_WIN32,
      HKPF_AMD64,
      HKPF_XBOX,
      HKPF_XBOX360,
      HKPF_TAGFILE,
      HKPF_TAGXML,
   };

	EnumLookupType SaveFlags[] = 
	{
		{hkSerializeUtil::SAVE_DEFAULT,                   "SAVE_DEFAULT"},
		{hkSerializeUtil::SAVE_TEXT_FORMAT,               "SAVE_TEXT_FORMAT"},
		{hkSerializeUtil::SAVE_SERIALIZE_IGNORED_MEMBERS, "SAVE_SERIALIZE_IGNORED_MEMBERS"},
		{hkSerializeUtil::SAVE_WRITE_ATTRIBUTES,          "SAVE_WRITE_ATTRIBUTES"},
		{hkSerializeUtil::SAVE_CONCISE,                   "SAVE_CONCISE"},
		{hkSerializeUtil::SAVE_TEXT_NUMBERS,              "SAVE_TEXT_NUMBERS"},
		{0, NULL}
	};

   EnumLookupType PackFlags[] = 
   {
      {HKPF_XML,   "XML"},
      {HKPF_DEFAULT, "DEFAULT"},
      {HKPF_WIN32, "WIN32"},
      {HKPF_AMD64, "AMD64"},
      {HKPF_XBOX,  "XBOX"},
      {HKPF_XBOX360, "XBOX360"},
      {HKPF_TAGFILE, "TAGFILE"},
      {HKPF_TAGXML, "TAGXML"},
      {0, NULL}
   };

	EnumLookupType LogFlags[] = 
	{
		{LOG_NONE,   "NONE"},
		{LOG_ALL,    "ALL"},
		{LOG_VERBOSE,"VERBOSE"},
		{LOG_DEBUG,  "DEBUG"},
		{LOG_INFO,   "INFO"},
		{LOG_WARN,   "WARN"},
		{LOG_ERROR,  "ERROR"},
		{0, NULL}
	};
}

typedef void (__stdcall * DumpClassesAllDecl)();

static hkPackfileWriter::Options GetWriteOptionsFromFormat(hkPackFormat format)
{
   hkPackfileWriter::Options options;
   options.m_layout = hkStructureLayout::MsvcWin32LayoutRules;

   switch(format)
   {
   case HKPF_XML:
   case HKPF_DEFAULT:
   case HKPF_WIN32:
      options.m_layout = hkStructureLayout::MsvcWin32LayoutRules;
      break;
   case HKPF_AMD64:
      options.m_layout = hkStructureLayout::MsvcAmd64LayoutRules;
      break;
   case HKPF_XBOX:
      options.m_layout = hkStructureLayout::MsvcXboxLayoutRules;
      break;
   case HKPF_XBOX360:
      options.m_layout = hkStructureLayout::Xbox360LayoutRules;
      break;
   }
   return options;
}

static void HK_CALL errorReport(const char* msg, void* userContext)
{
	Log::Error("%s", msg);
}

/// Converts some objects to different types for snapshots.
/// Some objects are inherently not serializable because they point to
/// external memory. i.e., the vertex and index arrays of an hkpMeshShape. When these objects
/// are encountered, we convert them before saving. i.e hkpMeshShape -> hkpStorageMeshShape.
class ConvertListener : public hkPackfileWriter::AddObjectListener
{
public:
   HK_DECLARE_CLASS_ALLOCATOR(HK_MEMORY_CLASS_BASE);
   ~ConvertListener() {}
   virtual void addObjectCallback( ObjectPointer& p, ClassPointer& k )
   {
      if (k == NULL 
         || strcmp(k->getName(), "hkMemoryResourceContainer") == 0
         )
      {
         p = NULL;
         k = NULL;
      }
   }
   hkArray<hkReferencedObject*> m_objects;
};


static hkResource* hkSerializeUtilLoad( hkStreamReader* stream
								, hkSerializeUtil::ErrorDetails* detailsOut=HK_NULL
								, const hkClassNameRegistry* classReg=HK_NULL
								, hkSerializeUtil::LoadOptions options=hkSerializeUtil::LOAD_DEFAULT )
{
	__try
	{
		return hkSerializeUtil::load(stream, detailsOut, classReg, options);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		if (detailsOut == NULL)
			detailsOut->id = hkSerializeUtil::ErrorDetails::ERRORID_LOAD_FAILED;
		return NULL;
	}
}
static bool ExecuteCmd(hkxcmdLine &cmdLine)
{
	string inpath;
	string outpath;
	int argc = cmdLine.argc;
	char **argv = cmdLine.argv;
   bool flagsSpecified = false;
	hkSerializeUtil::SaveOptionBits flags = (hkSerializeUtil::SaveOptionBits)(hkSerializeUtil::SAVE_DEFAULT);
   hkPackFormat pkFormat = HKPF_DEFAULT;

	list<hkxcmd *> plugins;

#pragma region Handle Input Args
	for (int i = 0; i < argc; i++)
	{
		char *arg = argv[i];
		if (arg == NULL)
			continue;
		if (arg[0] == '-' || arg[0] == '/')
		{

			switch (tolower(arg[1]))
			{
			case 'f':
				{
					const char *param = arg+2;
					if (*param == ':' || *param=='=') ++param;
					argv[i] = NULL;
					if ( param[0] == 0 && ( i+1<argc && ( argv[i+1][0] != '-' || argv[i+1][0] != '/' ) ) ) {
						param = argv[++i];
						argv[i] = NULL;
					}
					if ( param[0] == 0 )
						break;
					flags = (hkSerializeUtil::SaveOptionBits)StringToFlags(param, SaveFlags, hkSerializeUtil::SAVE_DEFAULT);
               flagsSpecified = true;
				} break;

			case 'd':
				{
					const char *param = arg+2;
					if (*param == ':' || *param=='=') ++param;
					argv[i] = NULL;
					if ( param[0] == 0 && ( i+1<argc && ( argv[i+1][0] != '-' || argv[i+1][0] != '/' ) ) ) {
						param = argv[++i];
						argv[i] = NULL;
					}
					if ( param[0] == 0 )
					{
						Log::SetLogLevel(LOG_DEBUG);
						break;
					}
					else
					{
						Log::SetLogLevel((LogLevel)StringToEnum(param, LogFlags, LOG_INFO));
					}
				} break;

			case 'o':
				{
					const char *param = arg+2;
					if (*param == ':' || *param=='=') ++param;
					argv[i] = NULL;
					if ( param[0] == 0 && ( i+1<argc && ( argv[i+1][0] != '-' || argv[i+1][0] != '/' ) ) ) {
						param = argv[++i];
						argv[i] = NULL;
					}
					if ( param[0] == 0 )
						break;
					if (outpath.empty())
					{
						outpath = param;
					}
					else
					{
						Log::Error("Output file already specified as '%s'", outpath.c_str());
					}
				} break;

			case 'i':
				{
					const char *param = arg+2;
					if (*param == ':' || *param=='=') ++param;
					argv[i] = NULL;
					if ( param[0] == 0 && ( i+1<argc && ( argv[i+1][0] != '-' || argv[i+1][0] != '/' ) ) ) {
						param = argv[++i];
						argv[i] = NULL;
					}
					if ( param[0] == 0 )
						break;
					if (inpath.empty())
					{
						inpath = param;
					}
					else
					{
						Log::Error("Input file already specified as '%s'", inpath.c_str());
					}
				} break;

         case 'v':
            {
               const char *param = arg+2;
               if (*param == ':' || *param=='=') ++param;
               argv[i] = NULL;
               if ( param[0] == 0 && ( i+1<argc && ( argv[i+1][0] != '-' || argv[i+1][0] != '/' ) ) ) {
                  param = argv[++i];
                  argv[i] = NULL;
               }
               if ( param[0] == 0 )
                  break;
               pkFormat = (hkPackFormat)StringToEnum(param, PackFlags, HKPF_DEFAULT);
            } break;

			default:
				Log::Error("Unknown argument specified '%s'", arg);
				break;
			}
		}
		else if (inpath.empty())
		{
			inpath = arg;
		}
		else if (outpath.empty())
		{
			outpath = arg;
		}
	}
#pragma endregion

	if (inpath.empty()){
		HelpString(hkxcmd::htLong);
		return false;
	}
	if (PathIsDirectory(inpath.c_str()))
	{
		char path[MAX_PATH];
		strcpy(path, inpath.c_str());
		PathAddBackslash(path);
		strcat(path, "*.hkx");
		GetFullPathName(path, MAX_PATH, path, NULL);
		inpath = path;
	}
	char rootDir[MAX_PATH];
	strcpy(rootDir, inpath.c_str());
	GetFullPathName(rootDir, MAX_PATH, rootDir, NULL);
	if (!PathIsDirectory(rootDir))
		PathRemoveFileSpec(rootDir);

   LPCSTR defextn = (pkFormat == HKPF_XML || pkFormat == HKPF_TAGXML) ? ".xml" : (pkFormat == HKPF_TAGFILE) ?  ".hkt" : ".hkx";

	vector<string> files;
	FindFiles(files, inpath.c_str());
	if (files.empty())
	{
		Log::Error("No files found in '%s'", inpath.c_str());
		return false;
	}

   if (!flagsSpecified && pkFormat == HKPF_DEFAULT)
   {
      flags = (hkSerializeUtil::SaveOptionBits)(hkSerializeUtil::SAVE_TEXT_FORMAT|hkSerializeUtil::SAVE_TEXT_NUMBERS);
   }
   if (pkFormat == HKPF_XML || pkFormat == HKPF_TAGXML) // set text format to indicate xml
   {
      flags = (hkSerializeUtil::SaveOptionBits)(flags | hkSerializeUtil::SAVE_TEXT_FORMAT);
   }
   hkPackfileWriter::Options packFileOptions = GetWriteOptionsFromFormat(pkFormat);

	hkMallocAllocator baseMalloc;
	// Need to have memory allocated for the solver. Allocate 1mb for it.
	hkMemoryRouter* memoryRouter = hkMemoryInitUtil::initDefault( &baseMalloc, hkMemorySystem::FrameInfo(1024 * 1024) );
	hkBaseSystem::init( memoryRouter, errorReport );

   // Normally all the patches would be added to the global singleton but
   // for this example we'll use a private manager to keep the scope small
   // and not leave useless patches in the global registry
	{
      hkVersionPatchManager patchManager;
      {
         extern void HK_CALL CustomRegisterPatches(hkVersionPatchManager& patchManager);
         CustomRegisterPatches(patchManager);
      }
      hkTypeInfoRegistry &defaultTypeRegistry = hkTypeInfoRegistry::getInstance();
      hkDefaultClassNameRegistry &defaultRegistry = hkDefaultClassNameRegistry::getInstance();
      {
         extern void HK_CALL CustomRegisterDefaultClasses();
         extern void HK_CALL ValidateClassSignatures();
         CustomRegisterDefaultClasses();
         ValidateClassSignatures();
      }
      hkDynamicClassNameRegistry dynamicRegistry;
      dynamicRegistry.merge(defaultRegistry);
      {
         extern void HK_CALL CustomRegisterOverrideClasses(hkDynamicClassNameRegistry& registry);
         CustomRegisterOverrideClasses(dynamicRegistry);
      }
      //dynamicRegistry.

		for (vector<string>::iterator itr = files.begin(); itr != files.end(); ++itr)
		{
			char infile[MAX_PATH], relpath[MAX_PATH];
			try
			{
				strcpy(infile, (*itr).c_str());
				GetFullPathName(infile, MAX_PATH, infile, NULL);

				LPCSTR extn = PathFindExtension(infile);
				if (stricmp(extn, ".hkx") != 0 && stricmp(extn, ".xml") != 0 && stricmp(extn, ".hkt") != 0)
				{
					Log::Verbose("Unexpected extension. Skipping '%s'", infile);
					continue;
				}
				PathRelativePathTo(relpath, rootDir, FILE_ATTRIBUTE_DIRECTORY, infile, 0);

				char outfile[MAX_PATH];
				if (outpath.empty())
				{
					char drive[MAX_PATH], dir[MAX_PATH], fname[MAX_PATH], ext[MAX_PATH];
					_splitpath(infile, drive, dir, fname, ext);
					strcat(fname, "-out");
					_makepath(outfile, drive, dir, fname, defextn);
					outpath = rootDir;
				}
				else
				{
               if (PathIsDirectory(outpath.c_str()))
               {
                  PathCombine(outfile, outpath.c_str(), relpath);
                  GetFullPathName(outfile, MAX_PATH, outfile, NULL);
                  PathRemoveExtension(outfile);
                  PathAddExtension(outfile, defextn);
               }
               else
               {
                  LPCSTR oextn = PathFindExtension(outpath.c_str());
                  if (oextn == NULL || (stricmp(oextn, ".xml") != 0 && stricmp(oextn, ".hkx") != 0 && stricmp(oextn, ".hkt") != 0))
                  {
                     PathCombine(outfile, outpath.c_str(), relpath);
                     GetFullPathName(outfile, MAX_PATH, outfile, NULL);
                  }
                  else
                  {
                     GetFullPathName(outpath.c_str(), MAX_PATH, outfile, NULL);
                  }
               }
				}

				char outdir[MAX_PATH];
				strcpy(outdir, outfile);
				PathRemoveFileSpec(outdir);
				CreateDirectories(outdir);

				if (stricmp(infile, outfile) == 0)
				{
					char drive[MAX_PATH], dir[MAX_PATH], fname[MAX_PATH], ext[MAX_PATH];
					_splitpath(infile, drive, dir, fname, ext);
					strcat(fname, "-out");
					_makepath(outfile, drive, dir, fname, ext);
				}

            char outrel[MAX_PATH];
            PathRelativePathTo(outrel, outpath.c_str(), FILE_ATTRIBUTE_DIRECTORY, outfile, 0);

            Log::Info("Converting '%s' ...", relpath );

            hkIstream stream(infile);
				hkStreamReader *reader = stream.getStreamReader();
				hkSerializeUtil::FormatDetails formatDetails;
				hkSerializeUtil::detectFormat( reader, formatDetails );
				hkBool32 isLoadable = hkSerializeUtil::isLoadable( reader );
				if (!isLoadable && formatDetails.m_formatType != hkSerializeUtil::FORMAT_TAGFILE_XML)
				{
					Log::Warn("File is not loadable: '%s'", relpath);
				}
				else
				{
               hkVariant root;
               hkBinaryPackfileReader bpkreader;
               hkXmlPackfileReader xpkreader;
               hkResource* resource = NULL;
               switch ( formatDetails.m_formatType )
               {
               case hkSerializeUtil::FORMAT_PACKFILE_BINARY:
                  {
                     bpkreader.loadEntireFile(stream.getStreamReader());
                     bpkreader.finishLoadedObjects(defaultTypeRegistry);
                     if ( hkPackfileData* pkdata = bpkreader.getPackfileData() )
                     {
                        hkArray<hkVariant>& obj = bpkreader.getLoadedObjects();
                        for ( int i =0,n=obj.getSize(); i<n; ++i)
                        {
                           hkVariant& value = obj[i];
                           if ( value.m_class->hasVtable() )
                              defaultTypeRegistry.finishLoadedObject(value.m_object, value.m_class->getName());
                        }
                     }
                     root = bpkreader.getTopLevelObject();
                  }
                  break;

               case hkSerializeUtil::FORMAT_PACKFILE_XML:
                  {
                     xpkreader.loadEntireFileWithRegistry(stream.getStreamReader(), &dynamicRegistry);
                     if ( hkPackfileData* pkdata = xpkreader.getPackfileData() )
                     {
                        hkArray<hkVariant>& obj = xpkreader.getLoadedObjects();
                        for ( int i =0,n=obj.getSize(); i<n; ++i)
                        {
                           hkVariant& value = obj[i];
                           if ( value.m_class->hasVtable() )
                              defaultTypeRegistry.finishLoadedObject(value.m_object, value.m_class->getName());
                        }
                     }
                     root = xpkreader.getTopLevelObject();
                  }
                  break;

               case hkSerializeUtil::FORMAT_TAGFILE_BINARY:
               case hkSerializeUtil::FORMAT_TAGFILE_XML:
               default:
                  {
                     hkSerializeUtil::ErrorDetails detailsOut;
                     hkSerializeUtil::LoadOptions loadflags = hkSerializeUtil::LOAD_FAIL_IF_VERSIONING;
                     resource = hkSerializeUtilLoad(reader, &detailsOut, &dynamicRegistry, loadflags);
                     root.m_object = resource->getContents<hkRootLevelContainer>();
                     if (root.m_object != NULL) 
                        root.m_class = &((hkRootLevelContainer*)root.m_object)->staticClass();

                  }
                  break;
               }


					//hkResource* resource = hkSerializeUtilLoad(reader, &detailsOut, &dynamicRegistry, loadflags);
					hkBool32 failed = true;
					if (root.m_object != NULL)
					{
						hkOstream stream(outfile);
                  hkRootLevelContainer *container = (hkRootLevelContainer*)root.m_object;
                  hkResult res;
                  ConvertListener listener;
                  if (pkFormat == HKPF_TAGXML || pkFormat == HKPF_TAGFILE)
                  {
                     //res = hkSerializeUtil::save( scene, scene->staticClass(), stream.getStreamWriter(), flags );
                     res = hkSerializeUtil::saveTagfile(root.m_object, *root.m_class, stream.getStreamWriter(), HK_NULL, flags );
                  }
                  else
                  {
                     res = hkSerializeUtil::savePackfile(root.m_object, *root.m_class, stream.getStreamWriter(), packFileOptions, HK_NULL, flags );
                  }
						failed = (res != HK_SUCCESS);

						if (failed)
						{
							Log::Error("Failed to save file '%s'", outfile);
						}
					}
					else
					{
						Log::Error("Failed to load file '%s'", relpath);
					}
               if (resource != NULL)
               {
                  resource->removeReference();
               }
				}
			}
			catch (...)
			{
				Log::Error("Unexpected exception occurred while processing '%s'", relpath);
			}
		}
	}

	hkBaseSystem::quit();
	hkMemoryInitUtil::quit();


	return true;
}

static bool SafeExecuteCmd(hkxcmdLine &cmdLine)
{
   __try{
      return ExecuteCmd(cmdLine);
   } __except (EXCEPTION_EXECUTE_HANDLER){
      return false;
   }
}

REGISTER_COMMAND(Convert, HelpString, SafeExecuteCmd);

