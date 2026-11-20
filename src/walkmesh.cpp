#include "walkmesh.hpp"

#include <DetourCommon.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include <cmath>
#include <cstring>

namespace physics {

namespace {

static_assert(sizeof(dtPolyRef) == sizeof(uint64_t), "the navigation mesh needs 64 bit polygon references");

constexpr int MaxNodes = 8192;
constexpr int MaxPolys = 2048;
constexpr float Snap = 3.0f;
constexpr int RandomAttempts = 16;
constexpr unsigned short WalkFlag = 1;

void toMesh(const RoutePoint& point, float out[3]) {
	out[0] = point.x;
	out[1] = point.z;
	out[2] = -point.y;
}

RoutePoint fromMesh(const float* point) {
	return { point[0], -point[2], point[1] };
}

dtQueryFilter walkFilter() {
	dtQueryFilter filter;
	filter.setIncludeFlags(WalkFlag);
	filter.setExcludeFlags(0);
	return filter;
}

}

WalkMesh::~WalkMesh() {
	clear();
}

void WalkMesh::clear() {
	if (worker_.joinable()) {
		worker_.join();
	}
	dtFreeNavMeshQuery(query_);
	query_ = nullptr;
	dtFreeNavMesh(mesh_);
	mesh_ = nullptr;
	tiles_ = 0;
	bytes_ = 0;
	started_ = false;
	done_.store(false);
	readyAt_.store(0);
}

void WalkMesh::load(const std::string& path) {
	clear();
	started_ = true;
	worker_ = std::thread([this, path]() {
		NavmeshHeader header;
		NavmeshReader reader;
		dtNavMesh* mesh = reader.open(path, header) ? dtAllocNavMesh() : nullptr;
		dtNavMeshParams params;
		std::memset(&params, 0, sizeof params);
		dtVcopy(params.orig, header.origin);
		params.tileWidth = header.tileWidth;
		params.tileHeight = header.tileHeight;
		params.maxTiles = int(header.maxTiles);
		params.maxPolys = int(header.maxPolys);
		if (mesh != nullptr && dtStatusSucceed(mesh->init(&params))) {
			size_t bytes = 0;
			size_t count = 0;
			int32_t x;
			int32_t y;
			uint32_t size;
			while (reader.next(x, y, size)) {
				unsigned char* data = static_cast<unsigned char*>(dtAlloc(int(size), DT_ALLOC_PERM));
				if (data == nullptr || !reader.read(data, size)) {
					dtFree(data);
					break;
				}
				if (dtStatusFailed(mesh->addTile(data, int(size), DT_TILE_FREE_DATA, 0, nullptr))) {
					dtFree(data);
					continue;
				}
				bytes += size;
				++count;
			}
			dtNavMeshQuery* query = dtAllocNavMeshQuery();
			if (query != nullptr && dtStatusSucceed(query->init(mesh, MaxNodes))) {
				mesh_ = mesh;
				query_ = query;
				header_ = header;
				tiles_ = count;
				bytes_ = bytes;
				mesh = nullptr;
			} else {
				dtFreeNavMeshQuery(query);
			}
		}
		dtFreeNavMesh(mesh);
		readyAt_.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_relaxed);
		{
			std::lock_guard<std::mutex> lock(mutex_);
			done_.store(true, std::memory_order_release);
		}
		changed_.notify_all();
	});
}

bool WalkMesh::ready() const {
	return done_.load(std::memory_order_acquire);
}

std::chrono::steady_clock::time_point WalkMesh::readyAt() const {
	return std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(readyAt_.load(std::memory_order_relaxed)));
}

bool WalkMesh::loaded() const {
	if (!done_.load(std::memory_order_acquire)) {
		if (!started_) {
			return false;
		}
		std::unique_lock<std::mutex> lock(mutex_);
		changed_.wait(lock, [this]() {
			return done_.load(std::memory_order_acquire);
		});
	}
	return mesh_ != nullptr;
}

size_t WalkMesh::tileCount() const {
	return loaded() ? tiles_ : 0;
}

size_t WalkMesh::byteCount() const {
	return loaded() ? bytes_ : 0;
}

