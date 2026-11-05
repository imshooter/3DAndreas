#include "physics.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <iterator>
#include <thread>

namespace physics {

namespace {

constexpr JPH::ObjectLayer StaticLayer = 0;
constexpr JPH::ObjectLayer MovingLayer = 1;
constexpr JPH::ObjectLayer PlayerLayer = 2;
constexpr JPH::ObjectLayer LayerCount = 3;

const JPH::BroadPhaseLayer StaticBroadPhase(0);
const JPH::BroadPhaseLayer MovingBroadPhase(1);

constexpr float MinimumHalfExtent = 0.005f;
constexpr size_t MaxMeshMaterials = 32;
constexpr unsigned MaxBuildThreads = 16;

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
	JPH::uint GetNumBroadPhaseLayers() const override {
		return 2;
	}

	JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
		return layer == StaticLayer ? StaticBroadPhase : MovingBroadPhase;
	}

#if defined JPH_EXTERNAL_PROFILE || defined JPH_PROFILE_ENABLED
	const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
		return layer == StaticBroadPhase ? "static" : "moving";
	}
#endif
};

class LayerBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
	bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhase) const override {
		if (layer == MovingLayer) {
			return true;
		}
		return broadPhase == MovingBroadPhase;
	}
};

class LayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
	bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override {
		return first == MovingLayer || second == MovingLayer;
	}
};

uint32_t collisionBit(Kind kind) {
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
		return CollideAll;
	}
	return CollideAll;
}

bool accepts(const JPH::CollisionGroup& self, const JPH::CollisionGroup& other) {
	Kind kind = Kind(self.GetSubGroupID() >> 8);
	Kind otherKind = Kind(other.GetSubGroupID() >> 8);
	if (kind == Kind::Zone || otherKind == Kind::Zone) {
		return true;
	}
	return (self.GetSubGroupID() & collisionBit(otherKind)) != 0;
}

class WorldFilter final : public JPH::GroupFilter {
public:
	bool CanCollide(const JPH::CollisionGroup& first, const JPH::CollisionGroup& second) const override {
		const State& s = state();
		JPH::CollisionGroup::GroupID one = first.GetGroupID();
		JPH::CollisionGroup::GroupID other = second.GetGroupID();
		if ((one & MapGroup) != 0) {
			return (other & MapGroup) == 0 && mapMatches(one & ~MapGroup, s.bodyContexts[other]) && accepts(first, second) && accepts(second, first);
		}
		if ((other & MapGroup) != 0) {
			return mapMatches(other & ~MapGroup, s.bodyContexts[one]) && accepts(first, second) && accepts(second, first);
		}
		return s.bodyContexts[one].matches(s.bodyContexts[other]) && accepts(first, second) && accepts(second, first);
	}
};

class SurfaceMaterial final : public JPH::PhysicsMaterial {
public:
	SurfaceMaterial() {
		SetEmbedded();
	}
};

SurfaceMaterial surfaceMaterials[SurfaceCount];

BroadPhaseLayers broadPhaseLayers;
LayerBroadPhaseFilter layerBroadPhaseFilter;
LayerPairFilter layerPairFilter;

float multiplyBounce(const JPH::Body& first, const JPH::SubShapeID&, const JPH::Body& second, const JPH::SubShapeID&) {
	return first.GetRestitution() * second.GetRestitution();
}

void trace(const char* format, ...) {
	char text[512];
	va_list arguments;
	va_start(arguments, format);
	std::vsnprintf(text, sizeof text, format, arguments);
	va_end(arguments);
	State& s = state();
	if (s.core != nullptr) {
		s.core->printLn("[3DAndreas] %s", text);
	}
}

JPH::ShapeRefC finishCompound(JPH::StaticCompoundShapeSettings& compound) {
	if (compound.mSubShapes.empty()) {
		return nullptr;
	}
	if (compound.mSubShapes.size() == 1) {
		const JPH::CompoundShapeSettings::SubShapeSettings& part = compound.mSubShapes.front();
		if (part.mPosition.IsNearZero()) {
			return part.mShapePtr;
		}
		JPH::RotatedTranslatedShapeSettings moved(part.mPosition, part.mRotation, part.mShapePtr);
		JPH::ShapeSettings::ShapeResult result = moved.Create();
		return result.IsValid() ? result.Get() : nullptr;
	}
	JPH::ShapeSettings::ShapeResult result = compound.Create();
	return result.IsValid() ? result.Get() : nullptr;
}

