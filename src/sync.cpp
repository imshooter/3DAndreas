#include "physics.hpp"

#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

#include <algorithm>
#include <cmath>

namespace physics {

namespace {

constexpr float StillSpeed = 0.05f;
constexpr float GrazeCosine = 0.1f;
constexpr float StillMoveTime = 0.1f;
constexpr float MaximumTurn = 0.5f;
constexpr float Nudge = 0.003f;
constexpr float MaximumAcceleration = 25.0f;
constexpr double FreshVelocity = 0.2;
constexpr float FreeFallLow = 5.0f;
constexpr float FreeFallHigh = 15.0f;
constexpr double BurstTime = 0.25;
constexpr float RestMoveTime = 0.1f;
constexpr float RestTurn = 0.02f;
constexpr float SagShare = 0.8f;

struct Due {
	JPH::BodyID id;
	Body* body;
	float urgency;
	bool turned;
	bool moving;
	JPH::Vec3 acceleration;
};

class FirstImpact final : public JPH::CastShapeCollector {
public:
	explicit FirstImpact(JPH::Vec3Arg motion) : heading_(motion.Normalized()) {
	}

	void AddHit(const JPH::ShapeCastResult& result) override {
		if (result.mFraction >= GetEarlyOutFraction() || result.mPenetrationAxis.IsNearZero()) {
			return;
		}
		if (heading_.Dot(result.mPenetrationAxis.Normalized()) <= GrazeCosine) {
			return;
		}
		share = result.mFraction;
		UpdateEarlyOutFraction(result.mFraction);
	}

