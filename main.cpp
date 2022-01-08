#include <Urho3D/Engine/Application.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/Input/Input.h>

#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Scene/Component.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/StaticModelGroup.h>
#include <Urho3D/Graphics/Skybox.h>
#include <Urho3D/Graphics/Light.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Drawable.h>
#include <Urho3D/Graphics/OctreeQuery.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/Viewport.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/Resource/XMLFile.h>
#include <Urho3D/UI/Slider.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/Button.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Graphics/Terrain.h>
#include <Urho3D/Graphics/Light.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/Math/Random.h>
#include <Urho3D/Math/RandomEngine.h>
#include <cmath>
// This is probably always OK.
using namespace Urho3D;

class GrassStaticModelGroup : public StaticModelGroup
{
	URHO3D_OBJECT(GrassStaticModelGroup, StaticModelGroup);
	public:
	explicit GrassStaticModelGroup(Context *context) : StaticModelGroup(context){}
	~GrassStaticModelGroup() override;
	
	static void RegisterObject(Context* context)
	{
		context->RegisterFactory<GrassStaticModelGroup>();

		URHO3D_COPY_BASE_ATTRIBUTES(StaticModelGroup);
	}
	
	protected:
	void OnWorldBoundingBoxUpdate() override
	{
		StaticModelGroup::OnWorldBoundingBoxUpdate();
		 worldBoundingBox_.Define(-M_LARGE_VALUE, M_LARGE_VALUE);
	}
};

GrassStaticModelGroup::~GrassStaticModelGroup() = default;

struct SkyPreset
{
	float Br_{0}, Bm_{0}, g_{0}, cirrus_{0}, cumulus_{0}, cumulusbrightness_{0};
	
	SkyPreset Lerp(const SkyPreset &rhs, float t)
	{
		SkyPreset p;
		p.Br_=Br_ + t * (rhs.Br_ - Br_);
		p.Bm_=Bm_ + t * (rhs.Bm_ - Bm_);
		p.g_=g_ + t * (rhs.g_ - g_);
		p.cirrus_=cirrus_ + t * (rhs.cirrus_ - cirrus_);
		p.cumulus_=cumulus_ + t * (rhs.cumulus_ - cumulus_);
		p.cumulusbrightness_=cumulusbrightness_ + t * (rhs.cumulusbrightness_ - cumulusbrightness_);
		
		return p;
	}
};

struct SkyPresetTemplate
{
	float Br_{0}, Bm_{0}, g_{0}, cumulusbrightness_{0};
	float cirruslow_{0}, cirrushigh_{0}, cumuluslow_{0}, cumulushigh_{0};
	
	SkyPreset Randomize()
	{
		SkyPreset p;
		p.Br_=Br_;
		p.Bm_=Bm_;
		p.g_=g_;
		p.cumulusbrightness_=cumulusbrightness_;
		
		p.cirrus_ = cirruslow_ + RandStandardNormal() * (cirrushigh_ - cirruslow_);
		p.cumulus_ = cumuluslow_ + RandStandardNormal() * (cumulushigh_ - cumuluslow_);
		return p;
	}
};

struct SunSettings
{
	Vector3 sunpos_{0,0,0};
	Color suncolor_{0,0,0};
	Color fogcolor_{0,0,0};
};

