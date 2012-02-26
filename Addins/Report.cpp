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

#include "hkbProjectData_2.h"
#include "hkbProjectStringData_1.h"

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
	case hkxcmd::htShort: Log::Info("Test"); break;
	case hkxcmd::htLong:  
		{
			char fullName[MAX_PATH], exeName[MAX_PATH];
			GetModuleFileName(NULL, fullName, MAX_PATH);
			_splitpath(fullName, NULL, NULL, exeName, NULL);
			Log::Info("Usage: %s test [-opts[modifiers]] [infile] [outfile]", exeName);
				Log::Info("  Simply read and write the file back out with specified format.");
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


extern "C" void PrintV(FILE* hFile, LPCSTR lpszFormat, va_list argptr)
{
   vfprintf_s(hFile, lpszFormat, argptr);
}

extern "C" void PrintLineV(FILE* hFile, LPCSTR lpszFormat, va_list argptr)
{
   PrintV(hFile, lpszFormat, argptr);
   fprintf_s(hFile, "\n");
}

extern "C" void PrintLine(FILE* hFile, LPCSTR lpszFormat, ...)
{
   va_list argList;
   va_start(argList, lpszFormat);
   PrintLineV(hFile, lpszFormat, argList);
   va_end(argList);
}

extern "C" void Print(FILE* hFile, LPCSTR lpszFormat, ...)
{
   va_list argList;
   va_start(argList, lpszFormat);
   PrintV(hFile, lpszFormat, argList);
   va_end(argList);
}


EnumLookupType TypeEnums[] = 
{
   {hkClassMember::TYPE_VOID, 	"hkClassMember::TYPE_VOID"},
   {hkClassMember::TYPE_BOOL, 	"hkClassMember::TYPE_BOOL"},
   {hkClassMember::TYPE_CHAR, 	"hkClassMember::TYPE_CHAR"},
   {hkClassMember::TYPE_INT8, 	"hkClassMember::TYPE_INT8"},
   {hkClassMember::TYPE_UINT8, 	"hkClassMember::TYPE_UINT8"},
   {hkClassMember::TYPE_INT16, 	"hkClassMember::TYPE_INT16"},
   {hkClassMember::TYPE_UINT16, 	"hkClassMember::TYPE_UINT16"},
   {hkClassMember::TYPE_INT32, 	"hkClassMember::TYPE_INT32"},
   {hkClassMember::TYPE_UINT32, 	"hkClassMember::TYPE_UINT32"},
   {hkClassMember::TYPE_INT64, 	"hkClassMember::TYPE_INT64"},
   {hkClassMember::TYPE_UINT64, 	"hkClassMember::TYPE_UINT64"},
   {hkClassMember::TYPE_REAL, 	"hkClassMember::TYPE_REAL"},
   {hkClassMember::TYPE_VECTOR4, 	"hkClassMember::TYPE_VECTOR4"},
   {hkClassMember::TYPE_QUATERNION, 	"hkClassMember::TYPE_QUATERNION"},
   {hkClassMember::TYPE_MATRIX3, 	"hkClassMember::TYPE_MATRIX3"},
   {hkClassMember::TYPE_ROTATION, 	"hkClassMember::TYPE_ROTATION"},
   {hkClassMember::TYPE_QSTRANSFORM, 	"hkClassMember::TYPE_QSTRANSFORM"},
   {hkClassMember::TYPE_MATRIX4, 	"hkClassMember::TYPE_MATRIX4"},
   {hkClassMember::TYPE_TRANSFORM, 	"hkClassMember::TYPE_TRANSFORM"},
   {hkClassMember::TYPE_ZERO, 	"hkClassMember::TYPE_ZERO"},
   {hkClassMember::TYPE_POINTER, 	"hkClassMember::TYPE_POINTER"},
   {hkClassMember::TYPE_FUNCTIONPOINTER, 	"hkClassMember::TYPE_FUNCTIONPOINTER"},
   {hkClassMember::TYPE_ARRAY, 	"hkClassMember::TYPE_ARRAY"},
   {hkClassMember::TYPE_INPLACEARRAY, 	"hkClassMember::TYPE_INPLACEARRAY"},
   {hkClassMember::TYPE_ENUM, 	"hkClassMember::TYPE_ENUM"},
   {hkClassMember::TYPE_STRUCT, 	"hkClassMember::TYPE_STRUCT"},
   {hkClassMember::TYPE_SIMPLEARRAY, 	"hkClassMember::TYPE_SIMPLEARRAY"},
   {hkClassMember::TYPE_HOMOGENEOUSARRAY, 	"hkClassMember::TYPE_HOMOGENEOUSARRAY"},
   {hkClassMember::TYPE_VARIANT, 	"hkClassMember::TYPE_VARIANT"},
   {hkClassMember::TYPE_CSTRING, 	"hkClassMember::TYPE_CSTRING"},
   {hkClassMember::TYPE_ULONG, 	"hkClassMember::TYPE_ULONG"},
   {hkClassMember::TYPE_FLAGS, 	"hkClassMember::TYPE_FLAGS"},
   {hkClassMember::TYPE_HALF, 	"hkClassMember::TYPE_HALF"},
   {hkClassMember::TYPE_STRINGPTR, 	"hkClassMember::TYPE_STRINGPTR"},
   {hkClassMember::TYPE_RELARRAY, 	"hkClassMember::TYPE_RELARRAY"},
   {0, NULL}
};

EnumLookupType CTypeEnums[] = 
{
   {hkClassMember::TYPE_VOID, 	"hkRefVariant"},
   {hkClassMember::TYPE_BOOL, 	"hkBool"},
   {hkClassMember::TYPE_CHAR, 	"hkChar"},
   {hkClassMember::TYPE_INT8, 	"hkInt8"},
   {hkClassMember::TYPE_UINT8, 	"hkUint8"},
   {hkClassMember::TYPE_INT16, 	"hkInt16"},
   {hkClassMember::TYPE_UINT16, 	"hkUint16"},
   {hkClassMember::TYPE_INT32, 	"hkInt32"},
   {hkClassMember::TYPE_UINT32, 	"hkUint32"},
   {hkClassMember::TYPE_INT64, 	"hkInt64"},
   {hkClassMember::TYPE_UINT64, 	"hkUint64"},
   {hkClassMember::TYPE_REAL, 	"hkReal"},
   {hkClassMember::TYPE_VECTOR4, 	"hkVector4"},
   {hkClassMember::TYPE_QUATERNION, 	"hkQuaternion"},
   {hkClassMember::TYPE_MATRIX3, 	"hkMatrix3"},
   {hkClassMember::TYPE_ROTATION, 	"hkRotation"},
   {hkClassMember::TYPE_QSTRANSFORM, 	"hkQsTransform"},
   {hkClassMember::TYPE_MATRIX4, 	"hkMatrix4"},
   {hkClassMember::TYPE_TRANSFORM, 	"hkTransform"},
   {hkClassMember::TYPE_ENUM, 	"enum unknown"},
   {hkClassMember::TYPE_VARIANT, 	"hkRefVariant"},
   {hkClassMember::TYPE_CSTRING, 	"char*"},
   {hkClassMember::TYPE_ULONG, 	"hkUlong"},
   {hkClassMember::TYPE_FLAGS, 	"hkFlags"},
   {hkClassMember::TYPE_HALF, 	"hkHalf"},
   {hkClassMember::TYPE_STRINGPTR, 	"hkStringPtr"},
   {0, NULL}
};


EnumLookupType TypeFlags[] = 
{
   {hkClassMember::FLAGS_NONE, 	"hkClassMember::FLAGS_NONE"},
   {hkClassMember::ALIGN_8, 	"hkClassMember::ALIGN_8"},
   {hkClassMember::ALIGN_16, 	"hkClassMember::ALIGN_16"},
   {hkClassMember::NOT_OWNED, 	"hkClassMember::NOT_OWNED"},
   {hkClassMember::SERIALIZE_IGNORED, 	"hkClassMember::SERIALIZE_IGNORED"},
   {0, NULL}
};

string replaceSubstring(string instr, string match, string repl) {

   string retval = instr;
   for (int pos = retval.find(match, 0); pos != string::npos; pos = retval.find(match, 0)) 
   {
      retval.erase(pos, match.length());
      retval.insert(pos, repl);
   }
   return retval;
}

extern LPCSTR LookupClassHeader(LPCSTR name);

static char s_rootPath[256] = "D:\\temp\\havok";

void SetRootPath(LPCSTR path)
{
   strcpy_s(s_rootPath, 256, path);
}

void HK_CALL DumpHavokClassSignature(hkClass* hclass)
{
   if (hclass == NULL)
      return;
   char buffer[260];
   const char *ptr = hclass->getName();
   if (_strnicmp(ptr, "hc", 2) == 0 )
      return;
   if (hclass->getNumDeclaredMembers() > 0)
   {
      const hkClassMember& mem = hclass->getDeclaredMember(0);
      if (mem.getName() == NULL)
         return;
   }
   int ver = hclass->getDescribedVersion();
   sprintf_s(buffer, 256, "%s\\sig\\%s_%d.sig", s_rootPath, ptr, ver);
   {
      char outdir[MAX_PATH];
      strcpy(outdir, buffer);
      PathRemoveFileSpec(outdir);
      CreateDirectories(outdir);

      hkOstream stream(buffer);
      hclass->writeSignature(stream.getStreamWriter());
   }
}

void HK_CALL DumpHavokClassReport(hkClass* hclass)
{
   char fbuffer[260], buffer[1024];
   const char *ptr = hclass->getName();
   int ver = hclass->getDescribedVersion();
   sprintf_s(fbuffer, 256, "%s\\rpt\\%s_%d.rpt", s_rootPath, ptr, ver);
   {
      char outdir[MAX_PATH];
      strcpy(outdir, fbuffer);
      PathRemoveFileSpec(outdir);
      CreateDirectories(outdir);

      hkOstream stream(fbuffer);
      //stream.printf("Address:\t%08lx\n", hclass);
      stream.printf("Signature:\t%08lx\n", hclass->getSignature());
      stream.printf("VTable:\t%s\n", hclass->hasVtable() ? "TRUE" : "FALSE");
      stream.printf("Name:\t%s\n", hclass->getName());
      if ( hkClass* pParent = hclass->getParent() )
         stream.printf("Parent:\t%s (%08lx)\n", pParent->getName(), pParent->getSignature());
      else
         stream.printf("Parent:\t%s\n", "HK_NULL");
      stream.printf("Size:\t%d\n", hclass->getObjectSize());
      stream.printf("#IFace:\t%d\n", hclass->getNumDeclaredInterfaces());
      for (int i=0,n=hclass->getNumDeclaredInterfaces();i<n;++i)
      {
         if ( const hkClass* iface = hclass->getDeclaredInterface(i) )
            stream.printf(" %.3d\t%s (%08lx)\n", i, iface->getName(), iface);
         else
            stream.printf(" %.3d\t%s\n", i, "HK_NULL");
      }
      stream.printf("#Enums:\t%d\n", hclass->getNumDeclaredEnums());
      for (int i=0,n=hclass->getNumDeclaredEnums();i<n;++i)
      {
         const hkClassEnum &hEnum = hclass->getDeclaredEnum(i);
         stream.printf(" %.3d\t%s (", i, hEnum.getName());
         for (int j=0,m=hEnum.getNumItems();j<m;++j)
         {
            const hkClassEnum::Item& item = hEnum.getItem(j);
            if (j != 0) stream.printf(";");
            stream.printf("%s=%d", item.getName(), item.getValue());
         }         
         stream.printf(") {%08lx}\n", hEnum.getFlags());
      }
      stream.printf("#Members:\t%d\n", hclass->getNumDeclaredMembers());
      for (int i=0,n=hclass->getNumDeclaredMembers();i<n;++i)
      {
         const hkClassMember &mem = hclass->getDeclaredMember(i);         
         stream.printf(" %.3d\t%s", i, mem.getName());
         if ( !mem.hasClass() ) stream.printf(",HK_NULL");
         else if (const hkClass* mclass = mem.getClass()) stream.printf(",%s", mclass->getName());
         else stream.printf(",HK_NULL");
         if ( !mem.hasEnumClass() ) stream.printf(",HK_NULL");
         else stream.printf(",%s", mem.getEnumClass().getName());

         mem.getTypeName(buffer, 1024);
         stream.printf(",%s", buffer);
         stream.printf(",%08lx", mem.getType());
         stream.printf(",%08lx", mem.getSubType());
         stream.printf(",%d", mem.getCstyleArraySize());
         stream.printf(",%08lx", mem.getFlags());
         stream.printf(",%d", mem.getOffset());
         stream.printf(",HK_NULL");

         if ( hclass->hasDeclaredDefault(i) )
         {
            hkTypedUnion value;
            if (HK_SUCCESS == hclass->getDeclaredDefault(i, value))
               stream.printf(",%08lx", value.getStorage().m_int32);
            else 
               stream.printf(",HK_NULL");
         }
         else stream.printf(",HK_NULL");
         stream.printf("\n");
      }
      stream.printf("Version:\t%d\n", hclass->getDescribedVersion());
      stream.flush();
   }
}

void HK_CALL DumpHavokClassHeader(hkClass* hclass)
{
   char fbuffer[260], buffer[1024];
   const char *ptr = hclass->getName();
   int ver = hclass->getDescribedVersion();
   sprintf_s(fbuffer, 256, "%s\\%s_%d.h", s_rootPath, ptr, ver);

   char outdir[MAX_PATH];
   strcpy(outdir, fbuffer);
   PathRemoveFileSpec(outdir);
   CreateDirectories(outdir);

   hkClass* pParent = hclass->getParent();
   FILE *hFile = NULL; fopen_s(&hFile, fbuffer, "wt" );
   if ( hFile != NULL )
   {
      PrintLine(hFile, "#pragma once");
      PrintLine(hFile, "");
      PrintLine(hFile, "#include <Common/Base/hkBase.h>");
      std::set<string> nameMap;
      if (pParent != NULL) 
      {
         LPCSTR hdrName = LookupClassHeader(pParent->getName());
         if (hdrName != NULL) PrintLine(hFile, "#include <%s>", hdrName);
         else PrintLine(hFile, "#include \"%s_%d.h\"", pParent->getName(), pParent->getDescribedVersion());
         nameMap.insert(string(pParent->getName()));
      }
      for (int i=0,n=hclass->getNumDeclaredMembers();i<n;++i)
      {
         const hkClassMember &mem = hclass->getDeclaredMember(i);         
         if ( mem.hasClass() )
         {
            if ( const hkClass* mClass = mem.getClass() )
            {
               string name = mClass->getName();
               if (nameMap.find(name) == nameMap.end())
               {
                  nameMap.insert(name);
                  LPCSTR hdrName = LookupClassHeader(mClass->getName());
                  if (hdrName != NULL) PrintLine(hFile, "#include <%s>", hdrName);
                  else PrintLine(hFile, "#include \"%s_%d.h\"", name.c_str(), mClass->getDescribedVersion());
               }
            }            
         }
      }

      PrintLine(hFile, "");
      if (!hclass->hasVtable())
      {
         if (pParent != NULL) PrintLine(hFile, "struct %s : public %s", ptr, pParent->getName());
         else PrintLine(hFile, "struct %s", ptr);
      }
      else
      {
         if (pParent != NULL) PrintLine(hFile, "class %s : public %s", ptr, pParent->getName());
         else PrintLine(hFile, "class %s", ptr);
      }
      PrintLine(hFile, "{");
      if (hclass->hasVtable())
      {
         PrintLine(hFile, "public:");
         PrintLine(hFile, "   HK_DECLARE_CLASS_ALLOCATOR( HK_MEMORY_CLASS_BEHAVIOR_RUNTIME );");
      }
      else
      {
         PrintLine(hFile, "   HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR( HK_MEMORY_CLASS_BEHAVIOR_RUNTIME, %s );", ptr);
      }
      PrintLine(hFile, "   HK_DECLARE_REFLECTION();");
      PrintLine(hFile, "");
      if (hclass->hasVtable())
         PrintLine(hFile, "public:");

      PrintLine(hFile, "   HK_FORCE_INLINE %s(void) {}", ptr);
      if (hclass->hasVtable())
      {
         bool first = pParent == NULL;
         if (pParent == NULL)
            PrintLine(hFile, "   HK_FORCE_INLINE %s( hkFinishLoadedObjectFlag flag ) ", ptr);
         else 
            PrintLine(hFile, "   HK_FORCE_INLINE %s( hkFinishLoadedObjectFlag flag ) : %s(flag) ", ptr, pParent->getName());
         for (int i=0,n=hclass->getNumDeclaredMembers();i<n;++i)
         {
            const hkClassMember &mem = hclass->getDeclaredMember(i);
            switch (mem.getType())
            {
            case hkClassMember::TYPE_POINTER:
            case hkClassMember::TYPE_ARRAY:
            case hkClassMember::TYPE_STRINGPTR:
            case hkClassMember::TYPE_VARIANT:
               if (first) PrintLine(hFile, "     m_%s(flag)", mem.getName());
               else PrintLine(hFile, "   , m_%s(flag)", mem.getName());
               first = false;
               break;
            }
         }
         PrintLine(hFile, "      {}");
      }

      for (int i=0,n=hclass->getNumDeclaredEnums();i<n;++i)
      {
         const hkClassEnum &hEnum = hclass->getDeclaredEnum(i);
         PrintLine(hFile, "   enum %s", hEnum.getName());
         PrintLine(hFile, "   {");
         for (int j=0,m=hEnum.getNumItems();j<m;++j)
         {
            const hkClassEnum::Item& item = hEnum.getItem(j);
            PrintLine(hFile, "      %s = %d,", item.getName(),item.getValue());
         }
         PrintLine(hFile, "   };");
         PrintLine(hFile, "");
      }         

      PrintLine(hFile, "   // Properties");

      for (int i=0,n=hclass->getNumDeclaredMembers();i<n;++i)
      {
         const hkClassMember &mem = hclass->getDeclaredMember(i);         

         bool print=true;
         string typeName;

         switch (mem.getType())
         {
         case hkClassMember::TYPE_ENUM:
            typeName = EnumToString(mem.getSubType(), CTypeEnums);
            if (mem.hasEnumClass())
            {
               PrintLine(hFile, "   hkEnum<%s,%s> m_%s;", mem.getEnumClass().getName(), typeName.c_str(), mem.getName());
               print=false;
            }
            else if (typeName.substr(0,2).compare("0x") != 0)
            {
               PrintLine(hFile, "   /*enum*/ %s m_%s;", typeName.c_str(), mem.getName());
               print=false;
            }            
            break;

         case hkClassMember::TYPE_FLAGS:
            typeName = EnumToString(mem.getSubType(), CTypeEnums);
            if (mem.hasEnumClass())
            {
               PrintLine(hFile, "   hkFlags<%s,%s> m_%s;", mem.getEnumClass().getName(), typeName.c_str(), mem.getName());
               print=false;
            }
            else if (typeName.substr(0,2).compare("0x") != 0)
            {
               PrintLine(hFile, "   /*flags*/ %s m_%s;", typeName.c_str(), mem.getName());
               print=false;
            }
            else 
               break;
         }
         if (print)
         {
            mem.getTypeName(buffer, 1024);
            typeName = buffer;
            typeName = replaceSubstring(typeName, "&lt;", "<");
            typeName = replaceSubstring(typeName, "&gt;", ">");
            typeName = replaceSubstring(typeName, "void*", "hkRefVariant");
            typeName = replaceSubstring(typeName, "<void>", "<hkRefVariant>");
            if ( hkClassMember::TYPE_POINTER == mem.getType() )
            {
               if ( typeName.substr(typeName.length()-1,1).compare("*") == 0 )
               {
                  typeName.replace(typeName.length()-1,1,1,'>');
                  typeName.insert(0, "hkRefPtr<");
               }
            }
            typeName = replaceSubstring(typeName, "struct ", "");

            if (mem.getCstyleArraySize() > 0)
            {
               string arString = FormatString("[%d]",mem.getCstyleArraySize());
               typeName = replaceSubstring(typeName, arString, "");
               PrintLine(hFile, "   %s m_%s%s;", typeName.c_str(), mem.getName(), arString.c_str());               
            }
            else
            {
               PrintLine(hFile, "   %s m_%s;", typeName.c_str(), mem.getName());
            }
         }
      }
      PrintLine(hFile, "};");
      PrintLine(hFile, "extern const hkClass %sClass;", ptr);
      PrintLine(hFile, "");
      fflush(hFile);
      fclose(hFile);
   }
}

extern "C" 
void DumpHavokClassToRegister(FILE* hFile, hkClass* hclass)
{
   if (hclass == NULL || (hclass->getNumDeclaredMembers() > 0 && hclass->getDeclaredMember(0).getName() == NULL))
      return;

   if ( NULL != LookupClassHeader(hclass->getName()) )
      return;

   if (hclass->hasVtable())
   {
      fprintf_s(hFile, "HK_CLASS(\"%s\")\n", hclass->getName());
   }
   else
   {
      fprintf_s(hFile, "HK_STRUCT(\"%s\")\n", hclass->getName());
   }  
}

extern "C" 
void DumpHavokClassToVerify(FILE* hFile, hkClass* hclass)
{
   if (hclass == NULL || (hclass->getNumDeclaredMembers() > 0 && hclass->getDeclaredMember(0).getName() == NULL))
      return;
   fprintf_s(hFile, "HKCLASS_VERIFY(%s, 0x%08lx)\n", hclass->getName(), hclass->getSignature());
}


void HK_CALL DumpHavokClassImpl(hkClass* hclass)
{
   char fbuffer[260], buffer[1024];
   const char *ptr = hclass->getName();
   int ver = hclass->getDescribedVersion();
   sprintf_s(fbuffer, 256, "%s\\%s_%d.cpp", s_rootPath, ptr, ver);

   char outdir[MAX_PATH];
   strcpy(outdir, fbuffer);
   PathRemoveFileSpec(outdir);
   CreateDirectories(outdir);

   hkClass* pParent = hclass->getParent();
   FILE *hFile = NULL; fopen_s(&hFile, fbuffer, "wt" );
   if ( hFile != NULL )
   {
      PrintLine(hFile, "#include \"StdAfx.h\"");
      PrintLine(hFile, "#include \"%s_%d.h\"", ptr, ver);
      PrintLine(hFile, "");
      PrintLine(hFile, "#include <Common/Serialize/hkSerialize.h>");
      PrintLine(hFile, "#include <Common/Serialize/Util/hkSerializeUtil.h>");
      PrintLine(hFile, "#include <Common/Serialize/Version/hkVersionPatchManager.h>");
      PrintLine(hFile, "#include <Common/Serialize/Data/Dict/hkDataObjectDict.h>");
      PrintLine(hFile, "#include <Common/Serialize/Data/Native/hkDataObjectNative.h>");
      PrintLine(hFile, "#include <Common/Serialize/Data/Util/hkDataObjectUtil.h>");
      PrintLine(hFile, "#include <Common/Base/Reflection/Registry/hkDynamicClassNameRegistry.h>");
      PrintLine(hFile, "#include <Common/Base/Reflection/Registry/hkVtableClassRegistry.h>");
      PrintLine(hFile, "#include <Common/Base/Reflection/hkClass.h>");
      PrintLine(hFile, "#include <Common/Base/Reflection/hkInternalClassMember.h>");
      PrintLine(hFile, "#include <Common/Serialize/Util/hkSerializationCheckingUtils.h>");
      PrintLine(hFile, "#include <Common/Serialize/Util/hkVersionCheckingUtils.h>");
      PrintLine(hFile, "");
      PrintLine(hFile, "");

      if ( 0 < hclass->getNumDeclaredEnums())
      {
         for (int i=0,n=hclass->getNumDeclaredEnums();i<n;++i)
         {
            const hkClassEnum &hEnum = hclass->getDeclaredEnum(i);

            PrintLine(hFile, "static const hkInternalClassEnumItem %sEnumItems[] =", hEnum.getName());
            PrintLine(hFile, "{");
            for (int j=0,m=hEnum.getNumItems();j<m;++j)
            {
               const hkClassEnum::Item& item = hEnum.getItem(j);
               PrintLine(hFile, "   {%d, \"%s\"},", item.getValue(), item.getName());
            }
            PrintLine(hFile, "};");
            PrintLine(hFile, "");
         }

         PrintLine(hFile, "static const hkInternalClassEnum %sClass_Enums[] = {", ptr);
         for (int i=0,n=hclass->getNumDeclaredEnums();i<n;++i)
         {
            const hkClassEnum &hEnum = hclass->getDeclaredEnum(i);
            PrintLine(hFile, "   {\"%s\", %sEnumItems, _countof(%sEnumItems), HK_NULL, %d },", hEnum.getName(),hEnum.getName(),hEnum.getName(), hEnum.getFlags());
         }
         PrintLine(hFile, "};");
         for (int i=0,n=hclass->getNumDeclaredEnums();i<n;++i)
         {
            const hkClassEnum &hEnum = hclass->getDeclaredEnum(i);
            PrintLine(hFile, "const hkClassEnum* %sEnum = reinterpret_cast<const hkClassEnum*>(&%sClass_Enums[%d]);", hEnum.getName(), ptr, i);
         }         
         PrintLine(hFile, "");
      }

      if ( hclass->getNumDeclaredMembers() > 0)
      {
         PrintLine(hFile, "static const hkInternalClassMember %sClass_Members[] =", ptr);
         PrintLine(hFile, "{");
         for (int i=0,n=hclass->getNumDeclaredMembers();i<n;++i)
         {
            const hkClassMember &mem = hclass->getDeclaredMember(i);
            mem.getTypeName(buffer, 1024);
            fprintf(hFile, "   { \"%s\"", mem.getName());

            if ( !mem.hasClass() ) fprintf(hFile, ",HK_NULL");
            else if (const hkClass* mclass = mem.getClass()) fprintf(hFile, ",&%sClass", mclass->getName());
            else fprintf(hFile, ",HK_NULL");

            if ( !mem.hasEnumClass() ) fprintf(hFile, ",HK_NULL");
            else fprintf(hFile, ",%sEnum", mem.getEnumClass().getName());

            string typeStr = EnumToString(mem.getType(), TypeEnums);
            fprintf(hFile, ",%s", typeStr.c_str());
            string subTypeStr = EnumToString(mem.getSubType(), TypeEnums);
            fprintf(hFile, ",%s", subTypeStr.c_str());
            fprintf(hFile, ",%d", mem.getCstyleArraySize());

            string flagsStr = FlagsToString((int)*(short*)&mem.getFlags(), TypeFlags);
            fprintf(hFile, ",%s", flagsStr.c_str());
            fprintf(hFile, ",HK_OFFSET_OF(%s,m_%s) /*%d*/", ptr, mem.getName(), mem.getOffset());
            if ( hclass->hasDeclaredDefault(i) )
            {
               //hkTypedUnion value;
               //if (HK_SUCCESS == hclass->getDeclaredDefault(i, value))
               //   fprintf(hFile, ",%08lx", value.getStorage().m_int32);
               //else 
               fprintf(hFile, ",HK_NULL");
            }
            else fprintf(hFile, ",HK_NULL");
            fprintf(hFile, "},\n");
         }
         PrintLine(hFile, "};");
         PrintLine(hFile, "");
      }


      PrintLine(hFile, "// Signature:  %08lx", hclass->getSignature());

      if (pParent != NULL)
         PrintLine(hFile, "extern const hkClass %sClass;", pParent->getName());

      PrintLine(hFile, "extern const hkClass %sClass;", ptr);
      PrintLine(hFile, "const hkClass %sClass(", ptr);
      PrintLine(hFile, "    \"%s\",", ptr);
      if (pParent == NULL) PrintLine(hFile, "    HK_NULL, // parent");
      else PrintLine(hFile, "    &%sClass, // parent", pParent->getName());
      PrintLine(hFile, "    sizeof(%s),", ptr);
      if (hclass->getNumDeclaredInterfaces() > 0)
         PrintLine(hFile, "    reinterpret_cast<const hkClassMember*>(%sClass_IFaces), HK_COUNT_OF(%sClass_IFaces),", ptr, ptr);
      else
         PrintLine(hFile, "    HK_NULL, 0, // interfaces");

      if (hclass->getNumDeclaredEnums() > 0)
         PrintLine(hFile, "    reinterpret_cast<const hkClassEnum*>(%sClass_Enums), HK_COUNT_OF(%sClass_Enums),", ptr, ptr);
      else
         PrintLine(hFile, "    HK_NULL, 0, // enums");
      if (hclass->getNumDeclaredMembers() > 0)
         PrintLine(hFile, "    reinterpret_cast<const hkClassMember*>(%sClass_Members), HK_COUNT_OF(%sClass_Members),", ptr, ptr);
      else
         PrintLine(hFile, "    HK_NULL, 0, // members");
      PrintLine(hFile, "    HK_NULL, // defaults");
      PrintLine(hFile, "    HK_NULL, // attributes");
      PrintLine(hFile, "    %d, // flags", hclass->getFlags());
      PrintLine(hFile, "    %d // version", ver);
      PrintLine(hFile, " );");
      if (hclass->hasVtable())
         PrintLine(hFile, "HK_REFLECTION_DEFINE_VIRTUAL(%s, %s);", ptr, ptr);
      else
         PrintLine(hFile, "HK_REFLECTION_DEFINE_SIMPLE(%s, %s);", ptr, ptr);
      PrintLine(hFile, "");
      fflush(hFile);
      fclose(hFile);
   }
}

EXTERN_C
void DumpHavokClassByAddress(hkClass* hclass)
{
   try
   {
      if (hclass == NULL || (hclass->getNumDeclaredMembers() > 0 && hclass->getDeclaredMember(0).getName() == NULL))
         return;
      DumpHavokClassSignature(hclass);
      DumpHavokClassReport(hclass);
      DumpHavokClassHeader(hclass);
      DumpHavokClassImpl(hclass);
   }
   catch (...)
   {
   }
}

void HK_CALL DumpHavokClassesByAddress(LPVOID Address)
{
   for ( hkClass **classes = static_cast<hkClass **>(Address); *classes != NULL; ++classes )
   {
      DumpHavokClassSignature(*classes);
      DumpHavokClassReport(*classes);
   }   
}

void HK_CALL DumpClassesAll()
{
   DumpHavokClassesByAddress(const_cast<hkClass**>(const_cast<hkClass * const *>(&hkBuiltinTypeRegistry::StaticLinkedClasses[0])));
}

enum hkPackFormat
{
   HKPF_XML,
   HKPF_DEFAULT,
   HKPF_WIN32,
   HKPF_AMD64,
   HKPF_XBOX,
   HKPF_XBOX360,
};

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

static bool ExecuteCmd(hkxcmdLine &cmdLine)
{
	string outpath;
	int argc = cmdLine.argc;
	char **argv = cmdLine.argv;
	hkSerializeUtil::SaveOptionBits flags = (hkSerializeUtil::SaveOptionBits)(hkSerializeUtil::SAVE_TEXT_FORMAT|hkSerializeUtil::SAVE_TEXT_NUMBERS);

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

			default:
				Log::Error("Unknown argument specified '%s'", arg);
				break;
			}
		}
		else if (outpath.empty())
		{
			outpath = arg;
		}
	}
#pragma endregion
   if (outpath.empty()){
      HelpString(hkxcmd::htLong);
      return false;
   }
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
      hkDefaultClassNameRegistry &dynamicRegistry = hkDefaultClassNameRegistry::getInstance();
      {
         extern void HK_CALL CustomRegisterDefaultClasses();
         extern void HK_CALL ValidateClassSignatures();
         CustomRegisterDefaultClasses();
         ValidateClassSignatures();
      } 

      SetRootPath(outpath.c_str());
      DumpClassesAll();
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

REGISTER_COMMAND(Report, HelpString, SafeExecuteCmd);
