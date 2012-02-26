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

// Scene
#include <Common/SceneData/Scene/hkxScene.h>
#include <Common/Serialize/Util/hkRootLevelContainer.h>
#include <Common/Serialize/Util/hkLoader.h>

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
	case hkxcmd::htShort: Log::Info("DumpList - Dump the transform or float list for a given skeleton"); break;
	case hkxcmd::htLong:  
		{
			char fullName[MAX_PATH], exeName[MAX_PATH];
			GetModuleFileName(NULL, fullName, MAX_PATH);
			_splitpath(fullName, NULL, NULL, exeName, NULL);
			Log::Info("Usage: %s DumpList [-opts[modifiers]] [infile] [outfile]", exeName);
			Log::Info("  Dump the transform or float list for a given skeleton.");
			Log::Info("    This is useful when exporting animation to get bone list synchronized with source.");
			Log::Info("");
			Log::Info("<Switches>");
			Log::Info(" -i <path>          Input File or directory");
			Log::Info(" -o <path>          Output File - Defaults to input file with '-out' appended");
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

static void HK_CALL errorReport(const char* msg, void* userContext)
{
	Log::Error("%s", msg);
}

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
	hkSerializeUtil::SaveOptionBits flags = (hkSerializeUtil::SaveOptionBits)(hkSerializeUtil::SAVE_TEXT_FORMAT|hkSerializeUtil::SAVE_TEXT_NUMBERS);

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

	// explicit exclusions due to crashes
	stringlist excludes;
	excludes.push_back("*project.hkx");
	excludes.push_back("*behavior.hkx");
	excludes.push_back("*charater.hkx");
	excludes.push_back("*character.hkx");

	vector<string> files;
	FindFiles(files, inpath.c_str(), excludes);
	if (files.empty())
	{
		Log::Error("No files found in '%s'", inpath.c_str());
		return false;
	}

	hkMallocAllocator baseMalloc;
	// Need to have memory allocated for the solver. Allocate 1mb for it.
	hkMemoryRouter* memoryRouter = hkMemoryInitUtil::initDefault( &baseMalloc, hkMemorySystem::FrameInfo(1024 * 1024) );
	hkBaseSystem::init( memoryRouter, errorReport );
	{
		for (vector<string>::iterator itr = files.begin(); itr != files.end(); ++itr)
		{
			char infile[MAX_PATH], relpath[MAX_PATH];
			try
			{
				strcpy(infile, (*itr).c_str());
				GetFullPathName(infile, MAX_PATH, infile, NULL);

				LPCSTR extn = PathFindExtension(infile);
				if (stricmp(extn, ".hkx") != 0)
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
					_makepath(outfile, drive, dir, fname, ext);
					outpath = rootDir;
				}
				else
				{
					LPCSTR oextn = PathFindExtension(outpath.c_str());
					if (oextn == NULL || stricmp(oextn, ".txt") != 0)
					{
						PathCombine(outfile, outpath.c_str(), relpath);
						GetFullPathName(outfile, MAX_PATH, outfile, NULL);
					}
					else
					{
						GetFullPathName(outpath.c_str(), MAX_PATH, outfile, NULL);
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
					_makepath(outfile, drive, dir, fname, ".txt");
				}

				Log::Info("Converting '%s' ...", relpath );

				hkIstream stream(infile);
				hkStreamReader *reader = stream.getStreamReader();
				hkSerializeUtil::FormatDetails detailsOut;
				hkSerializeUtil::detectFormat( reader, detailsOut );
				hkBool32 isLoadable = hkSerializeUtil::isLoadable( reader );
				if (!isLoadable)
				{
					Log::Warn("File is not loadable: '%s'", relpath);
				}
				else
				{
					hkDynamicClassNameRegistry dynamicRegistry;
					hkSerializeUtil::ErrorDetails detailsOut;
					hkResource* resource = hkSerializeUtilLoad(reader, &detailsOut, &dynamicRegistry, hkSerializeUtil::LOAD_DEFAULT);
					hkBool32 failed = true;
					if (resource)
					{
						if (hkRootLevelContainer* scene = resource->getContents<hkRootLevelContainer>())
						{
							if (hkaAnimationContainer *skelAnimCont = scene->findObject<hkaAnimationContainer>())
							{
								char path[MAX_PATH], drive[MAX_PATH], dir[MAX_PATH], fname[MAX_PATH], ext[MAX_PATH];
								int nskel = skelAnimCont->m_skeletons.getSize();
								if ( nskel == 0 )
								{
									Log::Warn("No skeletons found in resource. Skipping '%s'", relpath);
								}
								else
								{
									failed = false;

									if (nskel>1) Log::Info("Multiple skeletons found.");
									for (int iskel=0; iskel<nskel; ++iskel)
									{
										if (nskel>1)
										{
											_splitpath(outfile, drive, dir, fname, ext);
											sprintf(fname+strlen(fname), "-%d", iskel+1);
											_makepath(path, drive, dir, fname, ext);
										}
										else
										{
											strcpy(path, outfile);
										}

										hkRefPtr<hkaSkeleton> skeleton = skelAnimCont->m_skeletons[0];
										int nbones = skeleton->m_bones.getSize();
										Log::Info("Exporting skeleton '%s' with %d bones to '%s'"
											, skeleton->m_name, skeleton->m_bones.getSize(), PathFindFileName(path));

										{
											hkOstream stream(path);
											for (int i=0; i<nbones; ++i)
											{
												string name = skeleton->m_bones[i].m_name;
												stream.printf("%s\r\n", name.c_str());
											}
										}
										if (!skeleton->m_floatSlots.isEmpty())
										{
											strcat(fname, "-floats");
											_makepath(path, drive, dir, fname, ext);

											Log::Info("Exporting skeleton '%s' with %d floats to '%s'"
												, skeleton->m_name, skeleton->m_floatSlots.getSize(), PathFindFileName(path));

											hkOstream stream(path);
											for (int i=0, n=skeleton->m_floatSlots.getSize(); i<n; ++i)
											{
												string name = skeleton->m_floatSlots[i];
												stream.printf("%s\r\n", name.c_str());
											}
										}
									}
								}

							}
						}
						resource->removeReference();

						if (failed)
						{
							Log::Error("Failed to save file '%s'", outfile);
						}
					}
					else
					{
						Log::Error("Failed to load file '%s'", relpath);
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

REGISTER_COMMAND(DumpList, HelpString, SafeExecuteCmd);

