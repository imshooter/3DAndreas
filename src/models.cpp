#include "physics.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

namespace physics {

namespace {

constexpr int MaxDepth = 32;
constexpr float MinimumSize = 0.01f;
constexpr float MinimumMass = 0.05f;
constexpr float MaximumMass = 50000.0f;

enum class Substance {
	Unknown,
	Metal,
	Wood,
	Glass,
	Plastic,
	Rubber,
	Cardboard,
	Fabric,
	Stone,
	Plant,
	Count
};

struct SubstanceValues {
	float density;
	float friction;
	float bounce;
};

const SubstanceValues Substances[] = {
	{ 80.0f, 0.5f, 0.3f },
	{ 100.0f, 1.01f, 0.2f },
	{ 50.0f, 1.25f, 0.3f },
	{ 300.0f, 0.61f, 0.2f },
	{ 70.0f, 0.8f, 0.35f },
	{ 150.0f, 1.8f, 0.5f },
	{ 15.0f, 1.25f, 0.05f },
	{ 30.0f, 1.8f, 0.03f },
	{ 1500.0f, 1.25f, 0.1f },
	{ 50.0f, 1.8f, 0.05f },
};

const std::pair<const char*, Substance> SurfaceSubstances[] = {
	{ "GARAGE_DOOR", Substance::Metal },
	{ "THICK_METAL_PLATE", Substance::Metal },
	{ "SCAFFOLD_POLE", Substance::Metal },
	{ "LAMP_POST", Substance::Metal },
	{ "METAL_GATE", Substance::Metal },
	{ "METAL_CHAIN_FENCE", Substance::Metal },
	{ "GIRDER", Substance::Metal },
	{ "FIRE_HYDRANT", Substance::Metal },
	{ "CONTAINER", Substance::Metal },
	{ "NEWS_VENDOR", Substance::Metal },
	{ "CAR", Substance::Metal },
	{ "CAR_PANEL", Substance::Metal },
	{ "CAR_MOVINGCOMPONENT", Substance::Metal },
	{ "STAIRSMETAL", Substance::Metal },
	{ "FLOORMETAL", Substance::Metal },
	{ "THIN_METAL_SHEET", Substance::Metal },
	{ "METAL_BARREL", Substance::Metal },
	{ "METAL_DUMPSTER", Substance::Metal },
	{ "RAILTRACK", Substance::Metal },
	{ "WOOD_CRATES", Substance::Wood },
	{ "WOOD_SOLID", Substance::Wood },
	{ "WOOD_THIN", Substance::Wood },
	{ "WOOD_BENCH", Substance::Wood },
	{ "FLOORBOARD", Substance::Wood },
	{ "STAIRSWOOD", Substance::Wood },
	{ "P_OFFICEDESK", Substance::Wood },
	{ "P_711SHELF1", Substance::Wood },
	{ "P_711SHELF2", Substance::Wood },
	{ "P_711SHELF3", Substance::Wood },
	{ "P_RESTUARANTTABLE", Substance::Wood },
	{ "P_BARTABLE", Substance::Wood },
	{ "DOOR", Substance::Wood },
	{ "WOOD_PICKET_FENCE", Substance::Wood },
	{ "WOOD_SLATTED_FENCE", Substance::Wood },
	{ "WOOD_RANCH_FENCE", Substance::Wood },
	{ "GLASS", Substance::Glass },
	{ "GLASS_WINDOWS_LARGE", Substance::Glass },
	{ "GLASS_WINDOWS_SMALL", Substance::Glass },
	{ "UNBREAKABLE_GLASS", Substance::Glass },
	{ "PLASTIC", Substance::Plastic },
	{ "PLASTICBARRIER", Substance::Plastic },
	{ "PLASTIC_CONE", Substance::Plastic },
	{ "PLASTIC_DUMPSTER", Substance::Plastic },
	{ "WHEELBASE", Substance::Rubber },
	{ "RUBBER", Substance::Rubber },
	{ "CARDBOARDBOX", Substance::Cardboard },
	{ "TRANSPARENT_CLOTH", Substance::Fabric },
	{ "CARPET", Substance::Fabric },
	{ "STAIRSCARPET", Substance::Fabric },
	{ "BIN_BAG", Substance::Fabric },
	{ "FUCKED_CONCRETE", Substance::Stone },
	{ "TRANSPARENT_STONE", Substance::Stone },
	{ "STAIRSSTONE", Substance::Stone },
	{ "FLOORCONCRETE", Substance::Stone },
	{ "ROCK_DRY", Substance::Stone },
	{ "ROCK_WET", Substance::Stone },
	{ "ROCK_CLIFF", Substance::Stone },
	{ "CONCRETE_BEACH", Substance::Stone },
	{ "P_CONCRETE", Substance::Stone },
	{ "P_RUBBLE", Substance::Stone },
	{ "TARMAC", Substance::Stone },
	{ "TARMAC_FUCKED", Substance::Stone },
	{ "TARMAC_REALLYFUCKED", Substance::Stone },
	{ "PAVEMENT", Substance::Stone },
	{ "PAVEMENT_FUCKED", Substance::Stone },
	{ "GRAVEL", Substance::Stone },
	{ "PAINTED_GROUND", Substance::Stone },
	{ "HEDGE", Substance::Plant },
	{ "VEGETATION", Substance::Plant },
	{ "FLOWERBED", Substance::Plant },
	{ "CORNFIELD", Substance::Plant },
	{ "HAY_BALE", Substance::Plant },
};

Substance substanceOfSurface(int surface) {
	State& s = state();
	if (surface >= int(s.surfaces.size())) {
		return Substance::Unknown;
	}
	const std::string& name = s.surfaces[surface].name;
	for (const auto& [known, substance] : SurfaceSubstances) {
		if (name == known) {
			return substance;
		}
	}
	return Substance::Unknown;
}

JPH::Vec3 vertexPoint(const Vertex& vertex) {
	return JPH::Vec3(vertex.x, vertex.y, vertex.z) / VertexScale;
}

Substance substanceOf(const Model& model) {
	std::array<float, SurfaceCount> area = {};
	for (const Face& face : model.faces) {
		if (face.a >= model.vertices.size() || face.b >= model.vertices.size() || face.c >= model.vertices.size()) {
			continue;
		}
		JPH::Vec3 a = vertexPoint(model.vertices[face.a]);
		area[face.material] += 0.5f * (vertexPoint(model.vertices[face.b]) - a).Cross(vertexPoint(model.vertices[face.c]) - a).Length();
	}
	for (const Box& box : model.boxes) {
		float x = std::abs(box.maxX - box.minX);
		float y = std::abs(box.maxY - box.minY);
		float z = std::abs(box.maxZ - box.minZ);
		area[box.material] += 2.0f * (x * y + y * z + z * x);
	}
	for (const Sphere& sphere : model.spheres) {
		area[sphere.material] += 4.0f * JPH::JPH_PI * sphere.radius * sphere.radius;
	}
	std::array<float, size_t(Substance::Count)> share = {};
	for (int surface = 0; surface != SurfaceCount; ++surface) {
		if (area[surface] > 0.0f) {
			share[size_t(substanceOfSurface(surface))] += area[surface];
		}
	}
	return Substance(std::max_element(share.begin(), share.end()) - share.begin());
}

float partsVolume(const Model& model) {
	float volume = 0.0f;
	for (const Box& box : model.boxes) {
		volume += std::abs((box.maxX - box.minX) * (box.maxY - box.minY) * (box.maxZ - box.minZ));
	}
	for (const Sphere& sphere : model.spheres) {
		volume += 4.0f / 3.0f * JPH::JPH_PI * sphere.radius * sphere.radius * sphere.radius;
	}
	return volume;
}

struct Json {
	enum class Type {
		Null,
		Boolean,
		Number,
		Text,
		List,
		Object
	};

