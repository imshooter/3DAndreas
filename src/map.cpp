#include "physics.hpp"

#include <algorithm>
#include <cmath>

namespace physics {

namespace {

constexpr float GameFrames = 50.0f;

const Model* modelOf(int modelid) {
	State& s = state();
	auto found = s.models.find(modelid);
	return found == s.models.end() ? nullptr : &found->second;
}

bool nearPlacement(uint32_t index, int modelid, Vector3 centre, float radius, int interior) {
	State& s = state();
	const Placement& placement = s.placements[index];
	if (s.placementBodies[index].IsInvalid() || (modelid != -1 && placement.modelid != modelid)) {
		return false;
	}
	if (!s.colAndreas && interior >= 0 && placement.interior != interior) {
		return false;
	}
	Vector3 offset(placement.x - centre.x, placement.y - centre.y, placement.z - centre.z);
	return glm::dot(offset, offset) <= radius * radius;
}

bool removeBuilding(uint32_t index, int world) {
	State& s = state();
	if (s.placementRemoved[index] != 0) {
		return false;
	}
	JPH::BodyID id = s.placementBodies[index];
	if (world < 0) {
		wakeAround(id);
		s.system->GetBodyInterface().RemoveBody(id);
		s.placementRemoved[index] = 1;
		return true;
	}
	std::vector<int>& worlds = s.hiddenWorlds[index];
	if (std::find(worlds.begin(), worlds.end(), world) != worlds.end()) {
		return false;
	}
	worlds.push_back(world);
	s.placementHidden[index] = 1;
	wakeAround(id);
	return true;
}

bool restoreBuilding(uint32_t index, int world) {
	State& s = state();
	JPH::BodyID id = s.placementBodies[index];
	bool restored = false;
	if (world < 0) {
		if (s.placementRemoved[index] != 0) {
			s.system->GetBodyInterface().AddBody(id, JPH::EActivation::DontActivate);
			s.placementRemoved[index] = 0;
			restored = true;
		}
		if (s.placementHidden[index] != 0) {
			s.hiddenWorlds.erase(index);
			s.placementHidden[index] = 0;
			restored = true;
		}
	} else {
		auto found = s.hiddenWorlds.find(index);
		if (found == s.hiddenWorlds.end()) {
			return false;
		}
		std::vector<int>& worlds = found->second;
		auto at = std::find(worlds.begin(), worlds.end(), world);
		if (at == worlds.end()) {
			return false;
		}
		worlds.erase(at);
		if (worlds.empty()) {
			s.hiddenWorlds.erase(found);
			s.placementHidden[index] = 0;
		}
		restored = s.placementRemoved[index] == 0;
	}
	if (restored) {
		wakeAround(id);
	}
	return restored;
}

const Surface* surfaceAt(cell material) {
	State& s = state();
	return material < 0 || material >= cell(s.surfaces.size()) ? nullptr : &s.surfaces[size_t(material)];
}

cell AMX_NATIVE_CALL A3D_RemoveBuilding(AMX* amx, const cell* params) {
	State& s = state();
	Vector3 centre = readVector(params, 2);
	float radius = readFloat(params, 5);
	cell count = 0;
	for (uint32_t index = 0; index != uint32_t(s.placementBodies.size()); ++index) {
		if (nearPlacement(index, params[1], centre, radius, optional(params, 7, -1)) && removeBuilding(index, params[6])) {
			++count;
		}
	}
	return count;
}

cell AMX_NATIVE_CALL A3D_RestoreBuilding(AMX* amx, const cell* params) {
	State& s = state();
	Vector3 centre = readVector(params, 2);
	float radius = readFloat(params, 5);
	cell count = 0;
	for (uint32_t index = 0; index != uint32_t(s.placementBodies.size()); ++index) {
		if (nearPlacement(index, params[1], centre, radius, optional(params, 7, -1)) && restoreBuilding(index, params[6])) {
			++count;
		}
	}
	return count;
}

cell AMX_NATIVE_CALL A3D_GetWaterLevel(AMX* amx, const cell* params) {
	float level;
	if (!waterLevel(readFloat(params, 1), readFloat(params, 2), level)) {
		return false;
	}
	return writeFloat(amx, params[3], level);
}

cell AMX_NATIVE_CALL A3D_IsPointInWater(AMX* amx, const cell* params) {
	Vector3 point = readVector(params, 1);
	float level;
	if (!underWater(point, level, optional(params, 5, -1))) {
		return false;
	}
	writeFloat(amx, params[4], level - point.z);
	return true;
}

cell AMX_NATIVE_CALL A3D_GetModelBoundingBox(AMX* amx, const cell* params) {
	const Model* model = modelOf(params[1]);
	if (model == nullptr) {
		return false;
	}
	const Bounds& bounds = model->bounds;
	writeVector(amx, params, 2, Vector3(bounds.minX, bounds.minY, bounds.minZ));
	writeVector(amx, params, 5, Vector3(bounds.maxX, bounds.maxY, bounds.maxZ));
	return true;
}

cell AMX_NATIVE_CALL A3D_GetModelBoundingSphere(AMX* amx, const cell* params) {
	const Model* model = modelOf(params[1]);
	if (model == nullptr) {
		return false;
	}
	const Bounds& bounds = model->bounds;
	writeVector(amx, params, 2, Vector3(bounds.centreX, bounds.centreY, bounds.centreZ));
	writeFloat(amx, params[5], bounds.radius);
	return true;
}

cell AMX_NATIVE_CALL A3D_GetModelPhysics(AMX* amx, const cell* params) {
	const Model* model = modelOf(params[1]);
	if (model == nullptr || (model->flags & ModelDynamic) == 0) {
		return false;
	}
	const Dynamics& dynamics = model->dynamics;
	float buoyancy = dynamics.percentSubmerged > 0.0f ? 100.0f / dynamics.percentSubmerged : 0.0f;
	float damping = dynamics.airResistance > 0.0f && dynamics.airResistance < 1.0f ? -GameFrames * std::log(dynamics.airResistance) : 0.0f;
	writeFloat(amx, params[2], dynamics.mass);
	writeFloat(amx, params[3], dynamics.elasticity);
	writeFloat(amx, params[4], buoyancy);
	writeFloat(amx, params[5], damping);
	return true;
}

cell AMX_NATIVE_CALL A3D_GetMaterialName(AMX* amx, const cell* params) {
	State& s = state();
	const Surface* surface = surfaceAt(params[1]);
	IPawnScript* script = s.pawn->getScript(amx);
	cell* target = addressOf(amx, params[2]);
	if (surface == nullptr || script == nullptr || target == nullptr || params[3] <= 0) {
		return false;
	}
	script->SetString(target, StringView(surface->name), false, false, size_t(params[3]));
	return true;
}

cell AMX_NATIVE_CALL A3D_GetMaterialFlags(AMX* amx, const cell* params) {
	const Surface* surface = surfaceAt(params[1]);
	return surface == nullptr ? 0 : cell(surface->flags);
}

const AMX_NATIVE_INFO natives[] = {
	{ "A3D_RemoveBuilding", A3D_RemoveBuilding },
	{ "A3D_RestoreBuilding", A3D_RestoreBuilding },
	{ "A3D_GetWaterLevel", A3D_GetWaterLevel },
	{ "A3D_IsPointInWater", A3D_IsPointInWater },
	{ "A3D_GetModelBoundingBox", A3D_GetModelBoundingBox },
	{ "A3D_GetModelBoundingSphere", A3D_GetModelBoundingSphere },
	{ "A3D_GetModelPhysics", A3D_GetModelPhysics },
	{ "A3D_GetMaterialName", A3D_GetMaterialName },
	{ "A3D_GetMaterialFlags", A3D_GetMaterialFlags },
};

}

bool mapHidden(uint32_t placement, int world) {
	State& s = state();
	if (world < 0 || placement >= s.placementHidden.size() || s.placementHidden[placement] == 0) {
		return false;
	}
	auto found = s.hiddenWorlds.find(placement);
	return found != s.hiddenWorlds.end() && std::find(found->second.begin(), found->second.end(), world) != found->second.end();
}

void registerMapNatives(IPawnScript& script) {
	script.Register(natives, int(sizeof natives / sizeof natives[0]));
}

}
