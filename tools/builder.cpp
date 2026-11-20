#include "../src/database.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace physics;

namespace {

constexpr uint32_t Sector = 2048;
constexpr int SurfaceColumns = 29;

struct Entry {
	uint32_t offset = 0;
	uint32_t size = 0;
};

struct Archive {
	std::string path;
	std::vector<std::pair<std::string, Entry>> entries;
	std::unordered_map<std::string, Entry> byName;
};

struct Names {
	std::unordered_map<std::string, int> ids;
	std::unordered_map<int, std::string> names;
};

std::string lower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
	return text;
}

std::string trim(const std::string& text) {
	size_t first = text.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		return std::string();
	}
	size_t last = text.find_last_not_of(" \t\r\n");
	return text.substr(first, last - first + 1);
}

std::vector<std::string> fields(const std::string& line) {
	std::vector<std::string> parts;
	std::stringstream stream(line);
	std::string part;
	while (std::getline(stream, part, ',')) {
		parts.push_back(trim(part));
	}
	return parts;
}

std::string uncommented(const std::string& line, char mark) {
	size_t at = line.find(mark);
	return trim(at == std::string::npos ? line : line.substr(0, at));
}

std::filesystem::path resolve(const std::filesystem::path& base, const std::string& relative) {
	std::filesystem::path current = base;
	std::stringstream stream(relative);
	std::string part;
	while (std::getline(stream, part, '\\')) {
		std::stringstream inner(part);
		std::string piece;
		while (std::getline(inner, piece, '/')) {
			piece = trim(piece);
			if (piece.empty()) {
				continue;
			}
			std::filesystem::path exact = current / piece;
			if (std::filesystem::exists(exact)) {
				current = exact;
				continue;
			}
			bool found = false;
			std::error_code error;
			for (const auto& item : std::filesystem::directory_iterator(current, error)) {
				if (lower(item.path().filename().string()) == lower(piece)) {
					current = item.path();
					found = true;
					break;
				}
			}
			if (!found) {
				return std::filesystem::path();
			}
		}
	}
	return current;
}

bool readText(const std::filesystem::path& path, std::vector<std::string>& lines) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return false;
	}
	std::string line;
	while (std::getline(file, line)) {
		lines.push_back(line);
	}
	return true;
}

bool readArchive(const std::filesystem::path& path, Archive& archive) {
	std::ifstream file(path, std::ios::binary);
	char version[4];
	uint32_t count;
	if (!file.read(version, 4) || std::memcmp(version, "VER2", 4) != 0 || !file.read(reinterpret_cast<char*>(&count), 4)) {
		return false;
	}
	archive.path = path.string();
	for (uint32_t index = 0; index != count; ++index) {
		uint32_t offset;
		uint16_t streaming;
		uint16_t stored;
		char name[25] = {};
		if (!file.read(reinterpret_cast<char*>(&offset), 4) || !file.read(reinterpret_cast<char*>(&streaming), 2) || !file.read(reinterpret_cast<char*>(&stored), 2) || !file.read(name, 24)) {
			return false;
		}
		Entry entry = { offset * Sector, uint32_t(stored != 0 ? stored : streaming) * Sector };
		std::string key = lower(name);
		archive.entries.emplace_back(key, entry);
		archive.byName[key] = entry;
	}
	return true;
}

std::vector<char> readEntry(const Archive& archive, const Entry& entry) {
	std::ifstream file(archive.path, std::ios::binary);
	std::vector<char> data(entry.size);
	file.seekg(entry.offset);
	file.read(data.data(), std::streamsize(data.size()));
	data.resize(size_t(file.gcount()));
	return data;
}

template <typename T>
bool take(const std::vector<char>& data, size_t at, T& value) {
	if (at + sizeof(T) > data.size()) {
		return false;
	}
	std::memcpy(&value, data.data() + at, sizeof(T));
	return true;
}

