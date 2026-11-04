#include "physics.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>

namespace physics {

namespace {

constexpr float ActorTeleport = 10.0f;

void createActorBody(int actorid, Vector3 position, int world) {
	State& s = state();
	JPH::BodyCreationSettings settings(pedShape(), JPH::RVec3(position.x, position.y, position.z), JPH::Quat::sIdentity(), JPH::EMotionType::Kinematic, playerLayer());
	settings.mUserData = userData(Kind::Actor, actorid);
	settings.mRestitution = 1.0f;
	JPH::BodyID id = addBody(settings, world, -1, JPH::EActivation::DontActivate);
	s.actorBodies[actorid] = id;
	s.actorWorlds[actorid] = world;
	s.actorPositions[actorid] = position;
	s.actorMoving[actorid] = 0;
}

}

void updateActors(float elapsed) {
	State& s = state();
	if (s.actors == nullptr) {
		return;
	}
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	for (IActor* actor : *s.actors) {
		int actorid = actor->getID();
		if (actorid >= int(s.actorBodies.size())) {
			s.actorBodies.resize(actorid + 1);
			s.actorWorlds.resize(actorid + 1, -1);
			s.actorPositions.resize(actorid + 1);
			s.actorMoving.resize(actorid + 1, 0);
		}
		int world = actor->getVirtualWorld();
		JPH::BodyID& id = s.actorBodies[actorid];
		if (!id.IsInvalid() && s.actorWorlds[actorid] != world) {
			removeActor(actorid);
		}
		Vector3 position = actor->getPosition();
		if (id.IsInvalid()) {
			createActorBody(actorid, position, world);
			continue;
		}
		JPH::RVec3 target(position.x, position.y, position.z);
		if (position == s.actorPositions[actorid]) {
			if (s.actorMoving[actorid] != 0) {
				s.actorMoving[actorid] = 0;
				bodies.MoveKinematic(id, target, JPH::Quat::sIdentity(), elapsed);
			}
			continue;
		}
		float distance = glm::distance(position, s.actorPositions[actorid]);
		s.actorPositions[actorid] = position;
		if (distance > ActorTeleport) {
			s.actorMoving[actorid] = 0;
			bodies.SetPosition(id, target, JPH::EActivation::DontActivate);
			continue;
		}
		s.actorMoving[actorid] = 1;
		bodies.MoveKinematic(id, target, JPH::Quat::sIdentity(), elapsed);
	}
}

void removeActor(int actorid) {
	State& s = state();
	if (actorid < 0 || actorid >= int(s.actorBodies.size()) || s.actorBodies[actorid].IsInvalid()) {
		return;
	}
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	bodies.RemoveBody(s.actorBodies[actorid]);
	bodies.DestroyBody(s.actorBodies[actorid]);
	rememberBody(s.actorBodies[actorid], 0);
	s.actorBodies[actorid] = JPH::BodyID();
	s.actorWorlds[actorid] = -1;
}

}