JPH::Vec3 boxCentre(const Box& box) {
	return JPH::Vec3(box.minX + box.maxX, box.minY + box.maxY, box.minZ + box.maxZ) * 0.5f;
}

JPH::Vec3 boxHalf(const Box& box) {
	return (JPH::Vec3(box.maxX - box.minX, box.maxY - box.minY, box.maxZ - box.minZ) * 0.5f).Abs();
}

JPH::Ref<JPH::Shape> boxShape(JPH::Vec3Arg half, uint8_t surface) {
	JPH::Vec3 safe = JPH::Vec3::sMax(half, JPH::Vec3::sReplicate(MinimumHalfExtent));
	return new JPH::BoxShape(safe, std::min(JPH::cDefaultConvexRadius, safe.ReduceMin()), surfaceMaterial(surface));
}

JPH::Vec3 pointOf(const Vertex& vertex) {
	return JPH::Vec3(vertex.x / VertexScale, vertex.y / VertexScale, vertex.z / VertexScale);
}

JPH::ShapeRefC meshShape(const Model& model) {
	JPH::VertexList vertices;
	vertices.reserve(model.vertices.size());
	for (const Vertex& vertex : model.vertices) {
		vertices.push_back(JPH::Float3(vertex.x / VertexScale, vertex.y / VertexScale, vertex.z / VertexScale));
	}
	int slots[SurfaceCount];
	std::fill(std::begin(slots), std::end(slots), -1);
	JPH::PhysicsMaterialList materials;
	JPH::IndexedTriangleList triangles;
	triangles.reserve(model.faces.size());
	for (const Face& face : model.faces) {
		int& slot = slots[face.material];
		if (slot < 0 && materials.size() == MaxMeshMaterials) {
			slot = 0;
		}
		if (slot < 0) {
			slot = int(materials.size());
			materials.push_back(surfaceMaterial(face.material));
		}
		triangles.push_back(JPH::IndexedTriangle(face.a, face.c, face.b, JPH::uint32(slot)));
	}
	JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles), std::move(materials));
	if (settings.mIndexedTriangles.empty()) {
		return nullptr;
	}
	JPH::ShapeSettings::ShapeResult result = settings.Create();
	return result.IsValid() ? result.Get() : nullptr;
}

JPH::ShapeRefC buildStatic(const Model& model) {
	JPH::StaticCompoundShapeSettings compound;
	for (const Sphere& sphere : model.spheres) {
		if (sphere.radius > 0.0f) {
			compound.AddShape(JPH::Vec3(sphere.x, sphere.y, sphere.z), JPH::Quat::sIdentity(), new JPH::SphereShape(sphere.radius, surfaceMaterial(sphere.material)));
		}
	}
	for (const Box& box : model.boxes) {
		compound.AddShape(boxCentre(box), JPH::Quat::sIdentity(), boxShape(boxHalf(box), box.material));
	}
	if (!model.faces.empty()) {
		JPH::ShapeRefC mesh = meshShape(model);
		if (mesh != nullptr) {
			compound.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), mesh);
		}
	}
	return finishCompound(compound);
}

uint8_t firstSurface(const Model& model) {
	if (!model.faces.empty()) {
		return model.faces.front().material;
	}
	if (!model.boxes.empty()) {
		return model.boxes.front().material;
	}
	return model.spheres.empty() ? 0 : model.spheres.front().material;
}

