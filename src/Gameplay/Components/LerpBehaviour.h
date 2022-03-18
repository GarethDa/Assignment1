#pragma once
#include "IComponent.h"
#include "Gameplay/Physics/RigidBody.h"

/// <summary>
/// A simple behaviour that applies an impulse along the Z axis to the 
/// rigidbody of the parent when the space key is pressed
/// </summary>
class LerpBehaviour : public Gameplay::IComponent {
public:
	typedef std::shared_ptr<LerpBehaviour> Sptr;

	std::weak_ptr<Gameplay::IComponent> Panel;

	LerpBehaviour();
	virtual ~LerpBehaviour();

	virtual void Awake() override;
	virtual void Update(float deltaTime) override;

public:
	virtual void RenderImGui() override;
	MAKE_TYPENAME(LerpBehaviour);
	virtual nlohmann::json ToJson() const override;
	static LerpBehaviour::Sptr FromJson(const nlohmann::json& blob);

	void LerpBehaviour::SetParams(std::vector<glm::vec3> inPoints, float inSegmentTime, bool inClockwise);

protected:
	float _impulse;

	std::vector<glm::vec3> points;

	float segmentTime;
	float currentTime = 0.0f;
	int currentInd = 0;
	bool clockwise;

	Gameplay::Physics::RigidBody::Sptr _body;
};