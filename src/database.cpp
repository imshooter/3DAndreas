#include "database.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <unordered_set>

namespace physics {

namespace {

constexpr char Magic[4] = { 'P', 'H', 'D', 'B' };
constexpr uint32_t Version = 2;
constexpr uint32_t FirstVersion = 1;
constexpr uint16_t ColAndreasVersion = 2;
constexpr size_t SurfaceRecord = 14;
constexpr size_t SphereRecord = 17;
constexpr size_t BoxRecord = 25;
constexpr size_t VertexRecord = 6;
constexpr size_t FaceRecord = 7;
constexpr size_t PlacementRecord = 32;
constexpr size_t WaterRecord = 49;
constexpr size_t NodeRecord = 14;
constexpr size_t LinkRecord = 10;
constexpr size_t ColAndreasSphereRecord = 16;
constexpr size_t ColAndreasBoxRecord = 24;
constexpr size_t ColAndreasFaceRecord = 36;
constexpr size_t ColAndreasPlacementRecord = 30;

class Reader {
public:
	explicit Reader(const std::vector<char>& data) :
		data_(data) {
	}

	template <typename T>
	bool read(T& value) {
		if (at_ + sizeof(T) > data_.size()) {
			return false;
		}
		std::memcpy(&value, data_.data() + at_, sizeof(T));
		at_ += sizeof(T);
		return true;
	}

	bool holds(uint64_t count, size_t record) const {
		return count <= (data_.size() - at_) / record;
	}

	bool read(std::string& text) {
		uint8_t length;
		if (!read(length) || at_ + length > data_.size()) {
			return false;
		}
		text.assign(data_.data() + at_, length);
		at_ += length;
		return true;
	}

private:
	const std::vector<char>& data_;
	size_t at_ = 0;
};

class Writer {
public:
	template <typename T>
	void write(const T& value) {
		const char* bytes = reinterpret_cast<const char*>(&value);
		data_.insert(data_.end(), bytes, bytes + sizeof(T));
	}

	void write(const std::string& text) {
		uint8_t length = uint8_t(std::min<size_t>(text.size(), 255));
		write(length);
		data_.insert(data_.end(), text.begin(), text.begin() + length);
	}

	const std::vector<char>& data() const {
		return data_;
	}

private:
	std::vector<char> data_;
};

template <typename... T>
bool finite(T... values) {
	return (std::isfinite(values) && ...);
}

bool readGraph(Reader& reader, PathGraph& graph) {
	uint32_t nodes;
	uint32_t links;
	if (!reader.read(nodes) || !reader.read(links)) {
		return false;
	}
	if (!reader.holds(uint64_t(nodes) * NodeRecord + (uint64_t(nodes) + 1) * sizeof(uint32_t) + uint64_t(links) * LinkRecord, 1)) {
		return false;
	}
	graph.nodes.resize(nodes);
	graph.first.resize(size_t(nodes) + 1);
	graph.links.resize(links);
	for (PathNode& node : graph.nodes) {
		if (!reader.read(node.x) || !reader.read(node.y) || !reader.read(node.z) || !reader.read(node.flags) || !reader.read(node.width) || !finite(node.x, node.y, node.z)) {
			return false;
		}
	}
	uint32_t previous = 0;
	for (uint32_t& first : graph.first) {
		if (!reader.read(first) || first < previous || first > links) {
			return false;
		}
		previous = first;
	}
	if (graph.first.front() != 0 || graph.first.back() != links) {
		return false;
	}
	for (PathLink& link : graph.links) {
		if (!reader.read(link.target) || !reader.read(link.length) || !reader.read(link.lanes) || !reader.read(link.flags) || link.target >= nodes || !finite(link.length)) {
			return false;
		}
	}
	return true;
}

void writeGraph(Writer& writer, const PathGraph& graph) {
	writer.write(uint32_t(graph.nodes.size()));
	writer.write(uint32_t(graph.links.size()));
	for (const PathNode& node : graph.nodes) {
		writer.write(node.x);
		writer.write(node.y);
		writer.write(node.z);
		writer.write(node.flags);
		writer.write(node.width);
	}
	for (size_t index = 0; index != graph.nodes.size() + 1; ++index) {
		writer.write(index < graph.first.size() ? graph.first[index] : uint32_t(graph.links.size()));
	}
	for (const PathLink& link : graph.links) {
		writer.write(link.target);
		writer.write(link.length);
		writer.write(link.lanes);
		writer.write(link.flags);
	}
}

bool readFile(const std::string& path, std::vector<char>& data) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return false;
	}
	data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	return true;
}