JPH::ShapeRefC buildDynamic(const Model& model) {
	if (model.faces.empty() && model.spheres.size() + model.boxes.size() == 1) {
		JPH::StaticCompoundShapeSettings single;
		if (!model.spheres.empty()) {
			const Sphere& sphere = model.spheres.front();
			single.AddShape(JPH::Vec3(sphere.x, sphere.y, sphere.z), JPH::Quat::sIdentity(), new JPH::SphereShape(std::max(sphere.radius, MinimumHalfExtent), surfaceMaterial(sphere.material)));
		} else {
			const Box& box = model.boxes.front();
			single.AddShape(boxCentre(box), JPH::Quat::sIdentity(), boxShape(boxHalf(box), box.material));
		}
		return finishCompound(single);
	}
	JPH::Array<JPH::Vec3> points;
	points.reserve(model.vertices.size() + model.boxes.size() * 8 + model.spheres.size() * 6);
	for (const Vertex& vertex : model.vertices) {
		points.push_back(pointOf(vertex));
	}
	for (const Box& box : model.boxes) {
		JPH::Vec3 centre = boxCentre(box);
		JPH::Vec3 half = boxHalf(box);
		for (int corner = 0; corner != 8; ++corner) {
			points.push_back(centre + JPH::Vec3(corner & 1 ? half.GetX() : -half.GetX(), corner & 2 ? half.GetY() : -half.GetY(), corner & 4 ? half.GetZ() : -half.GetZ()));
		}
	}
	for (const Sphere& sphere : model.spheres) {
		for (int axis = 0; axis != 3; ++axis) {
			JPH::Vec3 offset = JPH::Vec3::sZero();
			offset.SetComponent(axis, sphere.radius);
			points.push_back(JPH::Vec3(sphere.x, sphere.y, sphere.z) + offset);
			points.push_back(JPH::Vec3(sphere.x, sphere.y, sphere.z) - offset);
		}
	}
	if (points.size() < 4) {
		return nullptr;
	}
	uint8_t surface = firstSurface(model);
	JPH::ConvexHullShapeSettings hull(points, 0.02f, surfaceMaterial(surface));
	JPH::ShapeSettings::ShapeResult result = hull.Create();
	if (result.IsValid()) {
		return result.Get();
	}
	JPH::AABox bounds;
	for (JPH::Vec3 point : points) {
		bounds.Encapsulate(point);
	}
	JPH::StaticCompoundShapeSettings single;
	single.AddShape(bounds.GetCenter(), JPH::Quat::sIdentity(), boxShape(bounds.GetExtent(), surface));
	return finishCompound(single);
}

JPH::ShapeRefC buildDescribed(const ModelSettings& settings, uint8_t surface) {
	JPH::Ref<JPH::Shape> shape;
	JPH::Quat rotation = JPH::Quat::sIdentity();
	if (settings.shape == ModelShape::Sphere) {
		shape = new JPH::SphereShape(std::max(settings.radius, MinimumHalfExtent), surfaceMaterial(surface));
	} else if (settings.shape == ModelShape::Box) {
		shape = boxShape(JPH::Vec3(settings.size.x, settings.size.y, settings.size.z) * 0.5f, surface);
	} else if (settings.shape == ModelShape::Cylinder) {
		float half = std::max(settings.height * 0.5f, MinimumHalfExtent);
		float radius = std::max(settings.radius, MinimumHalfExtent);
		shape = new JPH::CylinderShape(half, radius, std::min(JPH::cDefaultConvexRadius, std::min(half, radius)), surfaceMaterial(surface));
		if (settings.axis == 0) {
			rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), 0.5f * JPH::JPH_PI);
		} else if (settings.axis == 2) {
			rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI);
		}
	} else {
		return nullptr;
	}
	JPH::Vec3 offset(settings.offset.x, settings.offset.y, settings.offset.z);
	if (offset.IsNearZero() && rotation.IsClose(JPH::Quat::sIdentity())) {
		return shape;
	}
	JPH::RotatedTranslatedShapeSettings moved(offset, rotation, shape);
	JPH::ShapeSettings::ShapeResult result = moved.Create();
	return result.IsValid() ? result.Get() : nullptr;
}

void buildPlacedShapes() {
	State& s = state();
	std::vector<int> ids;
	ids.reserve(s.placements.size());
	for (const Placement& placement : s.placements) {
		if (s.staticShapes.count(placement.modelid) == 0 && s.models.count(placement.modelid) != 0) {
			ids.push_back(placement.modelid);
		}
	}
	std::sort(ids.begin(), ids.end());
	ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
	std::sort(ids.begin(), ids.end(), [&](int first, int second) {
		return s.models.at(first).faces.size() > s.models.at(second).faces.size();
	});
	std::vector<JPH::ShapeRefC> shapes(ids.size());
	std::atomic<size_t> next(0);
	auto work = [&]() {
		for (size_t index = next++; index < ids.size(); index = next++) {
			shapes[index] = buildStatic(s.models.at(ids[index]));
		}
	};
	unsigned count = std::clamp(unsigned(std::max(s.settings.threads, 1)), 1u, MaxBuildThreads);
	std::vector<std::thread> threads;
	for (unsigned index = 1; index < count; ++index) {
		threads.emplace_back(work);
	}
	work();
	for (std::thread& thread : threads) {
		thread.join();
	}
	for (size_t index = 0; index != ids.size(); ++index) {
		s.staticShapes.emplace(ids[index], shapes[index]);
	}
}

}