void readCollisionFile(const std::vector<char>& data, std::vector<CollisionEntry>& found) {
	size_t at = 0;
	while (at + 32 <= data.size()) {
		CollisionEntry entry;
		size_t used;
		bool read = readCollision(data.data() + at, data.size() - at, entry, used);
		if (used == 0) {
			return;
		}
		if (read) {
			found.push_back(std::move(entry));
		}
		at += used;
	}
}

void readDefinitions(const std::filesystem::path& path, Names& names) {
	std::vector<std::string> lines;
	if (!readText(path, lines)) {
		return;
	}
	std::string section;
	for (const std::string& raw : lines) {
		std::string line = uncommented(raw, '#');
		if (line.empty()) {
			continue;
		}
		std::string word = lower(line);
		if (word == "objs" || word == "tobj" || word == "anim" || word == "path" || word == "2dfx" || word == "txdp" || word == "hier" || word == "cars" || word == "peds" || word == "weap") {
			section = word;
			continue;
		}
		if (word == "end") {
			section.clear();
			continue;
		}
		if (section != "objs" && section != "tobj" && section != "anim") {
			continue;
		}
		std::vector<std::string> parts = fields(line);
		if (parts.size() < 2) {
			continue;
		}
		char* stop = nullptr;
		long id = std::strtol(parts[0].c_str(), &stop, 10);
		if (stop == parts[0].c_str()) {
			continue;
		}
		names.ids[lower(parts[1])] = int(id);
		names.names[int(id)] = lower(parts[1]);
	}
}

bool readPlacements(const std::filesystem::path& path, std::vector<Placement>& placements) {
	std::vector<std::string> lines;
	if (!readText(path, lines)) {
		return false;
	}
	bool instances = false;
	for (const std::string& raw : lines) {
		std::string line = uncommented(raw, '#');
		if (line.empty()) {
			continue;
		}
		std::string word = lower(line);
		if (word.find(',') == std::string::npos) {
			instances = word == "inst";
			continue;
		}
		if (!instances) {
			continue;
		}
		std::vector<std::string> parts = fields(line);
		if (parts.size() < 10) {
			continue;
		}
		Placement placement;
		placement.modelid = std::atoi(parts[0].c_str());
		placement.interior = std::atoi(parts[2].c_str()) & 0xFF;
		placement.x = float(std::atof(parts[3].c_str()));
		placement.y = float(std::atof(parts[4].c_str()));
		placement.z = float(std::atof(parts[5].c_str()));
		placement.qx = float(std::atof(parts[6].c_str()));
		placement.qy = float(std::atof(parts[7].c_str()));
		placement.qz = float(std::atof(parts[8].c_str()));
		placement.qw = float(std::atof(parts[9].c_str()));
		placements.push_back(placement);
	}
	return true;
}

void readStream(const std::vector<char>& data, std::vector<Placement>& placements) {
	uint32_t count;
	uint32_t offset;
	if (data.size() < 32 || std::memcmp(data.data(), "bnry", 4) != 0 || !take(data, 4, count) || !take(data, 28, offset)) {
		return;
	}
	for (uint32_t index = 0; index != count; ++index) {
		float values[7];
		int32_t extra[3];
		size_t at = offset + size_t(index) * 40;
		if (!take(data, at, values) || !take(data, at + 28, extra)) {
			return;
		}
		Placement placement;
		placement.x = values[0];
		placement.y = values[1];
		placement.z = values[2];
		placement.qx = values[3];
		placement.qy = values[4];
		placement.qz = values[5];
		placement.qw = values[6];
		placement.modelid = extra[0];
		placement.interior = extra[1] & 0xFF;
		placements.push_back(placement);
	}
}

void readSurfaces(const std::filesystem::path& path, std::vector<Surface>& surfaces) {
	std::vector<std::string> lines;
	if (!readText(path, lines)) {
		return;
	}
	for (const std::string& raw : lines) {
		std::string line = trim(raw);
		if (line.empty() || line[0] == ';' || line[0] == '#') {
			continue;
		}
		std::stringstream stream(line);
		Surface surface;
		std::string skidmark;
		std::string effect;
		stream >> surface.name >> surface.group >> surface.tyreGrip >> surface.wetGrip >> skidmark >> effect;
		for (int column = 0; column != SurfaceColumns; ++column) {
			int value = 0;
			stream >> value;
			if (value != 0) {
				surface.flags |= 1u << column;
			}
		}
		if (!surface.name.empty()) {
			surfaces.push_back(surface);
		}
	}
}