int16_t quantise(float value) {
	return int16_t(std::lround(std::clamp(value * VertexScale, -32768.0f, 32767.0f)));
}

bool readDynamics(Reader& reader, Dynamics& dynamics) {
	return reader.read(dynamics.mass) && reader.read(dynamics.turnMass) && reader.read(dynamics.airResistance) && reader.read(dynamics.elasticity) && reader.read(dynamics.percentSubmerged) && reader.read(dynamics.uprootLimit) && reader.read(dynamics.damageMultiplier) && reader.read(dynamics.damageEffect) && reader.read(dynamics.specialResponse) && reader.read(dynamics.cameraAvoid) && reader.read(dynamics.explosion);
}

void writeDynamics(Writer& writer, const Dynamics& dynamics) {
	writer.write(dynamics.mass);
	writer.write(dynamics.turnMass);
	writer.write(dynamics.airResistance);
	writer.write(dynamics.elasticity);
	writer.write(dynamics.percentSubmerged);
	writer.write(dynamics.uprootLimit);
	writer.write(dynamics.damageMultiplier);
	writer.write(dynamics.damageEffect);
	writer.write(dynamics.specialResponse);
	writer.write(dynamics.cameraAvoid);
	writer.write(dynamics.explosion);
}

uint64_t vertexKey(const Vertex& vertex) {
	return (uint64_t(uint16_t(vertex.x)) << 32) | (uint64_t(uint16_t(vertex.y)) << 16) | uint16_t(vertex.z);
}

bool hasArea(const Vertex& a, const Vertex& b, const Vertex& c) {
	int64_t ux = int64_t(b.x) - a.x;
	int64_t uy = int64_t(b.y) - a.y;
	int64_t uz = int64_t(b.z) - a.z;
	int64_t vx = int64_t(c.x) - a.x;
	int64_t vy = int64_t(c.y) - a.y;
	int64_t vz = int64_t(c.z) - a.z;
	return uy * vz - uz * vy != 0 || uz * vx - ux * vz != 0 || ux * vy - uy * vx != 0;
}