	Type type = Type::Null;
	bool boolean = false;
	double number = 0.0;
	std::string text;
	std::vector<Json> items;
	std::vector<std::pair<std::string, Json>> members;
};

class Parser {
public:
	explicit Parser(const std::string& source)
		: source_(source) {
	}

	bool parse(Json& value) {
		if (!parseValue(value, 0)) {
			return false;
		}
		skipSpace();
		return at_ == source_.size() || fail("text after the end of the document");
	}

	int line() const {
		return line_;
	}

	const char* error() const {
		return error_ == nullptr ? "unreadable" : error_;
	}

private:
	bool fail(const char* message) {
		if (error_ == nullptr) {
			error_ = message;
		}
		return false;
	}

	void skipSpace() {
		while (at_ < source_.size()) {
			char c = source_[at_];
			if (c == '\n') {
				++line_;
			} else if (c != ' ' && c != '\t' && c != '\r') {
				return;
			}
			++at_;
		}
	}

	bool take(char expected) {
		skipSpace();
		if (at_ < source_.size() && source_[at_] == expected) {
			++at_;
			return true;
		}
		return false;
	}

	bool word(const char* expected) {
		size_t length = std::strlen(expected);
		if (source_.compare(at_, length, expected) != 0) {
			return false;
		}
		at_ += length;
		return true;
	}