void readWater(const std::filesystem::path& path, std::vector<Water>& waters) {
	std::vector<std::string> lines;
	if (!readText(path, lines)) {
		return;
	}
	for (const std::string& raw : lines) {
		std::stringstream stream(raw);
		std::vector<float> numbers;
		float number;
		while (stream >> number) {
			numbers.push_back(number);
		}
		if (numbers.size() != 29 && numbers.size() != 22) {
			continue;
		}
		Water water;
		water.corners = numbers.size() == 29 ? 4 : 3;
		for (int corner = 0; corner != water.corners; ++corner) {
			water.x[corner] = numbers[corner * 7];
			water.y[corner] = numbers[corner * 7 + 1];
			water.z[corner] = numbers[corner * 7 + 2];
		}
		waters.push_back(water);
	}
}

std::unordered_map<std::string, Dynamics> readDynamics(const std::filesystem::path& path) {
	std::unordered_map<std::string, Dynamics> dynamics;
	std::vector<std::string> lines;
	if (!readText(path, lines)) {
		return dynamics;
	}
	for (const std::string& raw : lines) {
		std::string line = uncommented(raw, ';');
		if (line.empty() || line[0] == '*') {
			continue;
		}
		std::replace(line.begin(), line.end(), ',', ' ');
		std::stringstream stream(line);
		std::string name;
		Dynamics values;
		int damageEffect = 0;
		int specialResponse = 0;
		int cameraAvoid = 0;
		int explosion = 0;
		stream >> name >> values.mass >> values.turnMass >> values.airResistance >> values.elasticity >> values.percentSubmerged >> values.uprootLimit >> values.damageMultiplier >> damageEffect >> specialResponse >> cameraAvoid >> explosion;
		if (!stream || name.empty()) {
			continue;
		}
		values.damageEffect = uint8_t(damageEffect);
		values.specialResponse = uint8_t(specialResponse);
		values.cameraAvoid = uint8_t(cameraAvoid);
		values.explosion = uint8_t(explosion);
		dynamics[lower(name)] = values;
	}
	return dynamics;
}

std::vector<std::string> listed(const std::filesystem::path& path, const std::string& kind) {
	std::vector<std::string> files;
	std::vector<std::string> lines;
	if (!readText(path, lines)) {
		return files;
	}
	for (const std::string& raw : lines) {
		std::string line = uncommented(raw, '#');
		size_t space = line.find_first_of(" \t");
		if (space == std::string::npos || lower(line.substr(0, space)) != lower(kind)) {
			continue;
		}
		files.push_back(trim(line.substr(space + 1)));
	}
	return files;
}

constexpr int PathAreas = 64;
constexpr uint32_t GameBoats = 1u << 7;
constexpr uint32_t GameSwitchedOff = 1u << 5;
constexpr uint32_t GameHighway = 1u << 13;

struct GameNode {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	uint16_t link = 0;
	uint8_t width = 0;
	uint32_t flags = 0;
};

struct GameNavi {
	float dx = 0.0f;
	float dy = 0.0f;
	uint8_t left = 0;
	uint8_t right = 0;
};

struct GameArea {
	uint32_t vehicles = 0;
	std::vector<GameNode> nodes;
	std::vector<GameNavi> navis;
	std::vector<std::pair<uint16_t, uint16_t>> links;
	std::vector<uint16_t> naviLinks;
	std::vector<uint8_t> crossings;
};