bool readOwn(Reader& reader, Database& database) {
	uint32_t version;
	uint32_t surfaces;
	uint32_t models;
	uint32_t placements;
	uint32_t waters;
	if (!reader.read(version) || (version != Version && version != FirstVersion)) {
		return false;
	}
	if (!reader.read(surfaces) || !reader.read(models) || !reader.read(placements) || !reader.read(waters)) {
		return false;
	}
	if (!reader.holds(surfaces, SurfaceRecord)) {
		return false;
	}
	database.surfaces.resize(surfaces);
	for (Surface& surface : database.surfaces) {
		if (!reader.read(surface.name) || !reader.read(surface.group) || !reader.read(surface.tyreGrip) || !reader.read(surface.wetGrip) || !reader.read(surface.flags)) {
			return false;
		}
	}
	for (uint32_t index = 0; index != models; ++index) {
		uint16_t modelid;
		uint8_t reserved;
		uint16_t spheres;
		uint16_t boxes;
		uint32_t vertices;
		uint32_t faces;
		Model model;
		if (!reader.read(modelid) || !reader.read(model.flags) || !reader.read(reserved) || !reader.read(model.bounds)) {
			return false;
		}
		if ((model.flags & ModelDynamic) != 0 && !readDynamics(reader, model.dynamics)) {
			return false;
		}
		if (!reader.read(spheres) || !reader.read(boxes) || !reader.read(vertices) || !reader.read(faces)) {
			return false;
		}
		if (!reader.holds(uint64_t(spheres) * SphereRecord + uint64_t(boxes) * BoxRecord + uint64_t(vertices) * VertexRecord + uint64_t(faces) * FaceRecord, 1)) {
			return false;
		}
		model.spheres.resize(spheres);
		model.boxes.resize(boxes);
		model.vertices.resize(vertices);
		model.faces.resize(faces);
		for (Sphere& sphere : model.spheres) {
			if (!reader.read(sphere.x) || !reader.read(sphere.y) || !reader.read(sphere.z) || !reader.read(sphere.radius) || !reader.read(sphere.material)) {
				return false;
			}
			if (!finite(sphere.x, sphere.y, sphere.z, sphere.radius)) {
				return false;
			}
		}
		for (Box& box : model.boxes) {
			if (!reader.read(box.minX) || !reader.read(box.minY) || !reader.read(box.minZ) || !reader.read(box.maxX) || !reader.read(box.maxY) || !reader.read(box.maxZ) || !reader.read(box.material)) {
				return false;
			}
			if (!finite(box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ)) {
				return false;
			}
		}
		for (Vertex& vertex : model.vertices) {
			if (!reader.read(vertex.x) || !reader.read(vertex.y) || !reader.read(vertex.z)) {
				return false;
			}
		}
		for (Face& face : model.faces) {
			if (!reader.read(face.a) || !reader.read(face.b) || !reader.read(face.c) || !reader.read(face.material)) {
				return false;
			}
			if (face.a >= vertices || face.b >= vertices || face.c >= vertices) {
				return false;
			}
		}
		database.models[modelid] = std::move(model);
	}
	if (!reader.holds(placements, PlacementRecord)) {
		return false;
	}
	database.placements.resize(placements);
	for (Placement& placement : database.placements) {
		uint16_t modelid;
		uint8_t interior;
		uint8_t reserved;
		if (!reader.read(modelid) || !reader.read(interior) || !reader.read(reserved)) {
			return false;
		}
		if (!reader.read(placement.x) || !reader.read(placement.y) || !reader.read(placement.z)) {
			return false;
		}
		if (!reader.read(placement.qx) || !reader.read(placement.qy) || !reader.read(placement.qz) || !reader.read(placement.qw)) {
			return false;
		}
		if (!finite(placement.x, placement.y, placement.z, placement.qx, placement.qy, placement.qz, placement.qw)) {
			return false;
		}
		placement.modelid = modelid;
		placement.interior = interior;
	}
	if (!reader.holds(waters, WaterRecord)) {
		return false;
	}
	database.waters.resize(waters);
	for (Water& water : database.waters) {
		uint8_t corners;
		if (!reader.read(corners) || !reader.read(water.x) || !reader.read(water.y) || !reader.read(water.z)) {
			return false;
		}
		if (corners != 3 && corners != 4) {
			return false;
		}
		for (int corner = 0; corner != corners; ++corner) {
			if (!finite(water.x[corner], water.y[corner], water.z[corner])) {
				return false;
			}
		}
		water.corners = corners;
	}
	if (version == FirstVersion) {
		return true;
	}
	return readGraph(reader, database.roads) && readGraph(reader, database.walks);
}

bool readColAndreas(Reader& reader, Database& database) {
	uint16_t version;
	uint16_t models;
	uint32_t placements;
	if (!reader.read(version) || version != ColAndreasVersion || !reader.read(models) || !reader.read(placements)) {
		return false;
	}
	for (uint16_t index = 0; index != models; ++index) {
		uint16_t modelid;
		uint16_t spheres;
		uint16_t boxes;
		uint16_t faces;
		if (!reader.read(modelid) || !reader.read(spheres) || !reader.read(boxes) || !reader.read(faces)) {
			return false;
		}
		if (!reader.holds(uint64_t(spheres) * ColAndreasSphereRecord + uint64_t(boxes) * ColAndreasBoxRecord + uint64_t(faces) * ColAndreasFaceRecord, 1)) {
			return false;
		}
		Model model;
		model.spheres.resize(spheres);
		for (Sphere& sphere : model.spheres) {
			if (!reader.read(sphere.x) || !reader.read(sphere.y) || !reader.read(sphere.z) || !reader.read(sphere.radius) || !finite(sphere.x, sphere.y, sphere.z, sphere.radius)) {
				return false;
			}
		}
		model.boxes.resize(boxes);
		for (Box& box : model.boxes) {
			float centre[3];
			float half[3];
			if (!reader.read(centre) || !reader.read(half) || !finite(centre[0], centre[1], centre[2], half[0], half[1], half[2])) {
				return false;
			}
			box.minX = centre[0] - std::abs(half[0]);
			box.minY = centre[1] - std::abs(half[1]);
			box.minZ = centre[2] - std::abs(half[2]);
			box.maxX = centre[0] + std::abs(half[0]);
			box.maxY = centre[1] + std::abs(half[1]);
			box.maxZ = centre[2] + std::abs(half[2]);
		}
		std::unordered_map<uint64_t, uint16_t> known;
		known.reserve(size_t(faces) * 3);
		model.faces.reserve(faces);
		for (uint16_t face = 0; face != faces; ++face) {
			float corners[9];
			if (!reader.read(corners) || !finite(corners[0], corners[1], corners[2], corners[3], corners[4], corners[5], corners[6], corners[7], corners[8])) {
				return false;
			}
			uint16_t indices[3];
			for (int corner = 0; corner != 3; ++corner) {
				Vertex vertex = { quantise(corners[corner * 3]), quantise(corners[corner * 3 + 1]), quantise(corners[corner * 3 + 2]) };
				auto found = known.find(vertexKey(vertex));
				if (found == known.end()) {
					if (model.vertices.size() == 65536) {
						return false;
					}
					found = known.emplace(vertexKey(vertex), uint16_t(model.vertices.size())).first;
					model.vertices.push_back(vertex);
				}
				indices[corner] = found->second;
			}
			model.faces.push_back({ indices[0], indices[1], indices[2], 0 });
		}
		model.bounds = measureModel(model);
		database.models[modelid] = std::move(model);
	}
	if (!reader.holds(placements, ColAndreasPlacementRecord)) {
		return false;
	}
	database.placements.resize(placements);
	for (Placement& placement : database.placements) {
		uint16_t modelid;
		if (!reader.read(modelid) || !reader.read(placement.x) || !reader.read(placement.y) || !reader.read(placement.z)) {
			return false;
		}
		if (!reader.read(placement.qx) || !reader.read(placement.qy) || !reader.read(placement.qz) || !reader.read(placement.qw)) {
			return false;
		}
		if (!finite(placement.x, placement.y, placement.z, placement.qx, placement.qy, placement.qz, placement.qw)) {
			return false;
		}
		placement.modelid = modelid;
		placement.qw = -placement.qw;
	}
	return true;
}

}