	bool digits() {
		size_t start = at_;
		while (at_ < source_.size() && source_[at_] >= '0' && source_[at_] <= '9') {
			++at_;
		}
		return at_ != start;
	}

	bool parseValue(Json& value, int depth) {
		skipSpace();
		if (at_ == source_.size()) {
			return fail("the document ends too early");
		}
		if (depth > MaxDepth) {
			return fail("lists and objects nest too deep");
		}
		char c = source_[at_];
		if (c == '{') {
			return parseObject(value, depth);
		}
		if (c == '[') {
			return parseList(value, depth);
		}
		if (c == '"') {
			value.type = Json::Type::Text;
			return parseText(value.text);
		}
		if (c == '-' || (c >= '0' && c <= '9')) {
			return parseNumber(value);
		}
		if (word("true") || word("false")) {
			value.type = Json::Type::Boolean;
			value.boolean = c == 't';
			return true;
		}
		if (word("null")) {
			return true;
		}
		return fail("expected a value");
	}

	bool parseObject(Json& value, int depth) {
		value.type = Json::Type::Object;
		++at_;
		if (take('}')) {
			return true;
		}
		do {
			skipSpace();
			if (at_ == source_.size() || source_[at_] != '"') {
				return fail("expected a name in quotes");
			}
			std::string name;
			if (!parseText(name)) {
				return false;
			}
			if (!take(':')) {
				return fail("expected ':' after a name");
			}
			value.members.emplace_back(std::move(name), Json());
			if (!parseValue(value.members.back().second, depth + 1)) {
				return false;
			}
		} while (take(','));
		return take('}') || fail("expected ',' or '}'");
	}

	bool parseList(Json& value, int depth) {
		value.type = Json::Type::List;
		++at_;
		if (take(']')) {
			return true;
		}
		do {
			value.items.emplace_back();
			if (!parseValue(value.items.back(), depth + 1)) {
				return false;
			}
		} while (take(','));
		return take(']') || fail("expected ',' or ']'");
	}

	bool parseText(std::string& text) {
		++at_;
		while (at_ < source_.size()) {
			char c = source_[at_++];
			if (c == '"') {
				return true;
			}
			if (c == '\n' || c == '\r') {
				break;
			}
			if (c != '\\') {
				text.push_back(c);
				continue;
			}
			if (at_ == source_.size()) {
				break;
			}
			c = source_[at_++];
			switch (c) {
			case '"':
			case '\\':
			case '/':
				text.push_back(c);
				break;
			case 'b':
				text.push_back('\b');
				break;
			case 'f':
				text.push_back('\f');
				break;
			case 'n':
				text.push_back('\n');
				break;
			case 'r':
				text.push_back('\r');
				break;
			case 't':
				text.push_back('\t');
				break;
			case 'u':
				if (!parseEscape(text)) {
					return false;
				}
				break;
			default:
				return fail("unknown escape in a text");
			}
		}
		return fail("a text has no closing quote on its line");
	}