State& state() {
	static State* instance = new State();
	return *instance;
}

JPH::ObjectLayer staticLayer() {
	return StaticLayer;
}

JPH::ObjectLayer movingLayer() {
	return MovingLayer;
}

JPH::ObjectLayer playerLayer() {
	return PlayerLayer;
}

uint64_t userData(Kind kind, int index) {
	return (uint64_t(kind) << 32) | uint32_t(index);
}

Kind kindOf(uint64_t data) {
	return Kind(uint32_t(data >> 32));
}

int indexOf(uint64_t data) {
	return int(uint32_t(data));
}

JPH::CollisionGroup bodyGroup(const JPH::BodyID& id, Kind kind, uint32_t collisions) {
	State& s = state();
	return JPH::CollisionGroup(s.worlds, id.GetIndex(), (uint32_t(kind) << 8) | (collisions & CollideAll));
}

JPH::BodyID addBody(const JPH::BodyCreationSettings& settings, int world, int interior, JPH::EActivation activation) {
	State& s = state();
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	JPH::Body* body = bodies.CreateBody(settings);
	if (body == nullptr) {
		return JPH::BodyID();
	}
	JPH::BodyID id = body->GetID();
	s.bodyContexts[id.GetIndex()] = { world, interior };
	body->SetCollisionGroup(bodyGroup(id, kindOf(settings.mUserData), CollideAll));
	rememberBody(id, settings.mUserData);
	bodies.AddBody(id, activation);
	return id;
}

bool mapMatches(uint32_t placement, const Context& context) {
	const State& s = state();
	return (s.colAndreas || context.interior < 0 || s.placements[placement].interior == context.interior)
		&& !mapHidden(placement, context.world);
}

void wakeAround(const JPH::BodyID& id) {
	JPH::BodyInterface& bodies = state().system->GetBodyInterface();
	JPH::AABox box = bodies.GetTransformedShape(id).GetWorldSpaceBounds();
	box.ExpandBy(JPH::Vec3::sReplicate(0.1f));
	bodies.ActivateBodiesInAABox(box, {}, {});
}

void rememberBody(const JPH::BodyID& id, uint64_t data) {
	State& s = state();
	if (id.GetIndex() < s.userDataById.size()) {
		s.userDataById[id.GetIndex()] = data;
	}
}

uint64_t recalledData(const JPH::BodyID& id) {
	State& s = state();
	return id.GetIndex() < s.userDataById.size() ? s.userDataById[id.GetIndex()] : 0;
}

JPH::Quat toJolt(Vector3 euler) {
	GTAQuat rotation(euler);
	return JPH::Quat(-rotation.q.x, -rotation.q.y, -rotation.q.z, rotation.q.w).Normalized();
}

Vector3 toEuler(JPH::QuatArg rotation) {
	return GTAQuat(rotation.GetW(), -rotation.GetX(), -rotation.GetY(), -rotation.GetZ()).ToEuler();
}

const JPH::PhysicsMaterial* surfaceMaterial(uint8_t surface) {
	return &surfaceMaterials[surface];
}

uint8_t surfaceOf(const JPH::Body& body, const JPH::SubShapeID& part) {
	const JPH::PhysicsMaterial* material = body.GetShape()->GetMaterial(part);
	uintptr_t address = reinterpret_cast<uintptr_t>(material);
	uintptr_t first = reinterpret_cast<uintptr_t>(&surfaceMaterials[0]);
	if (address < first || address >= first + sizeof surfaceMaterials) {
		return 0;
	}
	return uint8_t(static_cast<const SurfaceMaterial*>(material) - surfaceMaterials);
}