Bounds measureModel(const Model& model) {
	float low[3] = { 1e30f, 1e30f, 1e30f };
	float high[3] = { -1e30f, -1e30f, -1e30f };
	auto take = [&](float x, float y, float z) {
		low[0] = std::min(low[0], x);
		low[1] = std::min(low[1], y);
		low[2] = std::min(low[2], z);
		high[0] = std::max(high[0], x);
		high[1] = std::max(high[1], y);
		high[2] = std::max(high[2], z);
	};
	for (const Sphere& sphere : model.spheres) {
		take(sphere.x - sphere.radius, sphere.y - sphere.radius, sphere.z - sphere.radius);
		take(sphere.x + sphere.radius, sphere.y + sphere.radius, sphere.z + sphere.radius);
	}
	for (const Box& box : model.boxes) {
		take(box.minX, box.minY, box.minZ);
		take(box.maxX, box.maxY, box.maxZ);
	}
	for (const Vertex& vertex : model.vertices) {
		take(vertex.x / VertexScale, vertex.y / VertexScale, vertex.z / VertexScale);
	}
	Bounds bounds;
	if (low[0] > high[0]) {
		return bounds;
	}
	bounds.minX = low[0];
	bounds.minY = low[1];
	bounds.minZ = low[2];
	bounds.maxX = high[0];
	bounds.maxY = high[1];
	bounds.maxZ = high[2];
	bounds.centreX = (low[0] + high[0]) * 0.5f;
	bounds.centreY = (low[1] + high[1]) * 0.5f;
	bounds.centreZ = (low[2] + high[2]) * 0.5f;
	bounds.radius = std::sqrt((high[0] - low[0]) * (high[0] - low[0]) + (high[1] - low[1]) * (high[1] - low[1]) + (high[2] - low[2]) * (high[2] - low[2])) * 0.5f;
	return bounds;
}

size_t cleanModel(Model& model) {
	std::unordered_map<uint64_t, uint16_t> first;
	std::vector<uint16_t> welded(model.vertices.size());
	for (size_t index = 0; index != model.vertices.size(); ++index) {
		welded[index] = first.emplace(vertexKey(model.vertices[index]), uint16_t(index)).first->second;
	}
	std::vector<Face> faces;
	std::unordered_set<uint64_t> seen;
	faces.reserve(model.faces.size());
	for (Face face : model.faces) {
		face.a = welded[face.a];
		face.b = welded[face.b];
		face.c = welded[face.c];
		if (face.a == face.b || face.b == face.c || face.c == face.a) {
			continue;
		}
		if (!hasArea(model.vertices[face.a], model.vertices[face.b], model.vertices[face.c])) {
			continue;
		}
		uint16_t turned[3] = { face.a, face.b, face.c };
		std::rotate(turned, std::min_element(turned, turned + 3), turned + 3);
		if (!seen.insert((uint64_t(turned[0]) << 32) | (uint64_t(turned[1]) << 16) | turned[2]).second) {
			continue;
		}
		faces.push_back(face);
	}
	std::vector<int> renumbered(model.vertices.size(), -1);
	for (const Face& face : faces) {
		renumbered[face.a] = 0;
		renumbered[face.b] = 0;
		renumbered[face.c] = 0;
	}
	std::vector<Vertex> vertices;
	for (size_t index = 0; index != model.vertices.size(); ++index) {
		if (renumbered[index] == 0) {
			renumbered[index] = int(vertices.size());
			vertices.push_back(model.vertices[index]);
		}
	}
	for (Face& face : faces) {
		face.a = uint16_t(renumbered[face.a]);
		face.b = uint16_t(renumbered[face.b]);
		face.c = uint16_t(renumbered[face.c]);
	}
	size_t dropped = model.faces.size() - faces.size();
	model.faces = std::move(faces);
	model.vertices = std::move(vertices);
	return dropped;
}

