#include "physics.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace physics {

namespace {

constexpr float RefineDistance = 50.0f;
constexpr float RefineMargin = 1.0f;
constexpr float DegreesToRadians = 3.14159265f / 180.0f;

uint32_t bitOf(Kind kind) {
	switch (kind) {
	case Kind::Map:
		return CollideMap;
	case Kind::Object:
		return CollideObjects;
	case Kind::Static:
		return CollideStatic;
	case Kind::Player:
		return CollidePlayers;
	case Kind::Vehicle:
		return CollideVehicles;
	case Kind::Actor:
		return CollideActors;
	case Kind::Zone:
		return 0;
	}
	return 0;
}

Hit hitOf(Kind kind) {
	switch (kind) {
	case Kind::Object:
		return Hit::Object;
	case Kind::Static:
		return Hit::Static;
	case Kind::Player:
		return Hit::Player;
	case Kind::Vehicle:
		return Hit::Vehicle;
	case Kind::Actor:
		return Hit::Actor;
	default:
		return Hit::Map;
	}
}

cell floatCell(float value) {
	return amx_ftoc(value);
}

void describe(const JPH::BodyID& id, QueryHit& hit) {
	uint64_t data = recalledData(id);
	hit.type = hitOf(kindOf(data));
	hit.id = indexOf(data);
}

void inspect(const JPH::BodyID& id, const JPH::SubShapeID& part, JPH::Vec3Arg direction, bool normal, QueryHit& hit) {
	State& s = state();
	JPH::BodyLockRead lock(s.system->GetBodyLockInterface(), id);
	if (!lock.Succeeded()) {
		return;
	}
	const JPH::Body& body = lock.GetBody();
	hit.material = surfaceOf(body, part);
	if (!normal) {
		return;
	}
	hit.normal = body.GetWorldSpaceSurfaceNormal(part, hit.point);
	if (hit.normal.Dot(direction) > 0.0f) {
		hit.normal = -hit.normal;
	}
}

JPH::RayCastSettings raySettings() {
	JPH::RayCastSettings settings;
	settings.mBackFaceModeTriangles = JPH::EBackFaceMode::CollideWithBackFaces;
	settings.mBackFaceModeConvex = JPH::EBackFaceMode::IgnoreBackFaces;
	settings.mTreatConvexAsSolid = false;
	return settings;
}

bool rounded(const JPH::BodyID& id, const JPH::SubShapeID& part) {
	State& s = state();
	JPH::BodyLockRead lock(s.system->GetBodyLockInterface(), id);
	if (!lock.Succeeded()) {
		return false;
	}
	JPH::SubShapeID remainder;
	const JPH::Shape* leaf = lock.GetBody().GetShape()->GetLeafShape(part, remainder);
	if (leaf == nullptr) {
		return false;
	}
	JPH::EShapeSubType type = leaf->GetSubType();
	return type == JPH::EShapeSubType::Sphere || type == JPH::EShapeSubType::Capsule || type == JPH::EShapeSubType::TaperedCapsule || type == JPH::EShapeSubType::Cylinder;
}

std::vector<JPH::BodyID> overlapping(const JPH::Shape* shape, JPH::RMat44Arg transform, int world, uint32_t flags, int interior) {
	State& s = state();
	JPH::CollideShapeSettings settings;
	settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
	JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
	QueryFilter filter(world, flags, interior);
	s.system->GetNarrowPhaseQuery().CollideShape(shape, JPH::Vec3::sReplicate(1.0f), transform, settings, transform.GetTranslation(), collector, {}, {}, filter);
	std::vector<JPH::BodyID> found;
	for (const JPH::CollideShapeResult& result : collector.mHits) {
		if (std::find(found.begin(), found.end(), result.mBodyID2) == found.end()) {
			found.push_back(result.mBodyID2);
		}
	}
	return found;
}

cell writeOverlaps(AMX* amx, const std::vector<JPH::BodyID>& found, cell typesAddress, cell idsAddress, cell size) {
	cell* types = addressOf(amx, typesAddress);
	cell* ids = addressOf(amx, idsAddress);
	if (types == nullptr || ids == nullptr) {
		return 0;
	}
	cell count = std::min(cell(found.size()), std::max(size, cell(0)));
	for (cell index = 0; index != count; ++index) {
		QueryHit hit;
		describe(found[index], hit);
		types[index] = cell(hit.type);
		ids[index] = hit.id;
	}
	return count;
}

bool inPlace(int entityWorld, int entityInterior, int world, int interior) {
	if (world >= 0 && entityWorld != world) {
		return false;
	}
	return interior < 0 || entityInterior == interior;
}

bool inCone(Vector3 apex, Vector3 direction, float cosine, float range, Vector3 target, float& distance) {
	Vector3 offset = target - apex;
	distance = glm::length(offset);
	if (distance > range) {
		return false;
	}
	return distance <= 0.0f || glm::dot(offset / distance, direction) >= cosine;
}

}