Color CalculateSkyboxColor(float timeofday, const Vector3 &pos, const SkyPreset &env)
{
	const Vector3 nitrogen(0.650, 0.570, 0.475);
	auto vecpow=[](const Vector3 &vec, float p)->Vector3 {return Vector3(std::pow(vec.x_, p), std::pow(vec.y_, p), std::pow(vec.z_, p));};
	auto vecdiv=[](float c, const Vector3 &vec)->Vector3 {return Vector3(c/vec.x_, c/vec.y_, c/vec.z_);};
	auto vecexp=[](const Vector3 &vec)->Vector3 {return Vector3(std::exp(vec.x_), std::exp(vec.y_), std::exp(vec.z_));};
	
	Vector3 fsun(0.0, std::sin(timeofday * 0.2617993875), std::cos(timeofday * 0.2617993875));
	float Br=env.Br_;
	float Bm=env.Bm_;
	float g=env.g_;
	Vector3 Kr = vecdiv(Br, vecpow(nitrogen, 4.f));
	Vector3 Km = vecdiv(Bm, vecpow(nitrogen, 0.84f));
	
	float mu = pos.DotProduct(fsun);
	float rayleigh = 3.f / (8.0f * 3.14f) * (1.0f + mu * mu);
	Vector3 mie = (Kr + Km * (1.0f - g * g) / (2.0f + g * g) / pow(1.0f + g * g - 2.0f * g * mu, 1.5f)) / (Br + Bm);
	Vector3 day_extinction=vecexp((Kr / Br)*-exp(-((pos.y_ + fsun.y_ * 4.0f) * (exp(-pos.y_ * 16.0f) + 0.1f) / 80.0f) / Br) * (exp(-pos.y_ * 16.0f) + 0.1f)) * exp(-pos.y_ * exp(-pos.y_ * 8.0f ) * 4.0f) * exp(-pos.y_ * 2.0f) * 4.0f;
	float v=1.0f - std::exp(fsun.y_);
	Vector3 night_extinction=Vector3(v,v,v)*0.2f;
	Vector3 extinction=day_extinction.Lerp(night_extinction, -fsun.y_ * 0.2f + 0.5f);
	Vector3 c=mie*extinction*rayleigh;
	//c.Normalize();
	//c=c*1.1f;
	return Color(std::max(0.0f, std::min(1.0f, c.x_)), std::max(0.0f, std::min(1.0f, c.y_)), std::max(0.0f, std::min(1.0f, c.z_)));
}

SunSettings CalculateSunSettings(float timeofday, float abovehorizon, const SkyPreset &env)
{
	SunSettings sun;
	sun.sunpos_ = Vector3(0.0, std::sin(timeofday * 0.2617993875), std::cos(timeofday * 0.2617993875));
	Vector3 pos(1, abovehorizon, 0);
	pos.Normalize();
	
	if(pos.y_ < 0.f) pos.y_ = 0.f;
	
	sun.fogcolor_=CalculateSkyboxColor(timeofday, pos, env);
	sun.suncolor_=CalculateSkyboxColor(timeofday, sun.sunpos_, env);
	sun.suncolor_.r_=std::max(0.01f, std::min(sun.suncolor_.r_, 1.f));
	sun.suncolor_.g_=std::max(0.01f, std::min(sun.suncolor_.g_, 1.f));
	sun.suncolor_.b_=std::max(0.01f, std::min(sun.suncolor_.b_, 1.f));
	
	return sun;
}

class AtmosphereSettings
{
	public:
	AtmosphereSettings(){}
	
	void AddPreset(const SkyPresetTemplate &t)
	{
		presets_.push_back(t);
		currentpreset_=lastpreset_=RandomPreset();
		time_=0.f;
	}
	void SetInterval(float i){interval_=i;}
	
	SkyPreset Update(float dt)
	{
		time_ += dt;
		if(time_>interval_)
		{
			// Roll a new preset
			lastpreset_=currentpreset_;
			currentpreset_=RandomPreset();
			changetime_=5.f;
			time_=0;
		}
		else
		{
			changetime_-=dt;
		}
		
		float factor=1.f-(std::max(0.0f, std::min(1.f, changetime_/5.f)));
		
		return lastpreset_.Lerp(currentpreset_, factor);
	}
	
	protected:
	ea::vector<SkyPresetTemplate> presets_;
	unsigned int currentpresetindex_;
	float changetime_{0};
	float time_{0};
	float interval_{5.f};
	RandomEngine random_;
	
	SkyPreset currentpreset_{0.001f, 0.0023f, 0.9599f, 2.1f, 1.4f, 1.1575f};
	SkyPreset lastpreset_{0.001f, 0.0023f, 0.9599f, 2.1f, 1.4f, 1.1575f};
	
	SkyPreset RandomPreset()
	{
		if(presets_.size()==0)
		{
			return SkyPreset{0.001f, 0.0023f, 0.9599f, 2.1f, 1.4f, 1.1575f};
		}
		unsigned int which=std::min(presets_.size()-1, random_.GetUInt(presets_.size()));
		return presets_[which].Randomize();
	}
};

class AwesomeGameApplication : public Application
{
    // This macro defines some methods that every `Urho3D::Object` descendant should have.
    URHO3D_OBJECT(AwesomeGameApplication, Application);
public:
    // Likewise every `Urho3D::Object` descendant should implement constructor with single `Context*` parameter.
    AwesomeGameApplication(Context* context)
        : Application(context)
    {
    }

