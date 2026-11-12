#include "physics.hpp"

#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace physics {

namespace {

constexpr float PlayerHalfHeight = 0.55f;
constexpr float PlayerRadius = 0.35f;
constexpr int MaxStepsPerTick = 4;
constexpr float MinimumReach = 0.1f;
constexpr float PlayerMass = 80.0f;
constexpr float PlayerCatchUp = 1.0f;
constexpr float PlayerLag = 1.0f;

bool hitOf(uint64_t data, Hit& hit, int& hitid) {
	hitid = indexOf(data);
	switch (kindOf(data)) {
	case Kind::Map:
		hit = Hit::Map;
		return true;
	case Kind::Object:
		hit = Hit::Object;
		return true;
	case Kind::Static:
		hit = Hit::Static;
		return true;
	case Kind::Player:
		hit = Hit::Player;
		return true;
	case Kind::Vehicle:
		hit = Hit::Vehicle;
		return true;
	case Kind::Actor:
		hit = Hit::Actor;
		return true;
	case Kind::Zone:
		return false;
	}
	return false;
}

void queueCollision(const JPH::Body& self, const JPH::Body& other, JPH::RVec3Arg point, float speed) {
	if (kindOf(self.GetUserData()) != Kind::Object) {
		return;
	}
	const Body* body = findBody(indexOf(self.GetUserData()));
	if (body == nullptr || speed < body->collisionSpeed) {
		return;
	}
	Collision collision;
	if (!hitOf(other.GetUserData(), collision.hit, collision.hitid)) {
		return;
	}
	collision.phys = indexOf(self.GetUserData());
	collision.id = self.GetID();
	collision.point = Vector3(float(point.GetX()), float(point.GetY()), float(point.GetZ()));
	collision.speed = speed;
	State& s = state();
	std::lock_guard<std::mutex> guard(s.eventLock);
	s.collisions.push_back(collision);
}

void queueCrossing(const JPH::BodyID& first, uint64_t firstData, const JPH::BodyID& second, uint64_t secondData, bool entered) {
	if (kindOf(firstData) != Kind::Zone) {
		if (kindOf(secondData) == Kind::Zone) {
			queueCrossing(second, secondData, first, firstData, entered);
		}
		return;
	}
	if (kindOf(secondData) != Kind::Object) {
		return;
	}
	State& s = state();
	std::lock_guard<std::mutex> guard(s.eventLock);
	s.crossings.push_back({ indexOf(firstData), first, indexOf(secondData), second, entered });
}

class Contacts final : public JPH::ContactListener {
public:
	void OnContactAdded(const JPH::Body& first, const JPH::Body& second, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override {
		if (first.IsSensor() || second.IsSensor()) {
			queueCrossing(first.GetID(), first.GetUserData(), second.GetID(), second.GetUserData(), true);
			return;
		}
		JPH::RVec3 point = manifold.GetWorldSpaceContactPointOn1(0);
		float speed = (first.GetPointVelocity(point) - second.GetPointVelocity(point)).Dot(manifold.mWorldSpaceNormal);
		queueCollision(first, second, point, speed);
		queueCollision(second, first, point, speed);
	}

	void OnContactRemoved(const JPH::SubShapeIDPair& pair) override {
		uint64_t first = recalledData(pair.GetBody1ID());
		uint64_t second = recalledData(pair.GetBody2ID());
		if (kindOf(first) == Kind::Zone || kindOf(second) == Kind::Zone) {
			queueCrossing(pair.GetBody1ID(), first, pair.GetBody2ID(), second, false);
		}
	}
};

class Activation final : public JPH::BodyActivationListener {
public:
	void OnBodyActivated(const JPH::BodyID& id, JPH::uint64 data) override {
	}