QueryFilter::QueryFilter(int world, uint32_t flags, int interior, JPH::BodyID first, JPH::BodyID second) :
	context_{ world, interior },
	flags_(flags),
	first_(first),
	second_(second) {
}

bool QueryFilter::ShouldCollideLocked(const JPH::Body& body) const {
	if (body.IsSensor() || body.GetID() == first_ || body.GetID() == second_) {
		return false;
	}
	if ((flags_ & bitOf(kindOf(body.GetUserData()))) == 0) {
		return false;
	}
	if (context_.world < 0 && context_.interior < 0) {
		return true;
	}
	JPH::CollisionGroup::GroupID group = body.GetCollisionGroup().GetGroupID();
	if ((group & MapGroup) != 0) {
		return mapMatches(group & ~MapGroup, context_);
	}
	return context_.matches(state().bodyContexts[group]);
}

JPH::RVec3 toJoltPoint(Vector3 point) {
	return JPH::RVec3(point.x, point.y, point.z);
}

Vector3 toPoint(JPH::RVec3Arg point) {
	return Vector3(float(point.GetX()), float(point.GetY()), float(point.GetZ()));
}

bool castRay(Vector3 from, Vector3 to, int world, uint32_t flags, bool normal, QueryHit& hit, int interior, JPH::BodyID first, JPH::BodyID second) {
	State& s = state();
	JPH::Vec3 direction(to.x - from.x, to.y - from.y, to.z - from.z);
	if (direction.IsNearZero()) {
		return false;
	}
	JPH::RRayCast ray(toJoltPoint(from), direction);
	JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> collector;
	QueryFilter filter(world, flags, interior, first, second);
	s.system->GetNarrowPhaseQuery().CastRay(ray, raySettings(), collector, {}, {}, filter);
	float water;
	if ((flags & CollideWater) != 0 && waterAlong(from, to, water) && (!collector.HadHit() || water < collector.mHit.mFraction)) {
		hit.point = ray.GetPointOnRay(water);
		hit.normal = direction.GetZ() > 0.0f ? -JPH::Vec3::sAxisZ() : JPH::Vec3::sAxisZ();
		hit.type = Hit::Water;
		hit.id = 0;
		hit.material = 0;
		return true;
	}
	if (!collector.HadHit()) {
		return false;
	}
	JPH::RayCastResult result = collector.mHit;
	hit.point = ray.GetPointOnRay(result.mFraction);
	float length = direction.Length();
	if (result.mFraction * length > RefineDistance && rounded(result.mBodyID, result.mSubShapeID2)) {
		JPH::Vec3 unit = direction / length;
		JPH::RRayCast close(hit.point - unit * RefineMargin, unit * (2.0f * RefineMargin));
		JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> refined;
		s.system->GetNarrowPhaseQuery().CastRay(close, raySettings(), refined, {}, {}, filter);
		if (refined.HadHit()) {
			result = refined.mHit;
			hit.point = close.GetPointOnRay(result.mFraction);
		}
	}
	describe(result.mBodyID, hit);
	inspect(result.mBodyID, result.mSubShapeID2, direction, normal, hit);
	return true;
}

