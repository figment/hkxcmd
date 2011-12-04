#pragma once

#include <Common/Base/hkBase.h>

class hkbAttachmentSetup : public hkReferencedObject
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
	HK_FORCE_INLINE hkbAttachmentSetup(void);

	/// Copy constructor.
	HK_FORCE_INLINE hkbAttachmentSetup(const hkbAttachmentSetup& other);


	enum AttachmentType
	{
		ATTACHMENT_TYPE_KEYFRAME_RIGID_BODY,
		ATTACHMENT_TYPE_BALL_SOCKET_CONSTRAINT,
		ATTACHMENT_TYPE_RAGDOLL_CONSTRAINT,
		ATTACHMENT_TYPE_SET_WORLD_FROM_MODEL,
		ATTACHMENT_TYPE_NONE
	};

	// Properties
	float m_blendInTime;
	float m_moveAttacherFraction ;
	float m_gain ;
	AttachmentType m_attachmentType;
	float m_extrapolationTimeStep;
	float m_fixUpGain ;
	float m_maxAngularDistance ;
	float m_maxLinearDistance ;

};
