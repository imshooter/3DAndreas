#include "physics.hpp"

#include <chrono>

using namespace physics;

namespace {

constexpr int ExtraBodies = 20000;
const char* const OwnDatabase = "scriptfiles/3dandreas/world.db";
const char* const OwnModels = "scriptfiles/3dandreas/models.json";
const char* const OwnNavmesh = "scriptfiles/3dandreas/navmesh.db";
const char* const ColAndreasDatabase = "scriptfiles/colandreas/ColAndreas.cadb";

float readNumber(IConfig& config, const char* key, float fallback) {
	float* value = config.getFloat(key);
	if (value != nullptr) {
		return *value;
	}
	int* whole = config.getInt(key);
	return whole != nullptr ? float(*whole) : fallback;
}

void defaultNumber(IEarlyConfig& config, bool defaults, const char* key, float value) {
	if (defaults || config.getType(key) == ConfigOptionType_None) {
		config.setFloat(key, value);
	}
}

double since(std::chrono::steady_clock::time_point start) {
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

}

class PhysicsComponent final : public IComponent, public PawnEventHandler, public CoreEventHandler, public PlayerConnectEventHandler, public PoolEventHandler<IVehicle>, public PoolEventHandler<IActor> {
public:
	PROVIDE_UID(0x7a3c91e45d2b8f16);

	StringView componentName() const override {
		return "3DAndreas";
	}

	SemanticVersion componentVersion() const override {
		return SemanticVersion(A3D_VERSION_MAJOR, A3D_VERSION_MINOR, A3D_VERSION_PATCH, 0);
	}

	void provideConfiguration(ILogger& logger, IEarlyConfig& config, bool defaults) override {
		if (defaults || config.getType("3dandreas.database") == ConfigOptionType_None) {
			config.setString("3dandreas.database", OwnDatabase);
		}
		if (defaults || config.getType("3dandreas.models") == ConfigOptionType_None) {
			config.setString("3dandreas.models", OwnModels);
		}
		if (defaults || config.getType("3dandreas.navmesh") == ConfigOptionType_None) {
			config.setString("3dandreas.navmesh", OwnNavmesh);
		}
		if (defaults || config.getType("3dandreas.threads") == ConfigOptionType_None) {
			config.setInt("3dandreas.threads", 2);
		}
		defaultNumber(config, defaults, "3dandreas.rate", 60.0f);
		defaultNumber(config, defaults, "3dandreas.sync_error", 0.1f);
		defaultNumber(config, defaults, "3dandreas.sync_horizon", 0.3f);
		defaultNumber(config, defaults, "3dandreas.collision_speed", 1.0f);
		defaultNumber(config, defaults, "3dandreas.bounce_speed", 0.1f);
		defaultNumber(config, defaults, "3dandreas.contact_distance", 0.02f);
		if (defaults || config.getType("3dandreas.contact_cache") == ConfigOptionType_None) {
			config.setBool("3dandreas.contact_cache", false);
		}
		defaultNumber(config, defaults, "3dandreas.draw_distance", 0.0f);
		defaultNumber(config, defaults, "3dandreas.floor", -100.0f);
		defaultNumber(config, defaults, "3dandreas.max_updates", 1000.0f);
		if (defaults || config.getType("3dandreas.vehicles") == ConfigOptionType_None) {
			config.setBool("3dandreas.vehicles", true);
		}
	}

	void onLoad(ICore* core) override {
		State& s = state();
		s.core = core;
		s.players = &core->getPlayers();
		s.random.seed(std::random_device()());
		IConfig& config = core->getConfig();
		StringView database = config.getString("3dandreas.database");
		if (!database.empty()) {
			s.settings.database = std::string(database.data(), database.size());
		}
		StringView models = config.getString("3dandreas.models");
		if (!models.empty()) {
			s.settings.models = std::string(models.data(), models.size());
		}
		StringView navmesh = config.getString("3dandreas.navmesh");
		if (!navmesh.empty()) {
			s.settings.navmesh = std::string(navmesh.data(), navmesh.size());
		}
		int* threads = config.getInt("3dandreas.threads");
		if (threads != nullptr) {
			s.settings.threads = *threads;
		}
		s.settings.rate = std::max(readNumber(config, "3dandreas.rate", 60.0f), 10.0f);
		s.settings.syncError = readNumber(config, "3dandreas.sync_error", 0.1f);
		s.settings.syncHorizon = std::max(readNumber(config, "3dandreas.sync_horizon", 0.3f), 0.05f);
		s.settings.collisionSpeed = readNumber(config, "3dandreas.collision_speed", 1.0f);
		s.settings.bounceSpeed = std::max(readNumber(config, "3dandreas.bounce_speed", 0.1f), 0.0f);
		s.settings.contactDistance = std::max(readNumber(config, "3dandreas.contact_distance", 0.02f), 0.0005f);
		bool* contactCache = config.getBool("3dandreas.contact_cache");
		s.settings.contactCache = contactCache != nullptr && *contactCache;
		s.settings.drawDistance = readNumber(config, "3dandreas.draw_distance", 0.0f);
		s.settings.floor = readNumber(config, "3dandreas.floor", -100.0f);
		s.settings.maxUpdates = std::max(readNumber(config, "3dandreas.max_updates", 1000.0f), 10.0f);
		bool* vehicles = config.getBool("3dandreas.vehicles");
		s.settings.vehicles = vehicles == nullptr || *vehicles;

		auto start = std::chrono::steady_clock::now();
		bool loaded = loadDatabase(s.settings.database);
		if (!loaded && s.settings.database == OwnDatabase && loadDatabase(ColAndreasDatabase)) {
			s.settings.database = ColAndreasDatabase;
			loaded = true;
		}
		double read = since(start);
		s.walkMesh.load(s.settings.navmesh);
		start = std::chrono::steady_clock::now();
		int described = loadModelSettings(s.settings.models);
		double settings = since(start);
		startWorld(int(s.placements.size()) + ExtraBodies);
		listen();
		start = std::chrono::steady_clock::now();
		int built = loaded ? buildMap() : 0;
		double map = since(start);
		core->getEventDispatcher().addEventHandler(this);
		s.players->getPlayerConnectDispatcher().addEventHandler(this);
		if (!loaded) {
			core->logLn(LogLevel::Warning, "[3DAndreas] %s could not be read, so bodies collide only with each other and with static bodies.", s.settings.database.c_str());
		}
		core->printLn("3DAndreas " A3D_VERSION ": %i map objects from %u models, %u water polygons and %u surfaces of %s, read in %.0f ms and built in %.0f ms, %i threads", built, unsigned(s.models.size()), unsigned(s.waters.size()), unsigned(s.surfaces.size()), s.settings.database.c_str(), read, map, s.settings.threads);
		if (described >= 0) {
			core->printLn("3DAndreas " A3D_VERSION ": %i models described by %s, read in %.1f ms", described, s.settings.models.c_str(), settings);
		}
		if (loaded && s.colAndreas) {
			core->logLn(LogLevel::Warning, "[3DAndreas] %s is a ColAndreas database, so hits carry no surface and there is no water. Build %s with phydb.", s.settings.database.c_str(), OwnDatabase);
		}
	}