bool blocked(Vector3 from, Vector3 to, int world, uint32_t flags, int interior) {
	State& s = state();
	JPH::Vec3 direction(to.x - from.x, to.y - from.y, to.z - from.z);
	if (direction.IsNearZero()) {
		return false;
	}
	float water;
	if ((flags & CollideWater) != 0 && waterAlong(from, to, water)) {
		return true;
	}
	JPH::AnyHitCollisionCollector<JPH::CastRayCollector> collector;
	QueryFilter filter(world, flags, interior);
	s.system->GetNarrowPhaseQuery().CastRay(JPH::RRayCast(toJoltPoint(from), direction), raySettings(), collector, {}, {}, filter);
	return collector.HadHit();
}

bool sweepSphere(float radius, Vector3 from, Vector3 to, int world, uint32_t flags, QueryHit& hit, int interior) {
	State& s = state();
	JPH::Vec3 direction(to.x - from.x, to.y - from.y, to.z - from.z);
	if (radius <= 0.0f || direction.IsNearZero()) {
		return false;
	}
	JPH::SphereShape sphere(radius);
	sphere.SetEmbedded();
	JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(&sphere, JPH::Vec3::sReplicate(1.0f), JPH::RMat44::sTranslation(toJoltPoint(from)), direction);
	JPH::ShapeCastSettings settings;
	settings.mBackFaceModeTriangles = JPH::EBackFaceMode::CollideWithBackFaces;
	JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
	QueryFilter filter(world, flags, interior);
	s.system->GetNarrowPhaseQuery().CastShape(cast, settings, toJoltPoint(from), collector, {}, {}, filter);
	float water;
	if ((flags & CollideWater) != 0 && waterAlong(from, to, water) && (!collector.HadHit() || water < collector.mHit.mFraction)) {
		hit.point = cast.GetPointOnRay(water);
		hit.normal = direction.GetZ() > 0.0f ? -JPH::Vec3::sAxisZ() : JPH::Vec3::sAxisZ();
		hit.type = Hit::Water;
		hit.id = 0;
		hit.material = 0;
		return true;
	}
	if (!collector.HadHit()) {
		return false;
	}
	hit.point = cast.GetPointOnRay(collector.mHit.mFraction);
	describe(collector.mHit.mBodyID2, hit);
	inspect(collector.mHit.mBodyID2, collector.mHit.mSubShapeID2, direction, false, hit);
	return true;
}

bool touching(const JPH::Shape* shape, JPH::RMat44Arg transform, int world, uint32_t flags, int interior) {
	State& s = state();
	JPH::CollideShapeSettings settings;
	settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
	JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;
	QueryFilter filter(world, flags, interior);
	s.system->GetNarrowPhaseQuery().CollideShape(shape, JPH::Vec3::sReplicate(1.0f), transform, settings, transform.GetTranslation(), collector, {}, {}, filter);
	return collector.HadHit();
}

cell writeNearest(AMX* amx, std::vector<std::pair<float, int>>& found, cell address, cell size) {
	cell* target = addressOf(amx, address);
	if (target == nullptr) {
		return 0;
	}
	std::sort(found.begin(), found.end());
	cell count = std::min(cell(found.size()), std::max(size, cell(0)));
	for (cell index = 0; index != count; ++index) {
		target[index] = found[index].second;
	}
	return count;
}

bool present(IPlayer& player) {
	PlayerState state = player.getState();
	return state != PlayerState_None && state != PlayerState_Wasted && state != PlayerState_Spectating;
}

bool mapBetween(Vector3 from, Vector3 to, int world) {
	return blocked(from, to, world, CollideMap);
}

void ownBodies(IPlayer& player, JPH::BodyID& own, JPH::BodyID& ride) {
	State& s = state();
	int playerid = player.getID();
	own = playerid < int(s.playerBodies.size()) ? s.playerBodies[playerid] : JPH::BodyID();
	ride = JPH::BodyID();
	IPlayerVehicleData* data = queryExtension<IPlayerVehicleData>(player);
	IVehicle* vehicle = data != nullptr ? data->getVehicle() : nullptr;
	if (vehicle != nullptr && vehicle->getID() < int(s.vehicleBodies.size())) {
		ride = s.vehicleBodies[vehicle->getID()];
	}
}