bool readPathArea(const std::vector<char>& data, GameArea& area) {
	uint32_t nodes;
	uint32_t vehicles;
	uint32_t peds;
	uint32_t navis;
	uint32_t links;
	if (!take(data, 0, nodes) || !take(data, 4, vehicles) || !take(data, 8, peds) || !take(data, 12, navis) || !take(data, 16, links)) {
		return false;
	}
	size_t at = 20;
	size_t needed = at + size_t(nodes) * 28 + size_t(navis) * 14 + size_t(links) * 4 + 768 + size_t(links) * 4;
	if (needed > data.size() || vehicles > nodes) {
		return false;
	}
	area.vehicles = vehicles;
	area.nodes.resize(nodes);
	for (GameNode& node : area.nodes) {
		int16_t position[3];
		take(data, at + 8, position);
		take(data, at + 16, node.link);
		take(data, at + 22, node.width);
		take(data, at + 24, node.flags);
		node.x = position[0] / 8.0f;
		node.y = position[1] / 8.0f;
		node.z = position[2] / 8.0f;
		at += 28;
	}
	area.navis.resize(navis);
	for (GameNavi& navi : area.navis) {
		int8_t direction[2];
		uint32_t flags;
		take(data, at + 8, direction);
		take(data, at + 10, flags);
		navi.dx = direction[0] / 100.0f;
		navi.dy = direction[1] / 100.0f;
		navi.left = uint8_t((flags >> 8) & 7);
		navi.right = uint8_t((flags >> 11) & 7);
		at += 14;
	}
	area.links.resize(links);
	for (auto& link : area.links) {
		take(data, at, link.first);
		take(data, at + 2, link.second);
		at += 4;
	}
	at += 768;
	area.naviLinks.resize(links);
	for (uint16_t& navi : area.naviLinks) {
		take(data, at, navi);
		at += 2;
	}
	at += links;
	area.crossings.resize(links);
	for (uint8_t& crossing : area.crossings) {
		take(data, at, crossing);
		at += 1;
	}
	return true;
}

bool readPaths(const std::vector<Archive>& archives, Database& database) {
	std::vector<GameArea> areas(PathAreas);
	for (int index = 0; index != PathAreas; ++index) {
		std::string name = "nodes" + std::to_string(index) + ".dat";
		const Archive* holder = nullptr;
		for (const Archive& archive : archives) {
			if (archive.byName.count(name) != 0) {
				holder = &archive;
				break;
			}
		}
		if (holder == nullptr || !readPathArea(readEntry(*holder, holder->byName.at(name)), areas[index])) {
			return false;
		}
	}
	std::vector<uint32_t> roadBase(PathAreas + 1, 0);
	std::vector<uint32_t> walkBase(PathAreas + 1, 0);
	for (int index = 0; index != PathAreas; ++index) {
		roadBase[index + 1] = roadBase[index] + areas[index].vehicles;
		walkBase[index + 1] = walkBase[index] + uint32_t(areas[index].nodes.size()) - areas[index].vehicles;
	}
	auto convert = [](uint32_t flags) {
		uint8_t converted = 0;
		converted |= (flags & GameHighway) != 0 ? PathHighway : 0;
		converted |= (flags & GameSwitchedOff) != 0 ? PathNoTraffic : 0;
		converted |= (flags & GameBoats) != 0 ? PathBoat : 0;
		return converted;
	};
	for (PathGraph* graph : { &database.roads, &database.walks }) {
		graph->nodes.clear();
		graph->first.clear();
		graph->links.clear();
	}
	for (int index = 0; index != PathAreas; ++index) {
		const GameArea& area = areas[index];
		for (uint32_t node = 0; node != uint32_t(area.nodes.size()); ++node) {
			const GameNode& game = area.nodes[node];
			bool vehicle = node < area.vehicles;
			PathGraph& graph = vehicle ? database.roads : database.walks;
			PathNode path;
			path.x = game.x;
			path.y = game.y;
			path.z = game.z;
			path.flags = convert(game.flags);
			path.width = game.width;
			graph.first.push_back(uint32_t(graph.links.size()));
			graph.nodes.push_back(path);
			uint32_t count = game.flags & 15;
			for (uint32_t slot = game.link; slot != game.link + count && slot < area.links.size(); ++slot) {
				uint16_t targetArea = area.links[slot].first;
				uint16_t targetNode = area.links[slot].second;
				if (targetArea >= PathAreas || targetNode >= areas[targetArea].nodes.size() || (targetNode < areas[targetArea].vehicles) != vehicle) {
					continue;
				}
				const GameNode& other = areas[targetArea].nodes[targetNode];
				PathLink link;
				link.target = vehicle ? roadBase[targetArea] + targetNode : walkBase[targetArea] + targetNode - areas[targetArea].vehicles;
				link.length = std::sqrt((other.x - game.x) * (other.x - game.x) + (other.y - game.y) * (other.y - game.y) + (other.z - game.z) * (other.z - game.z));
				link.lanes = 1;
				if (vehicle) {
					uint16_t naviArea = area.naviLinks[slot] >> 10;
					uint16_t naviIndex = area.naviLinks[slot] & 1023;
					if (naviArea < PathAreas && naviIndex < areas[naviArea].navis.size()) {
						const GameNavi& navi = areas[naviArea].navis[naviIndex];
						float along = (other.x - game.x) * navi.dx + (other.y - game.y) * navi.dy;
						if (navi.left != 0 || navi.right != 0) {
							link.lanes = along > 0.0f ? navi.left : navi.right;
						}
					}
				} else {
					link.flags |= (area.crossings[slot] & 1) != 0 ? PathCrossing : 0;
					link.flags |= (area.crossings[slot] & 2) != 0 ? PathLights : 0;
				}
				graph.links.push_back(link);
			}
		}
	}
	database.roads.first.push_back(uint32_t(database.roads.links.size()));
	database.walks.first.push_back(uint32_t(database.walks.links.size()));
	return true;
}

}

