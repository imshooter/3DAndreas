#include "physics.hpp"

#include <Jolt/Physics/Body/BodyLock.h>

#include <algorithm>
#include <cmath>

namespace physics {

namespace {

constexpr float WaterCell = 100.0f;
constexpr float LinearDrag = 0.5f;
constexpr float AngularDrag = 0.01f;
constexpr float Edge = 0.00001f;
constexpr float CornerTime = 0.000001f;

bool overTriangle(const Water& water, int a, int b, int c, float x, float y, float& level) {
	float across = (water.y[b] - water.y[c]) * (water.x[a] - water.x[c]) + (water.x[c] - water.x[b]) * (water.y[a] - water.y[c]);
	if (std::abs(across) < Edge) {
		return false;
	}
	float first = ((water.y[b] - water.y[c]) * (x - water.x[c]) + (water.x[c] - water.x[b]) * (y - water.y[c])) / across;
	float second = ((water.y[c] - water.y[a]) * (x - water.x[c]) + (water.x[a] - water.x[c]) * (y - water.y[c])) / across;
	float third = 1.0f - first - second;
	if (first < -Edge || second < -Edge || third < -Edge) {
		return false;
	}
	level = first * water.z[a] + second * water.z[b] + third * water.z[c];
	return true;
}

bool overWater(const Water& water, float x, float y, float& level) {
	if (overTriangle(water, 0, 1, 2, x, y, level)) {
		return true;
	}
	return water.corners == 4 && overTriangle(water, 1, 3, 2, x, y, level);
}

bool crossTriangle(const Water& water, int a, int b, int c, Vector3 from, Vector3 delta, float& fraction) {
	Vector3 first(water.x[a], water.y[a], water.z[a]);
	Vector3 normal = glm::cross(Vector3(water.x[b], water.y[b], water.z[b]) - first, Vector3(water.x[c], water.y[c], water.z[c]) - first);
	float across = glm::dot(normal, delta);
	if (std::abs(across) < Edge) {
		return false;
	}
	float t = glm::dot(normal, first - from) / across;
	if (t < 0.0f || t > 1.0f) {
		return false;
	}
	Vector3 point = from + delta * t;
	float level;
	if (!overTriangle(water, a, b, c, point.x, point.y, level)) {
		return false;
	}
	fraction = t;
	return true;
}

}

void indexWater() {
	State& s = state();
	s.waterCells.clear();
	s.waterColumns = 0;
	s.waterRows = 0;
	if (s.waters.empty()) {
		return;
	}
	float minX = 1e30f;
	float minY = 1e30f;
	float maxX = -1e30f;
	float maxY = -1e30f;
	for (const Water& water : s.waters) {
		for (int corner = 0; corner != water.corners; ++corner) {
			minX = std::min(minX, water.x[corner]);
			minY = std::min(minY, water.y[corner]);
			maxX = std::max(maxX, water.x[corner]);
			maxY = std::max(maxY, water.y[corner]);
		}
	}
	s.waterMinX = minX;
	s.waterMinY = minY;
	s.waterColumns = std::max(int(std::ceil((maxX - minX) / WaterCell)), 1);
	s.waterRows = std::max(int(std::ceil((maxY - minY) / WaterCell)), 1);
	s.waterCells.assign(size_t(s.waterColumns) * size_t(s.waterRows), std::vector<int>());
	for (int index = 0; index != int(s.waters.size()); ++index) {
		const Water& water = s.waters[index];
		float low[2] = { 1e30f, 1e30f };
		float high[2] = { -1e30f, -1e30f };
		for (int corner = 0; corner != water.corners; ++corner) {
			low[0] = std::min(low[0], water.x[corner]);
			low[1] = std::min(low[1], water.y[corner]);
			high[0] = std::max(high[0], water.x[corner]);
			high[1] = std::max(high[1], water.y[corner]);
		}
		int firstColumn = std::clamp(int((low[0] - minX) / WaterCell), 0, s.waterColumns - 1);
		int lastColumn = std::clamp(int((high[0] - minX) / WaterCell), 0, s.waterColumns - 1);
		int firstRow = std::clamp(int((low[1] - minY) / WaterCell), 0, s.waterRows - 1);
		int lastRow = std::clamp(int((high[1] - minY) / WaterCell), 0, s.waterRows - 1);
		for (int row = firstRow; row <= lastRow; ++row) {
			for (int column = firstColumn; column <= lastColumn; ++column) {
				s.waterCells[size_t(row) * size_t(s.waterColumns) + size_t(column)].push_back(index);
			}
		}
	}
}

bool waterLevel(float x, float y, float& level) {
	State& s = state();
	if (s.waterCells.empty()) {
		return false;
	}
	int column = int(std::floor((x - s.waterMinX) / WaterCell));
	int row = int(std::floor((y - s.waterMinY) / WaterCell));
	if (column < 0 || row < 0 || column >= s.waterColumns || row >= s.waterRows) {
		level = 0.0f;
		return true;
	}
	bool found = false;
	for (int index : s.waterCells[size_t(row) * size_t(s.waterColumns) + size_t(column)]) {
		float height;
		if (overWater(s.waters[index], x, y, height) && (!found || height > level)) {
			level = height;
			found = true;
		}
	}
	return found;
}

bool waterAlong(Vector3 from, Vector3 to, float& fraction, float radius) {
	State& s = state();
	if (s.waterCells.empty()) {
		return false;
	}
	Vector3 delta = to - from;
	bool found = false;
	fraction = 1.0f;
	float maxX = s.waterMinX + float(s.waterColumns) * WaterCell;
	float maxY = s.waterMinY + float(s.waterRows) * WaterCell;
	float level;
	if (radius > 0.0f && waterLevel(from.x, from.y, level) && std::abs(from.z - level) < radius) {
		fraction = 0.0f;
		return true;
	}
	const float sides[2] = { -radius, radius };
	int count = radius > 0.0f ? 2 : 1;
	for (int side = 0; side != count; ++side) {
		float start = from.z + sides[side];
		float end = to.z + sides[side];
		if ((start >= 0.0f) == (end >= 0.0f)) {
			continue;
		}
		float t = start / (start - end);
		Vector3 point = from + delta * t;
		if (t < fraction && (point.x < s.waterMinX || point.x >= maxX || point.y < s.waterMinY || point.y >= maxY)) {
			fraction = t;
			found = true;
		}
	}
	if (std::min(from.x, to.x) >= maxX || std::min(from.y, to.y) >= maxY || std::max(from.x, to.x) < s.waterMinX || std::max(from.y, to.y) < s.waterMinY) {
		return found;
	}
	float enter = 0.0f;
	float leave = 1.0f;
	auto clip = [&](float start, float distance, float low, float high) {
		if (distance == 0.0f) {
			return start >= low && start < high;
		}
		float first = (low - start) / distance;
		float last = (high - start) / distance;
		if (first > last) {
			std::swap(first, last);
		}
		enter = std::max(enter, first);
		leave = std::min(leave, last);
		return enter <= leave;
	};
	if (!clip(from.x, delta.x, s.waterMinX, maxX) || !clip(from.y, delta.y, s.waterMinY, maxY)) {
		return found;
	}
	Vector3 entry = from + delta * enter;
	int column = std::clamp(int(std::floor((entry.x - s.waterMinX) / WaterCell)), 0, s.waterColumns - 1);
	int row = std::clamp(int(std::floor((entry.y - s.waterMinY) / WaterCell)), 0, s.waterRows - 1);
	int stepX = (delta.x > 0.0f) - (delta.x < 0.0f);
	int stepY = (delta.y > 0.0f) - (delta.y < 0.0f);
	float nextX = stepX == 0 ? INFINITY : (s.waterMinX + float(column + (stepX > 0)) * WaterCell - from.x) / delta.x;
	float nextY = stepY == 0 ? INFINITY : (s.waterMinY + float(row + (stepY > 0)) * WaterCell - from.y) / delta.y;
	float strideX = stepX == 0 ? INFINITY : WaterCell / std::abs(delta.x);
	float strideY = stepY == 0 ? INFINITY : WaterCell / std::abs(delta.y);
	std::vector<uint8_t> tested(s.waters.size(), 0);
	auto visit = [&](int cellX, int cellY) {
		if (cellX < 0 || cellY < 0 || cellX >= s.waterColumns || cellY >= s.waterRows) {
			return;
		}
		for (int index : s.waterCells[size_t(cellY) * size_t(s.waterColumns) + size_t(cellX)]) {
			if (tested[size_t(index)] != 0) {
				continue;
			}
			tested[size_t(index)] = 1;
			const Water& water = s.waters[size_t(index)];
			for (int side = 0; side != count; ++side) {
				Vector3 start = from + Vector3(0.0f, 0.0f, sides[side]);
				float t;
				if (crossTriangle(water, 0, 1, 2, start, delta, t) && t < fraction) {
					fraction = t;
					found = true;
				}
				if (water.corners == 4 && crossTriangle(water, 1, 3, 2, start, delta, t) && t < fraction) {
					fraction = t;
					found = true;
				}
			}
		}
	};
	while (column >= 0 && row >= 0 && column < s.waterColumns && row < s.waterRows) {
		visit(column, row);
		float next = std::min(nextX, nextY);
		if (next > leave) {
			break;
		}
		if (nextX < nextY - CornerTime) {
			column += stepX;
			nextX += strideX;
		} else if (nextY < nextX - CornerTime) {
			row += stepY;
			nextY += strideY;
		} else {
			visit(column + stepX, row);
			visit(column, row + stepY);
			column += stepX;
			row += stepY;
			nextX += strideX;
			nextY += strideY;
		}
	}
	return found;
}

bool underWater(Vector3 point, float& level, int world) {
	if (!waterLevel(point.x, point.y, level) || level < point.z) {
		return false;
	}
	return !mapBetween(Vector3(point.x, point.y, level), point, world);
}

void setBuoyancy(int index, float buoyancy, float drag) {
	State& s = state();
	Body* body = findBody(index);
	if (body == nullptr) {
		return;
	}
	body->buoyancy = buoyancy;
	body->drag = drag;
	s.floating.erase(std::remove(s.floating.begin(), s.floating.end(), index), s.floating.end());
	if (buoyancy > 0.0f) {
		s.floating.push_back(index);
	}
}

void applyBuoyancy(float elapsed) {
	State& s = state();
	if (s.floating.empty() || s.waterCells.empty()) {
		return;
	}
	JPH::BodyInterface& bodies = s.system->GetBodyInterface();
	JPH::Vec3 gravity = s.system->GetGravity();
	for (int index : s.floating) {
		const Body* body = findBody(index);
		if (body == nullptr || !bodies.IsActive(body->id)) {
			continue;
		}
		JPH::RVec3 centre;
		float bottom;
		{
			JPH::BodyLockRead lock(s.system->GetBodyLockInterface(), body->id);
			if (!lock.Succeeded()) {
				continue;
			}
			centre = lock.GetBody().GetCenterOfMassPosition();
			bottom = lock.GetBody().GetWorldSpaceBounds().mMin.GetZ();
		}
		Vector3 point(float(centre.GetX()), float(centre.GetY()), float(centre.GetZ()));
		float level;
		if (!waterLevel(point.x, point.y, level) || level < bottom) {
			continue;
		}
		if (level > point.z && mapBetween(Vector3(point.x, point.y, level), point, body->world)) {
			continue;
		}
		JPH::BodyLockWrite lock(s.system->GetBodyLockInterface(), body->id);
		if (!lock.Succeeded()) {
			continue;
		}
		JPH::Body& floating = lock.GetBody();
		if (!floating.ApplyBuoyancyImpulse(JPH::RVec3(point.x, point.y, level), JPH::Vec3::sAxisZ(), body->buoyancy, LinearDrag, AngularDrag, JPH::Vec3::sZero(), gravity, elapsed)) {
			continue;
		}
		float keep = std::max(1.0f - body->drag * elapsed, 0.0f);
		floating.SetLinearVelocityClamped(floating.GetLinearVelocity() * keep);
		floating.SetAngularVelocityClamped(floating.GetAngularVelocity() * keep);
	}
}

}