namespace {

cell AMX_NATIVE_CALL A3D_RayCast(AMX* amx, const cell* params) {
	QueryHit hit;
	if (!castRay(readVector(params, 1), readVector(params, 4), params[13], uint32_t(params[14]), false, hit, optional(params, 15, -1))) {
		return false;
	}
	writeVector(amx, params, 7, toPoint(hit.point));
	writeCell(amx, params[10], cell(hit.type));
	writeCell(amx, params[11], hit.id);
	writeCell(amx, params[12], hit.material);
	return true;
}

cell AMX_NATIVE_CALL A3D_RayCastNormal(AMX* amx, const cell* params) {
	QueryHit hit;
	if (!castRay(readVector(params, 1), readVector(params, 4), params[16], uint32_t(params[17]), true, hit, optional(params, 18, -1))) {
		return false;
	}
	writeVector(amx, params, 7, toPoint(hit.point));
	writeVector(amx, params, 10, Vector3(hit.normal.GetX(), hit.normal.GetY(), hit.normal.GetZ()));
	writeCell(amx, params[13], cell(hit.type));
	writeCell(amx, params[14], hit.id);
	writeCell(amx, params[15], hit.material);
	return true;
}

cell AMX_NATIVE_CALL A3D_RayCastAll(AMX* amx, const cell* params) {
	State& s = state();
	Vector3 from = readVector(params, 1);
	Vector3 to = readVector(params, 4);
	JPH::Vec3 direction(to.x - from.x, to.y - from.y, to.z - from.z);
	cell* xs = addressOf(amx, params[7]);
	cell* ys = addressOf(amx, params[8]);
	cell* zs = addressOf(amx, params[9]);
	cell* types = addressOf(amx, params[10]);
	cell* ids = addressOf(amx, params[11]);
	cell* materials = addressOf(amx, params[12]);
	if (direction.IsNearZero() || xs == nullptr || ys == nullptr || zs == nullptr || types == nullptr || ids == nullptr || materials == nullptr) {
		return 0;
	}
	JPH::RRayCast ray(toJoltPoint(from), direction);
	JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
	QueryFilter filter(params[14], uint32_t(params[15]), optional(params, 16, -1));
	s.system->GetNarrowPhaseQuery().CastRay(ray, raySettings(), collector, {}, {}, filter);
	collector.Sort();
	float water;
	bool wet = (uint32_t(params[15]) & CollideWater) != 0 && waterAlong(from, to, water);
	cell found = cell(collector.mHits.size()) + (wet ? 1 : 0);
	cell count = std::min(found, std::max(params[13], cell(0)));
	size_t next = 0;
	for (cell index = 0; index != count; ++index) {
		QueryHit hit;
		if (wet && (next == collector.mHits.size() || water < collector.mHits[next].mFraction)) {
			hit.point = ray.GetPointOnRay(water);
			hit.type = Hit::Water;
			wet = false;
		} else {
			const JPH::RayCastResult& result = collector.mHits[next++];
			hit.point = ray.GetPointOnRay(result.mFraction);
			describe(result.mBodyID, hit);
			inspect(result.mBodyID, result.mSubShapeID2, direction, false, hit);
		}
		xs[index] = floatCell(float(hit.point.GetX()));
		ys[index] = floatCell(float(hit.point.GetY()));
		zs[index] = floatCell(float(hit.point.GetZ()));
		types[index] = cell(hit.type);
		ids[index] = hit.id;
		materials[index] = hit.material;
	}
	return count;
}

cell AMX_NATIVE_CALL A3D_IsLineOfSight(AMX* amx, const cell* params) {
	return !blocked(readVector(params, 1), readVector(params, 4), params[7], uint32_t(params[8]), optional(params, 9, -1));
}

cell AMX_NATIVE_CALL A3D_FindGround(AMX* amx, const cell* params) {
	Vector3 from = readVector(params, 1);
	float depth = readFloat(params, 7);
	QueryHit hit;
	if (depth <= 0.0f || !castRay(from, Vector3(from.x, from.y, from.z - depth), params[5], uint32_t(params[6]), false, hit, optional(params, 8, -1))) {
		return false;
	}
	return writeFloat(amx, params[4], float(hit.point.GetZ()));
}

cell AMX_NATIVE_CALL A3D_SweepSphere(AMX* amx, const cell* params) {
	Vector3 to = readVector(params, 5);
	QueryHit hit;
	if (!sweepSphere(readFloat(params, 1), readVector(params, 2), to, params[14], uint32_t(params[15]), hit, optional(params, 16, -1))) {
		writeVector(amx, params, 8, to);
		return false;
	}
	writeVector(amx, params, 8, toPoint(hit.point));
	writeCell(amx, params[11], cell(hit.type));
	writeCell(amx, params[12], hit.id);
	writeCell(amx, params[13], hit.material);
	return true;
}

cell AMX_NATIVE_CALL A3D_OverlapSphere(AMX* amx, const cell* params) {
	float radius = readFloat(params, 4);
	if (radius <= 0.0f) {
		return 0;
	}
	JPH::SphereShape sphere(radius);
	sphere.SetEmbedded();
	std::vector<JPH::BodyID> found = overlapping(&sphere, JPH::RMat44::sTranslation(toJoltPoint(readVector(params, 1))), params[8], uint32_t(params[9]), optional(params, 10, -1));
	return writeOverlaps(amx, found, params[5], params[6], params[7]);
}

cell AMX_NATIVE_CALL A3D_OverlapBox(AMX* amx, const cell* params) {
	Vector3 size = readVector(params, 1);
	if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
		return 0;
	}
	JPH::Vec3 half(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f);
	JPH::BoxShape box(half, std::min(JPH::cDefaultConvexRadius, half.ReduceMin()));
	box.SetEmbedded();
	JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(toJolt(readVector(params, 7)), toJoltPoint(readVector(params, 4)));
	std::vector<JPH::BodyID> found = overlapping(&box, transform, params[13], uint32_t(params[14]), optional(params, 15, -1));
	return writeOverlaps(amx, found, params[10], params[11], params[12]);
}

