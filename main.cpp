#include <Urho3D/Engine/Application.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/Input/Input.h>

#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Scene/Component.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/Skybox.h>
#include <Urho3D/Graphics/Light.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/Viewport.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/Resource/XMLFile.h>
#include <Urho3D/UI/Slider.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Graphics/Terrain.h>
#include <Urho3D/Graphics/Light.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/Math/Random.h>
#include <cmath>
// This is probably always OK.
using namespace Urho3D;



struct EnvironmentPreset
{
	float Br_{0}, Bm_{0}, g_{0}, cirrus_{0}, cumulus_{0}, cumulusbrightness_{0};
	
	EnvironmentPreset Lerp(const EnvironmentPreset &rhs, float t)
	{
		EnvironmentPreset p;
		p.Br_=Br_ + t * (rhs.Br_ - Br_);
		p.Bm_=Bm_ + t * (rhs.Bm_ - Bm_);
		p.g_=g_ + t * (rhs.g_ - g_);
		p.cirrus_=cirrus_ + t * (rhs.cirrus_ - cirrus_);
		p.cumulus_=cumulus_ + t * (rhs.cumulus_ - cumulus_);
		p.cumulusbrightness_=cumulusbrightness_ + t * (rhs.cumulusbrightness_ - cumulusbrightness_);
		
		return p;
	}
};

struct EnvironmentPresetTemplate
{
	float Br_{0}, Bm_{0}, g_{0}, cumulusbrightness_{0};
	float cirruslow_{0}, cirrushigh_{0}, cumuluslow_{0}, cumulushigh_{0};
	
	EnvironmentPreset Randomize()
	{
		EnvironmentPreset p;
		p.Br_=Br_;
		p.Bm_=Bm_;
		p.g_=g_;
		p.cumulusbrightness_=cumulusbrightness_;
		
		p.cirrus_ = cirruslow_ + RandStandardNormal() * (cirrushigh_ - cirruslow_);
		p.cumulus_ = cumuluslow_ + RandStandardNormal() * (cirrushigh_ - cirruslow_);
		return p;
	}
};

struct SunSettings
{
	Vector3 sunpos_{0,0,0};
	Color suncolor_{0,0,0};
	Color fogcolor_{0,0,0};
};

Color CalculateSkyboxColor(float timeofday, const Vector3 &pos, const EnvironmentPreset &env)
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
	return Color(c.x_, c.y_, c.z_);
}

