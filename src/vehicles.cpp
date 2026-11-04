#include "physics.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

#include <Server/Components/Vehicles/vehicle_models.hpp>

#include <algorithm>
#include <cmath>

namespace physics {

namespace {

constexpr float VehicleTeleport = 30.0f;
constexpr float StillVehicle = 0.01f;
constexpr float ThereSquared = 0.000001f;

JPH::ShapeRefC vehicleShape(int model) {
	State& s = state();
	auto found = s.vehicleShapes.find(model);
	if (found != s.vehicleShapes.end()) {
		return found->second;
	}
	Vector3 size;
	JPH::ShapeRefC shape;
	if (Impl::getVehicleModelInfo(model, VehicleModelInfo_Size, size) && size.x > 0.0f && size.y > 0.0f && size.z > 0.0f) {
		JPH::Vec3 half(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f);
		shape = new JPH::BoxShape(half, std::min(JPH::cDefaultConvexRadius, half.ReduceMin()), surfaceMaterial(SurfaceCar));
	}
	s.vehicleShapes.emplace(model, shape);
	return shape;
}

void createVehicleBody(int vehicleid, const JPH::Shape* shape, int model, Vector3 position, JPH::QuatArg rotation, int world, int interior) {
	State& s = state();
	JPH::BodyCreationSettings settings(shape, JPH::RVec3(position.x, position.y, position.z), rotation, JPH::EMotionType::Kinematic, playerLayer());
	settings.mUserData = userData(Kind::Vehicle, vehicleid);
	settings.mRestitution = 1.0f;
	JPH::BodyID id = addBody(settings, world, interior, JPH::EActivation::DontActivate);
	s.vehicleBodies[vehicleid] = id;
	s.vehicleModels[vehicleid] = model;
	s.vehicleWorlds[vehicleid] = world;
	s.vehicleInteriors[vehicleid] = interior;
	s.vehiclePositions[vehicleid] = position;
	s.vehicleMoving[vehicleid] = 0;
}

}

JPH::Quat vehicleRotation(const GTAQuat& rotation) {
	JPH::Quat quat(-rotation.q.x, -rotation.q.y, -rotation.q.z, rotation.q.w);
	return quat.LengthSq() > 0.0f ? quat.Normalized() : JPH::Quat::sIdentity();
}

void updateVehicles(float elapsed) {
	State& s = state();
	if (s.vehicles == nullptr || !s.settings.vehicles) {
		return;
	}
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	for (IVehicle* vehicle : *s.vehicles) {
		int vehicleid = vehicle->getID();
		if (vehicleid >= int(s.vehicleBodies.size())) {
			s.vehicleBodies.resize(vehicleid + 1);
			s.vehicleModels.resize(vehicleid + 1, -1);
			s.vehicleWorlds.resize(vehicleid + 1, -1);
			s.vehicleInteriors.resize(vehicleid + 1, -1);
			s.vehiclePositions.resize(vehicleid + 1);
			s.vehicleRotations.resize(vehicleid + 1);
			s.vehicleSeenAt.resize(vehicleid + 1, 0.0);
			s.vehicleMoving.resize(vehicleid + 1, 0);
		}
		int model = vehicle->getModel();
		int world = vehicle->getVirtualWorld();
		int interior = vehicle->getInterior();
		JPH::BodyID& id = s.vehicleBodies[vehicleid];
		if (!id.IsInvalid() && (s.vehicleModels[vehicleid] != model || s.vehicleWorlds[vehicleid] != world || s.vehicleInteriors[vehicleid] != interior)) {
			removeVehicle(vehicleid);
		}
		Vector3 position = vehicle->getPosition();
		GTAQuat synced = vehicle->getRotation();
		if (id.IsInvalid()) {
			JPH::ShapeRefC shape = vehicleShape(model);
			if (shape != nullptr) {
				createVehicleBody(vehicleid, shape, model, position, vehicleRotation(synced), world, interior);
				s.vehicleRotations[vehicleid] = synced.q;
				s.vehicleSeenAt[vehicleid] = s.now;
			}
			continue;
		}
		bool fresh = position != s.vehiclePositions[vehicleid] || synced.q != s.vehicleRotations[vehicleid];
		if (fresh) {
			float distance = glm::distance(position, s.vehiclePositions[vehicleid]);
			s.vehiclePositions[vehicleid] = position;
			s.vehicleRotations[vehicleid] = synced.q;
			s.vehicleSeenAt[vehicleid] = s.now;
			if (distance > VehicleTeleport) {
				s.vehicleMoving[vehicleid] = 0;
				bodies.SetPositionAndRotation(id, JPH::RVec3(position.x, position.y, position.z), vehicleRotation(synced), JPH::EActivation::DontActivate);
				continue;
			}
		}
		double since = s.now - s.vehicleSeenAt[vehicleid];
		Vector3 velocity = vehicle->getVelocity();
		JPH::RVec3 target = carriedPosition(position, velocity, since);
		if (!fresh && (since >= SyncLead || glm::length(velocity) * GameVelocity < StillVehicle)) {
			if (s.vehicleMoving[vehicleid] == 0) {
				continue;
			}
			bool there = (bodies.GetPosition(id) - target).LengthSq() < ThereSquared;
			bodies.MoveKinematic(id, target, vehicleRotation(synced), elapsed);
			s.vehicleMoving[vehicleid] = there ? 0 : 1;
			continue;
		}
		s.vehicleMoving[vehicleid] = 1;
		bodies.MoveKinematic(id, target, vehicleRotation(synced), elapsed);
	}
}

void removeVehicle(int vehicleid) {
	State& s = state();
	if (vehicleid < 0 || vehicleid >= int(s.vehicleBodies.size()) || s.vehicleBodies[vehicleid].IsInvalid()) {
		return;
	}
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	bodies.RemoveBody(s.vehicleBodies[vehicleid]);
	bodies.DestroyBody(s.vehicleBodies[vehicleid]);
	rememberBody(s.vehicleBodies[vehicleid], 0);
	s.vehicleBodies[vehicleid] = JPH::BodyID();
	s.vehicleModels[vehicleid] = -1;
	s.vehicleWorlds[vehicleid] = -1;
	s.vehicleInteriors[vehicleid] = -1;
}

}