	void OnBodyDeactivated(const JPH::BodyID& id, JPH::uint64 data) override {
		if (kindOf(data) != Kind::Object) {
			return;
		}
		State& s = state();
		std::lock_guard<std::mutex> guard(s.eventLock);
		s.stops.push_back({ indexOf(data), id });
	}
};

Contacts contacts;
Activation activation;

void createPlayerBody(int playerid, Vector3 position, int world, int interior) {
	State& s = state();
	JPH::BodyCreationSettings settings(pedShape(), JPH::RVec3(position.x, position.y, position.z), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, playerLayer());
	settings.mUserData = userData(Kind::Player, playerid);
	settings.mRestitution = 1.0f;
	settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;
	settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
	settings.mMassPropertiesOverride.mMass = PlayerMass;
	settings.mGravityFactor = 0.0f;
	settings.mLinearDamping = 0.0f;
	settings.mAllowSleeping = false;
	JPH::BodyID id = addBody(settings, world, interior, JPH::EActivation::Activate);
	s.playerBodies[playerid] = id;
	s.playerWorlds[playerid] = world;
	s.playerInteriors[playerid] = interior;
}

void updatePlayers(float elapsed) {
	State& s = state();
	if (s.players == nullptr) {
		return;
	}
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	for (IPlayer* player : s.players->entries()) {
		int playerid = player->getID();
		if (playerid >= int(s.playerBodies.size())) {
			s.playerBodies.resize(playerid + 1);
			s.playerWorlds.resize(playerid + 1, -1);
			s.playerInteriors.resize(playerid + 1, -1);
			s.playerSeen.resize(playerid + 1);
			s.playerSeenAt.resize(playerid + 1, 0.0);
		}
		JPH::BodyID& id = s.playerBodies[playerid];
		if (player->getState() != PlayerState_OnFoot) {
			removePlayer(playerid);
			continue;
		}
		Vector3 position = player->getPosition();
		int world = player->getVirtualWorld();
		int interior = int(player->getInterior());
		if (!id.IsInvalid() && (s.playerWorlds[playerid] != world || s.playerInteriors[playerid] != interior)) {
			removePlayer(playerid);
		}
		if (position != s.playerSeen[playerid]) {
			s.playerSeen[playerid] = position;
			s.playerSeenAt[playerid] = s.now;
		}
		if (id.IsInvalid()) {
			createPlayerBody(playerid, position, world, interior);
			continue;
		}
		JPH::RVec3 target = carriedPosition(position, player->getVelocity(), s.now - s.playerSeenAt[playerid]);
		JPH::RVec3 now = bodies.GetPosition(id);
		if ((now - target).Length() > PlayerLag) {
			bodies.SetPosition(id, target, JPH::EActivation::Activate);
			bodies.SetLinearVelocity(id, JPH::Vec3::sZero());
			continue;
		}
		JPH::Vec3 want = JPH::Vec3(target - now) / elapsed;
		float limit = glm::length(player->getVelocity()) * GameVelocity + PlayerCatchUp;
		if (want.Length() > limit) {
			want = want.Normalized() * limit;
		}
		bodies.SetLinearVelocity(id, want);
	}
}

void applyAccelerations() {
	State& s = state();
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	for (int index : s.accelerated) {
		const Body* body = findBody(index);
		if (body != nullptr) {
			bodies.AddForce(body->id, body->acceleration * body->mass);
		}
	}
}

void applyRollingFriction(float elapsed) {
	State& s = state();
	if (s.rolling.empty()) {
		return;
	}
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	for (int index : s.rolling) {
		const Body* body = findBody(index);
		if (body == nullptr || !bodies.IsActive(body->id)) {
			continue;
		}
		const JPH::BodyID& id = body->id;
		JPH::Vec3 linear;
		JPH::Vec3 angular;
		bodies.GetLinearAndAngularVelocity(id, linear, angular);
		float flat = std::sqrt(linear.GetX() * linear.GetX() + linear.GetY() * linear.GetY());
		if (flat <= 0.0f) {
			continue;
		}
		float scale = std::max(flat - body->rollingFriction * elapsed, 0.0f) / flat;
		linear.SetX(linear.GetX() * scale);
		linear.SetY(linear.GetY() * scale);
		bodies.SetLinearAndAngularVelocity(id, linear, angular * scale);
	}
}

void dispatchEvents() {
	State& s = state();
	std::vector<Collision> collisions;
	std::vector<Stop> stops;
	std::vector<Crossing> crossings;
	{
		std::lock_guard<std::mutex> guard(s.eventLock);
		collisions.swap(s.collisions);
		stops.swap(s.stops);
		crossings.swap(s.crossings);
	}
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	for (const Crossing& crossing : crossings) {
		Body* zone = findBody(crossing.zone);
		Body* body = findBody(crossing.phys);
		if (zone == nullptr || zone->id != crossing.zoneId || body == nullptr || body->id != crossing.physId) {
			continue;
		}
		Script* script = ownerOf(*zone);
		if (script != nullptr) {
			callPublic(*script, crossing.entered ? "A3D_OnEntered" : "A3D_OnLeft", { cell(crossing.zone), cell(crossing.phys) });
		}
	}
	for (const Stop& stop : stops) {
		Body* body = findBody(stop.phys);
		if (body == nullptr || body->id != stop.id || bodies.IsActive(body->id)) {
			continue;
		}
		syncRest(*body);
		Script* script = ownerOf(*body);
		if (script == nullptr) {
			continue;
		}
		JPH::RVec3 position = bodies.GetPosition(body->id);
		float x = float(position.GetX());
		float y = float(position.GetY());
		float z = float(position.GetZ());
		callPublic(*script, "A3D_OnStopped", { cell(stop.phys), amx_ftoc(x), amx_ftoc(y), amx_ftoc(z) });
	}
	for (const Collision& collision : collisions) {
		Body* body = findBody(collision.phys);
		if (body == nullptr || body->id != collision.id) {
			continue;
		}
		Script* script = ownerOf(*body);
		if (script == nullptr) {
			continue;
		}
		float x = collision.point.x;
		float y = collision.point.y;
		float z = collision.point.z;
		float speed = collision.speed;
		callPublic(*script, "A3D_OnCollided", { cell(collision.phys), cell(collision.hit), cell(collision.hitid), amx_ftoc(x), amx_ftoc(y), amx_ftoc(z), amx_ftoc(speed) });
	}
}

}

JPH::RVec3 carriedPosition(Vector3 position, Vector3 velocity, double since) {
	float lead = float(std::min(std::max(since, 0.0), SyncLead));
	Vector3 ahead = position + velocity * (GameVelocity * lead);
	return JPH::RVec3(ahead.x, ahead.y, ahead.z);
}

JPH::ShapeRefC pedShape() {
	State& s = state();
	if (s.playerShape == nullptr) {
		JPH::Ref<JPH::Shape> capsule = new JPH::CapsuleShape(PlayerHalfHeight, PlayerRadius, surfaceMaterial(SurfacePed));
		JPH::RotatedTranslatedShapeSettings upright(JPH::Vec3::sZero(), JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI), capsule);
		s.playerShape = upright.Create().Get();
	}
	return s.playerShape;
}

