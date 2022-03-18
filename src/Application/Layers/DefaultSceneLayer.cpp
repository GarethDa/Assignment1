#include "DefaultSceneLayer.h"

// GLM math library
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <GLM/gtx/common.hpp> // for fmod (floating modulus)

#include <filesystem>

// Graphics
#include "Graphics/Buffers/IndexBuffer.h"
#include "Graphics/Buffers/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Textures/Texture2D.h"
#include "Graphics/Textures/TextureCube.h"
#include "Graphics/VertexTypes.h"
#include "Graphics/Font.h"
#include "Graphics/GuiBatcher.h"
#include "Graphics/Framebuffer.h"

// Utilities
#include "Utils/MeshBuilder.h"
#include "Utils/MeshFactory.h"
#include "Utils/ObjLoader.h"
#include "Utils/ImGuiHelper.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/FileHelpers.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/StringUtils.h"
#include "Utils/GlmDefines.h"

// Gameplay
#include "Gameplay/Material.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"

// Components
#include "Gameplay/Components/IComponent.h"
#include "Gameplay/Components/Camera.h"
#include "Gameplay/Components/RotatingBehaviour.h"
#include "Gameplay/Components/JumpBehaviour.h"
#include "Gameplay/Components/RenderComponent.h"
#include "Gameplay/Components/MaterialSwapBehaviour.h"
#include "Gameplay/Components/TriggerVolumeEnterBehaviour.h"
#include "Gameplay/Components/SimpleCameraControl.h"
#include "Gameplay/Components/LerpBehaviour.h"

// Physics
#include "Gameplay/Physics/RigidBody.h"
#include "Gameplay/Physics/Colliders/BoxCollider.h"
#include "Gameplay/Physics/Colliders/PlaneCollider.h"
#include "Gameplay/Physics/Colliders/SphereCollider.h"
#include "Gameplay/Physics/Colliders/ConvexMeshCollider.h"
#include "Gameplay/Physics/Colliders/CylinderCollider.h"
#include "Gameplay/Physics/TriggerVolume.h"
#include "Graphics/DebugDraw.h"

// GUI
#include "Gameplay/Components/GUI/RectTransform.h"
#include "Gameplay/Components/GUI/GuiPanel.h"
#include "Gameplay/Components/GUI/GuiText.h"
#include "Gameplay/InputEngine.h"

#include "Application/Application.h"
#include "Gameplay/Components/ParticleSystem.h"
#include "Graphics/Textures/Texture3D.h"
#include "Graphics/Textures/Texture1D.h"

DefaultSceneLayer::DefaultSceneLayer() :
	ApplicationLayer()
{
	Name = "Default Scene";
	Overrides = AppLayerFunctions::OnAppLoad;
}

DefaultSceneLayer::~DefaultSceneLayer() = default;

void DefaultSceneLayer::OnAppLoad(const nlohmann::json& config) {
	_CreateScene();
}