void startWorld(int maxBodies) {
	State& s = state();
	JPH::RegisterDefaultAllocator();
	JPH::Trace = trace;
	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();
	s.temp = std::make_unique<JPH::TempAllocatorImpl>(32 * 1024 * 1024);
	s.jobs = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::max(s.settings.threads, 0));
	s.worlds = new WorldFilter();
	s.userDataById.assign(size_t(maxBodies), 0);
	s.bodyContexts.assign(size_t(maxBodies), Context());
	s.system = std::make_unique<JPH::PhysicsSystem>();
	s.system->Init(JPH::uint(maxBodies), 0, 65536, 20480, broadPhaseLayers, layerBroadPhaseFilter, layerPairFilter);
	s.system->SetGravity(JPH::Vec3(0.0f, 0.0f, -StandardGravity));
	JPH::PhysicsSettings settings;
	settings.mMinVelocityForRestitution = s.settings.bounceSpeed;
	settings.mSpeculativeContactDistance = s.settings.contactDistance;
	settings.mUseBodyPairContactCache = s.settings.contactCache;
	s.system->SetPhysicsSettings(settings);
	s.system->SetCombineRestitution(multiplyBounce);
}

void stopWorld() {
	State& s = state();
	s.system.reset();
	s.jobs.reset();
	s.temp.reset();
	s.staticShapes.clear();
	s.dynamicShapes.clear();
	s.vehicleShapes.clear();
	s.playerShape = nullptr;
	s.worlds = nullptr;
	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
}

bool loadDatabase(const std::string& path) {
	State& s = state();
	Database database;
	if (!readDatabase(path, database)) {
		return false;
	}
	s.colAndreas = database.colAndreas;
	s.models = std::move(database.models);
	s.placements = std::move(database.placements);
	s.surfaces = std::move(database.surfaces);
	s.waters = std::move(database.waters);
	indexWater();
	loadRoutes(std::move(database.roads), std::move(database.walks));
	return true;
}

int buildMap() {
	State& s = state();
	buildPlacedShapes();
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	std::vector<JPH::BodyID> created;
	created.reserve(s.placements.size());
	s.placementBodies.assign(s.placements.size(), JPH::BodyID());
	s.placementRemoved.assign(s.placements.size(), 0);
	s.placementHidden.assign(s.placements.size(), 0);
	s.hiddenWorlds.clear();
	for (uint32_t index = 0; index != uint32_t(s.placements.size()); ++index) {
		const Placement& placement = s.placements[index];
		JPH::ShapeRefC shape = staticShape(placement.modelid);
		if (shape == nullptr) {
			continue;
		}
		JPH::Quat rotation = JPH::Quat(placement.qx, placement.qy, placement.qz, -placement.qw).Normalized();
		JPH::BodyCreationSettings settings(shape, JPH::RVec3(placement.x, placement.y, placement.z), rotation, JPH::EMotionType::Static, StaticLayer);
		settings.mUserData = userData(Kind::Map, placement.modelid);
		settings.mRestitution = 1.0f;
		settings.mCollisionGroup = JPH::CollisionGroup(s.worlds, MapGroup | index, (uint32_t(Kind::Map) << 8) | CollideAll);
		JPH::Body* body = bodies.CreateBody(settings);
		if (body == nullptr) {
			break;
		}
		rememberBody(body->GetID(), settings.mUserData);
		created.push_back(body->GetID());
		s.placementBodies[index] = body->GetID();
	}
	if (!created.empty()) {
		JPH::BodyInterface::AddState add = bodies.AddBodiesPrepare(created.data(), int(created.size()));
		bodies.AddBodiesFinalize(created.data(), int(created.size()), add, JPH::EActivation::DontActivate);
		s.system->OptimizeBroadPhase();
	}
	s.mapBodies = int(created.size());
	return s.mapBodies;
}

JPH::ShapeRefC staticShape(int modelid) {
	State& s = state();
	auto cached = s.staticShapes.find(modelid);
	if (cached != s.staticShapes.end()) {
		return cached->second;
	}
	auto model = s.models.find(modelid);
	JPH::ShapeRefC shape = model == s.models.end() ? nullptr : buildStatic(model->second);
	s.staticShapes.emplace(modelid, shape);
	return shape;
}

JPH::ShapeRefC dynamicShape(int modelid) {
	State& s = state();
	auto cached = s.dynamicShapes.find(modelid);
	if (cached != s.dynamicShapes.end()) {
		return cached->second;
	}
	auto model = s.models.find(modelid);
	const ModelSettings* settings = findModelSettings(modelid);
	JPH::ShapeRefC shape;
	if (settings != nullptr && settings->shape != ModelShape::Collision) {
		shape = buildDescribed(*settings, model == s.models.end() ? 0 : firstSurface(model->second));
	} else if (model != s.models.end()) {
		shape = buildDynamic(model->second);
	}
	s.dynamicShapes.emplace(modelid, shape);
	return shape;
}

}
