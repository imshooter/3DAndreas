#include "physics.hpp"

#include <cmath>
#include <vector>

namespace physics {

namespace {

enum class DiscStepKind {
	Move,
	Disc,
	Wall,
	Pocket
};

struct DiscWall {
	float fromX;
	float fromY;
	float toX;
	float toY;
	float bounce;
	float keep;
};

struct DiscPocket {
	float x;
	float y;
	float radius;
};

struct Disc {
	float x;
	float y;
	float radius;
	float bounce;
	bool live;
};

struct DiscStep {
	int disc;
	DiscStepKind kind;
	int time;
	float x;
	float y;
	int next;
	int other;
	float speed;
};

struct DiscTable {
	bool used = false;
	AMX* owner = nullptr;
	float rolling = 0.05f;
	float damping = 0.3f;
	float stop = 0.01f;
	float tick = 0.1f;
	std::vector<DiscWall> walls;
	std::vector<DiscPocket> pockets;
	std::vector<Disc> discs;
	std::vector<DiscStep> steps;
};

std::vector<DiscTable> tables;
std::vector<int> freeTables;

DiscTable* findTable(cell id) {
	if (id < 0 || id >= cell(tables.size()) || !tables[size_t(id)].used) {
		return nullptr;
	}
	return &tables[size_t(id)];
}

Disc* findDisc(DiscTable& table, cell disc) {
	if (disc < 0 || disc >= cell(table.discs.size())) {
		return nullptr;
	}
	return &table.discs[size_t(disc)];
}

float meetTime(float x, float y, float vx, float vy, float radius) {
	float a = vx * vx + vy * vy;
	float b = x * vx + y * vy;
	if (a == 0.0f || b >= 0.0f) {
		return -1.0f;
	}
	float c = x * x + y * y - radius * radius;
	if (c <= 0.0f) {
		return 0.0f;
	}
	float d = b * b - a * c;
	if (d < 0.0f) {
		return -1.0f;
	}
	return (-b - std::sqrt(d)) / a;
}

struct Running {
	DiscTable& table;
	std::vector<float> vx;
	std::vector<float> vy;
	std::vector<int> last;
	size_t limit;

