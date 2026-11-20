#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace physics {

/**
 * @brief      Steps of a collision vertex in a metre. The game stores vertices as 16 bit integers in
 *             steps of 1/128 metre, and so does the database.
 */
constexpr float VertexScale = 128.0f;

/**
 * @brief      The model is a dynamic object of object.dat, such as a bin or a lamp post that can be
 *             knocked over in the game.
 */
constexpr uint8_t ModelDynamic = 1;

/**
 * @brief      A surface material of the game, one line of surfinfo.dat, numbered by its line.
 *
 * `flags` holds a bit for each of the 29 columns from SOFTLAND to CLIMBABLE that is not zero, in
 * the order of the file.
 */
struct Surface {
	std::string name;
	std::string group;
	float tyreGrip = 1.0f;
	float wetGrip = 0.0f;
	uint32_t flags = 0;
};

struct Sphere {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float radius = 0.0f;
	uint8_t material = 0;
};

struct Box {
	float minX = 0.0f;
	float minY = 0.0f;
	float minZ = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	float maxZ = 0.0f;
	uint8_t material = 0;
};

struct Vertex {
	int16_t x = 0;
	int16_t y = 0;
	int16_t z = 0;
};

struct Face {
	uint16_t a = 0;
	uint16_t b = 0;
	uint16_t c = 0;
	uint8_t material = 0;
};

/**
 * @brief      The box and the sphere that hold a model.
 */
struct Bounds {
	float minX = 0.0f;
	float minY = 0.0f;
	float minZ = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	float maxZ = 0.0f;
	float centreX = 0.0f;
	float centreY = 0.0f;
	float centreZ = 0.0f;
	float radius = 0.0f;
};

/**
 * @brief      How the game moves a dynamic object, one line of object.dat.
 */
struct Dynamics {
	float mass = 0.0f;
	float turnMass = 0.0f;
	float airResistance = 0.0f;
	float elasticity = 0.0f;
	float percentSubmerged = 0.0f;
	float uprootLimit = 0.0f;
	float damageMultiplier = 0.0f;
	uint8_t damageEffect = 0;
	uint8_t specialResponse = 0;
	uint8_t cameraAvoid = 0;
	uint8_t explosion = 0;
};

/**
 * @brief      The collision of a model, and for a dynamic object how the game moves it.
 */
struct Model {
	uint8_t flags = 0;
	Bounds bounds;
	Dynamics dynamics;
	std::vector<Sphere> spheres;
	std::vector<Box> boxes;
	std::vector<Vertex> vertices;
	std::vector<Face> faces;
};

/**
 * @brief      One model of a collision file of the game, with the name and id the file gives it.
 */
struct CollisionEntry {
	std::string name;
	uint16_t id = 0;
	Model model;
};

/**
 * @brief      A map object placed by the game, with the interior and the rotation of its IPL file.
 */
struct Placement {
	int modelid = 0;
	int interior = 0;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float qx = 0.0f;
	float qy = 0.0f;
	float qz = 0.0f;
	float qw = 1.0f;
};

/**
 * @brief      A water polygon of water.dat: a triangle, or a quad given as its four corners.
 */
struct Water {
	int corners = 4;
	float x[4] = {};
	float y[4] = {};
	float z[4] = {};
};

/**
 * @brief      The node is on a highway, where the traffic of the game drives fastest.
 */
constexpr uint8_t PathHighway = 1;

/**
 * @brief      The node is switched off, so the traffic of the game does not drive there: dirt roads,
 *             tracks, alleys and the ends of drives and car parks.
 */
constexpr uint8_t PathNoTraffic = 2;

/**
 * @brief      The node is on a lane for boats.
 */
constexpr uint8_t PathBoat = 4;

/**
 * @brief      The link crosses a road, for pedestrians.
 */
constexpr uint8_t PathCrossing = 1;

/**
 * @brief      The crossing has traffic lights.
 */
constexpr uint8_t PathLights = 2;

/**
 * @brief      A node of the paths of the game, where vehicles drive or pedestrians walk.
 */
struct PathNode {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	uint8_t flags = 0;
	uint8_t width = 0;
};

/**
 * @brief      A link from a node to the next. `lanes` is how many lanes go that way, `0` when the road
 *             is one way in the other direction.
 */
struct PathLink {
	uint32_t target = 0;
	float length = 0.0f;
	uint8_t lanes = 0;
	uint8_t flags = 0;
};

/**
 * @brief      A graph of path nodes. The links of node `n` go from `first[n]` to `first[n + 1]`.
 */
struct PathGraph {
	std::vector<PathNode> nodes;
	std::vector<uint32_t> first;
	std::vector<PathLink> links;
};

/**
 * @brief      Everything a database holds. `colAndreas` tells that it was read from a ColAndreas
 *             database.
 *
 * `roads` holds the paths vehicles drive and `walks` the paths pedestrians walk. A database of the
 * first version and a ColAndreas database have neither.
 */
struct Database {
	bool colAndreas = false;
	std::vector<Surface> surfaces;
	std::unordered_map<int, Model> models;
	std::vector<Placement> placements;
	std::vector<Water> waters;
	PathGraph roads;
	PathGraph walks;
};

/**
 * @brief      Read a database of this component, or a ColAndreas database, which has no materials,
 *             bounds, interiors or water.
 *
 * @return     - `true` the database was read
 *             - `false` the file is missing, truncated or of an unknown kind
 */
bool readDatabase(const std::string& path, Database& database);

/**
 * @brief      Write a database of this component.
 *
 * @return     - `true` the file was written
 *             - `false` it could not be opened
 */
bool writeDatabase(const std::string& path, const Database& database);

/**
 * @brief      Work out the bounds of a model from its spheres, boxes and vertices.
 */
Bounds measureModel(const Model& model);

/**
 * @brief      Read one entry of a collision file, from COLL to COL4, as the archives of the game and
 *             the collision plugin of a model file hold it. Every count, offset and vertex index is
 *             checked against the entry, so the data may come from anywhere.
 *
 * @param      data   The entry, which starts with its version.
 * @param      size   Bytes from `data` on.
 * @param      used   Bytes the entry takes, or `0` when its header is cut short or of no known
 *                    version, which ends the file. Model files often give a size that runs past
 *                    the data, so an entry is read from what there is.
 *
 * @return     - `true` the entry was read, with the bounds of its header
 *             - `false` it is cut short, of no known version, or holds a value that is not finite or a
 *               face with a vertex it does not have
 */
bool readCollision(const char* data, size_t size, CollisionEntry& entry, size_t& used);

/**
 * @brief      Weld the vertices of a model that lie at the same place, drop faces without area and
 *             faces given twice, and drop the vertices no face uses.
 *
 * @return     the number of faces dropped
 */
size_t cleanModel(Model& model);

}
