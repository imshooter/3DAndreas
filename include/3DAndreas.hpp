#pragma once

#include <sdk.hpp>

/**
 * @brief      The side of 3DAndreas that other open.mp components call in C++.
 *
 * A component asks for it in `onInit` with
 * `components->queryComponent<A3D::IComponent3DAndreas>()`, which gives
 * `nullptr` when 3DAndreas is not loaded. Every call reads the physics world
 * the server steps on its main thread, so it is made from that thread: in a
 * tick, an event or a native, never from a worker thread.
 */
namespace A3D {

/**
 * @brief      What a query may touch, with the values of `A3D_COLLIDE` in the
 *             include.
 */
enum Collide : uint32_t {
	CollideMap = 1,
	CollideObjects = 2,
	CollideStatic = 4,
	CollidePlayers = 8,
	CollideVehicles = 16,
	CollideWater = 32,
	CollideActors = 64,
	CollideAll = 95
};

/**
 * @brief      What a ray touched: the point, the side of the surface facing
 *             the ray, what it is as `A3D_HIT` in the include, its id in Pawn,
 *             and the surface of the game.
 */
struct RayHit {
	Vector3 point;
	Vector3 normal;
	int type = 0;
	int id = 0;
	int material = 0;
};

struct IComponent3DAndreas : public IComponent {
	PROVIDE_UID(0x7a3c91e45d2b8f16);

	/**
	 * @brief      Whether nothing of the kinds in `flags` lies between two
	 *             points in a virtual world, `-1` for every world.
	 */
	virtual bool isLineOfSight(Vector3 from, Vector3 to, int world, uint32_t flags) = 0;

	/**
	 * @brief      The first thing of the kinds in `flags` a segment touches.
	 *
	 * @return     `true` it touched something, written to `hit`
	 */
	virtual bool castRay(Vector3 from, Vector3 to, int world, uint32_t flags, RayHit& hit) = 0;

	/**
	 * @brief      The height of the first surface under a point, at most
	 *             `depth` below it.
	 *
	 * @return     `true` a surface was found, its height written to `z`
	 */
	virtual bool findGround(Vector3 point, float depth, int world, uint32_t flags, float& z) = 0;
};

}