	float share = 1.0f;

private:
	JPH::Vec3 heading_;
};

float freeShare(const JPH::BodyID& id, const Body& body, JPH::Vec3Arg motion) {
	State& s = state();
	if (motion.IsNearZero()) {
		return 1.0f;
	}
	const JPH::BodyInterface& bodies = s.system->GetBodyInterfaceNoLock();
	JPH::RefConst<JPH::Shape> shape = bodies.GetShape(id);
	JPH::RMat44 transform = bodies.GetWorldTransform(id);
	JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(shape, JPH::Vec3::sReplicate(1.0f), transform, motion);
	JPH::ShapeCastSettings settings;
	FirstImpact impact(motion);
	QueryFilter filter(body.world, body.collisions, body.interior, id);
	s.system->GetNarrowPhaseQuery().CastShape(cast, settings, transform.GetTranslation(), impact, {}, {}, filter);
	return impact.share;
}

Vector3 clientPosition(const Body& body) {
	State& s = state();
	if (body.speed <= 0.0f || body.duration <= 0.0) {
		return body.to;
	}
	double progress = std::min(std::max((s.now - body.sentAt) / body.duration, 0.0), 1.0);
	return body.from + (body.to - body.from) * float(progress);
}

float clientProgress(const Body& body) {
	State& s = state();
	if (body.speed <= 0.0f || body.duration <= 0.0) {
		return 1.0f;
	}
	return float(std::min(std::max((s.now - body.sentAt) / body.duration, 0.0), 1.0));
}

Vector3 clientRotation(const Body& body) {
	State& s = state();
	if (body.speed <= 0.0f || body.duration <= 0.0) {
		return body.rotation;
	}
	double progress = std::min(std::max((s.now - body.sentAt) / body.duration, 0.0), 1.0);
	return body.turnedFrom + (body.rotation - body.turnedFrom) * float(progress);
}

float angleBetween(JPH::QuatArg first, JPH::QuatArg second) {
	return 2.0f * std::acos(std::min(std::abs(first.Dot(second)), 1.0f));
}

float settle(JPH::Vec3Arg velocity, JPH::Vec3Arg acceleration, float span) {
	float slowing = velocity.Dot(acceleration);
	return slowing < 0.0f ? std::min(span, velocity.LengthSq() / -slowing) : span;
}

JPH::Vec3 travel(JPH::Vec3Arg velocity, JPH::Vec3Arg acceleration, float span) {
	return velocity * span + acceleration * (0.5f * span * span);
}

float nearestAngle(float angle, float previous) {
	while (angle - previous > 180.0f) {
		angle -= 360.0f;
	}
	while (angle - previous < -180.0f) {
		angle += 360.0f;
	}
	return angle;
}

Vector3 nearestEuler(Vector3 euler, Vector3 previous) {
	return Vector3(nearestAngle(euler.x, previous.x), nearestAngle(euler.y, previous.y), nearestAngle(euler.z, previous.z));
}

void callMover(Body& body, Vector3 position, float speed, Vector3 rotation) {
	Script* script = ownerOf(body);
	if (script == nullptr) {
		return;
	}
	callPublic(*script, "@A3D_MoveDynamicObject", { cell(body.objectid), amx_ftoc(position.x), amx_ftoc(position.y), amx_ftoc(position.z), amx_ftoc(speed), amx_ftoc(rotation.x), amx_ftoc(rotation.y), amx_ftoc(rotation.z) });
}

void place(Body& body, Vector3 position, Vector3 rotation) {
	State& s = state();
	if (body.sync == Sync::Object && s.objects != nullptr) {
		IObject* object = s.objects->get(body.objectid);
		if (object == nullptr) {
			return;
		}
		if (object->isMoving()) {
			object->stop();
			++s.updates;
		}
		object->setPosition(position);
		object->setRotation(GTAQuat(rotation));
		s.updates += 2;
	} else if (body.sync == Sync::Script) {
		callMover(body, position, 0.0f, rotation);
		++s.updates;
	}
	body.from = position;
	body.to = position;
	body.offset = 0.0f;
	body.turnedFrom = rotation;
	body.rotation = rotation;
	body.heading = toJolt(rotation);
	body.speed = 0.0f;
	body.duration = 0.0;
	body.sentAt = s.now;
}

void move(Body& body, Vector3 client, Vector3 target, float speed, Vector3 rotation, float horizon) {
	State& s = state();
	Vector3 turnedFrom = clientRotation(body);
	if (body.sync == Sync::Object && s.objects != nullptr) {
		IObject* object = s.objects->get(body.objectid);
		if (object == nullptr) {
			return;
		}
		object->move(ObjectMoveData { target, rotation, speed });
	} else if (body.sync == Sync::Script) {
		callMover(body, target, speed, rotation);
	}
	++s.updates;
	body.from = client;
	body.to = target;
	body.turnedFrom = turnedFrom;
	body.rotation = rotation;
	body.heading = toJolt(rotation);
	body.speed = speed;
	body.duration = horizon;
	body.sentAt = s.now;
}

}

void syncBodies() {
	State& s = state();
	JPH::BodyIDVector active;
	s.system->GetActiveBodies(JPH::EBodyType::RigidBody, active);
	JPH::BodyInterface& bodies = s.system->GetBodyInterfaceNoLock();
	float horizon = s.settings.syncHorizon;
	double rate = double(s.settings.maxUpdates);
	s.budget = std::min(s.budget + rate * std::max(s.now - s.syncedAt, 0.0), rate * BurstTime);
	s.syncedAt = s.now;
	std::vector<Due> due;
	for (const JPH::BodyID& id : active) {
		uint64_t data = bodies.GetUserData(id);
		if (kindOf(data) != Kind::Object) {
			continue;
		}
		Body* body = findBody(indexOf(data));
		if (body == nullptr) {
			continue;
		}
		JPH::RVec3 position = bodies.GetPosition(id);
		if (position.GetZ() < s.settings.floor) {
			bodies.SetLinearAndAngularVelocity(id, JPH::Vec3::sZero(), JPH::Vec3::sZero());
			bodies.DeactivateBody(id);
			continue;
		}
		if (body->sync == Sync::None) {
			continue;
		}
		JPH::Vec3 velocity = bodies.GetLinearVelocity(id);
		Vector3 current(float(position.GetX()), float(position.GetY()), float(position.GetZ()));
		JPH::Vec3 spin = bodies.GetAngularVelocity(id);
		bool arrived = s.now >= body->sentAt + body->duration;
		bool moving = velocity.Length() > StillSpeed;
		JPH::Vec3 braking = JPH::Vec3::sZero();
		float falling = 0.0f;
		double since = s.now - body->velocityAt;
		if (body->velocityAt >= 0.0 && since > 0.0 && since < FreshVelocity) {
			JPH::Vec3 change = (velocity - body->lastVelocity) / float(since);
			if (change.GetZ() > -FreeFallHigh && change.GetZ() < -FreeFallLow) {
				falling = change.GetZ();
			}
			change.SetZ(0.0f);
			if (change.Length() <= MaximumAcceleration) {
				braking = (change + body->braking) * 0.5f;
			}
		}
		body->braking = braking;
		if (JPH::Vec3(velocity.GetX(), velocity.GetY(), 0.0f).Dot(braking) >= 0.0f) {
			braking = JPH::Vec3::sZero();
		}
		body->lastVelocity = velocity;
		body->velocityAt = s.now;
		JPH::Quat ahead = bodies.GetRotation(id);
		double remaining = std::max(body->sentAt + body->duration - s.now, 0.0);
		if (spin.Length() > 0.001f) {
			ahead = (JPH::Quat::sRotation(spin.Normalized(), spin.Length() * float(remaining)) * ahead).Normalized();
		}
		float allowed = body->syncError + body->offset * (1.0f - clientProgress(*body));
		float strayed = glm::distance(current, clientPosition(*body)) / allowed;
		float turned = angleBetween(ahead, body->heading) / std::min(body->syncError / body->reach, MaximumTurn);
		float urgency = std::max(std::max(strayed, turned), arrived && moving ? 1.0f : 0.0f);
		if (!braking.IsNearZero() && remaining > 0.0) {
			JPH::Vec3 stop = travel(velocity, braking, settle(velocity, braking, float(remaining)));
			Vector3 stopAt = current + Vector3(stop.GetX(), stop.GetY(), 0.0f);
			urgency = std::max(urgency, glm::distance(Vector3(stopAt.x, stopAt.y, 0.0f), Vector3(body->to.x, body->to.y, 0.0f)) / body->syncError);
		}
		if (urgency < 1.0f) {
			continue;
		}
		due.push_back({ id, body, urgency, turned >= 1.0f, moving, braking + JPH::Vec3(0.0f, 0.0f, falling) });
	}
	std::sort(due.begin(), due.end(), [](const Due& first, const Due& second) {
		return first.urgency > second.urgency;
	});
	for (const Due& entry : due) {
		if (s.budget < 1.0) {
			break;
		}
		Body* body = entry.body;
		JPH::RVec3 position = bodies.GetPosition(entry.id);
		JPH::Vec3 velocity = bodies.GetLinearVelocity(entry.id);
		JPH::Quat rotation = bodies.GetRotation(entry.id);
		JPH::Vec3 spin = bodies.GetAngularVelocity(entry.id);
		Vector3 current(float(position.GetX()), float(position.GetY()), float(position.GetZ()));
		Vector3 client = clientPosition(*body);
		float span = settle(velocity, entry.acceleration, horizon);
		if (entry.acceleration.GetZ() < 0.0f) {
			span = std::min(span, std::sqrt(8.0f * SagShare * body->syncError / -entry.acceleration.GetZ()));
		}
		JPH::Vec3 motion = travel(velocity, entry.acceleration, span);
		float share = freeShare(entry.id, *body, motion);
		float duration = std::max(span * share, 1.0f / s.settings.rate);
		if (!entry.moving) {
			duration = std::min(duration, StillMoveTime);
		}
		Vector3 target = current + Vector3(motion.GetX(), motion.GetY(), motion.GetZ()) * share;
		if (spin.Length() > 0.001f) {
			rotation = (JPH::Quat::sRotation(spin.Normalized(), spin.Length() * duration) * rotation).Normalized();
		}
		Vector3 euler = nearestEuler(toEuler(rotation), body->rotation);
		float distance = glm::distance(client, target);
		if (distance < Nudge) {
			if (!entry.turned) {
				continue;
			}
			target.z += Nudge;
			distance = glm::distance(client, target);
		}
		body->offset = glm::distance(client, current);
		move(*body, client, target, distance / duration, euler, duration);
		s.budget -= 1.0;
	}
}

void syncRest(Body& body) {
	State& s = state();
	const JPH::BodyInterface& bodies = s.system->GetBodyInterfaceNoLock();
	JPH::RVec3 position = bodies.GetPosition(body.id);
	JPH::Quat rotation = bodies.GetRotation(body.id);
	Vector3 rest(float(position.GetX()), float(position.GetY()), float(position.GetZ()));
	Vector3 euler = nearestEuler(toEuler(rotation), body.rotation);
	Vector3 client = clientPosition(body);
	float distance = glm::distance(client, rest);
	if (distance < Nudge) {
		if (angleBetween(rotation, toJolt(clientRotation(body))) > RestTurn) {
			place(body, rest, euler);
		}
		return;
	}
	body.offset = distance;
	move(body, client, rest, distance / RestMoveTime, euler, RestMoveTime);
	s.budget -= 1.0;
}

void syncTeleport(Body& body) {
	State& s = state();
	const JPH::BodyInterface& bodies = s.system->GetBodyInterfaceNoLock();
	JPH::RVec3 position = bodies.GetPosition(body.id);
	Vector3 euler = nearestEuler(toEuler(bodies.GetRotation(body.id)), body.rotation);
	place(body, Vector3(float(position.GetX()), float(position.GetY()), float(position.GetZ())), euler);
}

void showBody(int index) {
	State& s = state();
	Body* body = findBody(index);
	if (body == nullptr || body->kind != Kind::Object || body->modelid < 0 || body->sync != Sync::None || s.objects == nullptr) {
		return;
	}
	const JPH::BodyInterface& bodies = s.system->GetBodyInterfaceNoLock();
	JPH::RVec3 position = bodies.GetPosition(body->id);
	IObject* object = s.objects->create(body->modelid, Vector3(float(position.GetX()), float(position.GetY()), float(position.GetZ())), toEuler(bodies.GetRotation(body->id)), s.settings.drawDistance);
	if (object == nullptr) {
		return;
	}
	int objectid = object->getID();
	if (objectid >= int(s.objectBodies.size())) {
		s.objectBodies.resize(size_t(objectid) + 1, -1);
	}
	s.objectBodies[size_t(objectid)] = index;
	body->objectid = objectid;
	body->sync = Sync::Object;
}

void hideBody(Body& body) {
	State& s = state();
	if (body.sync != Sync::Object || s.objects == nullptr) {
		return;
	}
	int objectid = body.objectid;
	body.sync = Sync::None;
	body.objectid = -1;
	if (objectid >= 0 && objectid < int(s.objectBodies.size())) {
		s.objectBodies[size_t(objectid)] = -1;
	}
	s.objects->release(objectid);
}

void loseObject(int objectid) {
	State& s = state();
	if (objectid < 0 || objectid >= int(s.objectBodies.size()) || s.objectBodies[size_t(objectid)] < 0) {
		return;
	}
	int index = s.objectBodies[size_t(objectid)];
	s.objectBodies[size_t(objectid)] = -1;
	Body* body = findBody(index);
	if (body == nullptr || body->sync != Sync::Object || body->objectid != objectid) {
		return;
	}
	body->sync = Sync::None;
	body->objectid = -1;
	s.orphans.push_back(index);
}

void reshowBodies() {
	State& s = state();
	if (s.restarted) {
		for (int index : s.orphans) {
			Body* body = findBody(index);
			if (body == nullptr || body->sync != Sync::None) {
				continue;
			}
			showBody(index);
			if (body->sync == Sync::Object) {
				syncTeleport(*body);
			}
		}
	}
	s.orphans.clear();
	s.restarted = false;
}

}