bool readDatabase(const std::string& path, Database& database) {
	std::vector<char> data;
	if (!readFile(path, data)) {
		return false;
	}
	Reader reader(data);
	char magic[4];
	if (!reader.read(magic)) {
		return false;
	}
	if (std::memcmp(magic, Magic, 4) == 0) {
		return readOwn(reader, database);
	}
	if (std::memcmp(magic, "cadf", 4) == 0) {
		database.colAndreas = true;
		return readColAndreas(reader, database);
	}
	return false;
}

bool writeDatabase(const std::string& path, const Database& database) {
	Writer writer;
	writer.write(Magic);
	writer.write(Version);
	writer.write(uint32_t(database.surfaces.size()));
	writer.write(uint32_t(database.models.size()));
	writer.write(uint32_t(database.placements.size()));
	writer.write(uint32_t(database.waters.size()));
	for (const Surface& surface : database.surfaces) {
		writer.write(surface.name);
		writer.write(surface.group);
		writer.write(surface.tyreGrip);
		writer.write(surface.wetGrip);
		writer.write(surface.flags);
	}
	std::vector<int> ids;
	ids.reserve(database.models.size());
	for (const auto& entry : database.models) {
		ids.push_back(entry.first);
	}
	std::sort(ids.begin(), ids.end());
	for (int modelid : ids) {
		const Model& model = database.models.at(modelid);
		writer.write(uint16_t(modelid));
		writer.write(model.flags);
		writer.write(uint8_t(0));
		writer.write(model.bounds);
		if ((model.flags & ModelDynamic) != 0) {
			writeDynamics(writer, model.dynamics);
		}
		writer.write(uint16_t(model.spheres.size()));
		writer.write(uint16_t(model.boxes.size()));
		writer.write(uint32_t(model.vertices.size()));
		writer.write(uint32_t(model.faces.size()));
		for (const Sphere& sphere : model.spheres) {
			writer.write(sphere.x);
			writer.write(sphere.y);
			writer.write(sphere.z);
			writer.write(sphere.radius);
			writer.write(sphere.material);
		}
		for (const Box& box : model.boxes) {
			writer.write(box.minX);
			writer.write(box.minY);
			writer.write(box.minZ);
			writer.write(box.maxX);
			writer.write(box.maxY);
			writer.write(box.maxZ);
			writer.write(box.material);
		}
		for (const Vertex& vertex : model.vertices) {
			writer.write(vertex.x);
			writer.write(vertex.y);
			writer.write(vertex.z);
		}
		for (const Face& face : model.faces) {
			writer.write(face.a);
			writer.write(face.b);
			writer.write(face.c);
			writer.write(face.material);
		}
	}
	for (const Placement& placement : database.placements) {
		writer.write(uint16_t(placement.modelid));
		writer.write(uint8_t(placement.interior));
		writer.write(uint8_t(0));
		writer.write(placement.x);
		writer.write(placement.y);
		writer.write(placement.z);
		writer.write(placement.qx);
		writer.write(placement.qy);
		writer.write(placement.qz);
		writer.write(placement.qw);
	}
	for (const Water& water : database.waters) {
		writer.write(uint8_t(water.corners));
		writer.write(water.x);
		writer.write(water.y);
		writer.write(water.z);
	}
	writeGraph(writer, database.roads);
	writeGraph(writer, database.walks);
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) {
		return false;
	}
	file.write(writer.data().data(), std::streamsize(writer.data().size()));
	return bool(file);
}

}