    void Setup() override
    {
        // Engine is not initialized yet. Set up all the parameters now.
        engineParameters_[EP_FULL_SCREEN] = false;
        engineParameters_[EP_WINDOW_HEIGHT] = 768;
        engineParameters_[EP_WINDOW_WIDTH] = 1024;
        // Resource prefix path is a list of semicolon-separated paths which will be checked for containing resource directories. They are relative to application executable file.
        engineParameters_[EP_RESOURCE_PREFIX_PATHS] = ".;..";
		engineParameters_[EP_WINDOW_MAXIMIZE] = true;
		engineParameters_[EP_WINDOW_RESIZABLE]=true;
    }

    void Start() override
    {
		GrassStaticModelGroup::RegisterObject(context_);
        // At this point engine is initialized, but first frame was not rendered yet. Further setup should be done here. To make sample a little bit user friendly show mouse cursor here.
        GetSubsystem<Input>()->SetMouseVisible(true);
		
		scene_=new Scene(context_);
		scene_->CreateComponent<Octree>();
		
		cameraNode_=scene_->CreateChild();
		camera_=cameraNode_->CreateComponent<Camera>();
		camera_->SetFov(50.f);
		camera_->SetViewMask(3);
		
		cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));
		
		auto* renderer = GetSubsystem<Renderer>();

		// Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
		SharedPtr<Viewport> viewport(new Viewport(context_, scene_, camera_));
		renderer->SetViewport(0, viewport);
		
		// Setup a skybox
		Node *n=scene_->CreateChild();
		StaticModel *skybox=n->CreateComponent<Skybox>();
		auto cache=GetSubsystem<ResourceCache>();
		
		skybox->SetModel(cache->GetResource<Model>("Models/Icosphere.mdl"));
		skyboxmaterial_=cache->GetResource<Material>("Materials/ProcSkybox.xml");
		skybox->SetMaterial(skyboxmaterial_);
		
		// Create a Zone component for ambient lighting & fog control
		Node* zoneNode = scene_->CreateChild("Zone");
		zone_ = zoneNode->CreateComponent<Zone>();
		zone_->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));
		zone_->SetAmbientColor(Color(0.15f, 0.15f, 0.15f));
		zone_->SetFogColor(Color(1.0f, 1.0f, 1.0f));
		zone_->SetFogStart(500.0f);
		zone_->SetFogEnd(750.0f);

		// Create a directional light to the world. Enable cascaded shadows on it
		lightNode_ = scene_->CreateChild("DirectionalLight");
		lightNode_->SetDirection(Vector3(0.6f, -1.0f, 0.8f));
		light_ = lightNode_->CreateComponent<Light>();
		light_->SetLightType(LIGHT_DIRECTIONAL);
		light_->SetCastShadows(true);
		light_->SetShadowBias(BiasParameters(0.0025f, 0.5f));
		light_->SetShadowCascade(CascadeParameters(20.0f, 100.0f, 400.0f, 0.0f, 1.6f));
		light_->SetSpecularIntensity(0.5f);
		light_->SetColor(Color(1.1f, 1.1f, 1.0f));
		
		backLightNode_ = scene_->CreateChild();
		Light *backlight=backLightNode_->CreateComponent<Light>();
		backlight->SetLightType(LIGHT_DIRECTIONAL);
		backlight->SetCastShadows(false);
		backlight->SetColor(Color(0.0f, 0.0f, 0.0f));
		
		Node* terrainNode = scene_->CreateChild("Terrain");
		terrainNode->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
		terrain_ = terrainNode->CreateComponent<Terrain>();
		terrain_->SetPatchSize(64);
		terrain_->SetSpacing(Vector3(2.0f, 0.5f, 2.0f)); // Spacing between vertices and vertical resolution of the height map
		terrain_->SetSmoothing(true);
		terrain_->SetHeightMap(cache->GetResource<Image>("Textures/elevation.png"));
		terrain_->SetMaterial(cache->GetResource<Material>("Materials/Terrain4.xml"));
		terrain_->SetViewMask(2);
		// The terrain consists of large triangles, which fits well for occlusion rendering, as a hill can occlude all
		// terrain patches and other objects behind it
		//terrain_->SetOccluder(true);
		terrain_->SetCastShadows(true);
		
		Node *on=scene_->CreateChild();
		StaticModel *om=on->CreateComponent<StaticModel>();
		om->SetModel(cache->GetResource<Model>("Models/Blob.mdl"));
		om->SetMaterial(cache->GetResource<Material>("Materials/TriplanarCliff4.xml"));
		om->SetCastShadows(true);
		om->SetViewMask(2);
		//Vector3 pos=on->GetPosition();
		//pos.y_=terrain_->GetHeight(pos) + 6.0;
		//on->SetPosition(pos);
		//on->SetScale(Vector3(100,100,100));
		
		grassTestNode_ = scene_->CreateChild();
		StaticModelGroup *smg1=grassTestNode_->CreateComponent<GrassStaticModelGroup>();
		smg1->SetModel(cache->GetResource<Model>("Models/GrassBunch.mdl"));
		smg1->SetMaterial(cache->GetResource<Material>("Materials/GrassTest.xml"));
		smg1->SetCastShadows(false);
		
		StaticModelGroup *smg2=grassTestNode_->CreateComponent<GrassStaticModelGroup>();
		smg2->SetModel(cache->GetResource<Model>("Models/GrassBunch2.mdl"));
		smg2->SetMaterial(cache->GetResource<Material>("Materials/GrassTest.xml"));
		smg2->SetCastShadows(false);
		
		int radius=80;
		
		for(int x=0; x<radius*2; ++x)
		{
			for(int y=0; y<radius*2; ++y)
			{
				//if(x!=12 || y!=12)
				float dx=(float)x - (float)radius;
				float dy=(float)y - (float)radius;
				float dist=std::sqrt(dx*dx+dy*dy);
				
				if(dist<=(float)radius)
				{
					Node *ch=grassTestNode_->CreateChild();
					ch->SetPosition(Vector3(((float)x-(float)radius), 0, ((float)y-(float)radius)));
					smg1->AddInstanceNode(ch);
					smg2->AddInstanceNode(ch);
				//ch->SetScale(Vector3(8,8,8));
				
				/*StaticModel *msh=ch->CreateComponent<GrassStaticModel>();
				msh->SetModel(cache->GetResource<Model>("Models/GrassBunch.mdl"));
				msh->SetMaterial(cache->GetResource<Material>("Materials/GrassTest.xml"));
				msh->SetCastShadows(false);
				
				msh=ch->CreateComponent<GrassStaticModel>();
				msh->SetModel(cache->GetResource<Model>("Models/GrassBunch2.mdl"));
				msh->SetMaterial(cache->GetResource<Material>("Materials/GrassTest.xml"));
				msh->SetCastShadows(false);*/
				//msh->SetViewMask(1);
				}
			}
		}
		
		Material *m=cache->GetResource<Material>("Materials/GrassTest.xml");
		m->SetShaderParameter("HeightMapData", Variant(Vector4(terrain_->GetHeightMap()->GetWidth(), terrain_->GetHeightMap()->GetHeight(), terrain_->GetSpacing().x_, terrain_->GetSpacing().y_)));
		//m->SetShaderParameter("InnerRadius", Variant(168.f));
		//m->SetShaderParameter("OuterRadius", Variant(240.f));
		m->SetShaderParameter("Radius", Variant(Vector2((float)radius*0.7f, (float)radius)));
		
		auto ui=GetSubsystem<UI>();
		auto* style = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");
		
		element_=ui->LoadLayout(cache->GetResource<XMLFile>("UI/sliders.xml"), style);
		ui->GetRoot()->AddChild(element_);
		element_->SetWidth(ui->GetRoot()->GetWidth());
		element_->SetPosition(IntVector2(0, ui->GetRoot()->GetHeight()-element_->GetHeight()));
		
		toggle_=ui->LoadLayout(cache->GetResource<XMLFile>("UI/ToggleButton.xml"), style);
		ui->GetRoot()->AddChild(toggle_);
		toggle_->SetPosition(IntVector2(0,0));
		
		
		dynamic_cast<Slider *>(element_->GetChild("BrSlider", true))->SetValue(10);
		dynamic_cast<Slider *>(element_->GetChild("BmSlider", true))->SetValue(25);
		dynamic_cast<Slider *>(element_->GetChild("gSlider", true))->SetValue(75);
		dynamic_cast<Slider *>(element_->GetChild("CirrusSlider", true))->SetValue(60);
		dynamic_cast<Slider *>(element_->GetChild("CumulusSlider", true))->SetValue(40);
		dynamic_cast<Slider *>(element_->GetChild("CumulusBrightnessSlider", true))->SetValue(33);
		dynamic_cast<Slider *>(element_->GetChild("SunSlider", true))->SetValue(10);
		
		SubscribeToEvent(StringHash("Update"), URHO3D_HANDLER(AwesomeGameApplication, HandleUpdate));
		SubscribeToEvent(toggle_, StringHash("Pressed"), URHO3D_HANDLER(AwesomeGameApplication, HandleToggle));
		SubscribeToEvent(element_->GetChild("AddPresetButton", true), StringHash("Pressed"), URHO3D_HANDLER(AwesomeGameApplication, HandleAddPreset));
	}

    void Stop() override
    {
        // This step is executed when application is closing. No more frames will be rendered after this method is invoked.
    }
	
	float GetSliderValue(const ea::string &name, float rangelow, float rangehigh)
	{
		Slider *s=dynamic_cast<Slider *>(element_->GetChild(name+"Slider", true));
		if(s)
		{
			float v=(s->GetValue()/100.0f);
			v=rangelow + v * (rangehigh - rangelow);
			Text *t=dynamic_cast<Text *>(element_->GetChild(name+"Value", true));
			if(t)
			{
				t->SetText(ToString("%.4f", v));
			}
			return v;
		}
		return 0;
	}
	
	void Update(float timeStep)
	{
		//float timeofday{0};
		float speedmul=0.1f;
		SkyPreset p;
		if(manual_)
		{
			//float Br{0.0001}, Bm{0.0001}, g{0.9}, cirrus{0.5}, cumulus{0.5}, cumulusbrightness{0.5};
			p.Br_=GetSliderValue("Br", 0.0001, 0.009);
			p.Bm_=GetSliderValue("Bm", 0.0001, 0.009);
			p.g_=GetSliderValue("g", 0.9, 0.9999);
			p.cirrus_=GetSliderValue("Cirrus", 0.0, 3.5);
			p.cumulus_=GetSliderValue("Cumulus", 0.0, 3.5);
			p.cumulusbrightness_=GetSliderValue("CumulusBrightness", 0.25, 3.0);
			timeofday_=GetSliderValue("Sun", 0.0, 24.0);
			speedmul=GetSliderValue("Speed", 0.1, 2.0);
		}
		else
		{
			p = atmosphere_.Update(timeStep);
			timeofday_ += timeStep*0.125f;
		}
		
		while (timeofday_ >=24.f) timeofday_ -= 24.f;
		
		skyboxmaterial_->SetShaderParameter("TimeOfDay", Variant(timeofday_));
		skyboxmaterial_->SetShaderParameter("Br", Variant(p.Br_));
		skyboxmaterial_->SetShaderParameter("Bm", Variant(p.Bm_));
		skyboxmaterial_->SetShaderParameter("G", Variant(p.g_));
		skyboxmaterial_->SetShaderParameter("Cirrus", Variant(p.cirrus_));
		skyboxmaterial_->SetShaderParameter("Cumulus", Variant(p.cumulus_));
		skyboxmaterial_->SetShaderParameter("CumulusBrightness", Variant(p.cumulusbrightness_));
		
		time_ += timeStep*speedmul;
		skyboxmaterial_->SetShaderParameter("CloudTime", Variant(time_));
		
		auto input=GetSubsystem<Input>();
		
		if(input->GetMouseButtonDown(MOUSEB_RIGHT))
		{
			input->SetMouseVisible(false);
		}
		else
		{
			input->SetMouseVisible(true);
		}
	
		SunSettings sun=CalculateSunSettings(timeofday_, 0.0f, p);
		zone_->SetFogColor(sun.fogcolor_);
		zone_->SetAmbientColor(Color(sun.fogcolor_.r_*0.4f+0.1f, sun.fogcolor_.g_*0.4f+0.1f, sun.fogcolor_.b_*0.4f+0.15f));
		lightNode_->SetDirection(-sun.sunpos_);
		backLightNode_->SetDirection(sun.sunpos_);
		light_->SetColor(sun.suncolor_);
		
		float px=cameraNode_->GetPosition().x_;
		float pz=cameraNode_->GetPosition().z_;
		Vector3 ps((float)((int)(px/1.f))*1.f, 0, (float)((int)(pz/1.f))*1.f);
		
		grassTestNode_->SetPosition(ps);
		
		auto cache=GetSubsystem<ResourceCache>();
		Material *m=cache->GetResource<Material>("Materials/GrassTest.xml");
		
		MoveCamera(timeStep);
	}
	void MoveCamera(float timeStep)
	{
		// Do not move if the UI has a focused element (the console)
		if (GetSubsystem<UI>()->GetFocusElement())
			return;

		auto* input = GetSubsystem<Input>();

		// Movement speed as world units per second
		const float MOVE_SPEED = 30.0f;
		// Mouse sensitivity as degrees per pixel
		const float MOUSE_SENSITIVITY = 0.1f;

		// Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
		if(input->GetMouseButtonDown(MOUSEB_RIGHT))
		{
			IntVector2 mouseMove = input->GetMouseMove();
			yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
			pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
			pitch_ = Clamp(pitch_, -90.0f, 90.0f);

			// Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
			cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));
			//URHO3D_LOGINFOF("Pitch: %.2f", pitch_);
		}

		// Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
		if (input->GetKeyDown(KEY_W))
			cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
		if (input->GetKeyDown(KEY_S))
			cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
		if (input->GetKeyDown(KEY_A))
			cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
		if (input->GetKeyDown(KEY_D))
			cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);
		

		Octree *octree=scene_->GetComponent<Octree>();
		float hitpos=0;
		Drawable *hitdrawable=nullptr;
		Vector3 ground;
		Vector3 pos=cameraNode_->GetPosition();
		Vector3 raypos=pos+Vector3(0,100.f,0);
		Ray ray(raypos, pos-raypos);
		static ea::vector<RayQueryResult> result;
		result.clear();
		RayOctreeQuery query(result, ray, RAY_TRIANGLE, 300.f, DRAWABLE_GEOMETRY, 2);
		octree->Raycast(query);
		if(result.size()!=0)
		{

			for(unsigned int i=0; i<result.size(); ++i)
			{
				if(result[i].distance_>=0)
				{
					ground=ray.origin_+ray.direction_*result[i].distance_;
					pos.y_=ground.y_+12.f;//terrain_->GetHeight(pos) + 12.0;
					//cameraNode_->SetPosition(pos);
					//return;
				}
			}
		}
		
		//Vector3 pos=cameraNode_->GetPosition();
		pos.y_=terrain_->GetHeight(pos) + 4.0;
		cameraNode_->SetPosition(pos);
	}

	
	protected:
	SharedPtr<Scene> scene_;
	Node *cameraNode_, *lightNode_, *backLightNode_;
	Camera *camera_;
	float yaw_{0}, pitch_{-20.7f};
	float time_{0};
	Material *skyboxmaterial_{nullptr};
	SharedPtr<UIElement> element_;
	Zone *zone_{nullptr};
	Light *light_{nullptr};
	Terrain *terrain_;
	bool manual_{true};
	float timeofday_{0.f};
	
	Node *grassTestNode_{nullptr};
	
	AtmosphereSettings atmosphere_;
	
	SharedPtr<UIElement> toggle_;
	
	void HandleUpdate(StringHash eventType, VariantMap &eventData)
	{
		float timeStep=eventData["TimeStep"].GetFloat();
		Update(timeStep);
	}
	
	void HandleToggle(StringHash eventType, VariantMap &eventData)
	{
		manual_= !manual_;
		element_->SetVisible(manual_);
		
		Text *t=dynamic_cast<Text *>(toggle_->GetChild("ToggleText", true));
		if(t)
		{
			if(manual_) t->SetText("Manual");
			else t->SetText("Auto");
		}
	}
	
	void HandleAddPreset(StringHash eventType, VariantMap &eventData)
	{
		SkyPresetTemplate p;
		
		p.Br_=GetSliderValue("Br", 0.0001, 0.009);
		p.Bm_=GetSliderValue("Bm", 0.0001, 0.009);
		p.g_=GetSliderValue("g", 0.9, 0.9999);
		p.cumulusbrightness_=GetSliderValue("CumulusBrightness", 0.25, 3.0);
		float cirrus=GetSliderValue("Cirrus", 0.0, 3.5);
		float cumulus=GetSliderValue("Cumulus", 0.0, 3.5);
		
		p.cirruslow_=cirrus*0.8f;
		p.cirrushigh_=cirrus;
		p.cumuluslow_=cumulus*0.8f;
		p.cumulushigh_=cumulus;
		
		atmosphere_.AddPreset(p);
	}
};

// A helper macro which defines main function. Forgetting it will result in linker errors complaining about missing `_main` or `_WinMain@16`.
URHO3D_DEFINE_APPLICATION_MAIN(AwesomeGameApplication);