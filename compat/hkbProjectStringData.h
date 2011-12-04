#pragma once

#include <Common/Base/hkBase.h>
#include <Common/Base/Types/hkBaseTypes.h>
#include <Common/Base/Container/Array/hkArray.h>

class hkbProjectStringData : public hkReferencedObject
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
	HK_FORCE_INLINE hkbProjectStringData(void);

	/// Copy constructor.
	HK_FORCE_INLINE hkbProjectStringData(const hkbProjectStringData& other);


	// Properties
	// Properties
	hkArray< hkStringPtr > m_animationFilenames ;
	hkArray< hkStringPtr > m_behaviorFilenames ;
	hkArray< hkStringPtr > m_characterFilenames ;
	hkArray< hkStringPtr > m_eventNames ;
	hkStringPtr m_animationPath ;
	hkStringPtr m_behaviorPath ;
	hkStringPtr m_characterPath ;
	hkStringPtr m_fullPathToSource;
	hkStringPtr m_rootPath;
};
