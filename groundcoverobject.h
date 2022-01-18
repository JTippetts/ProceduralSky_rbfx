#pragma once
#include <Urho3D/Core/Object.h>
#include <Urho3D/Core/Context.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Scene/Component.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/StaticModelGroup.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Resource/ResourceCache.h>

using namespace Urho3D;

class GroundCoverStaticModelGroup : public StaticModelGroup
{
	URHO3D_OBJECT(GroundCoverStaticModelGroup, StaticModelGroup);
	public:
	explicit GroundCoverStaticModelGroup(Context *context) : StaticModelGroup(context){}
	~GroundCoverStaticModelGroup() override;
	
	static void RegisterObject(Context* context)
	{
		context->RegisterFactory<GroundCoverStaticModelGroup>();

		URHO3D_COPY_BASE_ATTRIBUTES(StaticModelGroup);
	}
	
	protected:
	void OnWorldBoundingBoxUpdate() override
	{
		StaticModelGroup::OnWorldBoundingBoxUpdate();
		 worldBoundingBox_.Define(-M_LARGE_VALUE, M_LARGE_VALUE);
	}
};

GroundCoverStaticModelGroup::~GroundCoverStaticModelGroup() = default;

class GroundCoverObject : public Component
{
	URHO3D_OBJECT(GroundCoverObject, Component);
	public:
	static void RegisterObject(Context *context);
	GroundCoverObject(Context *context);
	~GroundCoverObject() override = default;
	
	void Build
	
	protected:
	GroundCoverStaticModelGroup *group_{nullptr};
	Material *material_{nullptr};
	Node *childnode_{nullptr};
	
	int radius_;
};