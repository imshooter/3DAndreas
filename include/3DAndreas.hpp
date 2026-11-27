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
 * Its functions only grow at the end, so a component built against an older
 * copy of this header keeps working. Those added later name the version they
 * came in, and a component that calls them checks `componentVersion()` first,
 * since an older 3DAndreas does not have them. Its queries include every
 * interior unless they take one.
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
 * @brief      What a ray touched, the values of `A3D_HIT` in the include.
 */
enum Hit : int {
	HitMap,
	HitObject,
	HitStatic,
	HitPlayer,
	HitVehicle,
	HitWater,
	HitActor
};

/**
 * @brief      The flags of a surface of the game, the values of
 *             `A3D_MATERIAL` in the include.
 */
enum Material : uint32_t {
	MaterialSoftLanding = 1u << 0,
	MaterialSeeThrough = 1u << 1,
	MaterialShootThrough = 1u << 2,
	MaterialSand = 1u << 3,
	MaterialWater = 1u << 4,
	MaterialShallowWater = 1u << 5,
	MaterialBeach = 1u << 6,
	MaterialSteep = 1u << 7,
	MaterialGlass = 1u << 8,
	MaterialStairs = 1u << 9,
	MaterialSkateable = 1u << 10,
	MaterialPavement = 1u << 11,
	MaterialRough = 1u << 12,
	MaterialFlammable = 1u << 13,
	MaterialSparks = 1u << 14,
	MaterialNoSprint = 1u << 15,
	MaterialFootsteps = 1u << 16,
	MaterialFootDust = 1u << 17,
	MaterialCarDirt = 1u << 18,
	MaterialCarClean = 1u << 19,
	MaterialWheelGrass = 1u << 20,
	MaterialWheelGravel = 1u << 21,
	MaterialWheelMud = 1u << 22,
	MaterialWheelDust = 1u << 23,
	MaterialWheelSand = 1u << 24,
	MaterialWheelSpray = 1u << 25,
	MaterialPlants = 1u << 26,
	MaterialProps = 1u << 27,
	MaterialClimbable = 1u << 28
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

	/**
	 * @brief      The shortest walk on the navigation mesh between two points
	 *             on the ground, as the corners where it turns, the first one
	 *             where it starts. It does not wait for the mesh to be read:
	 *             until then there is no walk. The mesh is the map alone, the
	 *             same in every virtual world.
	 *
	 * @param      partial  When the end cannot be reached, walk to the
	 *                      reachable point nearest it.
	 *
	 * @return     The corners written to `points`, at most `size`, or `0`:
	 *             no mesh, a point off it or no walk
	 */
	virtual int findWalkPath(Vector3 from, Vector3 to, Vector3* points, int size, bool partial) = 0;

	/**
	 * @brief      Everything of the kinds in `flags` a segment touches, nearest
	 *             first, in an interior, `-1` for every interior. A hit on the
	 *             map has its model as `id`. Since 0.8.3.
	 *
	 * @return     The hits written to `hits`, at most `size`
	 */
	virtual int castRayAll(Vector3 from, Vector3 to, int world, int interior, uint32_t flags, RayHit* hits, int size) = 0;

	/**
	 * @brief      The flags of a surface of the game, the values of
	 *             `A3D_MATERIAL` in the include, `0` for an unknown one.
	 *             Since 0.8.3.
	 */
	virtual uint32_t getMaterialFlags(int material) = 0;

	/**
	 * @brief      Whether a model of the map is one the game knocks over or
	 *             opens, such as fences, lamp posts and doors. Since 0.8.3.
	 */
	virtual bool hasModelPhysics(int model) = 0;

	/**
	 * @brief      Whether a point lies inside the collision of the map in an
	 *             interior, `-1` for every interior. Since 0.8.3.
	 *
	 * @param      depth  How far under the top of the collision it lies, `0.0`
	 *                    inside a solid part
	 */
	virtual bool isPointInsideMap(Vector3 point, int world, int interior, float& depth) = 0;
};

}
