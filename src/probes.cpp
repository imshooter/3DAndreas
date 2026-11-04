#include "physics.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <cmath>

namespace physics {

namespace {

constexpr float ProbeReach = 2000.0f;
constexpr int SpotAttempts = 16;
constexpr float MinimumUpright = 0.7f;
constexpr float SpotLift = 0.05f;
constexpr float TrajectoryStep = 1.0f / 30.0f;
constexpr float FullTurn = 6.28318531f;

bool firstSurface(Vector3 from, float reach, int world, int interior, bool& behind, float& distance) {
	State& s = state();
	JPH::RRayCast ray(toJoltPoint(from), JPH::Vec3(0.0f, 0.0f, reach));
	JPH::RayCastSettings settings;
	settings.mBackFaceModeTriangles = JPH::EBackFaceMode::CollideWithBackFaces;
	settings.mBackFaceModeConvex = JPH::EBackFaceMode::IgnoreBackFaces;
	settings.mTreatConvexAsSolid = false;
	JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> collector;
	QueryFilter filter(world, CollideMap, interior);
	s.system->GetNarrowPhaseQuery().CastRay(ray, settings, collector, {}, {}, filter);
	if (!collector.HadHit()) {
		return false;
	}
	JPH::BodyLockRead lock(s.system->GetBodyLockInterface(), collector.mHit.mBodyID);
	if (!lock.Succeeded()) {
		return false;
	}
	JPH::Vec3 normal = lock.GetBody().GetWorldSpaceSurfaceNormal(collector.mHit.mSubShapeID2, ray.GetPointOnRay(collector.mHit.mFraction));
	behind = normal.GetZ() * reach > 0.0f;
	distance = collector.mHit.mFraction * std::abs(reach);
	return true;
}

bool inSolidPart(Vector3 point, int world, int interior) {
	State& s = state();
	JPH::AllHitCollisionCollector<JPH::CollidePointCollector> collector;
	QueryFilter filter(world, CollideMap, interior);
	s.system->GetNarrowPhaseQuery().CollidePoint(toJoltPoint(point), collector, {}, {}, filter);
	for (const JPH::CollidePointResult& result : collector.mHits) {
		JPH::BodyLockRead lock(s.system->GetBodyLockInterface(), result.mBodyID);
		if (!lock.Succeeded()) {
			continue;
		}
		JPH::SubShapeID remainder;
		const JPH::Shape* leaf = lock.GetBody().GetShape()->GetLeafShape(result.mSubShapeID2, remainder);
		if (leaf != nullptr && leaf->GetSubType() != JPH::EShapeSubType::Mesh) {
			return true;
		}
	}
	return false;
}

cell AMX_NATIVE_CALL A3D_IsPointInsideMap(AMX* amx, const cell* params) {
	Vector3 point = readVector(params, 1);
	int world = params[5];
	int interior = optional(params, 6, -1);
	if (inSolidPart(point, world, interior)) {
		writeFloat(amx, params[4], 0.0f);
		return true;
	}
	bool behind = false;
	float above = 0.0f;
	if (!firstSurface(point, ProbeReach, world, interior, behind, above) || !behind) {
		return false;
	}
	bool under = false;
	float below = 0.0f;
	if (firstSurface(point, -ProbeReach, world, interior, under, below) && !under) {
		return false;
	}
	writeFloat(amx, params[4], above);
	return true;
}

cell AMX_NATIVE_CALL A3D_FindRandomPoint(AMX* amx, const cell* params) {
	State& s = state();
	Vector3 centre = readVector(params, 1);
	float radius = std::max(readFloat(params, 4), 0.0f);
	float height = readFloat(params, 8);
	float clearance = readFloat(params, 9);
	int world = params[10];
	int interior = optional(params, 11, -1);
	if (height <= 0.0f || clearance < 0.0f) {
		return false;
	}
	std::uniform_real_distribution<float> unit(0.0f, 1.0f);
	for (int attempt = 0; attempt != SpotAttempts; ++attempt) {
		float angle = unit(s.random) * FullTurn;
		float distance = radius * std::sqrt(unit(s.random));
		Vector3 column(centre.x + std::cos(angle) * distance, centre.y + std::sin(angle) * distance, centre.z);
		QueryHit hit;
		if (!castRay(column + Vector3(0.0f, 0.0f, height), column - Vector3(0.0f, 0.0f, height), world, CollideMap | CollideStatic, true, hit, interior)) {
			continue;
		}
		if (hit.normal.GetZ() < MinimumUpright) {
			continue;
		}
		Vector3 ground = toPoint(hit.point);
		float level;
		if (underWater(ground + Vector3(0.0f, 0.0f, SpotLift), level, world)) {
			continue;
		}
		if (clearance > 0.0f) {
			JPH::SphereShape sphere(clearance);
			sphere.SetEmbedded();
			JPH::RVec3 resting = hit.point + hit.normal * (clearance + SpotLift);
			if (touching(&sphere, JPH::RMat44::sTranslation(resting), world, CollideAll, interior)) {
				continue;
			}
		}
		return writeVector(amx, params, 5, ground);
	}
	return false;
}

cell AMX_NATIVE_CALL A3D_PredictTrajectory(AMX* amx, const cell* params) {
	State& s = state();
	Vector3 start = readVector(params, 1);
	Vector3 velocity = readVector(params, 4);
	float radius = readFloat(params, 14);
	float gravity = readFloat(params, 15);
	float duration = readFloat(params, 16);
	int world = params[17];
	uint32_t flags = uint32_t(params[18]);
	int interior = optional(params, 19, -1);
	if (duration <= 0.0f) {
		return false;
	}
	Vector3 from = start;
	for (int step = 0; float(step) * TrajectoryStep < duration; ++step) {
		float time = float(step) * TrajectoryStep;
		float next = std::min(time + TrajectoryStep, duration);
		Vector3 to = start + velocity * next - Vector3(0.0f, 0.0f, 0.5f * gravity * next * next);
		QueryHit hit;
		bool met = radius > 0.0f ? sweepSphere(radius, from, to, world, flags, hit, interior) : castRay(from, to, world, flags, false, hit, interior);
		if (met) {
			Vector3 point = toPoint(hit.point);
			float covered = glm::length(to - from);
			float share = covered > 0.0f ? glm::length(point - from) / covered : 0.0f;
			writeVector(amx, params, 7, point);
			writeFloat(amx, params[10], time + share * (next - time));
			writeCell(amx, params[11], cell(hit.type));
			writeCell(amx, params[12], hit.id);
			writeCell(amx, params[13], hit.material);
			return true;
		}
		if (to.z < s.settings.floor) {
			return false;
		}
		from = to;
	}
	return false;
}

const AMX_NATIVE_INFO natives[] = {
	{ "A3D_IsPointInsideMap", A3D_IsPointInsideMap },
	{ "A3D_FindRandomPoint", A3D_FindRandomPoint },
	{ "A3D_PredictTrajectory", A3D_PredictTrajectory },
};

}

void registerProbeNatives(IPawnScript& script) {
	script.Register(natives, int(sizeof natives / sizeof natives[0]));
}

}