bool callPublic(Script& script, const char* name, const std::vector<cell>& arguments) {
	int index = -1;
	if (script.api == nullptr || script.api->FindPublic(name, &index) != AMX_ERR_NONE) {
		return false;
	}
	for (auto argument = arguments.rbegin(); argument != arguments.rend(); ++argument) {
		script.api->Push(*argument);
	}
	cell result;
	script.api->Exec(&result, index);
	return true;
}

Script* ownerOf(const Body& body) {
	State& s = state();
	auto found = s.scripts.find(body.owner);
	return found == s.scripts.end() ? nullptr : &found->second;
}

void listen() {
	State& s = state();
	s.system->SetContactListener(&contacts);
	s.system->SetBodyActivationListener(&activation);
}

Body* findBody(int index) {
	State& s = state();
	if (index < 0 || index >= int(s.bodies.size()) || !s.bodies[index].used) {
		return nullptr;
	}
	return &s.bodies[index];
}

int createBody(AMX* owner, const JPH::Shape* shape, int modelid, Vector3 position, JPH::QuatArg rotation, float mass, int world, int interior, Kind kind, const ModelSettings* model) {
	State& s = state();
	if (!finite(position) || rotation.IsNaN() || !finite(mass)) {
		return InvalidPhys;
	}
	int index;
	if (!s.freeBodies.empty()) {
		index = s.freeBodies.back();
		s.freeBodies.pop_back();
	} else {
		index = int(s.bodies.size());
		s.bodies.emplace_back();
	}
	bool dynamic = kind == Kind::Object;
	bool zone = kind == Kind::Zone;
	if (!dynamic) {
		model = nullptr;
	}
	JPH::EMotionType motion = dynamic ? JPH::EMotionType::Dynamic : zone ? JPH::EMotionType::Kinematic : JPH::EMotionType::Static;
	JPH::BodyCreationSettings settings(shape, JPH::RVec3(position.x, position.y, position.z), rotation, motion, dynamic ? movingLayer() : staticLayer());
	settings.mUserData = userData(kind, index);
	settings.mFriction = model != nullptr ? model->friction.value_or(0.5f) : 0.5f;
	settings.mRestitution = !dynamic ? 1.0f : model != nullptr ? model->bounce.value_or(0.3f) : 0.3f;
	settings.mIsSensor = zone;
	if (dynamic) {
		settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
		settings.mMassPropertiesOverride.mMass = mass > 0.0f ? mass : 1.0f;
		settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
		settings.mLinearDamping = model != nullptr ? model->linearDamping.value_or(0.1f) : 0.1f;
		settings.mAngularDamping = model != nullptr ? model->angularDamping.value_or(0.3f) : 0.3f;
	}
	JPH::BodyID id = addBody(settings, world, interior, dynamic || zone ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	if (id.IsInvalid()) {
		s.freeBodies.push_back(index);
		return InvalidPhys;
	}
	Body& body = s.bodies[index];
	body = Body();
	body.id = id;
	body.used = true;
	body.kind = kind;
	body.modelid = modelid;
	body.world = world;
	body.interior = interior;
	body.owner = owner;
	body.mass = dynamic ? settings.mMassPropertiesOverride.mMass : 0.0f;
	body.collisionSpeed = s.settings.collisionSpeed;
	body.from = position;
	body.to = position;
	body.rotation = toEuler(rotation);
	body.turnedFrom = body.rotation;
	body.heading = rotation;
	body.reach = std::max(shape->GetLocalBounds().GetExtent().Length(), MinimumReach);
	body.syncError = s.settings.syncError;
	if (model != nullptr) {
		body.rollingFriction = model->rollingFriction.value_or(0.0f);
		body.syncError = model->syncError.value_or(body.syncError);
	}
	if (body.rollingFriction > 0.0f) {
		s.rolling.push_back(index);
	}
	showBody(index);
	if (model != nullptr && model->buoyancy.value_or(0.0f) > 0.0f) {
		setBuoyancy(index, *model->buoyancy, model->drag);
	}
	return index;
}

bool destroyBody(int index) {
	State& s = state();
	Body* body = findBody(index);
	if (body == nullptr) {
		return false;
	}
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	if (body->kind != Kind::Zone) {
		wakeAround(body->id);
	}
	bodies.RemoveBody(body->id);
	bodies.DestroyBody(body->id);
	rememberBody(body->id, 0);
	hideBody(*body);
	*body = Body();
	s.accelerated.erase(std::remove(s.accelerated.begin(), s.accelerated.end(), index), s.accelerated.end());
	s.rolling.erase(std::remove(s.rolling.begin(), s.rolling.end(), index), s.rolling.end());
	s.floating.erase(std::remove(s.floating.begin(), s.floating.end(), index), s.floating.end());
	s.freeBodies.push_back(index);
	return true;
}

void regroupBody(Body& body) {
	State& s = state();
	{
		JPH::BodyLockWrite lock(s.system->GetBodyLockInterface(), body.id);
		if (lock.Succeeded()) {
			s.bodyContexts[body.id.GetIndex()] = { body.world, body.interior };
			lock.GetBody().SetCollisionGroup(bodyGroup(body.id, body.kind, body.collisions));
		}
	}
	s.system->GetBodyInterface().InvalidateContactCache(body.id);
	wakeAround(body.id);
}

void setAcceleration(int index, JPH::Vec3Arg acceleration) {
	State& s = state();
	Body* body = findBody(index);
	if (body == nullptr) {
		return;
	}
	body->acceleration = acceleration;
	s.accelerated.erase(std::remove(s.accelerated.begin(), s.accelerated.end(), index), s.accelerated.end());
	if (!acceleration.IsNearZero()) {
		s.accelerated.push_back(index);
	}
}

void forgetScript(AMX* amx) {
	State& s = state();
	for (int index = 0; index != int(s.bodies.size()); ++index) {
		if (s.bodies[index].used && s.bodies[index].owner == amx) {
			destroyBody(index);
		}
	}
}

void removePlayer(int playerid) {
	State& s = state();
	if (playerid < 0 || playerid >= int(s.playerBodies.size()) || s.playerBodies[playerid].IsInvalid()) {
		return;
	}
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	bodies.RemoveBody(s.playerBodies[playerid]);
	bodies.DestroyBody(s.playerBodies[playerid]);
	rememberBody(s.playerBodies[playerid], 0);
	s.playerBodies[playerid] = JPH::BodyID();
	s.playerWorlds[playerid] = -1;
	s.playerInteriors[playerid] = -1;
}

void step(double elapsed) {
	State& s = state();
	if (s.restarted || !s.orphans.empty()) {
		reshowBodies();
	}
	s.now += elapsed;
	s.accumulator = std::min(s.accumulator + elapsed, 0.25);
	double interval = 1.0 / double(s.settings.rate);
	int steps = 0;
	while (s.accumulator >= interval && steps != MaxStepsPerTick) {
		s.accumulator -= interval;
		++steps;
	}
	if (steps == 0) {
		return;
	}
	auto start = std::chrono::steady_clock::now();
	float delta = float(steps * interval);
	updatePlayers(delta);
	updateVehicles(delta);
	updateActors(delta);
	applyAccelerations();
	applyBuoyancy(delta);
	s.system->Update(delta, steps, s.temp.get(), s.jobs.get());
	applyRollingFriction(delta);
	syncBodies();
	s.milliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	dispatchEvents();
}

}