JPH::RMat44 modelTransform(const cell* params) {
	return JPH::RMat44::sRotationTranslation(toJolt(readVector(params, 5)), toJoltPoint(readVector(params, 2)));
}

cell AMX_NATIVE_CALL A3D_OverlapModel(AMX* amx, const cell* params) {
	JPH::ShapeRefC shape = dynamicShape(params[1]);
	if (shape == nullptr) {
		return 0;
	}
	std::vector<JPH::BodyID> found = overlapping(shape, modelTransform(params), params[11], uint32_t(params[12]), optional(params, 13, -1));
	return writeOverlaps(amx, found, params[8], params[9], params[10]);
}

cell AMX_NATIVE_CALL A3D_IsModelColliding(AMX* amx, const cell* params) {
	JPH::ShapeRefC shape = dynamicShape(params[1]);
	return shape != nullptr && touching(shape, modelTransform(params), params[8], uint32_t(params[9]), optional(params, 10, -1));
}

cell AMX_NATIVE_CALL A3D_GetPlayersInSphere(AMX* amx, const cell* params) {
	State& s = state();
	Vector3 centre = readVector(params, 1);
	float radius = readFloat(params, 4);
	bool sight = optional(params, 9, 0) != 0;
	std::vector<std::pair<float, int>> found;
	for (IPlayer* player : s.players->entries()) {
		if (!present(*player) || !inPlace(player->getVirtualWorld(), int(player->getInterior()), params[7], params[8])) {
			continue;
		}
		Vector3 position = player->getPosition();
		float distance = glm::distance(position, centre);
		if (distance > radius) {
			continue;
		}
		if (sight && blocked(centre, position + Vector3(0.0f, 0.0f, ChestHeight), params[7], CollideMap | CollideStatic, params[8])) {
			continue;
		}
		found.emplace_back(distance, player->getID());
	}
	return writeNearest(amx, found, params[5], params[6]);
}

