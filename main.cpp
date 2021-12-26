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


// This is probably always OK.
using namespace Urho3D;


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
		}
		
		s=dynamic_cast<Slider *>(element_->GetChild("BmSlider", true));
		if(s)
		{
			float v=(s->GetValue()/100.0f);
			v=0.0001 + v * (0.009 - 0.0001);
			skyboxmaterial_->SetShaderParameter("Bm", Variant(v));
			Text *t=dynamic_cast<Text *>(element_->GetChild("BmValue", true));
			if(t)
			{
				t->SetText(ToString("%.4f", v));
			}
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
		}
		
		s=dynamic_cast<Slider *>(element_->GetChild("CirrusSlider", true));
		if(s)
		{
			float v=(s->GetValue()/100.0f)*1.5f;
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
			float v=(s->GetValue()/100.0f)*1.5f;
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
			v*=12.f;
			skyboxmaterial_->SetShaderParameter("TimeOfDay", Variant(v));
			Text *t=dynamic_cast<Text *>(element_->GetChild("SunValue", true));
			if(t)
			{
				t->SetText(ToString("%.4f", v));
			}
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
		pitch_ = Clamp(pitch_, -90.0f, 0.0f);

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
	Node *cameraNode_;
	Camera *camera_;
	float yaw_{0}, pitch_{-20.7f};
	float time_{0};
	Material *skyboxmaterial_{nullptr};
	SharedPtr<UIElement> element_;
	
	void HandleUpdate(StringHash eventType, VariantMap &eventData)
	{
		float timeStep=eventData["TimeStep"].GetFloat();
		Update(timeStep);
	}
};

// A helper macro which defines main function. Forgetting it will result in linker errors complaining about missing `_main` or `_WinMain@16`.
URHO3D_DEFINE_APPLICATION_MAIN(AwesomeGameApplication);