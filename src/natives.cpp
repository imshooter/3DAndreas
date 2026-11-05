#include "physics.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <cmath>

namespace physics {

float readFloat(const cell* params, int index) {
	return amx_ctof(params[index]);
}

cell optional(const cell* params, int index, cell fallback) {
	return params[0] >= cell(index * sizeof(cell)) ? params[index] : fallback;
}

float optionalFloat(const cell* params, int index, float fallback) {
	return params[0] >= cell(index * sizeof(cell)) ? readFloat(params, index) : fallback;
}

Vector3 readVector(const cell* params, int index) {
	return Vector3(amx_ctof(params[index]), amx_ctof(params[index + 1]), amx_ctof(params[index + 2]));
}

cell* addressOf(AMX* amx, cell address) {
	State& s = state();
	IPawnScript* script = s.pawn->getScript(amx);
	cell* target = nullptr;
	if (script == nullptr || script->GetAddr(address, &target) != AMX_ERR_NONE) {
		return nullptr;
	}
	return target;
}

bool writeCell(AMX* amx, cell address, cell value) {
	cell* target = addressOf(amx, address);
	if (target == nullptr) {
		return false;
	}
	*target = value;
	return true;
}

bool writeFloat(AMX* amx, cell address, float value) {
	return writeCell(amx, address, amx_ftoc(value));
}

bool writeVector(AMX* amx, const cell* params, int index, Vector3 value) {
	return writeFloat(amx, params[index], value.x) && writeFloat(amx, params[index + 1], value.y) && writeFloat(amx, params[index + 2], value.z);
}

namespace {

Body* movingBody(const cell* params) {
	Body* body = findBody(params[1]);
	return body != nullptr && body->kind == Kind::Object ? body : nullptr;
}

JPH::BodyInterface& bodies() {
	return state().system->GetBodyInterface();
}

JPH::Ref<JPH::Shape> boxOf(Vector3 size) {
	JPH::Vec3 half(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f);
	return new JPH::BoxShape(half, std::min(JPH::cDefaultConvexRadius, half.ReduceMin()));
}

bool positive(Vector3 size) {
	return size.x > 0.0f && size.y > 0.0f && size.z > 0.0f;
}

cell AMX_NATIVE_CALL A3D_CreateObject(AMX* amx, const cell* params) {
	JPH::ShapeRefC shape = dynamicShape(params[1]);
	if (shape == nullptr) {
		return InvalidPhys;
	}
	const ModelSettings& model = modelValues(params[1], *shape);
	float mass = readFloat(params, 8);
	if (mass <= 0.0f) {
		mass = model.mass.value_or(0.0f);
	}
	return createBody(amx, shape, params[1], readVector(params, 2), toJolt(readVector(params, 5)), mass, params[9], optional(params, 10, -1), Kind::Object, &model);
}

cell AMX_NATIVE_CALL A3D_CreateSphere(AMX* amx, const cell* params) {
	float radius = readFloat(params, 2);
	if (radius <= 0.0f) {
		return InvalidPhys;
	}
	return createBody(amx, new JPH::SphereShape(radius), params[1], readVector(params, 3), JPH::Quat::sIdentity(), readFloat(params, 6), params[7], optional(params, 8, -1), Kind::Object);
}

cell AMX_NATIVE_CALL A3D_CreateBox(AMX* amx, const cell* params) {
	Vector3 size = readVector(params, 2);
	if (!positive(size)) {
		return InvalidPhys;
	}
	return createBody(amx, boxOf(size), params[1], readVector(params, 5), toJolt(readVector(params, 8)), readFloat(params, 11), params[12], optional(params, 13, -1), Kind::Object);
}

cell AMX_NATIVE_CALL A3D_CreateStaticObject(AMX* amx, const cell* params) {
	JPH::ShapeRefC shape = staticShape(params[1]);
	if (shape == nullptr) {
		return InvalidPhys;
	}
	return createBody(amx, shape, params[1], readVector(params, 2), toJolt(readVector(params, 5)), 0.0f, params[8], optional(params, 9, -1), Kind::Static);
}

cell AMX_NATIVE_CALL A3D_CreateStaticBox(AMX* amx, const cell* params) {
	Vector3 size = readVector(params, 1);
	if (!positive(size)) {
		return InvalidPhys;
	}
	return createBody(amx, boxOf(size), -1, readVector(params, 4), toJolt(readVector(params, 7)), 0.0f, params[10], optional(params, 11, -1), Kind::Static);
}

cell AMX_NATIVE_CALL A3D_CreateStaticCylinder(AMX* amx, const cell* params) {
	float radius = readFloat(params, 1);
	float height = readFloat(params, 2);
	if (radius <= 0.0f || height <= 0.0f) {
		return InvalidPhys;
	}
	JPH::Ref<JPH::Shape> cylinder = new JPH::CylinderShape(height * 0.5f, radius, std::min(JPH::cDefaultConvexRadius, std::min(radius, height * 0.5f)));
	JPH::RotatedTranslatedShapeSettings upright(JPH::Vec3::sZero(), JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI), cylinder);
	JPH::ShapeSettings::ShapeResult result = upright.Create();
	if (!result.IsValid()) {
		return InvalidPhys;
	}
	return createBody(amx, result.Get(), -1, readVector(params, 3), toJolt(readVector(params, 6)), 0.0f, params[9], optional(params, 10, -1), Kind::Static);
}

cell AMX_NATIVE_CALL A3D_CreateZone(AMX* amx, const cell* params) {
	Vector3 size = readVector(params, 1);
	if (!positive(size)) {
		return InvalidPhys;
	}
	return createBody(amx, boxOf(size), -1, readVector(params, 4), toJolt(readVector(params, 7)), 0.0f, params[10], optional(params, 11, -1), Kind::Zone);
}

cell AMX_NATIVE_CALL A3D_CreateZoneSphere(AMX* amx, const cell* params) {
	float radius = readFloat(params, 1);
	if (radius <= 0.0f) {
		return InvalidPhys;
	}
	return createBody(amx, new JPH::SphereShape(radius), -1, readVector(params, 2), JPH::Quat::sIdentity(), 0.0f, params[5], optional(params, 6, -1), Kind::Zone);
}

cell AMX_NATIVE_CALL A3D_DestroyObject(AMX* amx, const cell* params) {
	return destroyBody(params[1]);
}

cell AMX_NATIVE_CALL A3D_IsValidObject(AMX* amx, const cell* params) {
	return findBody(params[1]) != nullptr;
}

cell AMX_NATIVE_CALL A3D_SetVelocity(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	if (body == nullptr) {
		return false;
	}
	Vector3 velocity = readVector(params, 2);
	bodies().SetLinearVelocity(body->id, JPH::Vec3(velocity.x, velocity.y, velocity.z));
	bodies().ActivateBody(body->id);
	return true;
}

cell AMX_NATIVE_CALL A3D_GetVelocity(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	if (body == nullptr) {
		return false;
	}
	JPH::Vec3 velocity = bodies().GetLinearVelocity(body->id);
	return writeVector(amx, params, 2, Vector3(velocity.GetX(), velocity.GetY(), velocity.GetZ()));
}

cell AMX_NATIVE_CALL A3D_GetSpeed(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	float speed = body == nullptr ? 0.0f : bodies().GetLinearVelocity(body->id).Length();
	return amx_ftoc(speed);
}

cell AMX_NATIVE_CALL A3D_GetMoveAngle(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	if (body == nullptr) {
		float none = 0.0f;
		return amx_ftoc(none);
	}
	JPH::Vec3 velocity = bodies().GetLinearVelocity(body->id);
	float angle = std::atan2(-velocity.GetX(), velocity.GetY()) * 180.0f / JPH::JPH_PI;
	if (angle < 0.0f) {
		angle += 360.0f;
	}
	return amx_ftoc(angle);
}

cell AMX_NATIVE_CALL A3D_ApplyImpulse(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	if (body == nullptr) {
		return false;
	}
	Vector3 impulse = readVector(params, 2);
	bodies().AddImpulse(body->id, JPH::Vec3(impulse.x, impulse.y, impulse.z));
	return true;
}

cell AMX_NATIVE_CALL A3D_SetAcceleration(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	if (body == nullptr) {
		return false;
	}
	Vector3 acceleration = readVector(params, 2);
	setAcceleration(params[1], JPH::Vec3(acceleration.x, acceleration.y, acceleration.z));
	bodies().ActivateBody(body->id);
	return true;
}

cell AMX_NATIVE_CALL A3D_SetGravity(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	if (body == nullptr) {
		return false;
	}
	JPH::BodyLockWrite lock(state().system->GetBodyLockInterface(), body->id);
	if (!lock.Succeeded()) {
		return false;
	}
	lock.GetBody().GetMotionProperties()->SetGravityFactor(readFloat(params, 2) / StandardGravity);
	return true;
}

cell AMX_NATIVE_CALL A3D_SetRollingFriction(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	float deceleration = readFloat(params, 2);
	if (body == nullptr || deceleration < 0.0f) {
		return false;
	}
	body->rollingFriction = deceleration;
	State& s = state();
	s.rolling.erase(std::remove(s.rolling.begin(), s.rolling.end(), params[1]), s.rolling.end());
	if (deceleration > 0.0f) {
		s.rolling.push_back(params[1]);
	}
	return true;
}

cell AMX_NATIVE_CALL A3D_SetPlanar(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	if (body == nullptr) {
		return false;
	}
	JPH::BodyLockWrite lock(state().system->GetBodyLockInterface(), body->id);
	if (!lock.Succeeded()) {
		return false;
	}
	JPH::Body& locked = lock.GetBody();
	JPH::EAllowedDOFs freedom = params[2] ? JPH::EAllowedDOFs(uint8_t(JPH::EAllowedDOFs::All) & ~uint8_t(JPH::EAllowedDOFs::TranslationZ)) : JPH::EAllowedDOFs::All;
	JPH::MassProperties mass = locked.GetShape()->GetMassProperties();
	mass.ScaleToMass(body->mass);
	locked.GetMotionProperties()->SetMassProperties(freedom, mass);
	return true;
}

cell AMX_NATIVE_CALL A3D_SetCollisions(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	if (body == nullptr) {
		return false;
	}
	body->collisions = uint32_t(params[2]) & CollideAll;
	regroupBody(*body);
	bodies().ActivateBody(body->id);
	return true;
}

cell AMX_NATIVE_CALL A3D_GetCollisions(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	return body == nullptr ? 0 : cell(body->collisions);
}

cell AMX_NATIVE_CALL A3D_SetCollisionSpeed(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	float speed = readFloat(params, 2);
	if (body == nullptr || speed < 0.0f) {
		return false;
	}
	body->collisionSpeed = speed;
	return true;
}

cell AMX_NATIVE_CALL A3D_SetWorld(AMX* amx, const cell* params) {
	Body* body = findBody(params[1]);
	if (body == nullptr) {
		return false;
	}
	body->world = params[2];
	regroupBody(*body);
	if (body->kind == Kind::Object) {
		bodies().ActivateBody(body->id);
	}
	return true;
}

cell AMX_NATIVE_CALL A3D_GetWorld(AMX* amx, const cell* params) {
	Body* body = findBody(params[1]);
	return body == nullptr ? -1 : cell(body->world);
}

cell AMX_NATIVE_CALL A3D_SetInterior(AMX* amx, const cell* params) {
	Body* body = findBody(params[1]);
	if (body == nullptr) {
		return false;
	}
	body->interior = params[2];
	regroupBody(*body);
	if (body->kind == Kind::Object) {
		bodies().ActivateBody(body->id);
	}
	return true;
}

cell AMX_NATIVE_CALL A3D_GetInterior(AMX* amx, const cell* params) {
	Body* body = findBody(params[1]);
	return body == nullptr ? -1 : cell(body->interior);
}

cell AMX_NATIVE_CALL A3D_GetPosition(AMX* amx, const cell* params) {
	Body* body = findBody(params[1]);
	if (body == nullptr) {
		return false;
	}
	JPH::RVec3 position = bodies().GetPosition(body->id);
	return writeVector(amx, params, 2, Vector3(float(position.GetX()), float(position.GetY()), float(position.GetZ())));
}

cell AMX_NATIVE_CALL A3D_SetPosition(AMX* amx, const cell* params) {
	Body* body = findBody(params[1]);
	if (body == nullptr) {
		return false;
	}
	Vector3 position = readVector(params, 2);
	bodies().SetPosition(body->id, JPH::RVec3(position.x, position.y, position.z), body->kind == Kind::Object ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	syncTeleport(*body);
	return true;
}

cell AMX_NATIVE_CALL A3D_GetRotation(AMX* amx, const cell* params) {
	Body* body = findBody(params[1]);
	if (body == nullptr) {
		return false;
	}
	return writeVector(amx, params, 2, toEuler(bodies().GetRotation(body->id)));
}

cell AMX_NATIVE_CALL A3D_SetRotation(AMX* amx, const cell* params) {
	Body* body = findBody(params[1]);
	if (body == nullptr) {
		return false;
	}
	bodies().SetRotation(body->id, toJolt(readVector(params, 2)), body->kind == Kind::Object ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	syncTeleport(*body);
	return true;
}

cell AMX_NATIVE_CALL A3D_SetFriction(AMX* amx, const cell* params) {
	Body* body = findBody(params[1]);
	if (body == nullptr) {
		return false;
	}
	bodies().SetFriction(body->id, readFloat(params, 2));
	return true;
}

cell AMX_NATIVE_CALL A3D_SetBounce(AMX* amx, const cell* params) {
	Body* body = findBody(params[1]);
	if (body == nullptr) {
		return false;
	}
	bodies().SetRestitution(body->id, readFloat(params, 2));
	return true;
}

cell AMX_NATIVE_CALL A3D_SetDamping(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	if (body == nullptr) {
		return false;
	}
	JPH::BodyLockWrite lock(state().system->GetBodyLockInterface(), body->id);
	if (!lock.Succeeded()) {
		return false;
	}
	JPH::MotionProperties* motion = lock.GetBody().GetMotionProperties();
	motion->SetLinearDamping(readFloat(params, 2));
	motion->SetAngularDamping(readFloat(params, 3));
	return true;
}

cell AMX_NATIVE_CALL A3D_SetBuoyancy(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	float buoyancy = readFloat(params, 2);
	float drag = readFloat(params, 3);
	if (body == nullptr || buoyancy < 0.0f || drag < 0.0f) {
		return false;
	}
	setBuoyancy(params[1], buoyancy, drag);
	bodies().ActivateBody(body->id);
	return true;
}

cell AMX_NATIVE_CALL A3D_SetSyncError(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	float error = readFloat(params, 2);
	if (body == nullptr || error <= 0.0f) {
		return false;
	}
	body->syncError = error;
	return true;
}

cell AMX_NATIVE_CALL A3D_IsMoving(AMX* amx, const cell* params) {
	Body* body = movingBody(params);
	return body != nullptr && bodies().IsActive(body->id);
}

cell AMX_NATIVE_CALL A3D_GetObjectID(AMX* amx, const cell* params) {
	Body* body = findBody(params[1]);
	if (body == nullptr || body->sync != Sync::Object) {
		return InvalidObject;
	}
	return body->objectid;
}

cell AMX_NATIVE_CALL A3D_AttachDynamicObject(AMX* amx, const cell* params) {
	State& s = state();
	Body* body = movingBody(params);
	if (body == nullptr) {
		return false;
	}
	if (body->sync == Sync::Object && s.objects != nullptr) {
		s.objects->release(body->objectid);
	}
	body->sync = Sync::Script;
	body->objectid = params[2];
	syncTeleport(*body);
	return true;
}

cell AMX_NATIVE_CALL A3D_SetExtra(AMX* amx, const cell* params) {
	Body* body = findBody(params[1]);
	if (body == nullptr || params[2] < 0 || params[2] >= ExtraSlots) {
		return false;
	}
	body->extra[size_t(params[2])] = params[3];
	return true;
}

cell AMX_NATIVE_CALL A3D_GetExtra(AMX* amx, const cell* params) {
	Body* body = findBody(params[1]);
	if (body == nullptr || params[2] < 0 || params[2] >= ExtraSlots) {
		return 0;
	}
	return body->extra[size_t(params[2])];
}

cell AMX_NATIVE_CALL A3D_GetStats(AMX* amx, const cell* params) {
	State& s = state();
	IPawnScript* script = s.pawn->getScript(amx);
	cell* active = nullptr;
	cell* updates = nullptr;
	if (script == nullptr || script->GetAddr(params[1], &active) != AMX_ERR_NONE || script->GetAddr(params[2], &updates) != AMX_ERR_NONE) {
		return false;
	}
	int players = int(std::count_if(s.playerBodies.begin(), s.playerBodies.end(), [](const JPH::BodyID& id) {
		return !id.IsInvalid();
	}));
	*active = cell(s.system->GetNumActiveBodies(JPH::EBodyType::RigidBody)) - players;
	*updates = cell(s.updates);
	return writeFloat(amx, params[3], float(s.milliseconds));
}

const AMX_NATIVE_INFO natives[] = {
	{ "A3D_CreateObject", A3D_CreateObject },
	{ "A3D_CreateSphere", A3D_CreateSphere },
	{ "A3D_CreateBox", A3D_CreateBox },
	{ "A3D_CreateStaticObject", A3D_CreateStaticObject },
	{ "A3D_CreateStaticBox", A3D_CreateStaticBox },
	{ "A3D_CreateStaticCylinder", A3D_CreateStaticCylinder },
	{ "A3D_CreateZone", A3D_CreateZone },
	{ "A3D_CreateZoneSphere", A3D_CreateZoneSphere },
	{ "A3D_DestroyObject", A3D_DestroyObject },
	{ "A3D_IsValidObject", A3D_IsValidObject },
	{ "A3D_SetVelocity", A3D_SetVelocity },
	{ "A3D_GetVelocity", A3D_GetVelocity },
	{ "A3D_GetSpeed", A3D_GetSpeed },
	{ "A3D_GetMoveAngle", A3D_GetMoveAngle },
	{ "A3D_ApplyImpulse", A3D_ApplyImpulse },
	{ "A3D_SetAcceleration", A3D_SetAcceleration },
	{ "A3D_SetGravity", A3D_SetGravity },
	{ "A3D_SetRollingFriction", A3D_SetRollingFriction },
	{ "A3D_SetPlanar", A3D_SetPlanar },
	{ "A3D_SetCollisions", A3D_SetCollisions },
	{ "A3D_GetCollisions", A3D_GetCollisions },
	{ "A3D_SetCollisionSpeed", A3D_SetCollisionSpeed },
	{ "A3D_SetWorld", A3D_SetWorld },
	{ "A3D_GetWorld", A3D_GetWorld },
	{ "A3D_SetInterior", A3D_SetInterior },
	{ "A3D_GetInterior", A3D_GetInterior },
	{ "A3D_GetPosition", A3D_GetPosition },
	{ "A3D_SetPosition", A3D_SetPosition },
	{ "A3D_GetRotation", A3D_GetRotation },
	{ "A3D_SetRotation", A3D_SetRotation },
	{ "A3D_SetFriction", A3D_SetFriction },
	{ "A3D_SetBounce", A3D_SetBounce },
	{ "A3D_SetDamping", A3D_SetDamping },
	{ "A3D_SetBuoyancy", A3D_SetBuoyancy },
	{ "A3D_SetSyncError", A3D_SetSyncError },
	{ "A3D_IsMoving", A3D_IsMoving },
	{ "A3D_GetObjectID", A3D_GetObjectID },
	{ "A3D_AttachDynamicObject", A3D_AttachDynamicObject },
	{ "A3D_SetExtra", A3D_SetExtra },
	{ "A3D_GetExtra", A3D_GetExtra },
	{ "A3D_GetStats", A3D_GetStats },
};

}

void registerNatives(IPawnScript& script) {
	script.Register(natives, int(sizeof natives / sizeof natives[0]));
	registerQueryNatives(script);
	registerMapNatives(script);
	registerGeometryNatives(script);
	registerProbeNatives(script);
	registerScreenNatives(script);
	registerRouteNatives(script);
	registerWalkNatives(script);
}

}