void DefaultSceneLayer::_CreateScene()
{
	using namespace Gameplay;
	using namespace Gameplay::Physics;

	Application& app = Application::Get();

	bool loadScene = false;
	// For now we can use a toggle to generate our scene vs load from file
	if (loadScene && std::filesystem::exists("scene.json")) {
		app.LoadScene("scene.json");
	} else {
		// This time we'll have 2 different shaders, and share data between both of them using the UBO
		// This shader will handle reflective materials 
		ShaderProgram::Sptr reflectiveShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_blinn_phong_textured.glsl" }
		});
		reflectiveShader->SetDebugName("Reflective");

		// This shader handles our basic materials without reflections (cause they expensive)
		ShaderProgram::Sptr basicShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_blinn_phong_textured.glsl" }
		});
		basicShader->SetDebugName("Blinn-phong");

		// This shader handles our basic materials without reflections (cause they expensive)
		ShaderProgram::Sptr specShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/textured_specular.glsl" }
		});
		specShader->SetDebugName("Textured-Specular");

		// This shader handles our foliage vertex shader example
		ShaderProgram::Sptr foliageShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/foliage.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/screendoor_transparency.glsl" }
		});
		foliageShader->SetDebugName("Foliage");

		// This shader handles our cel shading example
		ShaderProgram::Sptr toonShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/toon_shading.glsl" }
		});
		toonShader->SetDebugName("Toon Shader");

		// This shader handles our displacement mapping example
		ShaderProgram::Sptr displacementShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/displacement_mapping.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_tangentspace_normal_maps.glsl" }
		});
		displacementShader->SetDebugName("Displacement Mapping");

		// This shader handles our tangent space normal mapping
		ShaderProgram::Sptr tangentSpaceMapping = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_tangentspace_normal_maps.glsl" }
		});
		tangentSpaceMapping->SetDebugName("Tangent Space Mapping");

		// This shader handles our multitexturing example
		ShaderProgram::Sptr multiTextureShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/vert_multitextured.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_multitextured.glsl" }
		});
		multiTextureShader->SetDebugName("Multitexturing");

		// Load in the meshes
		MeshResource::Sptr monkeyMesh = ResourceManager::CreateAsset<MeshResource>("Monkey.obj");

		MeshResource::Sptr cactusMesh = ResourceManager::CreateAsset<MeshResource>("cactus.obj");
		MeshResource::Sptr smallCactusMesh = ResourceManager::CreateAsset<MeshResource>("smallBall.obj");
		MeshResource::Sptr bigCactusMesh = ResourceManager::CreateAsset<MeshResource>("bigBall.obj");
		MeshResource::Sptr snakeMesh = ResourceManager::CreateAsset<MeshResource>("snake.obj");

		MeshResource::Sptr rock1Mesh = ResourceManager::CreateAsset<MeshResource>("rock1.obj");
		MeshResource::Sptr rock2Mesh = ResourceManager::CreateAsset<MeshResource>("rock2.obj");

		MeshResource::Sptr plank1Mesh = ResourceManager::CreateAsset<MeshResource>("plank1.obj");
		MeshResource::Sptr plank2Mesh = ResourceManager::CreateAsset<MeshResource>("plank2.obj");

		// Load in some textures
		Texture2D::Sptr    boxTexture   = ResourceManager::CreateAsset<Texture2D>("textures/box-diffuse.png");
		Texture2D::Sptr    boxSpec      = ResourceManager::CreateAsset<Texture2D>("textures/box-specular.png");
		Texture2D::Sptr    monkeyTex    = ResourceManager::CreateAsset<Texture2D>("textures/monkey-uvMap.png");
		Texture2D::Sptr    leafTex      = ResourceManager::CreateAsset<Texture2D>("textures/leaves.png");
		leafTex->SetMinFilter(MinFilter::Nearest);
		leafTex->SetMagFilter(MagFilter::Nearest);

		Texture2D::Sptr    sandTexture = ResourceManager::CreateAsset<Texture2D>("textures/sand.png");
		Texture2D::Sptr    cactusTexture = ResourceManager::CreateAsset<Texture2D>("textures/cactus.png");
		Texture2D::Sptr    ballCactusTexture = ResourceManager::CreateAsset<Texture2D>("textures/ballCactus.png");
		Texture2D::Sptr    snakeTexture = ResourceManager::CreateAsset<Texture2D>("textures/snake.png");
		Texture2D::Sptr    rockTexture = ResourceManager::CreateAsset<Texture2D>("textures/rock.png");
		Texture2D::Sptr    plankTexture = ResourceManager::CreateAsset<Texture2D>("textures/wood.png");

		// Loading in a 1D LUT
		Texture1D::Sptr toonLut = ResourceManager::CreateAsset<Texture1D>("luts/toon-1D.png"); 
		toonLut->SetWrap(WrapMode::ClampToEdge);

		// Here we'll load in the cubemap, as well as a special shader to handle drawing the skybox
		TextureCube::Sptr testCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/ocean/ocean.jpg");
		ShaderProgram::Sptr      skyboxShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" }
		});

		// Create an empty scene
		Scene::Sptr scene = std::make_shared<Scene>(); 

		// Setting up our enviroment map
		scene->SetSkyboxTexture(testCubemap); 
		scene->SetSkyboxShader(skyboxShader);
		// Since the skybox I used was for Y-up, we need to rotate it 90 deg around the X-axis to convert it to z-up 
		scene->SetSkyboxRotation(glm::rotate(MAT4_IDENTITY, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));

		// Loading in a color lookup table
		Texture3D::Sptr lut1 = ResourceManager::CreateAsset<Texture3D>("luts/customLUT.CUBE");  
		Texture3D::Sptr lut2 = ResourceManager::CreateAsset<Texture3D>("luts/coolLUT.CUBE");
		Texture3D::Sptr lut3 = ResourceManager::CreateAsset<Texture3D>("luts/warmLUT.CUBE");

		// Configure the color correction LUT
		scene->SetColorLUT(lut1, 0);
		scene->SetColorLUT(lut2, 1);
		scene->SetColorLUT(lut3, 2);

		// Create our materials
		// This will be our box material, with no environment reflections
		Material::Sptr boxMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			boxMaterial->Name = "Box";
			boxMaterial->Set("u_Material.Diffuse", boxTexture);
			boxMaterial->Set("u_Material.Shininess", 0.1f);
			boxMaterial->Set("s_1Dtex", toonLut);
			//boxMaterial->Set("offsets", offsets);
		}

		Material::Sptr sandMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			sandMaterial->Name = "Sand";
			sandMaterial->Set("u_Material.Diffuse", sandTexture);
			sandMaterial->Set("u_Material.Shininess", 0.0f);
			sandMaterial->Set("s_1Dtex", toonLut);
			//sandMaterial->Set("offsets", offsets);
		}

		Material::Sptr cactusMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			cactusMaterial->Name = "Cactus";
			cactusMaterial->Set("u_Material.Diffuse", cactusTexture);
			cactusMaterial->Set("u_Material.Shininess", 0.0f);
			cactusMaterial->Set("s_1Dtex", toonLut);
			//cactusMaterial->Set("offsets", offsets);
		}

		Material::Sptr ballCactusMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			ballCactusMaterial->Name = "BallCactus";
			ballCactusMaterial->Set("u_Material.Diffuse", ballCactusTexture);
			ballCactusMaterial->Set("u_Material.Shininess", 0.0f);
			ballCactusMaterial->Set("s_1Dtex", toonLut);
			//ballCactusMaterial->Set("offsets", offsets);
		}

		Material::Sptr snakeMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			snakeMaterial->Name = "Snake";
			snakeMaterial->Set("u_Material.Diffuse", snakeTexture);
			snakeMaterial->Set("u_Material.Shininess", 0.0f);
			snakeMaterial->Set("s_1Dtex", toonLut);
			//snakeMaterial->Set("offsets", offsets);
		}

		Material::Sptr rockMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			rockMaterial->Name = "Rock";
			rockMaterial->Set("u_Material.Diffuse", rockTexture);
			rockMaterial->Set("u_Material.Shininess", 0.6f);
			rockMaterial->Set("s_1Dtex", toonLut);
			//snakeMaterial->Set("offsets", offsets);
		}

		Material::Sptr plankMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			plankMaterial->Name = "Plank";
			plankMaterial->Set("u_Material.Diffuse", plankTexture);
			plankMaterial->Set("u_Material.Shininess", 0.1f);
			plankMaterial->Set("s_1Dtex", toonLut);
			//snakeMaterial->Set("offsets", offsets);
		}

		// This will be the reflective material, we'll make the whole thing 90% reflective
		Material::Sptr monkeyMaterial = ResourceManager::CreateAsset<Material>(reflectiveShader);
		{
			monkeyMaterial->Name = "Monkey";
			monkeyMaterial->Set("u_Material.Diffuse", monkeyTex);
			monkeyMaterial->Set("u_Material.Shininess", 0.5f);
			monkeyMaterial->Set("s_1Dtex", toonLut);
			//monkeyMaterial->Set("offsets", offsets);
		}

		// Create some lights for our scene
		scene->Lights.resize(2);
		scene->Lights[0].Position = glm::vec3(15.0f, 15.0f, 25.0f);
		scene->Lights[0].Color = glm::vec3(1.0f, 1.0f, 0.6f);
		scene->Lights[0].Range = 3000.0f;

		scene->Lights[1].Position = glm::vec3(0.0f, 0.0f, 10.0f);
		scene->Lights[1].Color = glm::vec3(1.0f, 0.6f, 0.0f);
		scene->Lights[1].Range = 100.0f;
		
		// We'll create a mesh that is a simple plane that we can resize later
		MeshResource::Sptr planeMesh = ResourceManager::CreateAsset<MeshResource>();
		planeMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(1.0f)));
		planeMesh->GenerateMesh();

		MeshResource::Sptr sphere = ResourceManager::CreateAsset<MeshResource>();
		sphere->AddParam(MeshBuilderParam::CreateIcoSphere(ZERO, ONE, 5));
		sphere->GenerateMesh();

		// Set up the scene's camera
		GameObject::Sptr camera = scene->MainCamera->GetGameObject()->SelfRef();
		{
			camera->SetPostion({ -9, -6, 15 });
			camera->LookAt(glm::vec3(0.0f));

			camera->Add<SimpleCameraControl>();

			// This is now handled by scene itself!
			//Camera::Sptr cam = camera->Add<Camera>();
			// Make sure that the camera is set as the scene's main camera!
			//scene->MainCamera = cam;
		}

		// Set up all our sample objects
		GameObject::Sptr plane = scene->CreateGameObject("Plane");
		{
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = plane->Add<RenderComponent>();
			renderer->SetMesh(tiledMesh);
			renderer->SetMaterial(sandMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = plane->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		GameObject::Sptr cactus = scene->CreateGameObject("Cactus");
		{
			cactus->SetPostion({ 0.0f, 0.0f, -0.1f });
			cactus->SetRotation({ 90.0f, 0.0f, 0.0f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = cactus->Add<RenderComponent>();
			renderer->SetMesh(cactusMesh);
			renderer->SetMaterial(cactusMaterial);

			RigidBody::Sptr physics = cactus->Add<RigidBody>(/*static by default*/);
		}

		GameObject::Sptr cactus2 = scene->CreateGameObject("Cactus2");
		{
			cactus2->SetPostion({ -6.0f, 3.6f, -0.1f });
			cactus2->SetRotation({ 93.0f, 15.0f, 63.0f });
			cactus2->SetScale({ 1.0f, 1.5f, 1.0f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = cactus2->Add<RenderComponent>();
			renderer->SetMesh(cactusMesh);
			renderer->SetMaterial(cactusMaterial);

			RigidBody::Sptr physics = cactus2->Add<RigidBody>(/*static by default*/);
		}

		GameObject::Sptr ballCactusSmall = scene->CreateGameObject("BallCactusSmall");
		{
			ballCactusSmall->SetPostion({ 0.0f, 0.0f, -0.1f });
			ballCactusSmall->SetRotation({ 90.0f, 0.0f, 0.0f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = ballCactusSmall->Add<RenderComponent>();
			renderer->SetMesh(smallCactusMesh);
			renderer->SetMaterial(ballCactusMaterial);

			RigidBody::Sptr physics = ballCactusSmall->Add<RigidBody>(/*static by default*/);
		}

		GameObject::Sptr ballCactusSmall2 = scene->CreateGameObject("BallCactusSmall2");
		{
			ballCactusSmall2->SetPostion({ -12.2f, -5.9f, -0.3f });
			ballCactusSmall2->SetRotation({ 90.0f, 0.0f, 0.0f });
			ballCactusSmall2->SetScale({ 2.3f, 2.9f, 1.9f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = ballCactusSmall2->Add<RenderComponent>();
			renderer->SetMesh(smallCactusMesh);
			renderer->SetMaterial(ballCactusMaterial);

			RigidBody::Sptr physics = ballCactusSmall2->Add<RigidBody>(/*static by default*/);
		}

		GameObject::Sptr ballCactusBig = scene->CreateGameObject("BallCactusBig");
		{
			ballCactusBig->SetPostion({ 0.0f, 0.0f, -0.1f });
			ballCactusBig->SetRotation({ 90.0f, 0.0f, 0.0f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = ballCactusBig->Add<RenderComponent>();
			renderer->SetMesh(bigCactusMesh);
			renderer->SetMaterial(ballCactusMaterial);

			RigidBody::Sptr physics = ballCactusBig->Add<RigidBody>(/*static by default*/);
		}

		GameObject::Sptr ballCactusBig2 = scene->CreateGameObject("BallCactusBig2");
		{
			ballCactusBig2->SetPostion({ -1.0f, -7.9f, -0.2f });
			ballCactusBig2->SetRotation({ 102.0f, -3.0f, 171.0f });
			ballCactusBig2->SetScale({ 1.6f, 1.2f, 1.1f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = ballCactusBig2->Add<RenderComponent>();
			renderer->SetMesh(bigCactusMesh);
			renderer->SetMaterial(ballCactusMaterial);

			RigidBody::Sptr physics = ballCactusBig2->Add<RigidBody>(/*static by default*/);
		}

		GameObject::Sptr ballCactusBig3 = scene->CreateGameObject("BallCactusBig3");
		{
			ballCactusBig3->SetPostion({ -2.0f, -4.3f, -0.2f });
			ballCactusBig3->SetRotation({ 100.0f, -3.0f, -144.0f });
			ballCactusBig3->SetScale({ 1.6f, 1.8f, 1.1f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = ballCactusBig3->Add<RenderComponent>();
			renderer->SetMesh(bigCactusMesh);
			renderer->SetMaterial(ballCactusMaterial);

			RigidBody::Sptr physics = ballCactusBig3->Add<RigidBody>(/*static by default*/);
		}
		
		GameObject::Sptr snake = scene->CreateGameObject("Snake");
		{
			snake->SetScale({ 2.5f, 2.5f, 2.5f });
			snake->SetRotation({ 90.0f, 0.0f, -95.0f });
			snake->SetPostion({ 1.0f, 1.0f, 0.0f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = snake->Add<RenderComponent>();
			renderer->SetMesh(snakeMesh);
			renderer->SetMaterial(snakeMaterial);

			RigidBody::Sptr physics = snake->Add<RigidBody>(RigidBodyType::Kinematic);

			std::vector<glm::vec3> points{glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(2.8f, 12.8f, 0.0f), glm::vec3(-7.0f, 15.0f, 0.0f), glm::vec3(-10.0f, 4.0f, 0.0f) };

			snake->Add<LerpBehaviour>()->SetParams(points, 5.0f, false);
		}
		
		
		GameObject::Sptr snake2 = scene->CreateGameObject("Snake2");
		{
			//snake2->SetScale({ 1.5f, 1.5f, 1.5f });
			snake2->SetRotation({ 90.0f, 0.0f, 156.0f });
			snake2->SetPostion({ -6.8f, 0.0f, 0.0f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = snake2->Add<RenderComponent>();
			renderer->SetMesh(snakeMesh);
			renderer->SetMaterial(snakeMaterial);

			RigidBody::Sptr physics = snake2->Add<RigidBody>(RigidBodyType::Kinematic);

			std::vector<glm::vec3> points{ glm::vec3(-6.8f, 0.0f, 0.0f), glm::vec3(6.7f, -4.3f, 0.0f), glm::vec3(3.2f, -11.3f, 0.0f) };

			snake2->Add<LerpBehaviour>()->SetParams(points, 4.0f, true);
		}

		GameObject::Sptr rock1 = scene->CreateGameObject("Rock1");
		{
			rock1->SetPostion({ 0.0f, 0.0f, -0.1f });
			rock1->SetScale({ 2.0f, 2.0f, 2.0f });
			rock1->SetRotation({ 90.0f, 0.0f, 0.0f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = rock1->Add<RenderComponent>();
			renderer->SetMesh(rock1Mesh);
			renderer->SetMaterial(rockMaterial);

			RigidBody::Sptr physics = rock1->Add<RigidBody>(/*static by default*/);
		}

		GameObject::Sptr rock2 = scene->CreateGameObject("Rock2");
		{
			rock2->SetPostion({ 0.0f, 0.0f, -0.1f });
			rock2->SetScale({ 1.6f, 1.6f, 1.6f });
			rock2->SetRotation({ 90.0f, 0.0f, 0.0f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = rock2->Add<RenderComponent>();
			renderer->SetMesh(rock2Mesh);
			renderer->SetMaterial(rockMaterial);

			RigidBody::Sptr physics = rock2->Add<RigidBody>(/*static by default*/);
		}

		GameObject::Sptr rock3 = scene->CreateGameObject("Rock3");
		{
			rock3->SetPostion({ -7.8f, 11.5f, -0.1f });
			rock3->SetScale({ 1.4f, 1.5f, 1.85f });
			rock3->SetRotation({ 90.0f, 0.0f, 0.0f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = rock3->Add<RenderComponent>();
			renderer->SetMesh(rock1Mesh);
			renderer->SetMaterial(rockMaterial);

			RigidBody::Sptr physics = rock3->Add<RigidBody>(/*static by default*/);
		}

		GameObject::Sptr plank1 = scene->CreateGameObject("Plank1");
		{
			plank1->SetPostion({ -4.0f, -0.35f, 0.6f });
			plank1->SetScale({ 1.5f, 1.5f, 1.5f });
			plank1->SetRotation({ 90.0f, 0.0f, 0.0f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = plank1->Add<RenderComponent>();
			renderer->SetMesh(plank1Mesh);
			renderer->SetMaterial(plankMaterial);

			RigidBody::Sptr physics = plank1->Add<RigidBody>(/*static by default*/);
		}

		GameObject::Sptr plank2 = scene->CreateGameObject("Plank2");
		{
			plank2->SetPostion({ -4.2f, 0.5f, 0.5f });
			plank2->SetScale({ 1.5f, 1.5f, 1.5f });
			plank2->SetRotation({ 90.0f, 0.0f, 0.0f });

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = plank2->Add<RenderComponent>();
			renderer->SetMesh(plank2Mesh);
			renderer->SetMaterial(plankMaterial);

			RigidBody::Sptr physics = plank2->Add<RigidBody>(/*static by default*/);
		}

		/*
		GameObject::Sptr demoBase = scene->CreateGameObject("Demo Parent");

		// Box to showcase the specular material
		GameObject::Sptr specBox = scene->CreateGameObject("Specular Object");
		{
			MeshResource::Sptr boxMesh = ResourceManager::CreateAsset<MeshResource>();
			boxMesh->AddParam(MeshBuilderParam::CreateCube(ZERO, ONE));
			boxMesh->GenerateMesh();

			// Set and rotation position in the scene
			specBox->SetPostion(glm::vec3(0, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = specBox->Add<RenderComponent>();
			renderer->SetMesh(boxMesh);
			renderer->SetMaterial(testMaterial);

			demoBase->AddChild(specBox);
		}

		// sphere to showcase the foliage material
		GameObject::Sptr foliageBall = scene->CreateGameObject("Foliage Sphere");
		{
			// Set and rotation position in the scene
			foliageBall->SetPostion(glm::vec3(-4.0f, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = foliageBall->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(foliageMaterial);

			demoBase->AddChild(foliageBall);
		}

		// Box to showcase the foliage material
		GameObject::Sptr foliageBox = scene->CreateGameObject("Foliage Box");
		{
			MeshResource::Sptr box = ResourceManager::CreateAsset<MeshResource>();
			box->AddParam(MeshBuilderParam::CreateCube(glm::vec3(0, 0, 0.5f), ONE));
			box->GenerateMesh();

			// Set and rotation position in the scene
			foliageBox->SetPostion(glm::vec3(-6.0f, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = foliageBox->Add<RenderComponent>();
			renderer->SetMesh(box);
			renderer->SetMaterial(foliageMaterial);

			demoBase->AddChild(foliageBox);
		}

		// Box to showcase the specular material
		GameObject::Sptr toonBall = scene->CreateGameObject("Toon Object");
		{
			// Set and rotation position in the scene
			toonBall->SetPostion(glm::vec3(-2.0f, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = toonBall->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(toonMaterial);

			demoBase->AddChild(toonBall);
		}

		GameObject::Sptr displacementBall = scene->CreateGameObject("Displacement Object");
		{
			// Set and rotation position in the scene
			displacementBall->SetPostion(glm::vec3(2.0f, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = displacementBall->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(displacementTest);

			demoBase->AddChild(displacementBall);
		}

		GameObject::Sptr multiTextureBall = scene->CreateGameObject("Multitextured Object");
		{
			// Set and rotation position in the scene 
			multiTextureBall->SetPostion(glm::vec3(4.0f, -4.0f, 1.0f));

			// Add a render component 
			RenderComponent::Sptr renderer = multiTextureBall->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(multiTextureMat);

			demoBase->AddChild(multiTextureBall);
		}

		GameObject::Sptr normalMapBall = scene->CreateGameObject("Normal Mapped Object");
		{
			// Set and rotation position in the scene 
			normalMapBall->SetPostion(glm::vec3(6.0f, -4.0f, 1.0f));

			// Add a render component 
			RenderComponent::Sptr renderer = normalMapBall->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(normalmapMat);

			demoBase->AddChild(normalMapBall);
		}
		*/

		// Create a trigger volume for testing how we can detect collisions with objects!
		GameObject::Sptr trigger = scene->CreateGameObject("Trigger");
		{
			TriggerVolume::Sptr volume = trigger->Add<TriggerVolume>();
			CylinderCollider::Sptr collider = CylinderCollider::Create(glm::vec3(3.0f, 3.0f, 1.0f));
			collider->SetPosition(glm::vec3(0.0f, 0.0f, 0.5f));
			volume->AddCollider(collider);

			trigger->Add<TriggerVolumeEnterBehaviour>();
		}

		/////////////////////////// UI //////////////////////////////
		/*
		GameObject::Sptr canvas = scene->CreateGameObject("UI Canvas");
		{
			RectTransform::Sptr transform = canvas->Add<RectTransform>();
			transform->SetMin({ 16, 16 });
			transform->SetMax({ 256, 256 });

			GuiPanel::Sptr canPanel = canvas->Add<GuiPanel>();


			GameObject::Sptr subPanel = scene->CreateGameObject("Sub Item");
			{
				RectTransform::Sptr transform = subPanel->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 128, 128 });

				GuiPanel::Sptr panel = subPanel->Add<GuiPanel>();
				panel->SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

				panel->SetTexture(ResourceManager::CreateAsset<Texture2D>("textures/upArrow.png"));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 16.0f);
				font->Bake();

				GuiText::Sptr text = subPanel->Add<GuiText>();
				text->SetText("Hello world!");
				text->SetFont(font);

				monkey1->Get<JumpBehaviour>()->Panel = text;
			}

			canvas->AddChild(subPanel);
		}
		*/

		GuiBatcher::SetDefaultTexture(ResourceManager::CreateAsset<Texture2D>("textures/ui-sprite.png"));
		GuiBatcher::SetDefaultBorderRadius(8);

		// Save the asset manifest for all the resources we just loaded
		ResourceManager::SaveManifest("scene-manifest.json");
		// Save the scene to a JSON file
		scene->Save("scene.json");

		// Send the scene to the application
		app.LoadScene(scene);
	}
}