	bool readHex(unsigned& code) {
		if (source_.size() - at_ < 4) {
			return false;
		}
		code = 0;
		for (int digit = 0; digit != 4; ++digit) {
			char c = source_[at_++];
			code <<= 4;
			if (c >= '0' && c <= '9') {
				code |= unsigned(c - '0');
			} else if (c >= 'a' && c <= 'f') {
				code |= unsigned(c - 'a' + 10);
			} else if (c >= 'A' && c <= 'F') {
				code |= unsigned(c - 'A' + 10);
			} else {
				return false;
			}
		}
		return true;
	}

	bool parseEscape(std::string& text) {
		unsigned code;
		if (!readHex(code)) {
			return fail("expected four hex digits after \\u");
		}
		if (code >= 0xD800 && code < 0xDC00) {
			unsigned low;
			if (source_.compare(at_, 2, "\\u") != 0) {
				return fail("a high surrogate without its low surrogate");
			}
			at_ += 2;
			if (!readHex(low) || low < 0xDC00 || low >= 0xE000) {
				return fail("a high surrogate without its low surrogate");
			}
			code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
		}
		if (code < 0x80) {
			text.push_back(char(code));
		} else if (code < 0x800) {
			text.push_back(char(0xC0 | (code >> 6)));
			text.push_back(char(0x80 | (code & 0x3F)));
		} else if (code < 0x10000) {
			text.push_back(char(0xE0 | (code >> 12)));
			text.push_back(char(0x80 | ((code >> 6) & 0x3F)));
			text.push_back(char(0x80 | (code & 0x3F)));
		} else {
			text.push_back(char(0xF0 | (code >> 18)));
			text.push_back(char(0x80 | ((code >> 12) & 0x3F)));
			text.push_back(char(0x80 | ((code >> 6) & 0x3F)));
			text.push_back(char(0x80 | (code & 0x3F)));
		}
		return true;
	}

	bool parseNumber(Json& value) {
		size_t start = at_;
		if (source_[at_] == '-') {
			++at_;
		}
		if (!digits()) {
			return fail("expected a digit");
		}
		if (at_ < source_.size() && source_[at_] == '.') {
			++at_;
			if (!digits()) {
				return fail("expected a digit after '.'");
			}
		}
		if (at_ < source_.size() && (source_[at_] == 'e' || source_[at_] == 'E')) {
			++at_;
			if (at_ < source_.size() && (source_[at_] == '+' || source_[at_] == '-')) {
				++at_;
			}
			if (!digits()) {
				return fail("expected a digit in the exponent");
			}
		}
		value.type = Json::Type::Number;
		std::from_chars_result result = std::from_chars(source_.data() + start, source_.data() + at_, value.number);
		return result.ec == std::errc() || fail("a number out of range");
	}