cell AMX_NATIVE_CALL A3D_GetPlayersInCone(AMX* amx, const cell* params) {
	State& s = state();
	Vector3 apex = readVector(params, 1);
	Vector3 direction = readVector(params, 4);
	float length = glm::length(direction);
	float range = readFloat(params, 8);
	if (length <= 0.0f || range <= 0.0f) {
		return 0;
	}
	direction /= length;
	float cosine = std::cos(readFloat(params, 7) * DegreesToRadians);
	int world = params[11];
	bool sight = params[13] != 0;
	std::vector<std::pair<float, int>> found;
	for (IPlayer* player : s.players->entries()) {
		if (player->getID() == params[14] || !present(*player) || !inPlace(player->getVirtualWorld(), int(player->getInterior()), world, params[12])) {
			continue;
		}
		Vector3 target = player->getPosition() + Vector3(0.0f, 0.0f, ChestHeight);
		float distance;
		if (!inCone(apex, direction, cosine, range, target, distance)) {
			continue;
		}
		if (sight && blocked(apex, target, world, CollideMap | CollideStatic, params[12])) {
			continue;
		}
		found.emplace_back(distance, player->getID());
	}
	return writeNearest(amx, found, params[9], params[10]);
}

cell AMX_NATIVE_CALL A3D_GetVehiclesInSphere(AMX* amx, const cell* params) {
	State& s = state();
	if (s.vehicles == nullptr) {
		return 0;
	}
	Vector3 centre = readVector(params, 1);
	float radius = readFloat(params, 4);
	bool sight = optional(params, 9, 0) != 0;
	std::vector<std::pair<float, int>> found;
	for (IVehicle* vehicle : *s.vehicles) {
		if (!inPlace(vehicle->getVirtualWorld(), vehicle->getInterior(), params[7], params[8])) {
			continue;
		}
		Vector3 position = vehicle->getPosition();
		float distance = glm::distance(position, centre);
		if (distance > radius) {
			continue;
		}
		if (sight && blocked(centre, position, params[7], CollideMap | CollideStatic, params[8])) {
			continue;
		}
		found.emplace_back(distance, vehicle->getID());
	}
	return writeNearest(amx, found, params[5], params[6]);
}

cell AMX_NATIVE_CALL A3D_GetActorsInSphere(AMX* amx, const cell* params) {
	State& s = state();
	if (s.actors == nullptr) {
		return 0;
	}
	Vector3 centre = readVector(params, 1);
	float radius = readFloat(params, 4);
	int world = params[7];
	bool sight = params[8] != 0;
	std::vector<std::pair<float, int>> found;
	for (IActor* actor : *s.actors) {
		if (world >= 0 && actor->getVirtualWorld() != world) {
			continue;
		}
		Vector3 position = actor->getPosition();
		float distance = glm::distance(position, centre);
		if (distance > radius) {
			continue;
		}
		if (sight && blocked(centre, position + Vector3(0.0f, 0.0f, ChestHeight), world, CollideMap | CollideStatic)) {
			continue;
		}
		found.emplace_back(distance, actor->getID());
	}
	return writeNearest(amx, found, params[5], params[6]);
}

cell AMX_NATIVE_CALL A3D_GetActorsInCone(AMX* amx, const cell* params) {
	State& s = state();
	Vector3 apex = readVector(params, 1);
	Vector3 direction = readVector(params, 4);
	float length = glm::length(direction);
	float range = readFloat(params, 8);
	if (s.actors == nullptr || length <= 0.0f || range <= 0.0f) {
		return 0;
	}
	direction /= length;
	float cosine = std::cos(readFloat(params, 7) * DegreesToRadians);
	int world = params[11];
	bool sight = params[12] != 0;
	std::vector<std::pair<float, int>> found;
	for (IActor* actor : *s.actors) {
		if (world >= 0 && actor->getVirtualWorld() != world) {
			continue;
		}
		Vector3 target = actor->getPosition() + Vector3(0.0f, 0.0f, ChestHeight);
		float distance;
		if (!inCone(apex, direction, cosine, range, target, distance)) {
			continue;
		}
		if (sight && blocked(apex, target, world, CollideMap | CollideStatic)) {
			continue;
		}
		found.emplace_back(distance, actor->getID());
	}
	return writeNearest(amx, found, params[9], params[10]);
}