SunSettings CalculateSunSettings(float timeofday, float abovehorizon, const EnvironmentPreset &env)
{
	SunSettings sun;
	sun.sunpos_ = Vector3(0.0, std::sin(timeofday * 0.2617993875), std::cos(timeofday * 0.2617993875));
	Vector3 pos(1, abovehorizon, 0);
	pos.Normalize();
	
	if(pos.y_ < 0.f) pos.y_ = 0.f;
	
	sun.fogcolor_=CalculateSkyboxColor(timeofday, pos, env);
	sun.suncolor_=CalculateSkyboxColor(timeofday, sun.sunpos_, env);
	sun.suncolor_.r_=std::min(sun.suncolor_.r_, 1.f);
	sun.suncolor_.g_=std::min(sun.suncolor_.g_, 1.f);
	sun.suncolor_.b_=std::min(sun.suncolor_.b_, 1.f);
	
	return sun;
}

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
    }

    void Start() override
    {
        // At this point engine is initialized, but first frame was not rendered yet. Further setup should be done here. To make sample a little bit user friendly show mouse cursor here.
        GetSubsystem<Input>()->SetMouseVisible(true);
		
		scene_=new Scene(context_);
		scene_->CreateComponent<Octree>();
		
		cameraNode_=scene_->CreateChild();
		camera_=cameraNode_->CreateComponent<Camera>();
		camera_->SetFov(60.f);
		
		cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));
		
		auto* renderer = GetSubsystem<Renderer>();

		// Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
		SharedPtr<Viewport> viewport(new Viewport(context_, scene_, camera_));
		renderer->SetViewport(0, viewport);
		
		// Setup a skybox
		Node *n=scene_->CreateChild();
		StaticModel *skybox=n->CreateComponent<Skybox>();
		auto cache=GetSubsystem<ResourceCache>();
		
		skybox->SetModel(cache->GetResource<Model>("Models/Dome.mdl"));
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
		light_->SetShadowBias(BiasParameters(0.00025f, 0.5f));
		light_->SetShadowCascade(CascadeParameters(10.0f, 50.0f, 200.0f, 0.0f, 0.8f));
		light_->SetSpecularIntensity(0.5f);
		// Apply slightly overbright lighting to match the skybox
		light_->SetColor(Color(1.2f, 1.2f, 1.2f));
		Node* terrainNode = scene_->CreateChild("Terrain");
		terrainNode->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
		auto* terrain = terrainNode->CreateComponent<Terrain>();
		terrain->SetPatchSize(64);
		terrain->SetSpacing(Vector3(2.0f, 0.5f, 2.0f)); // Spacing between vertices and vertical resolution of the height map
		terrain->SetSmoothing(true);
		terrain->SetHeightMap(cache->GetResource<Image>("Textures/HeightMap.png"));
		terrain->SetMaterial(cache->GetResource<Material>("Materials/Terrain.xml"));
		// The terrain consists of large triangles, which fits well for occlusion rendering, as a hill can occlude all
		// terrain patches and other objects behind it
		terrain->SetOccluder(true);
		
		auto ui=GetSubsystem<UI>();
		auto* style = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");
		
		element_=ui->LoadLayout(cache->GetResource<XMLFile>("UI/sliders.xml"), style);
		ui->GetRoot()->AddChild(element_);
		element_->SetWidth(ui->GetRoot()->GetWidth());
		element_->SetPosition(IntVector2(0, ui->GetRoot()->GetHeight()-element_->GetHeight()));
		
		dynamic_cast<Slider *>(element_->GetChild("BrSlider", true))->SetValue(50);
		dynamic_cast<Slider *>(element_->GetChild("BmSlider", true))->SetValue(40);
		dynamic_cast<Slider *>(element_->GetChild("gSlider", true))->SetValue(60);
		dynamic_cast<Slider *>(element_->GetChild("CirrusSlider", true))->SetValue(60);
		dynamic_cast<Slider *>(element_->GetChild("CumulusSlider", true))->SetValue(40);
		dynamic_cast<Slider *>(element_->GetChild("CumulusBrightnessSlider", true))->SetValue(50);
		dynamic_cast<Slider *>(element_->GetChild("SunSlider", true))->SetValue(10);
		
		SubscribeToEvent(StringHash("Update"), URHO3D_HANDLER(AwesomeGameApplication, HandleUpdate));
	}

    void Stop() override
    {
        // This step is executed when application is closing. No more frames will be rendered after this method is invoked.
    }
	
	void Update(float timeStep)
	{
		float timeofday, Br, Bm, g;
		Slider *s=dynamic_cast<Slider *>(element_->GetChild("BrSlider", true));
		if(s)
		{
			float v=(s->GetValue()/100.0f);
			v=0.0001 + v * (0.009 - 0.0001);
			skyboxmaterial_->SetShaderParameter("Br", Variant(v));
			Text *t=dynamic_cast<Text *>(element_->GetChild("BrValue", true));
			if(t)
			{
				t->SetText(ToString("%.4f", v));
			}
			Br=v;
		}
		
		s=dynamic_cast<Slider *>(element_->GetChild("BmSlider", true));
		if(s)
		{
			float v=(s->GetValue()/100.0f);
			v=0.0001 + v * (0.09 - 0.0001);
			skyboxmaterial_->SetShaderParameter("Bm", Variant(v));
			Text *t=dynamic_cast<Text *>(element_->GetChild("BmValue", true));
			if(t)
			{
				t->SetText(ToString("%.4f", v));
			}
			Bm=v;
		}
		
		s=dynamic_cast<Slider *>(element_->GetChild("gSlider", true));
		if(s)
		{
			float v=(s->GetValue()/100.0f);
			v=0.9 + v * (0.9999 - 0.9);
			skyboxmaterial_->SetShaderParameter("G", Variant(v));
			Text *t=dynamic_cast<Text *>(element_->GetChild("gValue", true));
			if(t)
			{
				t->SetText(ToString("%.4f", v));
			}
			g=v;
		}
		
		s=dynamic_cast<Slider *>(element_->GetChild("CirrusSlider", true));
		if(s)
		{
			float v=(s->GetValue()/100.0f)*3.5f;
			//v=0.9 + v * (0.9999 - 0.9);
			skyboxmaterial_->SetShaderParameter("Cirrus", Variant(v));
			Text *t=dynamic_cast<Text *>(element_->GetChild("CirrusValue", true));
			if(t)
			{
				t->SetText(ToString("%.4f", v));
			}
		}
		
		s=dynamic_cast<Slider *>(element_->GetChild("CumulusSlider", true));
		if(s)
		{
			float v=(s->GetValue()/100.0f)*3.5f;
			//v=0.9 + v * (0.9999 - 0.9);
			skyboxmaterial_->SetShaderParameter("Cumulus", Variant(v));
			Text *t=dynamic_cast<Text *>(element_->GetChild("CumulusValue", true));
			if(t)
			{
				t->SetText(ToString("%.4f", v));
			}
		}
		
		s=dynamic_cast<Slider *>(element_->GetChild("CumulusBrightnessSlider", true));
		if(s)
		{
			float v=(s->GetValue()/100.0f);
			v=0.5 + v * (3.0 - 0.5);
			skyboxmaterial_->SetShaderParameter("CumulusBrightness", Variant(v));
			Text *t=dynamic_cast<Text *>(element_->GetChild("CumulusBrightnessValue", true));
			if(t)
			{
				t->SetText(ToString("%.4f", v));
			}
		}
		
		s=dynamic_cast<Slider *>(element_->GetChild("SunSlider", true));
		if(s)
		{
			float v=(s->GetValue()/100.0f);
			//v=0.9 + v * (0.9999 - 0.9);
			v*=24.f;
			skyboxmaterial_->SetShaderParameter("TimeOfDay", Variant(v));
			Text *t=dynamic_cast<Text *>(element_->GetChild("SunValue", true));
			if(t)
			{
				t->SetText(ToString("%.4f", v));
			}
			timeofday=v;
		}
		
		float speedmul=1;
		s=dynamic_cast<Slider *>(element_->GetChild("SpeedSlider", true));
		if(s)
		{
			float v=(s->GetValue()/100.0f);
			v=0.1 + v * (2.0 - 0.1);
			Text *t=dynamic_cast<Text *>(element_->GetChild("SpeedValue", true));
			if(t)
			{
				t->SetText(ToString("%.1f", v));
			}
			speedmul=v;
		}
		
		time_ += timeStep*speedmul;
		
		if(skyboxmaterial_)
		{
			skyboxmaterial_->SetShaderParameter("CloudTime", Variant(time_));
		}
		
		auto input=GetSubsystem<Input>();
		
		if(input->GetMouseButtonDown(MOUSEB_RIGHT))
		{
			input->SetMouseVisible(false);
		}
		else
		{
			input->SetMouseVisible(true);
		}
		
		EnvironmentPreset p;
		p.Br_=Br;
		p.Bm_=Bm;
		p.g_=g;
	
		SunSettings sun=CalculateSunSettings(timeofday, 0.0f, p);
		zone_->SetFogColor(sun.fogcolor_);
		zone_->SetAmbientColor(Color(sun.fogcolor_.r_*0.2f, sun.fogcolor_.g_*0.2f, sun.fogcolor_.b_*0.2f));
		lightNode_->SetDirection(-sun.sunpos_);
		light_->SetColor(sun.suncolor_);
		
		MoveCamera(timeStep);
	}
	void MoveCamera(float timeStep)
{
    // Do not move if the UI has a focused element (the console)
    if (GetSubsystem<UI>()->GetFocusElement())
        return;

    auto* input = GetSubsystem<Input>();

    // Movement speed as world units per second
    const float MOVE_SPEED = 20.0f;
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
}

	
	protected:
	SharedPtr<Scene> scene_;
	Node *cameraNode_, *lightNode_;
	Camera *camera_;
	float yaw_{0}, pitch_{-20.7f};
	float time_{0};
	Material *skyboxmaterial_{nullptr};
	SharedPtr<UIElement> element_;
	Zone *zone_{nullptr};
	Light *light_{nullptr};
	
	void HandleUpdate(StringHash eventType, VariantMap &eventData)
	{
		float timeStep=eventData["TimeStep"].GetFloat();
		Update(timeStep);
	}
};

// A helper macro which defines main function. Forgetting it will result in linker errors complaining about missing `_main` or `_WinMain@16`.
URHO3D_DEFINE_APPLICATION_MAIN(AwesomeGameApplication);