	const std::string& source_;
	size_t at_ = 0;
	int line_ = 1;
	const char* error_ = nullptr;
};

struct Field {
	const char* name;
	std::optional<float> ModelSettings::*target;
	bool positive;
};

const Field Fields[] = {
	{ "mass", &ModelSettings::mass, true },
	{ "friction", &ModelSettings::friction, false },
	{ "bounce", &ModelSettings::bounce, false },
	{ "linear_damping", &ModelSettings::linearDamping, false },
	{ "angular_damping", &ModelSettings::angularDamping, false },
	{ "rolling_friction", &ModelSettings::rollingFriction, false },
	{ "buoyancy", &ModelSettings::buoyancy, false },
	{ "sync_error", &ModelSettings::syncError, true },
};

const char* const Shapes[] = { "model", "sphere", "box", "cylinder" };
const char* const Axes[] = { "x", "y", "z" };

void report(const std::string& path, const char* format, ...) {
	char text[512];
	va_list arguments;
	va_start(arguments, format);
	std::vsnprintf(text, sizeof text, format, arguments);
	va_end(arguments);
	State& s = state();
	if (s.core != nullptr) {
		s.core->logLn(LogLevel::Warning, "[3DAndreas] %s: %s", path.c_str(), text);
	}
}

bool readModelId(const std::string& name, int& modelid) {
	const char* last = name.data() + name.size();
	std::from_chars_result result = std::from_chars(name.data(), last, modelid);
	return !name.empty() && result.ec == std::errc() && result.ptr == last;
}

bool readTriple(const Json& value, Vector3& vector) {
	if (value.type != Json::Type::List || value.items.size() != 3) {
		return false;
	}
	for (const Json& item : value.items) {
		if (item.type != Json::Type::Number) {
			return false;
		}
	}
	vector = Vector3(float(value.items[0].number), float(value.items[1].number), float(value.items[2].number));
	return true;
}

int choice(const Json& value, const char* const* names, int count) {
	if (value.type != Json::Type::Text) {
		return -1;
	}
	for (int index = 0; index != count; ++index) {
		if (value.text == names[index]) {
			return index;
		}
	}
	return -1;
}

bool readSetting(const std::string& path, int modelid, const std::string& name, const Json& value, ModelSettings& settings, bool& sized, bool& placed) {
	if (name == "name") {
		return true;
	}
	if (name == "shape") {
		int shape = choice(value, Shapes, 4);
		if (shape < 0) {
			report(path, "model %i: \"shape\" must be \"model\", \"sphere\", \"box\" or \"cylinder\", so the model is left out", modelid);
			return false;
		}
		settings.shape = ModelShape(shape);
		return true;
	}
	if (name == "axis") {
		int axis = choice(value, Axes, 3);
		if (axis < 0) {
			report(path, "model %i: \"axis\" must be \"x\", \"y\" or \"z\", so the model is left out", modelid);
			return false;
		}
		settings.axis = axis;
		return true;
	}
	if (name == "size" || name == "offset") {
		Vector3 vector;
		if (!readTriple(value, vector) || (name == "size" && (vector.x <= 0.0f || vector.y <= 0.0f || vector.z <= 0.0f))) {
			report(path, "model %i: \"%s\" must be a list of three %snumbers, so the model is left out", modelid, name.c_str(), name == "size" ? "positive " : "");
			return false;
		}
		if (name == "size") {
			settings.size = vector;
			sized = true;
		} else {
			settings.offset = vector;
			placed = true;
		}
		return true;
	}
	const Field* field = std::find_if(std::begin(Fields), std::end(Fields), [&name](const Field& candidate) {
		return name == candidate.name;
	});
	bool length = name == "radius" || name == "height";
	if (field == std::end(Fields) && !length && name != "drag") {
		report(path, "model %i: there is no setting \"%s\", so the model is left out", modelid, name.c_str());
		return false;
	}
	bool positive = length || (field != std::end(Fields) && field->positive);
	if (value.type != Json::Type::Number || value.number < 0.0 || (positive && value.number <= 0.0)) {
		report(path, "model %i: \"%s\" must be a %s, so the model is left out", modelid, name.c_str(), positive ? "positive number" : "number of at least 0");
		return false;
	}
	float number = float(value.number);
	if (field != std::end(Fields)) {
		settings.*(field->target) = number;
	} else if (name == "radius") {
		settings.radius = number;
	} else if (name == "height") {
		settings.height = number;
	} else {
		settings.drag = number;
	}
	return true;
}

bool completeShape(const std::string& path, int modelid, ModelSettings& settings, bool sized, bool placed) {
	State& s = state();
	auto found = s.models.find(modelid);
	const Model* model = found == s.models.end() ? nullptr : &found->second;
	if (settings.shape == ModelShape::Collision) {
		if (model == nullptr || (model->spheres.empty() && model->boxes.empty() && model->faces.empty())) {
			report(path, "model %i has no collision in the database, so it needs a \"shape\" and is left out", modelid);
			return false;
		}
		return true;
	}
	bool missing = settings.shape == ModelShape::Box ? !sized : (settings.radius <= 0.0f || (settings.shape == ModelShape::Cylinder && settings.height <= 0.0f));
	if (model == nullptr) {
		if (missing) {
			report(path, "model %i is not in the database, so its shape needs every size and is left out", modelid);
			return false;
		}
		return true;
	}
	const Bounds& bounds = model->bounds;
	Vector3 extent(bounds.maxX - bounds.minX, bounds.maxY - bounds.minY, bounds.maxZ - bounds.minZ);
	if (!placed) {
		settings.offset = Vector3(bounds.minX + bounds.maxX, bounds.minY + bounds.maxY, bounds.minZ + bounds.maxZ) * 0.5f;
	}
	if (settings.shape == ModelShape::Box && !sized) {
		settings.size = glm::max(extent, Vector3(MinimumSize));
	}
	if (settings.shape == ModelShape::Sphere && settings.radius <= 0.0f) {
		settings.radius = std::max((extent.x + extent.y + extent.z) / 6.0f, MinimumSize * 0.5f);
	}
	if (settings.shape == ModelShape::Cylinder) {
		float along = extent[settings.axis];
		float across = (extent.x + extent.y + extent.z - along) * 0.25f;
		if (settings.height <= 0.0f) {
			settings.height = std::max(along, MinimumSize);
		}
		if (settings.radius <= 0.0f) {
			settings.radius = std::max(across, MinimumSize * 0.5f);
		}
	}
	return true;
}

bool readModel(const std::string& path, int modelid, const Json& entry, ModelSettings& settings) {
	if (entry.type != Json::Type::Object) {
		report(path, "model %i is not an object, so it is left out", modelid);
		return false;
	}
	bool sized = false;
	bool placed = false;
	for (const auto& [name, value] : entry.members) {
		if (!readSetting(path, modelid, name, value, settings, sized, placed)) {
			return false;
		}
	}
	return completeShape(path, modelid, settings, sized, placed);
}

}

int loadModelSettings(const std::string& path) {
	State& s = state();
	s.modelSettings.clear();
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return -1;
	}
	std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	if (source.compare(0, 3, "\xEF\xBB\xBF") == 0) {
		source.erase(0, 3);
	}
	Json document;
	Parser parser(source);
	if (!parser.parse(document)) {
		report(path, "line %i: %s, so no model is read", parser.line(), parser.error());
		return 0;
	}
	if (document.type != Json::Type::Object) {
		report(path, "the file is not an object of model ids, so no model is read");
		return 0;
	}
	for (const auto& [name, entry] : document.members) {
		int modelid;
		if (!readModelId(name, modelid)) {
			report(path, "\"%s\" is not a model id, so it is left out", name.c_str());
			continue;
		}
		ModelSettings settings;
		if (!readModel(path, modelid, entry, settings)) {
			continue;
		}
		if (!s.modelSettings.emplace(modelid, settings).second) {
			report(path, "model %i is given twice, so only the first counts", modelid);
		}
	}
	return int(s.modelSettings.size());
}