cell AMX_NATIVE_CALL A3D_CreateExplosion(AMX* amx, const cell* params) {
	State& s = state();
	Vector3 centre = readVector(params, 1);
	float radius = readFloat(params, 4);
	float force = readFloat(params, 5);
	if (radius <= 0.0f) {
		return 0;
	}
	JPH::SphereShape sphere(radius);
	sphere.SetEmbedded();
	std::vector<JPH::BodyID> found = overlapping(&sphere, JPH::RMat44::sTranslation(toJoltPoint(centre)), params[6], CollideObjects, optional(params, 7, -1));
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	cell pushed = 0;
	for (const JPH::BodyID& id : found) {
		JPH::Vec3 offset = JPH::Vec3(bodies.GetCenterOfMassPosition(id) - toJoltPoint(centre));
		float distance = offset.Length();
		JPH::Vec3 direction = distance > 0.01f ? offset / distance : JPH::Vec3::sAxisZ();
		float share = 1.0f - distance / radius;
		if (share <= 0.0f) {
			continue;
		}
		bodies.AddImpulse(id, direction * force * share);
		++pushed;
	}
	return pushed;
}

cell AMX_NATIVE_CALL A3D_GetPlayerTarget(AMX* amx, const cell* params) {
	State& s = state();
	IPlayer* player = s.players->get(params[1]);
	if (player == nullptr) {
		return false;
	}
	const PlayerAimData& aim = player->getAimData();
	float length = glm::length(aim.camFrontVector);
	if (length <= 0.0f) {
		return false;
	}
	JPH::BodyID own;
	JPH::BodyID ride;
	ownBodies(*player, own, ride);
	Vector3 end = aim.camPos + aim.camFrontVector / length * readFloat(params, 8);
	QueryHit hit;
	if (!castRay(aim.camPos, end, player->getVirtualWorld(), uint32_t(params[9]), false, hit, int(player->getInterior()), own, ride)) {
		return false;
	}
	writeVector(amx, params, 2, toPoint(hit.point));
	writeCell(amx, params[5], cell(hit.type));
	writeCell(amx, params[6], hit.id);
	writeCell(amx, params[7], hit.material);
	return true;
}

const AMX_NATIVE_INFO natives[] = {
	{ "A3D_RayCast", A3D_RayCast },
	{ "A3D_RayCastNormal", A3D_RayCastNormal },
	{ "A3D_RayCastAll", A3D_RayCastAll },
	{ "A3D_IsLineOfSight", A3D_IsLineOfSight },
	{ "A3D_FindGround", A3D_FindGround },
	{ "A3D_SweepSphere", A3D_SweepSphere },
	{ "A3D_OverlapSphere", A3D_OverlapSphere },
	{ "A3D_OverlapBox", A3D_OverlapBox },
	{ "A3D_OverlapModel", A3D_OverlapModel },
	{ "A3D_IsModelColliding", A3D_IsModelColliding },
	{ "A3D_GetPlayersInSphere", A3D_GetPlayersInSphere },
	{ "A3D_GetPlayersInCone", A3D_GetPlayersInCone },
	{ "A3D_GetVehiclesInSphere", A3D_GetVehiclesInSphere },
	{ "A3D_GetActorsInSphere", A3D_GetActorsInSphere },
	{ "A3D_GetActorsInCone", A3D_GetActorsInCone },
	{ "A3D_CreateExplosion", A3D_CreateExplosion },
	{ "A3D_GetPlayerTarget", A3D_GetPlayerTarget },
};

}

void registerQueryNatives(IPawnScript& script) {
	script.Register(natives, int(sizeof natives / sizeof natives[0]));
}

}