	void add(int disc, DiscStepKind kind, float time, int other = -1, float speed = 0.0f) {
		if (table.steps.size() == limit) {
			return;
		}
		const Disc& d = table.discs[size_t(disc)];
		int index = int(table.steps.size());
		table.steps.push_back({ disc, kind, int(std::lround(time * 1000.0f)), d.x, d.y, -1, other, speed });
		if (last[size_t(disc)] != -1) {
			table.steps[size_t(last[size_t(disc)])].next = index;
		}
		last[size_t(disc)] = index;
	}
};

int runTable(DiscTable& table, int cue, float speedX, float speedY, float limit, int most) {
	size_t count = table.discs.size();
	Running run { table, std::vector<float>(count, 0.0f), std::vector<float>(count, 0.0f), std::vector<int>(count, -1), size_t(most) };
	table.steps.clear();
	run.vx[size_t(cue)] = speedX;
	run.vy[size_t(cue)] = speedY;
	run.add(cue, DiscStepKind::Move, 0.0f);

	float now = 0.0f;
	float tick = table.tick;
	size_t reserve = count * 3;
	while (now < limit && table.steps.size() + reserve < run.limit) {
		enum class Hit { Tick, Disc, Wall, End, Pocket };
		float best = tick - now;
		Hit hit = Hit::Tick;
		int which = -1;
		int other = -1;
		float normalX = 0.0f;
		float normalY = 0.0f;
		bool moving = false;

		for (size_t i = 0; i != count; ++i) {
			const Disc& d = table.discs[i];
			if (!d.live || (run.vx[i] == 0.0f && run.vy[i] == 0.0f)) {
				continue;
			}
			moving = true;

			for (size_t j = 0; j != count; ++j) {
				const Disc& o = table.discs[j];
				if (j == i || !o.live || (j < i && (run.vx[j] != 0.0f || run.vy[j] != 0.0f))) {
					continue;
				}
				float t = meetTime(o.x - d.x, o.y - d.y, run.vx[j] - run.vx[i], run.vy[j] - run.vy[i], d.radius + o.radius);
				if (t >= 0.0f && t < best) {
					best = t;
					hit = Hit::Disc;
					which = int(i);
					other = int(j);
				}
			}

			for (size_t w = 0; w != table.walls.size(); ++w) {
				const DiscWall& wall = table.walls[w];
				float lx = wall.toX - wall.fromX;
				float ly = wall.toY - wall.fromY;
				float length = std::sqrt(lx * lx + ly * ly);
				if (length == 0.0f) {
					continue;
				}
				lx /= length;
				ly /= length;
				float nx = -ly;
				float ny = lx;
				float distance = nx * (d.x - wall.fromX) + ny * (d.y - wall.fromY);
				if (distance < 0.0f) {
					nx = -nx;
					ny = -ny;
					distance = -distance;
				}
				float closing = nx * run.vx[i] + ny * run.vy[i];
				if (closing < 0.0f) {
					float t = distance > d.radius ? (distance - d.radius) / -closing : 0.0f;
					float along = lx * (d.x + run.vx[i] * t - wall.fromX) + ly * (d.y + run.vy[i] * t - wall.fromY);
					if (t < best && along >= 0.0f && along <= length) {
						best = t;
						hit = Hit::Wall;
						which = int(i);
						other = int(w);
						normalX = nx;
						normalY = ny;
					}
				}
				for (int end = 0; end != 2; ++end) {
					float ex = end ? wall.toX : wall.fromX;
					float ey = end ? wall.toY : wall.fromY;
					float t = meetTime(d.x - ex, d.y - ey, run.vx[i], run.vy[i], d.radius);
					if (t >= 0.0f && t < best) {
						best = t;
						hit = Hit::End;
						which = int(i);
						other = int(w);
						normalX = ex;
						normalY = ey;
					}
				}
			}

			for (size_t p = 0; p != table.pockets.size(); ++p) {
				const DiscPocket& pocket = table.pockets[p];
				float t = meetTime(d.x - pocket.x, d.y - pocket.y, run.vx[i], run.vy[i], pocket.radius + d.radius);
				if (t >= 0.0f && t < best) {
					best = t;
					hit = Hit::Pocket;
					which = int(i);
					other = int(p);
				}
			}
		}

		if (!moving) {
			break;
		}

		for (size_t i = 0; i != count; ++i) {
			Disc& d = table.discs[i];
			if (d.live) {
				d.x += run.vx[i] * best;
				d.y += run.vy[i] * best;
			}
		}
		now += best;

		switch (hit) {
		case Hit::Tick:
			for (size_t i = 0; i != count; ++i) {
				if (!table.discs[i].live || (run.vx[i] == 0.0f && run.vy[i] == 0.0f)) {
					continue;
				}
				float speed = std::sqrt(run.vx[i] * run.vx[i] + run.vy[i] * run.vy[i]);
				float left = speed - (table.rolling + table.damping * speed) * table.tick;
				float keep = left > table.stop ? left / speed : 0.0f;
				run.vx[i] *= keep;
				run.vy[i] *= keep;
				run.add(int(i), DiscStepKind::Move, now);
			}
			tick = now + table.tick;
			break;
		case Hit::Disc: {
			Disc& a = table.discs[size_t(which)];
			Disc& b = table.discs[size_t(other)];
			float gap = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
			float nx = gap > 0.0f ? (b.x - a.x) / gap : 1.0f;
			float ny = gap > 0.0f ? (b.y - a.y) / gap : 0.0f;
			float closing = (run.vx[size_t(which)] - run.vx[size_t(other)]) * nx + (run.vy[size_t(which)] - run.vy[size_t(other)]) * ny;
			if (closing > 0.0f) {
				float impulse = (1.0f + (a.bounce + b.bounce) * 0.5f) * 0.5f * closing;
				run.vx[size_t(which)] -= impulse * nx;
				run.vy[size_t(which)] -= impulse * ny;
				run.vx[size_t(other)] += impulse * nx;
				run.vy[size_t(other)] += impulse * ny;
			}
			run.add(which, DiscStepKind::Disc, now, other, closing);
			run.add(other, DiscStepKind::Disc, now, which, closing);
			break;
		}
		case Hit::Wall:
		case Hit::End: {
			Disc& d = table.discs[size_t(which)];
			const DiscWall& wall = table.walls[size_t(other)];
			float nx = normalX;
			float ny = normalY;
			if (hit == Hit::End) {
				float gap = std::sqrt((d.x - normalX) * (d.x - normalX) + (d.y - normalY) * (d.y - normalY));
				nx = gap > 0.0f ? (d.x - normalX) / gap : 0.0f;
				ny = gap > 0.0f ? (d.y - normalY) / gap : 0.0f;
			}
			float normal = run.vx[size_t(which)] * nx + run.vy[size_t(which)] * ny;
			if (normal < 0.0f) {
				run.vx[size_t(which)] = (run.vx[size_t(which)] - normal * nx) * wall.keep - wall.bounce * normal * nx;
				run.vy[size_t(which)] = (run.vy[size_t(which)] - normal * ny) * wall.keep - wall.bounce * normal * ny;
			}
			run.add(which, DiscStepKind::Wall, now, other, -normal);
			break;
		}
		case Hit::Pocket:
			table.discs[size_t(which)].live = false;
			run.vx[size_t(which)] = 0.0f;
			run.vy[size_t(which)] = 0.0f;
			run.add(which, DiscStepKind::Pocket, now, other);
			break;
		}
	}

	for (size_t i = 0; i != count; ++i) {
		if (table.discs[i].live && (run.vx[i] != 0.0f || run.vy[i] != 0.0f)) {
			run.limit += 1;
			run.add(int(i), DiscStepKind::Move, now);
		}
	}
	return int(table.steps.size());
}

cell AMX_NATIVE_CALL A3D_CreateDiscTable(AMX* amx, const cell* params) {
	float rolling = readFloat(params, 1);
	float damping = readFloat(params, 2);
	float stop = readFloat(params, 3);
	float tick = readFloat(params, 4);
	if (!finite(rolling) || !finite(damping) || !finite(stop) || !finite(tick) || rolling < 0.0f || damping < 0.0f || stop < 0.0f || tick <= 0.0f) {
		return -1;
	}
	int id;
	if (freeTables.empty()) {
		id = int(tables.size());
		tables.emplace_back();
	} else {
		id = freeTables.back();
		freeTables.pop_back();
	}
	DiscTable& table = tables[size_t(id)];
	table = DiscTable();
	table.used = true;
	table.owner = amx;
	table.rolling = rolling;
	table.damping = damping;
	table.stop = stop;
	table.tick = tick;
	return id;
}

cell AMX_NATIVE_CALL A3D_DestroyDiscTable(AMX* amx, const cell* params) {
	if (findTable(params[1]) == nullptr) {
		return false;
	}
	tables[size_t(params[1])] = DiscTable();
	freeTables.push_back(int(params[1]));
	return true;
}

cell AMX_NATIVE_CALL A3D_IsValidDiscTable(AMX* amx, const cell* params) {
	return findTable(params[1]) != nullptr;
}

cell AMX_NATIVE_CALL A3D_AddDiscWall(AMX* amx, const cell* params) {
	DiscTable* table = findTable(params[1]);
	DiscWall wall { readFloat(params, 2), readFloat(params, 3), readFloat(params, 4), readFloat(params, 5), readFloat(params, 6), readFloat(params, 7) };
	if (table == nullptr || !finite(wall.fromX) || !finite(wall.fromY) || !finite(wall.toX) || !finite(wall.toY) || !finite(wall.bounce) || !finite(wall.keep)) {
		return -1;
	}
	table->walls.push_back(wall);
	return cell(table->walls.size()) - 1;
}

cell AMX_NATIVE_CALL A3D_AddDiscPocket(AMX* amx, const cell* params) {
	DiscTable* table = findTable(params[1]);
	DiscPocket pocket { readFloat(params, 2), readFloat(params, 3), readFloat(params, 4) };
	if (table == nullptr || !finite(pocket.x) || !finite(pocket.y) || !finite(pocket.radius) || pocket.radius < 0.0f) {
		return -1;
	}
	table->pockets.push_back(pocket);
	return cell(table->pockets.size()) - 1;
}

cell AMX_NATIVE_CALL A3D_AddDisc(AMX* amx, const cell* params) {
	DiscTable* table = findTable(params[1]);
	Disc disc { readFloat(params, 2), readFloat(params, 3), readFloat(params, 4), readFloat(params, 5), true };
	if (table == nullptr || !finite(disc.x) || !finite(disc.y) || !finite(disc.radius) || !finite(disc.bounce) || disc.radius <= 0.0f) {
		return -1;
	}
	table->discs.push_back(disc);
	return cell(table->discs.size()) - 1;
}

cell AMX_NATIVE_CALL A3D_SetDiscPosition(AMX* amx, const cell* params) {
	DiscTable* table = findTable(params[1]);
	Disc* disc = table != nullptr ? findDisc(*table, params[2]) : nullptr;
	float x = readFloat(params, 3);
	float y = readFloat(params, 4);
	if (disc == nullptr || !finite(x) || !finite(y)) {
		return false;
	}
	disc->x = x;
	disc->y = y;
	return true;
}

cell AMX_NATIVE_CALL A3D_GetDiscPosition(AMX* amx, const cell* params) {
	DiscTable* table = findTable(params[1]);
	Disc* disc = table != nullptr ? findDisc(*table, params[2]) : nullptr;
	if (disc == nullptr) {
		return false;
	}
	return writeFloat(amx, params[3], disc->x) && writeFloat(amx, params[4], disc->y);
}

cell AMX_NATIVE_CALL A3D_SetDiscOnTable(AMX* amx, const cell* params) {
	DiscTable* table = findTable(params[1]);
	Disc* disc = table != nullptr ? findDisc(*table, params[2]) : nullptr;
	if (disc == nullptr) {
		return false;
	}
	disc->live = params[3] != 0;
	return true;
}

cell AMX_NATIVE_CALL A3D_IsDiscOnTable(AMX* amx, const cell* params) {
	DiscTable* table = findTable(params[1]);
	Disc* disc = table != nullptr ? findDisc(*table, params[2]) : nullptr;
	return disc != nullptr && disc->live;
}

cell AMX_NATIVE_CALL A3D_RunDiscTable(AMX* amx, const cell* params) {
	DiscTable* table = findTable(params[1]);
	Disc* disc = table != nullptr ? findDisc(*table, params[2]) : nullptr;
	float speedX = readFloat(params, 3);
	float speedY = readFloat(params, 4);
	float limit = readFloat(params, 5);
	int most = int(params[6]);
	if (disc == nullptr || !disc->live || !finite(speedX) || !finite(speedY) || !finite(limit) || limit <= 0.0f || most < int(table->discs.size()) * 4) {
		return 0;
	}
	return runTable(*table, int(params[2]), speedX, speedY, limit, most);
}

cell AMX_NATIVE_CALL A3D_GetDiscStepCount(AMX* amx, const cell* params) {
	DiscTable* table = findTable(params[1]);
	return table != nullptr ? cell(table->steps.size()) : 0;
}

cell AMX_NATIVE_CALL A3D_GetDiscStep(AMX* amx, const cell* params) {
	DiscTable* table = findTable(params[1]);
	if (table == nullptr || params[2] < 0 || params[2] >= cell(table->steps.size())) {
		return false;
	}
	const DiscStep& step = table->steps[size_t(params[2])];
	return writeCell(amx, params[3], step.disc)
		&& writeCell(amx, params[4], cell(step.kind))
		&& writeCell(amx, params[5], step.time)
		&& writeFloat(amx, params[6], step.x)
		&& writeFloat(amx, params[7], step.y)
		&& writeCell(amx, params[8], step.next)
		&& writeCell(amx, params[9], step.other)
		&& writeFloat(amx, params[10], step.speed);
}

const AMX_NATIVE_INFO natives[] = {
	{ "A3D_CreateDiscTable", A3D_CreateDiscTable },
	{ "A3D_DestroyDiscTable", A3D_DestroyDiscTable },
	{ "A3D_IsValidDiscTable", A3D_IsValidDiscTable },
	{ "A3D_AddDiscWall", A3D_AddDiscWall },
	{ "A3D_AddDiscPocket", A3D_AddDiscPocket },
	{ "A3D_AddDisc", A3D_AddDisc },
	{ "A3D_SetDiscPosition", A3D_SetDiscPosition },
	{ "A3D_GetDiscPosition", A3D_GetDiscPosition },
	{ "A3D_SetDiscOnTable", A3D_SetDiscOnTable },
	{ "A3D_IsDiscOnTable", A3D_IsDiscOnTable },
	{ "A3D_RunDiscTable", A3D_RunDiscTable },
	{ "A3D_GetDiscStepCount", A3D_GetDiscStepCount },
	{ "A3D_GetDiscStep", A3D_GetDiscStep },
};

}

void registerDiscNatives(IPawnScript& script) {
	script.Register(natives, int(sizeof natives / sizeof natives[0]));
}

void forgetDiscs(AMX* amx) {
	for (size_t id = 0; id != tables.size(); ++id) {
		if (tables[id].used && tables[id].owner == amx) {
			tables[id] = DiscTable();
			freeTables.push_back(int(id));
		}
	}
}

void stopDiscs() {
	tables.clear();
	freeTables.clear();
}

}