bool WalkMesh::place(const RoutePoint& at, float reach, uint64_t& ref, float point[3]) const {
	float centre[3];
	toMesh(at, centre);
	float extents[3] = { reach, reach * 2.0f, reach };
	dtQueryFilter filter = walkFilter();
	dtPolyRef found = 0;
	if (dtStatusFailed(query_->findNearestPoly(centre, extents, &filter, &found, point)) || found == 0) {
		return false;
	}
	ref = found;
	return true;
}

bool WalkMesh::nearest(const RoutePoint& at, float reach, RoutePoint& point) const {
	dtPolyRef ref;
	float found[3];
	if (!loaded() || !(reach > 0.0f) || !place(at, reach, ref, found)) {
		return false;
	}
	point = fromMesh(found);
	return true;
}

bool WalkMesh::random(const RoutePoint& at, float radius, float (*unit)(), RoutePoint& point) const {
	dtPolyRef start;
	float centre[3];
	if (!loaded() || !(radius > 0.0f) || !place(at, Snap, start, centre)) {
		return false;
	}
	dtQueryFilter filter = walkFilter();
	for (int attempt = 0; attempt != RandomAttempts; ++attempt) {
		dtPolyRef found = 0;
		float position[3];
		if (dtStatusFailed(query_->findRandomPointAroundCircle(start, centre, radius, &filter, unit, &found, position)) || found == 0) {
			return false;
		}
		if (dtVdist2DSqr(position, centre) <= radius * radius) {
			point = fromMesh(position);
			return true;
		}
	}
	return false;
}

bool WalkMesh::path(const RoutePoint& from, const RoutePoint& to, std::vector<RoutePoint>& points, float& length, bool partial) const {
	dtPolyRef startRef;
	dtPolyRef endRef;
	float start[3];
	float end[3];
	if (!loaded() || !place(from, Snap, startRef, start) || !place(to, Snap, endRef, end)) {
		return false;
	}
	dtQueryFilter filter = walkFilter();
	std::vector<dtPolyRef> polys(MaxPolys);
	int count = 0;
	dtStatus status = query_->findPath(startRef, endRef, start, end, &filter, polys.data(), &count, MaxPolys);
	if (dtStatusFailed(status) || count == 0) {
		return false;
	}
	if (dtStatusDetail(status, DT_PARTIAL_RESULT) || polys[size_t(count - 1)] != endRef) {
		float target[3];
		dtVcopy(target, end);
		if (!partial || dtStatusFailed(query_->closestPointOnPoly(polys[size_t(count - 1)], target, end, nullptr))) {
			return false;
		}
	}
	std::vector<float> corners(size_t(MaxPolys) * 3);
	int cornerCount = 0;
	if (dtStatusFailed(query_->findStraightPath(start, end, polys.data(), count, corners.data(), nullptr, nullptr, &cornerCount, MaxPolys, 0)) || cornerCount == 0) {
		return false;
	}
	points.clear();
	length = 0.0f;
	for (int index = 0; index != cornerCount; ++index) {
		RoutePoint point = fromMesh(&corners[size_t(index) * 3]);
		if (!points.empty()) {
			const RoutePoint& last = points.back();
			length += std::sqrt((point.x - last.x) * (point.x - last.x) + (point.y - last.y) * (point.y - last.y) + (point.z - last.z) * (point.z - last.z));
		}
		points.push_back(point);
	}
	return true;
}

bool WalkMesh::raycast(const RoutePoint& from, const RoutePoint& to, RoutePoint& hit) const {
	dtPolyRef startRef;
	float start[3];
	if (!loaded() || !place(from, Snap, startRef, start)) {
		hit = from;
		return true;
	}
	float end[3];
	toMesh(to, end);
	dtQueryFilter filter = walkFilter();
	float t = 0.0f;
	float normal[3];
	std::vector<dtPolyRef> polys(MaxPolys);
	int count = 0;
	if (dtStatusFailed(query_->raycast(startRef, start, end, &filter, &t, normal, polys.data(), &count, MaxPolys))) {
		hit = from;
		return true;
	}
	if (t > 1.0f) {
		return false;
	}
	float position[3];
	dtVlerp(position, start, end, t);
	float height = position[1];
	if (count > 0 && dtStatusSucceed(query_->getPolyHeight(polys[size_t(count - 1)], position, &height))) {
		position[1] = height;
	}
	hit = fromMesh(position);
	return true;
}

}