	void onInit(IComponentList* components) override {
		State& s = state();
		s.pawn = components->queryComponent<IPawnComponent>();
		if (s.pawn != nullptr) {
			s.pawn->getEventDispatcher().addEventHandler(this);
		}
		s.objects = components->queryComponent<IObjectsComponent>();
		s.vehicles = components->queryComponent<IVehiclesComponent>();
		if (s.vehicles != nullptr) {
			s.vehicles->getPoolEventDispatcher().addEventHandler(this);
		}
		s.actors = components->queryComponent<IActorsComponent>();
		if (s.actors != nullptr) {
			s.actors->getPoolEventDispatcher().addEventHandler(this);
		}
	}

	void onAmxLoad(IPawnScript& script) override {
		State& s = state();
		registerNatives(script);
		s.scripts[script.GetAMX()].api = &script;
	}

	void onAmxUnload(IPawnScript& script) override {
		State& s = state();
		forgetScript(script.GetAMX());
		forgetRoutes(script.GetAMX());
		s.scripts.erase(script.GetAMX());
	}

	void onTick(Microseconds elapsed, TimePoint now) override {
		State& s = state();
		if (s.system != nullptr) {
			step(double(elapsed.count()) / 1000000.0);
		}
		updateRoutes();
	}

	void onPlayerDisconnect(IPlayer& player, PeerDisconnectReason reason) override {
		removePlayer(player.getID());
		removeRoutePlayer(player.getID());
	}

	void onPoolEntryDestroyed(IVehicle& vehicle) override {
		removeVehicle(vehicle.getID());
	}

	void onPoolEntryDestroyed(IActor& actor) override {
		removeActor(actor.getID());
	}

	void onFree(IComponent* component) override {
		State& s = state();
		if (component == s.objects) {
			s.objects = nullptr;
		}
		if (component == s.vehicles) {
			s.vehicles->getPoolEventDispatcher().removeEventHandler(this);
			s.vehicles = nullptr;
		}
		if (component == s.actors) {
			s.actors->getPoolEventDispatcher().removeEventHandler(this);
			s.actors = nullptr;
		}
		if (component == s.pawn && s.pawn != nullptr) {
			s.pawn->getEventDispatcher().removeEventHandler(this);
			s.pawn = nullptr;
		}
	}

	void reset() override {
	}

	void free() override {
		State& s = state();
		if (s.core != nullptr) {
			s.core->getEventDispatcher().removeEventHandler(this);
			s.players->getPlayerConnectDispatcher().removeEventHandler(this);
		}
		s.objects = nullptr;
		if (s.vehicles != nullptr) {
			s.vehicles->getPoolEventDispatcher().removeEventHandler(this);
			s.vehicles = nullptr;
		}
		if (s.actors != nullptr) {
			s.actors->getPoolEventDispatcher().removeEventHandler(this);
			s.actors = nullptr;
		}
		for (int vehicleid = 0; vehicleid != int(s.vehicleBodies.size()); ++vehicleid) {
			removeVehicle(vehicleid);
		}
		for (int actorid = 0; actorid != int(s.actorBodies.size()); ++actorid) {
			removeActor(actorid);
		}
		for (int index = 0; index != int(s.bodies.size()); ++index) {
			destroyBody(index);
		}
		for (int playerid = 0; playerid != int(s.playerBodies.size()); ++playerid) {
			removePlayer(playerid);
		}
		stopRoutes();
		stopWorld();
		delete this;
	}
};

COMPONENT_ENTRY_POINT() {
	return new PhysicsComponent();
}
