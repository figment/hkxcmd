#pragma once

#include <Common/Base/hkBase.h>

#include "hkbAttachmentSetup.h"
#include "hkbProjectStringData.h"

class hkbProjectData : public hkReferencedObject
{
public:
	// +version(1)
	// +vtable(true)

	HK_DECLARE_CLASS_ALLOCATOR( HK_MEMORY_CLASS_BEHAVIOR_RUNTIME );
	HK_DECLARE_REFLECTION();


	//
	// Members
	//
public:

	/// Default constructor.
	HK_FORCE_INLINE hkbProjectData(void);

	/// Copy constructor.
	HK_FORCE_INLINE hkbProjectData(const hkbProjectData& other);

	enum EventMode
	{
		EVENT_MODE_DEFAULT,
		EVENT_MODE_PROCESS_ALL,
		EVENT_MODE_IGNORE_FROM_GENERATOR,
	};

	// Properties
	//hkArray<hkbAttachmentSetup> m_attachmentSetups;
	hkVector4 m_worldUpWS;
	hkbProjectStringData m_stringData;
	EventMode defaultEventMode;
};
