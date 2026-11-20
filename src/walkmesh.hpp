#pragma once

#include "navmesh.hpp"
#include "routing.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class dtNavMesh;
class dtNavMeshQuery;

namespace physics {

/**
 * @brief      The navigation mesh of the map: where a pedestrian can walk, read from the file the
 *             `navbuild` tool makes, with Detour to find paths on it.
 *
 * The file is read on a thread of its own. A question asked before it is read waits for it, and
 * every question fails when there is no file.
 */
class WalkMesh {
public:
	WalkMesh() = default;
	WalkMesh(const WalkMesh&) = delete;
	WalkMesh& operator=(const WalkMesh&) = delete;
	~WalkMesh();

	/**
	 * @brief      Start reading a navigation mesh file.
	 */
	void load(const std::string& path);

	/**
	 * @brief      Stop reading and forget the mesh.
	 */
	void clear();

	/**
	 * @brief      Whether the file was read, without waiting.
	 */
	bool ready() const;

	/**
	 * @brief      When the file was read, once `ready` says so.
	 */
	std::chrono::steady_clock::time_point readyAt() const;

	/**
	 * @brief      Whether there is a mesh, after waiting for the file to be read.
	 */
	bool loaded() const;

	/**
	 * @brief      The nearest point of the mesh, within `reach` across and twice that up and down.
	 */
	bool nearest(const RoutePoint& at, float reach, RoutePoint& point) const;

	/**
	 * @brief      A random point of the mesh within `radius` of a point, that can be walked to from it.
	 *             Detour picks a polygon that reaches into the circle and a point in it, so a point
	 *             outside the circle is picked again, a few times at most.
	 *
	 * @param      unit  A random number from `0.0` to `1.0` each call.
	 */
	bool random(const RoutePoint& at, float radius, float (*unit)(), RoutePoint& point) const;

	/**
	 * @brief      The shortest walk between two points, as the corners where it turns.
	 *
	 * @param      partial  When the end cannot be reached, walk to the reachable point nearest it.
	 *
	 * @return     - `true` the walk reaches the end, or the point nearest it when `partial`
	 *             - `false` a point is off the mesh, or the walk cannot reach the end
	 */
	bool path(const RoutePoint& from, const RoutePoint& to, std::vector<RoutePoint>& points, float& length, bool partial = false) const;

	/**
	 * @brief      Whether something stops a straight walk between two points, and where it stops. A
	 *             start off the mesh stops the walk where it starts.
	 *
	 * @return     - `true` the walk is stopped at `hit`
	 *             - `false` the way is clear
	 */
	bool raycast(const RoutePoint& from, const RoutePoint& to, RoutePoint& hit) const;

	size_t tileCount() const;
	size_t byteCount() const;

private:
	bool place(const RoutePoint& at, float reach, uint64_t& ref, float point[3]) const;

	dtNavMesh* mesh_ = nullptr;
	dtNavMeshQuery* query_ = nullptr;
	NavmeshHeader header_;
	size_t tiles_ = 0;
	size_t bytes_ = 0;
	std::thread worker_;
	std::atomic<bool> done_{ false };
	std::atomic<std::chrono::steady_clock::rep> readyAt_{ 0 };
	bool started_ = false;
	mutable std::mutex mutex_;
	mutable std::condition_variable changed_;
};

}