const ModelSettings* findModelSettings(int modelid) {
	State& s = state();
	auto found = s.modelSettings.find(modelid);
	return found == s.modelSettings.end() ? nullptr : &found->second;
}

const ModelSettings& modelValues(int modelid, const JPH::Shape& shape) {
	State& s = state();
	auto cached = s.modelValues.find(modelid);
	if (cached != s.modelValues.end()) {
		return cached->second;
	}
	const ModelSettings* described = findModelSettings(modelid);
	ModelSettings values = described != nullptr ? *described : ModelSettings();
	const Model* model = findModel(modelid);
	Substance substance = model == nullptr ? Substance::Unknown : substanceOf(*model);
	const SubstanceValues& estimate = Substances[size_t(substance)];
	float volume = shape.GetVolume();
	if (values.shape == ModelShape::Collision && model != nullptr && model->faces.empty()) {
		volume = std::min(volume, partsVolume(*model));
	}
	if (!values.mass && volume > 0.0f) {
		values.mass = std::clamp(volume * estimate.density, MinimumMass, MaximumMass);
	}
	if (substance != Substance::Unknown) {
		values.friction = values.friction.value_or(estimate.friction);
		values.bounce = values.bounce.value_or(estimate.bounce);
	}
	return s.modelValues.emplace(modelid, values).first->second;
}

}
