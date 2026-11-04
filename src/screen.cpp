#include "physics.hpp"

#include <cmath>

namespace physics {

namespace {

constexpr float ScreenWidth = 640.0f;
constexpr float ScreenHeight = 448.0f;
constexpr float NearPlane = 0.1f;
constexpr float DefaultFieldOfView = 70.0f;
constexpr float DefaultAspectRatio = 4.0f / 3.0f;
constexpr float DegreesToRadians = 3.14159265f / 180.0f;

struct View {
	Vector3 position;
	Vector3 front;
	Vector3 right;
	Vector3 up;
	float wide = 1.0f;
	float tall = 1.0f;
	int world = 0;
	int interior = 0;
};

bool viewOf(IPlayer& player, View& view) {
	const PlayerAimData& aim = player.getAimData();
	float length = glm::length(aim.camFrontVector);
	if (length <= 0.0f) {
		return false;
	}
	view.position = aim.camPos;
	view.front = aim.camFrontVector / length;
	Vector3 right = glm::cross(view.front, Vector3(0.0f, 0.0f, 1.0f));
	float side = glm::length(right);
	view.right = side > 0.0001f ? right / side : Vector3(1.0f, 0.0f, 0.0f);
	view.up = glm::cross(view.right, view.front);
	float fieldOfView = aim.camZoom > 1.0f ? aim.camZoom : DefaultFieldOfView;
	float aspectRatio = aim.aspectRatio > 0.5f ? aim.aspectRatio : DefaultAspectRatio;
	view.wide = std::tan(fieldOfView * 0.5f * DegreesToRadians);
	view.tall = view.wide / aspectRatio;
	view.world = player.getVirtualWorld();
	view.interior = int(player.getInterior());
	return true;
}

IPlayer* viewer(const cell* params, View& view) {
	IPlayer* player = state().players->get(params[1]);
	return player != nullptr && viewOf(*player, view) ? player : nullptr;
}

bool project(const View& view, Vector3 point, float& x, float& y) {
	Vector3 offset = point - view.position;
	float depth = glm::dot(offset, view.front);
	if (depth < NearPlane) {
		return false;
	}
	x = ScreenWidth * 0.5f * (1.0f + glm::dot(offset, view.right) / depth / view.wide);
	y = ScreenHeight * 0.5f * (1.0f - glm::dot(offset, view.up) / depth / view.tall);
	return true;
}

bool onScreen(const View& view, Vector3 point, bool sight) {
	float x;
	float y;
	if (!project(view, point, x, y) || x < 0.0f || x > ScreenWidth || y < 0.0f || y > ScreenHeight) {
		return false;
	}
	return !sight || !blocked(view.position, point, view.world, CollideMap | CollideStatic, view.interior);
}

Vector3 throughScreen(const View& view, float x, float y) {
	float across = (x / (ScreenWidth * 0.5f) - 1.0f) * view.wide;
	float upward = (1.0f - y / (ScreenHeight * 0.5f)) * view.tall;
	return glm::normalize(view.front + view.right * across + view.up * upward);
}

cell AMX_NATIVE_CALL A3D_WorldToScreen(AMX* amx, const cell* params) {
	View view;
	float x;
	float y;
	if (viewer(params, view) == nullptr || !project(view, readVector(params, 2), x, y)) {
		return false;
	}
	writeFloat(amx, params[5], x);
	return writeFloat(amx, params[6], y);
}

cell AMX_NATIVE_CALL A3D_ScreenToWorld(AMX* amx, const cell* params) {
	View view;
	if (viewer(params, view) == nullptr) {
		return false;
	}
	return writeVector(amx, params, 4, throughScreen(view, readFloat(params, 2), readFloat(params, 3)));
}

cell AMX_NATIVE_CALL A3D_GetScreenTarget(AMX* amx, const cell* params) {
	View view;
	IPlayer* player = viewer(params, view);
	if (player == nullptr) {
		return false;
	}
	JPH::BodyID own;
	JPH::BodyID ride;
	ownBodies(*player, own, ride);
	Vector3 end = view.position + throughScreen(view, readFloat(params, 2), readFloat(params, 3)) * readFloat(params, 10);
	QueryHit hit;
	if (!castRay(view.position, end, view.world, uint32_t(params[11]), false, hit, view.interior, own, ride)) {
		return false;
	}
	writeVector(amx, params, 4, toPoint(hit.point));
	writeCell(amx, params[7], cell(hit.type));
	writeCell(amx, params[8], hit.id);
	writeCell(amx, params[9], hit.material);
	return true;
}

cell AMX_NATIVE_CALL A3D_IsPointOnScreen(AMX* amx, const cell* params) {
	View view;
	return viewer(params, view) != nullptr && onScreen(view, readVector(params, 2), params[5] != 0);
}

cell AMX_NATIVE_CALL A3D_GetPlayersOnScreen(AMX* amx, const cell* params) {
	State& s = state();
	View view;
	IPlayer* player = viewer(params, view);
	if (player == nullptr) {
		return 0;
	}
	bool sight = params[4] != 0;
	float range = readFloat(params, 5);
	std::vector<std::pair<float, int>> found;
	for (IPlayer* other : s.players->entries()) {
		if (other == player || !present(*other) || other->getVirtualWorld() != view.world || int(other->getInterior()) != view.interior) {
			continue;
		}
		Vector3 target = other->getPosition() + Vector3(0.0f, 0.0f, ChestHeight);
		float distance = glm::distance(target, view.position);
		if (distance <= range && onScreen(view, target, sight)) {
			found.emplace_back(distance, other->getID());
		}
	}
	return writeNearest(amx, found, params[2], params[3]);
}

cell AMX_NATIVE_CALL A3D_GetVehiclesOnScreen(AMX* amx, const cell* params) {
	State& s = state();
	View view;
	IPlayer* player = viewer(params, view);
	if (player == nullptr || s.vehicles == nullptr) {
		return 0;
	}
	IPlayerVehicleData* data = queryExtension<IPlayerVehicleData>(player);
	IVehicle* ride = data != nullptr ? data->getVehicle() : nullptr;
	bool sight = params[4] != 0;
	float range = readFloat(params, 5);
	std::vector<std::pair<float, int>> found;
	for (IVehicle* vehicle : *s.vehicles) {
		if (vehicle == ride || vehicle->getVirtualWorld() != view.world || vehicle->getInterior() != view.interior) {
			continue;
		}
		Vector3 target = vehicle->getPosition();
		float distance = glm::distance(target, view.position);
		if (distance <= range && onScreen(view, target, sight)) {
			found.emplace_back(distance, vehicle->getID());
		}
	}
	return writeNearest(amx, found, params[2], params[3]);
}

cell AMX_NATIVE_CALL A3D_GetActorsOnScreen(AMX* amx, const cell* params) {
	State& s = state();
	View view;
	if (viewer(params, view) == nullptr || s.actors == nullptr) {
		return 0;
	}
	bool sight = params[4] != 0;
	float range = readFloat(params, 5);
	std::vector<std::pair<float, int>> found;
	for (IActor* actor : *s.actors) {
		if (actor->getVirtualWorld() != view.world) {
			continue;
		}
		Vector3 target = actor->getPosition() + Vector3(0.0f, 0.0f, ChestHeight);
		float distance = glm::distance(target, view.position);
		if (distance <= range && onScreen(view, target, sight)) {
			found.emplace_back(distance, actor->getID());
		}
	}
	return writeNearest(amx, found, params[2], params[3]);
}

const AMX_NATIVE_INFO natives[] = {
	{ "A3D_WorldToScreen", A3D_WorldToScreen },
	{ "A3D_ScreenToWorld", A3D_ScreenToWorld },
	{ "A3D_GetScreenTarget", A3D_GetScreenTarget },
	{ "A3D_IsPointOnScreen", A3D_IsPointOnScreen },
	{ "A3D_GetPlayersOnScreen", A3D_GetPlayersOnScreen },
	{ "A3D_GetVehiclesOnScreen", A3D_GetVehiclesOnScreen },
	{ "A3D_GetActorsOnScreen", A3D_GetActorsOnScreen },
};

}

void registerScreenNatives(IPawnScript& script) {
	script.Register(natives, int(sizeof natives / sizeof natives[0]));
}

}