int main(int argc, char** argv) {
	if (argc < 3) {
		std::printf("usage: phydb <gta san andreas directory> <output file> [--no-samp]\n");
		return 1;
	}
	auto start = std::chrono::steady_clock::now();
	std::filesystem::path game = argv[1];
	std::string output = argv[2];
	bool samp = !(argc > 3 && std::string(argv[3]) == "--no-samp");
	std::filesystem::path gta = resolve(game, "data/gta.dat");
	std::filesystem::path defaults = resolve(game, "data/default.dat");
	if (gta.empty()) {
		std::printf("no data/gta.dat in %s\n", game.string().c_str());
		return 1;
	}

	Names names;
	for (const std::string& file : listed(defaults, "IDE")) {
		readDefinitions(resolve(game, file), names);
	}
	for (const std::string& file : listed(gta, "IDE")) {
		readDefinitions(resolve(game, file), names);
	}
	if (samp) {
		readDefinitions(resolve(game, "SAMP/SAMP.ide"), names);
		readDefinitions(resolve(game, "SAMP/CUSTOM.ide"), names);
	}

	std::vector<Archive> archives;
	std::vector<std::string> files = { "models/gta3.img", "models/gta_int.img" };
	if (samp) {
		files.push_back("SAMP/SAMPCOL.img");
	}
	for (const std::string& file : files) {
		Archive archive;
		std::filesystem::path path = resolve(game, file);
		if (path.empty() || !readArchive(path, archive)) {
			std::printf("cannot read %s\n", file.c_str());
			return 1;
		}
		archives.push_back(std::move(archive));
	}

	Database database;
	std::unordered_map<std::string, Dynamics> dynamics = readDynamics(resolve(game, "data/object.dat"));
	int collisionFiles = 0;
	int unnamed = 0;
	for (const Archive& archive : archives) {
		for (const auto& entry : archive.entries) {
			if (entry.first.size() < 4 || entry.first.compare(entry.first.size() - 4, 4, ".col") != 0) {
				continue;
			}
			++collisionFiles;
			std::vector<CollisionEntry> found;
			readCollisionFile(readEntry(archive, entry.second), found);
			for (CollisionEntry& collision : found) {
				if (collision.name.find("lod") != std::string::npos) {
					continue;
				}
				auto byId = names.names.find(collision.id);
				int modelid = -1;
				if (byId != names.names.end() && byId->second == collision.name) {
					modelid = collision.id;
				} else {
					auto byName = names.ids.find(collision.name);
					modelid = byName == names.ids.end() ? -1 : byName->second;
				}
				if (modelid < 0) {
					++unnamed;
					continue;
				}
				auto dynamic = dynamics.find(collision.name);
				if (dynamic != dynamics.end()) {
					collision.model.flags |= ModelDynamic;
					collision.model.dynamics = dynamic->second;
				}
				database.models[modelid] = std::move(collision.model);
			}
		}
	}

	size_t readFaces = 0;
	size_t readVertices = 0;
	size_t droppedFaces = 0;
	size_t empty = 0;
	for (auto& entry : database.models) {
		readFaces += entry.second.faces.size();
		readVertices += entry.second.vertices.size();
		droppedFaces += cleanModel(entry.second);
		empty += entry.second.spheres.empty() && entry.second.boxes.empty() && entry.second.faces.empty();
	}

	std::vector<Placement> placements;
	int streams = 0;
	for (const std::string& file : listed(gta, "IPL")) {
		if (lower(file).size() < 4 || lower(file).compare(file.size() - 4, 4, ".ipl") != 0) {
			continue;
		}
		std::filesystem::path path = resolve(game, file);
		if (path.empty() || !readPlacements(path, placements)) {
			continue;
		}
		std::string stem = lower(path.stem().string());
		for (int index = 0;; ++index) {
			std::string name = stem + "_stream" + std::to_string(index) + ".ipl";
			const Archive* holder = nullptr;
			for (const Archive& archive : archives) {
				if (archive.byName.count(name) != 0) {
					holder = &archive;
					break;
				}
			}
			if (holder == nullptr) {
				break;
			}
			readStream(readEntry(*holder, holder->byName.at(name)), placements);
			++streams;
		}
	}
	size_t withoutCollision = 0;
	for (const Placement& placement : placements) {
		auto model = database.models.find(placement.modelid);
		if (model == database.models.end()) {
			continue;
		}
		if (model->second.spheres.empty() && model->second.boxes.empty() && model->second.faces.empty()) {
			++withoutCollision;
			continue;
		}
		database.placements.push_back(placement);
	}

	if (!readPaths(archives, database)) {
		std::printf("cannot read the paths nodes0.dat to nodes63.dat\n");
		return 1;
	}
	readSurfaces(resolve(game, "data/surfinfo.dat"), database.surfaces);
	readWater(resolve(game, "data/water.dat"), database.waters);

	if (!writeDatabase(output, database)) {
		std::printf("cannot write %s\n", output.c_str());
		return 1;
	}
	size_t faces = 0;
	size_t vertices = 0;
	size_t dynamicModels = 0;
	for (const auto& entry : database.models) {
		faces += entry.second.faces.size();
		vertices += entry.second.vertices.size();
		dynamicModels += (entry.second.flags & ModelDynamic) != 0;
	}
	double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	std::printf("%zu models (%zu dynamic, %zu without collision) from %d collision files, %d unnamed\n", database.models.size(), dynamicModels, empty, collisionFiles, unnamed);
	std::printf("%zu faces and %zu vertices kept of %zu and %zu, %zu faces without area or given twice\n", faces, vertices, readFaces, readVertices, droppedFaces);
	std::printf("%zu placements of %zu read (%d streams, %zu of models without collision), %zu water polygons, %zu surfaces\n", database.placements.size(), placements.size(), streams, withoutCollision, database.waters.size(), database.surfaces.size());
	std::printf("%zu road nodes and %zu links, %zu walk nodes and %zu links\n", database.roads.nodes.size(), database.roads.links.size(), database.walks.nodes.size(), database.walks.links.size());
	std::printf("%s written in %.2f s, %ju bytes\n", output.c_str(), seconds, uintmax_t(std::filesystem::file_size(output)));
	return 0;
}
