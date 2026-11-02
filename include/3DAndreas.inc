#if defined _INC_3DANDREAS
    #endinput
#endif
#define _INC_3DANDREAS

/**
 * Natives and callbacks of the 3DAndreas component.
 *
 * A physics object is a body simulated by the component. A moving one shows as a global object
 * that the component creates and moves, or as a streamer object attached with
 * `A3D_AttachDynamicObject`. Static objects and zones never move and show nothing. Include this
 * file after streamer.inc for streamer objects.
 *
 * The same world answers questions about the map: what a segment hits, where the ground is, whether
 * two points see each other, what a sphere or a box overlaps, and which players and vehicles stand
 * inside a sphere or a cone.
 *
 * Players on foot and actors are upright capsules and vehicles are boxes the size of their model,
 * moved with them, so physics objects bounce off them and queries find them. `3dandreas.vehicles`
 * set to `false` in config.json leaves vehicles out.
 *
 * The screen of a player comes from the camera their client last sent on foot: where it is, where
 * it looks, its field of view and the aspect ratio of the screen. Screen coordinates are the 640 by
 * 448 units of text draws, from the top left corner.
 *
 * The map comes from `scriptfiles/3dandreas/world.db`, which the `phydb` tool builds from the
 * files of the game: the collision of every model with the surface of each part, the map objects
 * with their interiors, the water, the surfaces and how the game moves the objects that can be
 * knocked over. A ColAndreas database still loads, without surfaces, water and that data.
 *
 * `scriptfiles/3dandreas/models.json` describes models as `A3D_CreateObject` makes them: a sphere,
 * a box or a cylinder in place of the collision, which gives a shape to models the game leaves
 * without one, such as balls and weapons, and the mass, friction, bounce, damping and buoyancy each
 * one starts with. A model the file leaves out gets a mass from the volume of its shape and the
 * material of its collision, and the friction and bounce of that material.
 *
 * A surface is a line of surfinfo.dat, from `0` for `DEFAULT` to `178`. Players carry `62`, the
 * surface `PED`, vehicles `63`, the surface `CAR`, and spheres and boxes of scripts `0`.
 *
 * Rays pass through the water to the bottom, unless `A3D_COLLIDE_WATER` is in their flags. Then they
 * stop at the surface as `A3D_HIT_WATER`, with no surface of the game. `A3D_COLLIDE_ALL` holds every
 * kind of body, not the water, and bodies never collide with the water, they float with
 * `A3D_SetBuoyancy`.
 *
 * Routes follow the paths of the game in the database: the roads vehicles drive, with their one way
 * streets and the lanes for boats, and the paths pedestrians walk, joined to the roads where they
 * end. A route is found in microseconds by a contraction hierarchy built when the server starts.
 * Until it is ready, which takes about a second, routes are found the slow way and come out the
 * same. Routes for vehicles cost a few seconds at each turn, more to the left than to the right,
 * and half a minute to turn back. A route can be shown on the radar and the map of a player as a
 * line that moves on as they go.
 *
 * `scriptfiles/3dandreas/navmesh.db`, which the `navbuild` tool makes from the database, tells where
 * a pedestrian can walk anywhere on the map and in the interiors, around buildings, walls and
 * fences. With it, walks start and end where the points are rather than on the nearest path, short
 * walks take the shortest way across squares and yards, and `A3D_GetNearestWalkPoint`,
 * `A3D_GetRandomWalkPoint` and `A3D_WalkRayCast` answer. Without it, they fail and walks keep to the
 * paths. The mesh takes about 65 megabytes of memory, and the routes about 30.
 */

/**
 * # Header
 */

#define INVALID_PHYSIC_ID               (Physic:-1)

enum A3D_HIT {
    A3D_HIT_MAP,
    A3D_HIT_OBJECT,
    A3D_HIT_STATIC,
    A3D_HIT_PLAYER,
    A3D_HIT_VEHICLE,
    A3D_HIT_WATER,
    A3D_HIT_ACTOR
};

enum A3D_COLLIDE (<<= 1) {
    A3D_COLLIDE_MAP = 1,
    A3D_COLLIDE_OBJECTS,
    A3D_COLLIDE_STATIC,
    A3D_COLLIDE_PLAYERS,
    A3D_COLLIDE_VEHICLES,
    A3D_COLLIDE_WATER,
    A3D_COLLIDE_ACTORS
};

#define A3D_COLLIDE_ALL                 (A3D_COLLIDE_MAP | A3D_COLLIDE_OBJECTS | A3D_COLLIDE_STATIC | A3D_COLLIDE_PLAYERS | A3D_COLLIDE_VEHICLES | A3D_COLLIDE_ACTORS)

enum A3D_MATERIAL (<<= 1) {
    A3D_MATERIAL_SOFT_LANDING = 1,
    A3D_MATERIAL_SEE_THROUGH,
    A3D_MATERIAL_SHOOT_THROUGH,
    A3D_MATERIAL_SAND,
    A3D_MATERIAL_WATER,
    A3D_MATERIAL_SHALLOW_WATER,
    A3D_MATERIAL_BEACH,
    A3D_MATERIAL_STEEP,
    A3D_MATERIAL_GLASS,
    A3D_MATERIAL_STAIRS,
    A3D_MATERIAL_SKATEABLE,
    A3D_MATERIAL_PAVEMENT,
    A3D_MATERIAL_ROUGH,
    A3D_MATERIAL_FLAMMABLE,
    A3D_MATERIAL_SPARKS,
    A3D_MATERIAL_NO_SPRINT,
    A3D_MATERIAL_FOOTSTEPS,
    A3D_MATERIAL_FOOT_DUST,
    A3D_MATERIAL_CAR_DIRT,
    A3D_MATERIAL_CAR_CLEAN,
    A3D_MATERIAL_WHEEL_GRASS,
    A3D_MATERIAL_WHEEL_GRAVEL,
    A3D_MATERIAL_WHEEL_MUD,
    A3D_MATERIAL_WHEEL_DUST,
    A3D_MATERIAL_WHEEL_SAND,
    A3D_MATERIAL_WHEEL_SPRAY,
    A3D_MATERIAL_PLANTS,
    A3D_MATERIAL_PROPS,
    A3D_MATERIAL_CLIMBABLE
};

enum A3D_PART {
    A3D_PART_FRONT_BUMPER,
    A3D_PART_REAR_BUMPER,
    A3D_PART_ROOF,
    A3D_PART_DRIVER_SEAT,
    A3D_PART_PASSENGER_SEAT,
    A3D_PART_DRIVER_DOOR,
    A3D_PART_PASSENGER_DOOR,
    A3D_PART_REAR_LEFT_DOOR,
    A3D_PART_REAR_RIGHT_DOOR,
    A3D_PART_PETROL_CAP,
    A3D_PART_FRONT_LEFT_WHEEL,
    A3D_PART_FRONT_RIGHT_WHEEL,
    A3D_PART_REAR_LEFT_WHEEL,
    A3D_PART_REAR_RIGHT_WHEEL,
    A3D_PART_MIDDLE_LEFT_WHEEL,
    A3D_PART_MIDDLE_RIGHT_WHEEL
};

#define INVALID_ROUTE_ID                (Route:-1)

enum A3D_ROUTE {
    A3D_ROUTE_FASTEST,
    A3D_ROUTE_SHORTEST,
    A3D_ROUTE_ANY_WAY,
    A3D_ROUTE_TRAFFIC,
    A3D_ROUTE_BOAT,
    A3D_ROUTE_WALK
};

enum A3D_TURN {
    A3D_TURN_NONE,
    A3D_TURN_SLIGHT_LEFT,
    A3D_TURN_LEFT,
    A3D_TURN_SHARP_LEFT,
    A3D_TURN_SLIGHT_RIGHT,
    A3D_TURN_RIGHT,
    A3D_TURN_SHARP_RIGHT,
    A3D_TURN_BACK
};

/**
 * # Functions
 */

/**
 * @brief      Create a moving physics object shaped like the collision of a model, or as
 *             `models.json` describes the model.
 *
 * A model in `models.json` takes the shape, friction, bounce, damping, rolling friction, buoyancy
 * and sync error the file gives it, and its mass when `mass` is `0.0`. What the file leaves out
 * comes from the model: the mass from the volume of its shape and the density of what most of its
 * collision is made of, the friction and bounce from that material.
 *
 * @param      modelid  Model shown, and whose collision gives the shape.
 * @param      x        Position on the X axis.
 * @param      y        Position on the Y axis.
 * @param      z        Position on the Z axis.
 * @param      rx       Rotation on the X axis.
 * @param      ry       Rotation on the Y axis.
 * @param      rz       Rotation on the Z axis.
 * @param      mass     Mass in kilograms, or `0.0` for the mass in `models.json`, or else the one
 *                      the model gives from its volume.
 * @param      worldid  Virtual world it collides in, or `-1` for every world.
 *
 * @return     - the physics object
 *             - `INVALID_PHYSIC_ID` the model has no collision and no shape in `models.json`, or
 *               the world is full
 */
native Physic:A3D_CreateObject(modelid, Float:x, Float:y, Float:z, Float:rx = 0.0, Float:ry = 0.0, Float:rz = 0.0, Float:mass = 0.0, worldid = -1);

/**
 * @brief      Create a moving physics object shaped like a sphere.
 *
 * @param      modelid  Model shown, or `-1` for none.
 * @param      radius   Radius of the sphere.
 * @param      x        Position on the X axis.
 * @param      y        Position on the Y axis.
 * @param      z        Position on the Z axis.
 * @param      mass     Mass in kilograms.
 * @param      worldid  Virtual world it collides in, or `-1` for every world.
 *
 * @return     - the physics object
 *             - `INVALID_PHYSIC_ID` the radius is not positive, or the world is full
 */
native Physic:A3D_CreateSphere(modelid, Float:radius, Float:x, Float:y, Float:z, Float:mass = 1.0, worldid = -1);

/**
 * @brief      Create a moving physics object shaped like a box.
 *
 * @param      modelid  Model shown, or `-1` for none.
 * @param      sizeX    Size on the X axis.
 * @param      sizeY    Size on the Y axis.
 * @param      sizeZ    Size on the Z axis.
 * @param      x        Position on the X axis.
 * @param      y        Position on the Y axis.
 * @param      z        Position on the Z axis.
 * @param      rx       Rotation on the X axis.
 * @param      ry       Rotation on the Y axis.
 * @param      rz       Rotation on the Z axis.
 * @param      mass     Mass in kilograms.
 * @param      worldid  Virtual world it collides in, or `-1` for every world.
 *
 * @return     - the physics object
 *             - `INVALID_PHYSIC_ID` a size is not positive, or the world is full
 */
native Physic:A3D_CreateBox(modelid, Float:sizeX, Float:sizeY, Float:sizeZ, Float:x, Float:y, Float:z, Float:rx = 0.0, Float:ry = 0.0, Float:rz = 0.0, Float:mass = 1.0, worldid = -1);

/**
 * @brief      Create a physics object that never moves, shaped like the collision of a model.
 *
 * Nothing is shown. Use it under an object created by the script.
 *
 * @param      modelid  Model whose collision gives the shape.
 * @param      x        Position on the X axis.
 * @param      y        Position on the Y axis.
 * @param      z        Position on the Z axis.
 * @param      rx       Rotation on the X axis.
 * @param      ry       Rotation on the Y axis.
 * @param      rz       Rotation on the Z axis.
 * @param      worldid  Virtual world it collides in, or `-1` for every world.
 *
 * @return     - the physics object
 *             - `INVALID_PHYSIC_ID` the model has no collision, or the world is full
 */
native Physic:A3D_CreateStaticObject(modelid, Float:x, Float:y, Float:z, Float:rx = 0.0, Float:ry = 0.0, Float:rz = 0.0, worldid = -1);

/**
 * @brief      Create an invisible box that never moves, such as a wall or a floor.
 *
 * @param      sizeX    Size on the X axis.
 * @param      sizeY    Size on the Y axis.
 * @param      sizeZ    Size on the Z axis.
 * @param      x        Position on the X axis.
 * @param      y        Position on the Y axis.
 * @param      z        Position on the Z axis.
 * @param      rx       Rotation on the X axis.
 * @param      ry       Rotation on the Y axis.
 * @param      rz       Rotation on the Z axis.
 * @param      worldid  Virtual world it collides in, or `-1` for every world.
 *
 * @return     - the physics object
 *             - `INVALID_PHYSIC_ID` a size is not positive, or the world is full
 */
native Physic:A3D_CreateStaticBox(Float:sizeX, Float:sizeY, Float:sizeZ, Float:x, Float:y, Float:z, Float:rx = 0.0, Float:ry = 0.0, Float:rz = 0.0, worldid = -1);

/**
 * @brief      Create an invisible upright cylinder that never moves, such as a pillar.
 *
 * @param      radius   Radius of the cylinder.
 * @param      height   Height of the cylinder.
 * @param      x        Position of its centre on the X axis.
 * @param      y        Position of its centre on the Y axis.
 * @param      z        Position of its centre on the Z axis.
 * @param      rx       Rotation on the X axis.
 * @param      ry       Rotation on the Y axis.
 * @param      rz       Rotation on the Z axis.
 * @param      worldid  Virtual world it collides in, or `-1` for every world.
 *
 * @return     - the physics object
 *             - `INVALID_PHYSIC_ID` the radius or the height is not positive, or the world is full
 */
native Physic:A3D_CreateStaticCylinder(Float:radius, Float:height, Float:x, Float:y, Float:z, Float:rx = 0.0, Float:ry = 0.0, Float:rz = 0.0, worldid = -1);

/**
 * @brief      Create a box shaped zone that reports moving physics objects entering and leaving it.
 *
 * A zone stops nothing. `A3D_OnEntered` and `A3D_OnLeft` are called in the script that created it.
 * An object that comes to rest inside the zone stays inside it until it moves out.
 *
 * @param      sizeX    Size on the X axis.
 * @param      sizeY    Size on the Y axis.
 * @param      sizeZ    Size on the Z axis.
 * @param      x        Position on the X axis.
 * @param      y        Position on the Y axis.
 * @param      z        Position on the Z axis.
 * @param      rx       Rotation on the X axis.
 * @param      ry       Rotation on the Y axis.
 * @param      rz       Rotation on the Z axis.
 * @param      worldid  Virtual world it watches, or `-1` for every world.
 *
 * @return     - the zone
 *             - `INVALID_PHYSIC_ID` a size is not positive, or the world is full
 */
native Physic:A3D_CreateZone(Float:sizeX, Float:sizeY, Float:sizeZ, Float:x, Float:y, Float:z, Float:rx = 0.0, Float:ry = 0.0, Float:rz = 0.0, worldid = -1);

/**
 * @brief      Create a sphere shaped zone that reports moving physics objects entering and leaving it.
 *
 * @param      radius   Radius of the sphere.
 * @param      x        Position on the X axis.
 * @param      y        Position on the Y axis.
 * @param      z        Position on the Z axis.
 * @param      worldid  Virtual world it watches, or `-1` for every world.
 *
 * @return     - the zone
 *             - `INVALID_PHYSIC_ID` the radius is not positive, or the world is full
 */
native Physic:A3D_CreateZoneSphere(Float:radius, Float:x, Float:y, Float:z, worldid = -1);

/**
 * @brief      Destroy a physics object, a static object or a zone, and the global object that shows it.
 *
 * @param      physicid  Physics object to destroy.
 *
 * @return     - `true` it was destroyed
 *             - `false` `physicid` is not a physics object
 */
native bool:A3D_DestroyObject(Physic:physicid);

/**
 * @brief      Whether a physics object, a static object or a zone exists.
 *
 * @param      physicid  Physics object to check.
 *
 * @return     - `true` it exists
 *             - `false` it does not
 */
native bool:A3D_IsValidObject(Physic:physicid);

/**
 * @brief      Set the velocity of a moving physics object and wake it up.
 *
 * @param      physicid  Physics object to push.
 * @param      vx        Velocity on the X axis, in metres per second.
 * @param      vy        Velocity on the Y axis, in metres per second.
 * @param      vz        Velocity on the Z axis, in metres per second.
 *
 * @return     - `true` the velocity was set
 *             - `false` `physicid` is not a moving physics object
 */
native bool:A3D_SetVelocity(Physic:physicid, Float:vx, Float:vy, Float:vz);

/**
 * @brief      The velocity of a moving physics object.
 *
 * @param      physicid  Physics object to read.
 * @param      vx        Velocity on the X axis, in metres per second.
 * @param      vy        Velocity on the Y axis, in metres per second.
 * @param      vz        Velocity on the Z axis, in metres per second.
 *
 * @return     - `true` the velocity was written
 *             - `false` `physicid` is not a moving physics object
 */
native bool:A3D_GetVelocity(Physic:physicid, &Float:vx, &Float:vy, &Float:vz);

/**
 * @brief      The speed of a moving physics object.
 *
 * @param      physicid  Physics object to read.
 *
 * @return     - the speed in metres per second
 *             - `0.0` `physicid` is not a moving physics object
 */
native Float:A3D_GetSpeed(Physic:physicid);

/**
 * @brief      The direction a moving physics object travels on the ground, as a facing angle.
 *
 * @param      physicid  Physics object to read.
 *
 * @return     - the angle in degrees, `0.0` to the north and growing to the west
 *             - `0.0` `physicid` is not a moving physics object
 */
native Float:A3D_GetMoveAngle(Physic:physicid);

/**
 * @brief      Push a moving physics object through its centre of mass.
 *
 * @param      physicid  Physics object to push.
 * @param      x         Impulse on the X axis, in kilograms times metres per second.
 * @param      y         Impulse on the Y axis.
 * @param      z         Impulse on the Z axis.
 *
 * @return     - `true` the impulse was applied
 *             - `false` `physicid` is not a moving physics object
 */
native bool:A3D_ApplyImpulse(Physic:physicid, Float:x, Float:y, Float:z);

/**
 * @brief      Push a moving physics object with a constant acceleration until it is set to zero.
 *
 * @param      physicid  Physics object to push.
 * @param      ax        Acceleration on the X axis, in metres per second squared.
 * @param      ay        Acceleration on the Y axis.
 * @param      az        Acceleration on the Z axis.
 *
 * @return     - `true` the acceleration was set
 *             - `false` `physicid` is not a moving physics object
 */
native bool:A3D_SetAcceleration(Physic:physicid, Float:ax, Float:ay, Float:az);

/**
 * @brief      Set how strongly a moving physics object falls.
 *
 * @param      physicid  Physics object to change.
 * @param      gravity   Gravity in metres per second squared. Starts at `9.81`, and `0.0` floats.
 *
 * @return     - `true` the gravity was set
 *             - `false` `physicid` is not a moving physics object
 */
native bool:A3D_SetGravity(Physic:physicid, Float:gravity);

/**
 * @brief      Slow a moving physics object down by a constant amount, as a ball rolling on cloth.
 *
 * Only the speed along the ground is reduced. It adds to the friction and the damping.
 *
 * @param      physicid      Physics object to change.
 * @param      deceleration  Speed lost per second, in metres per second squared. Starts at `0.0`.
 *
 * @return     - `true` the friction was set
 *             - `false` `physicid` is not a moving physics object, or `deceleration` is negative
 */
native bool:A3D_SetRollingFriction(Physic:physicid, Float:deceleration);

/**
 * @brief      Keep a moving physics object at its height, so it only moves along the ground.
 *
 * @param      physicid  Physics object to change.
 * @param      planar    `true` to keep its height, `false` to let it move freely again.
 *
 * @return     - `true` it was changed
 *             - `false` `physicid` is not a moving physics object
 */
native bool:A3D_SetPlanar(Physic:physicid, bool:planar = true);

/**
 * @brief      Choose what a moving physics object collides with.
 *
 * Zones see it whatever is chosen.
 *
 * @param      physicid  Physics object to change.
 * @param      flags     What it collides with, `A3D_COLLIDE_ALL` by default.
 *
 * @return     - `true` it was changed
 *             - `false` `physicid` is not a moving physics object
 */
native bool:A3D_SetCollisions(Physic:physicid, A3D_COLLIDE:flags);

/**
 * @brief      What a moving physics object collides with.
 *
 * @param      physicid  Physics object to read.
 *
 * @return     - the flags
 *             - `0` `physicid` is not a moving physics object
 */
native A3D_COLLIDE:A3D_GetCollisions(Physic:physicid);

/**
 * @brief      Set the slowest impact of a moving physics object that calls `A3D_OnCollided`.
 *
 * @param      physicid  Physics object to change.
 * @param      speed     Speed in metres per second. Starts at `3dandreas.collision_speed`.
 *
 * @return     - `true` the speed was set
 *             - `false` `physicid` is not a moving physics object, or `speed` is negative
 */
native bool:A3D_SetCollisionSpeed(Physic:physicid, Float:speed);

/**
 * @brief      Move a physics object, a static object or a zone to another virtual world.
 *
 * @param      physicid  Physics object to move.
 * @param      worldid   Virtual world, or `-1` for every world.
 *
 * @return     - `true` it was moved
 *             - `false` `physicid` is not a physics object
 */
native bool:A3D_SetWorld(Physic:physicid, worldid);

/**
 * @brief      The virtual world of a physics object.
 *
 * @param      physicid  Physics object to read.
 *
 * @return     - the world, `-1` for every world
 *             - `-1` `physicid` is not a physics object
 */
native A3D_GetWorld(Physic:physicid);

/**
 * @brief      The position of a physics object.
 *
 * @param      physicid  Physics object to read.
 * @param      x         Position on the X axis.
 * @param      y         Position on the Y axis.
 * @param      z         Position on the Z axis.
 *
 * @return     - `true` the position was written
 *             - `false` `physicid` is not a physics object
 */
native bool:A3D_GetPosition(Physic:physicid, &Float:x, &Float:y, &Float:z);

/**
 * @brief      Move a physics object to a new place at once.
 *
 * @param      physicid  Physics object to move.
 * @param      x         Position on the X axis.
 * @param      y         Position on the Y axis.
 * @param      z         Position on the Z axis.
 *
 * @return     - `true` it was moved
 *             - `false` `physicid` is not a physics object
 */
native bool:A3D_SetPosition(Physic:physicid, Float:x, Float:y, Float:z);

/**
 * @brief      The rotation of a physics object, as an object rotation.
 *
 * @param      physicid  Physics object to read.
 * @param      rx        Rotation on the X axis.
 * @param      ry        Rotation on the Y axis.
 * @param      rz        Rotation on the Z axis.
 *
 * @return     - `true` the rotation was written
 *             - `false` `physicid` is not a physics object
 */
native bool:A3D_GetRotation(Physic:physicid, &Float:rx, &Float:ry, &Float:rz);

/**
 * @brief      Turn a physics object to a new rotation at once.
 *
 * @param      physicid  Physics object to turn.
 * @param      rx        Rotation on the X axis.
 * @param      ry        Rotation on the Y axis.
 * @param      rz        Rotation on the Z axis.
 *
 * @return     - `true` it was turned
 *             - `false` `physicid` is not a physics object
 */
native bool:A3D_SetRotation(Physic:physicid, Float:rx, Float:ry, Float:rz);

/**
 * @brief      Set how much a physics object grips what it touches.
 *
 * @param      physicid  Physics object to change.
 * @param      friction  Friction, `0.0` for ice and about `1.0` for rubber. Starts at `0.5`.
 *
 * @return     - `true` the friction was set
 *             - `false` `physicid` is not a physics object
 */
native bool:A3D_SetFriction(Physic:physicid, Float:friction);

/**
 * @brief      Set how much a physics object bounces.
 *
 * Two objects that touch bounce by the product of their bounces, and the map counts as `1.0`. A
 * static object with no bounce, such as the cloth of a table, stops every bounce on it.
 *
 * @param      physicid  Physics object to change.
 * @param      bounce    Restitution, `0.0` for no bounce and `1.0` for a perfect one. Starts at `0.3`
 *                       for moving objects and `1.0` for static objects and zones.
 *
 * @return     - `true` the bounce was set
 *             - `false` `physicid` is not a physics object
 */
native bool:A3D_SetBounce(Physic:physicid, Float:bounce);

/**
 * @brief      Set how fast a moving physics object loses speed and spin on its own.
 *
 * @param      physicid  Physics object to change.
 * @param      linear    Loss of speed. Starts at `0.1`.
 * @param      angular   Loss of spin. Starts at `0.3`.
 *
 * @return     - `true` the damping was set
 *             - `false` `physicid` is not a moving physics object
 */
native bool:A3D_SetDamping(Physic:physicid, Float:linear, Float:angular);

/**
 * @brief      Make a moving physics object float in the water of the map.
 *
 * Without it, objects sink like stones. An object floats with `1.0 / buoyancy` of its volume under
 * the surface: `2.0` floats half under, `1.0` stays at any depth, and less sinks slowly. While it
 * touches the water it loses `drag` of its speed each second, so it settles and rests.
 * `A3D_GetModelPhysics` gives the buoyancy the game uses for a model.
 *
 * @param      physicid  Physics object to change.
 * @param      buoyancy  How much it floats, or `0.0` to stop it floating.
 * @param      drag      Share of its speed the water takes each second.
 *
 * @return     - `true` the buoyancy was set
 *             - `false` `physicid` is not a moving physics object, or a value is negative
 */
native bool:A3D_SetBuoyancy(Physic:physicid, Float:buoyancy, Float:drag = 1.0);

/**
 * @brief      Set how far the object players see may stray from the body before a new move is sent.
 *
 * It bounds the rotation too: the object may turn away from the body by the angle that moves its far
 * end by that distance. Small objects that must look exact, such as pool balls, need a small value.
 * Every update sent costs a packet for each player who sees the object.
 *
 * @param      physicid  Physics object to change.
 * @param      error     Distance in metres. Starts at `3dandreas.sync_error`.
 *
 * @return     - `true` the distance was set
 *             - `false` `physicid` is not a moving physics object, or `error` is not positive
 */
native bool:A3D_SetSyncError(Physic:physicid, Float:error);

/**
 * @brief      Whether a moving physics object is awake.
 *
 * @param      physicid  Physics object to check.
 *
 * @return     - `true` it moves
 *             - `false` it rests, or `physicid` is not a moving physics object
 */
native bool:A3D_IsMoving(Physic:physicid);

/**
 * @brief      The global object that shows a physics object.
 *
 * @param      physicid  Physics object to read.
 *
 * @return     - the object
 *             - `INVALID_OBJECT_ID` it shows none, or `physicid` is not a physics object
 */
native A3D_GetObjectID(Physic:physicid);

/**
 * @brief      Show a moving physics object with a streamer object instead of a global object.
 *
 * The global object is destroyed, and the component moves the streamer object from then on.
 *
 * @param      physicid  Physics object to show.
 * @param      objectid  Streamer object that follows it.
 *
 * @return     - `true` the streamer object follows it
 *             - `false` `physicid` is not a moving physics object
 */
native bool:A3D_AttachDynamicObject(Physic:physicid, objectid);

/**
 * @brief      Keep a number on a physics object, such as the item it stands for.
 *
 * @param      physicid  Physics object to change.
 * @param      slot      Slot of the number, from `0` to `7`.
 * @param      value     Number to keep.
 *
 * @return     - `true` the number was kept
 *             - `false` `physicid` is not a physics object, or `slot` is out of range
 */
native bool:A3D_SetExtra(Physic:physicid, slot, value);

/**
 * @brief      Read a number kept on a physics object with `A3D_SetExtra`.
 *
 * @param      physicid  Physics object to read.
 * @param      slot      Slot of the number, from `0` to `7`.
 *
 * @return     - the number, `0` when none was kept
 *             - `0` `physicid` is not a physics object, or `slot` is out of range
 */
native A3D_GetExtra(Physic:physicid, slot);

/**
 * @brief      What the component has done since the server started.
 *
 * @param      active        Bodies awake now: moving physics objects, zones, and the players, vehicles
 *                           and actors in motion.
 * @param      updates       Object updates sent to players.
 * @param      milliseconds  Time spent simulating and sending updates.
 *
 * @return     - `true` the numbers were written
 */
native bool:A3D_GetStats(&active, &updates, &Float:milliseconds);

/**
 * @brief      Find the first thing a segment between two points hits.
 *
 * The map is hit from both sides of its surfaces, since the collision of the game is one sided.
 * Zones are never hit, players only on foot, and a segment that starts inside a box, a sphere, a
 * player or a vehicle leaves it unseen.
 *
 * @param      startX   Start of the segment on the X axis.
 * @param      startY   Start on the Y axis.
 * @param      startZ   Start on the Z axis.
 * @param      endX     End of the segment on the X axis.
 * @param      endY     End on the Y axis.
 * @param      endZ     End on the Z axis.
 * @param      x        Point hit on the X axis.
 * @param      y        Point hit on the Y axis.
 * @param      z        Point hit on the Z axis.
 * @param      type     What was hit.
 * @param      hitid     Model of the map object, or the physics object, static object, player,
 *                       vehicle or actor hit.
 * @param      material  Surface of the game hit, see `A3D_GetMaterialName`.
 * @param      worldid   Virtual world to look in, or `-1` for every world.
 * @param      flags     What the segment may hit.
 *
 * @return     - `true` something was hit
 *             - `false` the segment is clear
 */
native bool:A3D_RayCast(Float:startX, Float:startY, Float:startZ, Float:endX, Float:endY, Float:endZ, &Float:x, &Float:y, &Float:z, &A3D_HIT:type = A3D_HIT_MAP, &hitid = 0, &material = 0, worldid = -1, A3D_COLLIDE:flags = A3D_COLLIDE_ALL);

/**
 * @brief      Find the first thing a segment hits, and the direction its surface faces there.
 *
 * The normal points towards the start of the segment. A flat floor gives `0.0`, `0.0`, `1.0`.
 *
 * @param      startX   Start of the segment on the X axis.
 * @param      startY   Start on the Y axis.
 * @param      startZ   Start on the Z axis.
 * @param      endX     End of the segment on the X axis.
 * @param      endY     End on the Y axis.
 * @param      endZ     End on the Z axis.
 * @param      x        Point hit on the X axis.
 * @param      y        Point hit on the Y axis.
 * @param      z        Point hit on the Z axis.
 * @param      normalX  Normal of the surface on the X axis.
 * @param      normalY  Normal on the Y axis.
 * @param      normalZ  Normal on the Z axis.
 * @param      type     What was hit.
 * @param      hitid     Model of the map object, or the physics object, static object, player,
 *                       vehicle or actor hit.
 * @param      material  Surface of the game hit, see `A3D_GetMaterialName`.
 * @param      worldid   Virtual world to look in, or `-1` for every world.
 * @param      flags     What the segment may hit.
 *
 * @return     - `true` something was hit
 *             - `false` the segment is clear
 */
native bool:A3D_RayCastNormal(Float:startX, Float:startY, Float:startZ, Float:endX, Float:endY, Float:endZ, &Float:x, &Float:y, &Float:z, &Float:normalX, &Float:normalY, &Float:normalZ, &A3D_HIT:type = A3D_HIT_MAP, &hitid = 0, &material = 0, worldid = -1, A3D_COLLIDE:flags = A3D_COLLIDE_ALL);

/**
 * @brief      Find everything a segment hits, nearest first.
 *
 * A model the segment goes through is hit where it enters and where it leaves.
 *
 * @param      startX   Start of the segment on the X axis.
 * @param      startY   Start on the Y axis.
 * @param      startZ   Start on the Z axis.
 * @param      endX     End of the segment on the X axis.
 * @param      endY     End on the Y axis.
 * @param      endZ     End on the Z axis.
 * @param      x        Points hit on the X axis.
 * @param      y        Points hit on the Y axis.
 * @param      z        Points hit on the Z axis.
 * @param      types      What each point hit.
 * @param      hitids     What each point hit, as the `hitid` of `A3D_RayCast`.
 * @param      materials  Surface of the game at each point.
 * @param      size       Room in each array.
 * @param      worldid    Virtual world to look in, or `-1` for every world.
 * @param      flags      What the segment may hit.
 *
 * @return     - the number of points written
 */
native A3D_RayCastAll(Float:startX, Float:startY, Float:startZ, Float:endX, Float:endY, Float:endZ, Float:x[], Float:y[], Float:z[], A3D_HIT:types[], hitids[], materials[], size = sizeof (x), worldid = -1, A3D_COLLIDE:flags = A3D_COLLIDE_ALL);

/**
 * @brief      Whether nothing stands between two points.
 *
 * Stops at the first thing found, so it is cheaper than `A3D_RayCast`.
 *
 * @param      x1       First point on the X axis.
 * @param      y1       First point on the Y axis.
 * @param      z1       First point on the Z axis.
 * @param      x2       Second point on the X axis.
 * @param      y2       Second point on the Y axis.
 * @param      z2       Second point on the Z axis.
 * @param      worldid  Virtual world to look in, or `-1` for every world.
 * @param      flags    What blocks the sight, the map and static objects by default.
 *
 * @return     - `true` the points see each other
 *             - `false` something stands between them
 */
native bool:A3D_IsLineOfSight(Float:x1, Float:y1, Float:z1, Float:x2, Float:y2, Float:z2, worldid = -1, A3D_COLLIDE:flags = A3D_COLLIDE_MAP | A3D_COLLIDE_STATIC);

/**
 * @brief      Find the ground under a point.
 *
 * Looks down from the point, so under a bridge or inside a building it finds the floor below and
 * not the roof above.
 *
 * @param      x        Point on the X axis.
 * @param      y        Point on the Y axis.
 * @param      z        Point on the Z axis, where the search starts.
 * @param      groundZ  Height of the ground.
 * @param      worldid  Virtual world to look in, or `-1` for every world.
 * @param      flags    What counts as ground, the map and static objects by default.
 * @param      depth    How far down to look.
 *
 * @return     - `true` the ground was found
 *             - `false` nothing lies below within `depth`
 */
native bool:A3D_FindGround(Float:x, Float:y, Float:z, &Float:groundZ, worldid = -1, A3D_COLLIDE:flags = A3D_COLLIDE_MAP | A3D_COLLIDE_STATIC, Float:depth = 1000.0);

/**
 * @brief      Move a sphere along a segment and find where it first touches something.
 *
 * Like `A3D_RayCast` for something with a size: a camera that must not enter walls, or a spot
 * where a thrown object would stop.
 *
 * @param      radius   Radius of the sphere.
 * @param      startX   Start of its centre on the X axis.
 * @param      startY   Start on the Y axis.
 * @param      startZ   Start on the Z axis.
 * @param      endX     End of its centre on the X axis.
 * @param      endY     End on the Y axis.
 * @param      endZ     End on the Z axis.
 * @param      x        Centre where it stops on the X axis, or the end when nothing is touched.
 * @param      y        Centre on the Y axis.
 * @param      z        Centre on the Z axis.
 * @param      type     What it touched.
 * @param      hitid     Model of the map object, or the physics object, static object, player,
 *                       vehicle or actor touched.
 * @param      material  Surface of the game touched, see `A3D_GetMaterialName`.
 * @param      worldid   Virtual world to look in, or `-1` for every world.
 * @param      flags     What the sphere may touch.
 *
 * @return     - `true` the sphere touched something
 *             - `false` it reached the end, or the radius is not positive
 */
native bool:A3D_SweepSphere(Float:radius, Float:startX, Float:startY, Float:startZ, Float:endX, Float:endY, Float:endZ, &Float:x, &Float:y, &Float:z, &A3D_HIT:type = A3D_HIT_MAP, &hitid = 0, &material = 0, worldid = -1, A3D_COLLIDE:flags = A3D_COLLIDE_ALL);

/**
 * @brief      Find everything a sphere overlaps.
 *
 * @param      x        Centre on the X axis.
 * @param      y        Centre on the Y axis.
 * @param      z        Centre on the Z axis.
 * @param      radius   Radius of the sphere.
 * @param      types    What each thing found is.
 * @param      hitids   What each thing found is, as the `hitid` of `A3D_RayCast`.
 * @param      size     Room in each array.
 * @param      worldid  Virtual world to look in, or `-1` for every world.
 * @param      flags    What may be found.
 *
 * @return     - the number of things written
 */
native A3D_OverlapSphere(Float:x, Float:y, Float:z, Float:radius, A3D_HIT:types[], hitids[], size = sizeof (hitids), worldid = -1, A3D_COLLIDE:flags = A3D_COLLIDE_ALL);

/**
 * @brief      Find everything a box overlaps, such as the room a vehicle or an object needs.
 *
 * @param      sizeX    Size on the X axis.
 * @param      sizeY    Size on the Y axis.
 * @param      sizeZ    Size on the Z axis.
 * @param      x        Centre on the X axis.
 * @param      y        Centre on the Y axis.
 * @param      z        Centre on the Z axis.
 * @param      rx       Rotation on the X axis.
 * @param      ry       Rotation on the Y axis.
 * @param      rz       Rotation on the Z axis.
 * @param      types    What each thing found is.
 * @param      hitids   What each thing found is, as the `hitid` of `A3D_RayCast`.
 * @param      size     Room in each array.
 * @param      worldid  Virtual world to look in, or `-1` for every world.
 * @param      flags    What may be found.
 *
 * @return     - the number of things written
 */
native A3D_OverlapBox(Float:sizeX, Float:sizeY, Float:sizeZ, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, A3D_HIT:types[], hitids[], size = sizeof (hitids), worldid = -1, A3D_COLLIDE:flags = A3D_COLLIDE_ALL);

/**
 * @brief      Find everything a model would overlap at a position, such as before placing an object.
 *
 * The model counts as the shape it moves with: the shape `models.json` gives it, or the convex
 * hull of its collision, which covers the gaps of a hollow model.
 *
 * @param      modelid  Model to place.
 * @param      x        Position on the X axis.
 * @param      y        Position on the Y axis.
 * @param      z        Position on the Z axis.
 * @param      rx       Rotation on the X axis.
 * @param      ry       Rotation on the Y axis.
 * @param      rz       Rotation on the Z axis.
 * @param      types    What each thing found is.
 * @param      hitids   What each thing found is, as the `hitid` of `A3D_RayCast`.
 * @param      size     Room in each array.
 * @param      worldid  Virtual world to look in, or `-1` for every world.
 * @param      flags    What may be found.
 *
 * @return     - the number of things written
 *             - `0` the model has no collision and no shape in `models.json`
 */
native A3D_OverlapModel(modelid, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, A3D_HIT:types[], hitids[], size = sizeof (hitids), worldid = -1, A3D_COLLIDE:flags = A3D_COLLIDE_ALL);

/**
 * @brief      Whether a model would touch anything at a position.
 *
 * Stops at the first thing found, so it is cheaper than `A3D_OverlapModel`. The model counts as
 * the shape it moves with, like in `A3D_OverlapModel`.
 *
 * @param      modelid  Model to place.
 * @param      x        Position on the X axis.
 * @param      y        Position on the Y axis.
 * @param      z        Position on the Z axis.
 * @param      rx       Rotation on the X axis.
 * @param      ry       Rotation on the Y axis.
 * @param      rz       Rotation on the Z axis.
 * @param      worldid  Virtual world to look in, or `-1` for every world.
 * @param      flags    What may be touched.
 *
 * @return     - `true` it would touch something
 *             - `false` the place is free, or the model has no collision and no shape in
 *               `models.json`
 */
native bool:A3D_IsModelColliding(modelid, Float:x, Float:y, Float:z, Float:rx = 0.0, Float:ry = 0.0, Float:rz = 0.0, worldid = -1, A3D_COLLIDE:flags = A3D_COLLIDE_ALL);

/**
 * @brief      Find the players within a distance of a point, nearest first.
 *
 * Players who are not spawned, are dead or spectate are left out.
 *
 * @param      x           Centre on the X axis.
 * @param      y           Centre on the Y axis.
 * @param      z           Centre on the Z axis.
 * @param      radius      Distance from the centre.
 * @param      players     The players found.
 * @param      size        Room in `players`.
 * @param      worldid     Virtual world of the players, or `-1` for any.
 * @param      interiorid  Interior of the players, or `-1` for any.
 * @param      sight       Whether the centre must see it, with the map and static objects in the
 *                         way.
 *
 * @return     - the number of players written
 */
native A3D_GetPlayersInSphere(Float:x, Float:y, Float:z, Float:radius, players[], size = sizeof (players), worldid = -1, interiorid = -1, bool:sight = false);

/**
 * @brief      Find the players inside a cone, such as the view of a guard or the spread of a
 *             shotgun, nearest first.
 *
 * A player is measured at the height of the chest. With `sight`, a player behind the map or a
 * static object is left out.
 *
 * @param      x           Apex of the cone on the X axis.
 * @param      y           Apex on the Y axis.
 * @param      z           Apex on the Z axis.
 * @param      directionX  Direction of the cone on the X axis.
 * @param      directionY  Direction on the Y axis.
 * @param      directionZ  Direction on the Z axis.
 * @param      angle       Angle between the direction and the side of the cone, in degrees.
 * @param      range       Length of the cone.
 * @param      players     The players found.
 * @param      size        Room in `players`.
 * @param      worldid     Virtual world of the players, or `-1` for any.
 * @param      interiorid  Interior of the players, or `-1` for any.
 * @param      sight       Whether the apex must see the player.
 * @param      exceptid    A player to leave out, such as the one who looks.
 *
 * @return     - the number of players written
 */
native A3D_GetPlayersInCone(Float:x, Float:y, Float:z, Float:directionX, Float:directionY, Float:directionZ, Float:angle, Float:range, players[], size = sizeof (players), worldid = -1, interiorid = -1, bool:sight = false, exceptid = INVALID_PLAYER_ID);

/**
 * @brief      Find the vehicles within a distance of a point, nearest first.
 *
 * @param      x           Centre on the X axis.
 * @param      y           Centre on the Y axis.
 * @param      z           Centre on the Z axis.
 * @param      radius      Distance from the centre.
 * @param      vehicles    The vehicles found.
 * @param      size        Room in `vehicles`.
 * @param      worldid     Virtual world of the vehicles, or `-1` for any.
 * @param      interiorid  Interior of the vehicles, or `-1` for any.
 * @param      sight       Whether the centre must see it, with the map and static objects in the
 *                         way.
 *
 * @return     - the number of vehicles written
 */
native A3D_GetVehiclesInSphere(Float:x, Float:y, Float:z, Float:radius, vehicles[], size = sizeof (vehicles), worldid = -1, interiorid = -1, bool:sight = false);

/**
 * @brief      Find the actors within a distance of a point, nearest first.
 *
 * @param      x        Centre on the X axis.
 * @param      y        Centre on the Y axis.
 * @param      z        Centre on the Z axis.
 * @param      radius   Distance from the centre.
 * @param      actors   The actors found.
 * @param      size     Room in `actors`.
 * @param      worldid  Virtual world of the actors, or `-1` for any.
 * @param      sight    Whether the centre must see them, with the map and static objects in the way.
 *
 * @return     - the number of actors written
 */
native A3D_GetActorsInSphere(Float:x, Float:y, Float:z, Float:radius, actors[], size = sizeof (actors), worldid = -1, bool:sight = false);

/**
 * @brief      Find the actors inside a cone, such as the view of a guard, nearest first.
 *
 * An actor is measured at the height of the chest.
 *
 * @param      x           Apex of the cone on the X axis.
 * @param      y           Apex on the Y axis.
 * @param      z           Apex on the Z axis.
 * @param      directionX  Direction of the cone on the X axis.
 * @param      directionY  Direction on the Y axis.
 * @param      directionZ  Direction on the Z axis.
 * @param      angle       Angle between the direction and the side of the cone, in degrees.
 * @param      range       Length of the cone.
 * @param      actors      The actors found.
 * @param      size        Room in `actors`.
 * @param      worldid     Virtual world of the actors, or `-1` for any.
 * @param      sight       Whether the apex must see them, with the map and static objects in the
 *                         way.
 *
 * @return     - the number of actors written
 */
native A3D_GetActorsInCone(Float:x, Float:y, Float:z, Float:directionX, Float:directionY, Float:directionZ, Float:angle, Float:range, actors[], size = sizeof (actors), worldid = -1, bool:sight = false);

/**
 * @brief      Push the moving physics objects around a point away from it.
 *
 * The push is `force` at the centre and fades to nothing at `radius`. Objects behind a wall are
 * pushed too.
 *
 * @param      x        Centre on the X axis.
 * @param      y        Centre on the Y axis.
 * @param      z        Centre on the Z axis.
 * @param      radius   Distance the explosion reaches.
 * @param      force    Impulse at the centre, in kilograms times metres per second.
 * @param      worldid  Virtual world of the explosion, or `-1` for every world.
 *
 * @return     - the number of objects pushed
 */
native A3D_CreateExplosion(Float:x, Float:y, Float:z, Float:radius, Float:force, worldid = -1);

/**
 * @brief      Find what a player looks at, from their camera.
 *
 * Reads the camera the client last sent. The player and the vehicle they are in are never hit.
 *
 * @param      playerid  Player who looks.
 * @param      x         Point looked at on the X axis.
 * @param      y         Point looked at on the Y axis.
 * @param      z         Point looked at on the Z axis.
 * @param      type      What is looked at.
 * @param      hitid     Model of the map object, or the physics object, static object, player,
 *                       vehicle or actor looked at.
 * @param      material  Surface of the game looked at, see `A3D_GetMaterialName`.
 * @param      range     How far to look.
 * @param      flags     What may be looked at.
 *
 * @return     - `true` something is looked at within `range`
 *             - `false` nothing, or the player is not connected or sent no camera yet
 */
native bool:A3D_GetPlayerTarget(playerid, &Float:x, &Float:y, &Float:z, &A3D_HIT:type = A3D_HIT_MAP, &hitid = 0, &material = 0, Float:range = 100.0, A3D_COLLIDE:flags = A3D_COLLIDE_ALL);

/**
 * @brief      Whether a point lies inside the collision of the map, such as a player who fell under
 *             the ground or into a closed model.
 *
 * The surface above the point must be seen from behind and no floor below it seen from above. So a
 * point under a bridge, a roof or the floor of an interior placed in the sky is in the open. A
 * point in a box or a sphere of the map is inside.
 *
 * @param      x        Point on the X axis.
 * @param      y        Point on the Y axis.
 * @param      z        Point on the Z axis.
 * @param      depth    How far under the surface above it the point lies, `0.0` in a box or a
 *                      sphere.
 * @param      worldid  Virtual world, for the map objects removed in it, or `-1` for every world.
 *
 * @return     - `true` the point is inside the map
 *             - `false` it is in the open
 */
native bool:A3D_IsPointInsideMap(Float:x, Float:y, Float:z, &Float:depth = 0.0, worldid = -1);

/**
 * @brief      Find a random spot on the ground around a point where something fits, such as a spawn
 *             or a dropped item.
 *
 * The spot lies on ground that faces up within 45 degrees, out of the water, with room for a sphere
 * of `clearance` resting on it, free of the map and of every kind of body.
 *
 * @param      x          Centre on the X axis.
 * @param      y          Centre on the Y axis.
 * @param      z          Centre on the Z axis.
 * @param      radius     Distance from the centre to look within.
 * @param      spotX      Spot found on the X axis.
 * @param      spotY      Spot found on the Y axis.
 * @param      spotZ      Height of the ground at the spot.
 * @param      height     How far above and below `z` the ground is looked for. Inside a building it
 *                        must stay under the ceiling.
 * @param      clearance  Radius of the room needed.
 * @param      worldid    Virtual world to look in, or `-1` for every world.
 *
 * @return     - `true` a spot was found
 *             - `false` none of 16 tries found one
 */
native bool:A3D_FindRandomPoint(Float:x, Float:y, Float:z, Float:radius, &Float:spotX, &Float:spotY, &Float:spotZ, Float:height = 5.0, Float:clearance = 0.5, worldid = -1);

/**
 * @brief      Follow something thrown, such as a grenade, and find where it first lands.
 *
 * The path is the curve of the throw under `gravity`, without air resistance. With a `radius`, a
 * sphere of that size is followed and the point is its centre when it touches. Start outside the
 * thrower, or leave `A3D_COLLIDE_PLAYERS` out of `flags`.
 *
 * @param      x         Start on the X axis.
 * @param      y         Start on the Y axis.
 * @param      z         Start on the Z axis.
 * @param      vx        Speed on the X axis, in metres per second.
 * @param      vy        Speed on the Y axis.
 * @param      vz        Speed on the Z axis.
 * @param      hitX      Point where it lands on the X axis.
 * @param      hitY      Point on the Y axis.
 * @param      hitZ      Point on the Z axis.
 * @param      time      Seconds it flies.
 * @param      type      What it lands on.
 * @param      hitid     Model of the map object, or the physics object, static object, player,
 *                       vehicle or actor.
 * @param      material  Surface of the game it lands on.
 * @param      radius    Radius of what is thrown, or `0.0` for a point.
 * @param      gravity   Gravity in metres per second squared.
 * @param      duration  Longest flight followed, in seconds.
 * @param      worldid   Virtual world to look in, or `-1` for every world.
 * @param      flags     What it may land on.
 *
 * @return     - `true` it lands within `duration`
 *             - `false` it flies on, or falls below `3dandreas.floor`
 */
native bool:A3D_PredictTrajectory(Float:x, Float:y, Float:z, Float:vx, Float:vy, Float:vz, &Float:hitX, &Float:hitY, &Float:hitZ, &Float:time = 0.0, &A3D_HIT:type = A3D_HIT_MAP, &hitid = 0, &material = 0, Float:radius = 0.0, Float:gravity = 9.81, Float:duration = 10.0, worldid = -1, A3D_COLLIDE:flags = A3D_COLLIDE_ALL);

/**
 * @brief      Find where a point of the world shows on the screen of a player.
 *
 * A point beside the screen gets coordinates beyond it.
 *
 * @param      playerid  Player whose screen it is.
 * @param      x         Point on the X axis.
 * @param      y         Point on the Y axis.
 * @param      z         Point on the Z axis.
 * @param      screenX   Where it shows across the screen.
 * @param      screenY   Where it shows down the screen.
 *
 * @return     - `true` the point lies in front of the camera
 *             - `false` it lies behind it, or the player sent no camera yet
 */
native bool:A3D_WorldToScreen(playerid, Float:x, Float:y, Float:z, &Float:screenX, &Float:screenY);

/**
 * @brief      Find the direction a point of the screen of a player looks into, from the position of
 *             their camera.
 *
 * @param      playerid    Player whose screen it is.
 * @param      screenX     Point across the screen.
 * @param      screenY     Point down the screen.
 * @param      directionX  Direction on the X axis, one metre long.
 * @param      directionY  Direction on the Y axis.
 * @param      directionZ  Direction on the Z axis.
 *
 * @return     - `true` the direction was written
 *             - `false` the player sent no camera yet
 */
native bool:A3D_ScreenToWorld(playerid, Float:screenX, Float:screenY, &Float:directionX, &Float:directionY, &Float:directionZ);

/**
 * @brief      Find what a point of the screen of a player shows, such as the place they clicked on a
 *             grid of text draws.
 *
 * The player and the vehicle they are in are never hit.
 *
 * @param      playerid  Player whose screen it is.
 * @param      screenX   Point across the screen.
 * @param      screenY   Point down the screen.
 * @param      x         Point shown on the X axis.
 * @param      y         Point shown on the Y axis.
 * @param      z         Point shown on the Z axis.
 * @param      type      What it shows.
 * @param      hitid     Model of the map object, or the physics object, static object, player,
 *                       vehicle or actor.
 * @param      material  Surface of the game it shows.
 * @param      range     How far to look.
 * @param      flags     What may be shown.
 *
 * @return     - `true` something shows there within `range`
 *             - `false` nothing, or the player sent no camera yet
 */
native bool:A3D_GetScreenTarget(playerid, Float:screenX, Float:screenY, &Float:x, &Float:y, &Float:z, &A3D_HIT:type = A3D_HIT_MAP, &hitid = 0, &material = 0, Float:range = 100.0, A3D_COLLIDE:flags = A3D_COLLIDE_ALL);

/**
 * @brief      Whether a point shows on the screen of a player.
 *
 * @param      playerid  Player whose screen it is.
 * @param      x         Point on the X axis.
 * @param      y         Point on the Y axis.
 * @param      z         Point on the Z axis.
 * @param      sight     Whether the camera must see it, with the map and static objects in the way.
 *
 * @return     - `true` the point shows
 *             - `false` it does not, or the player sent no camera yet
 */
native bool:A3D_IsPointOnScreen(playerid, Float:x, Float:y, Float:z, bool:sight = false);

/**
 * @brief      Find the players on the screen of a player, nearest first, in the same virtual world
 *             and interior.
 *
 * A player is measured at the height of the chest.
 *
 * @param      playerid  Player whose screen it is.
 * @param      players   The players found.
 * @param      size      Room in `players`.
 * @param      sight     Whether the camera must see them, with the map and static objects in the
 *                       way.
 * @param      range     How far from the camera to look.
 *
 * @return     - the number of players written
 */
native A3D_GetPlayersOnScreen(playerid, players[], size = sizeof (players), bool:sight = false, Float:range = 300.0);

/**
 * @brief      Find the vehicles on the screen of a player, nearest first, in the same virtual world
 *             and interior, leaving out the one they are in.
 *
 * @param      playerid  Player whose screen it is.
 * @param      vehicles  The vehicles found.
 * @param      size      Room in `vehicles`.
 * @param      sight     Whether the camera must see them, with the map and static objects in the
 *                       way.
 * @param      range     How far from the camera to look.
 *
 * @return     - the number of vehicles written
 */
native A3D_GetVehiclesOnScreen(playerid, vehicles[], size = sizeof (vehicles), bool:sight = false, Float:range = 300.0);

/**
 * @brief      Find the actors on the screen of a player, nearest first, in the same virtual world.
 *
 * @param      playerid  Player whose screen it is.
 * @param      actors    The actors found.
 * @param      size      Room in `actors`.
 * @param      sight     Whether the camera must see them, with the map and static objects in the
 *                       way.
 * @param      range     How far from the camera to look.
 *
 * @return     - the number of actors written
 */
native A3D_GetActorsOnScreen(playerid, actors[], size = sizeof (actors), bool:sight = false, Float:range = 300.0);

/**
 * @brief      Remove the map objects of a model around a point, as `RemoveBuildingForPlayer` does
 *             for what players see.
 *
 * A removal may hold for one virtual world only. Physics objects resting on a removed map object
 * wake up and fall.
 *
 * @param      modelid  Model to remove, or `-1` for every model.
 * @param      x        Centre on the X axis.
 * @param      y        Centre on the Y axis.
 * @param      z        Centre on the Z axis.
 * @param      radius   Distance from the centre to the position of the map objects.
 * @param      worldid  Virtual world they are removed in, or `-1` for every world.
 *
 * @return     - the number of map objects removed
 */
native A3D_RemoveBuilding(modelid, Float:x, Float:y, Float:z, Float:radius, worldid = -1);

/**
 * @brief      Bring back map objects removed by `A3D_RemoveBuilding`.
 *
 * Restoring for every world undoes every removal of a map object. A map object removed for every
 * world comes back only when it is restored for every world.
 *
 * @param      modelid  Model to bring back, or `-1` for every model.
 * @param      x        Centre on the X axis.
 * @param      y        Centre on the Y axis.
 * @param      z        Centre on the Z axis.
 * @param      radius   Distance from the centre to the position of the map objects.
 * @param      worldid  Virtual world they come back in, or `-1` for every world.
 *
 * @return     - the number of map objects brought back
 */
native A3D_RestoreBuilding(modelid, Float:x, Float:y, Float:z, Float:radius, worldid = -1);

/**
 * @brief      Find the height of the water at a point of the map.
 *
 * Where two pools cover the point, the higher one counts. Past the edges of the map lies the sea at
 * `0.0`, as in the game. The level is that of calm water, without waves.
 *
 * @param      x      Point on the X axis.
 * @param      y      Point on the Y axis.
 * @param      level  Height of the surface of the water.
 *
 * @return     - `true` water covers the point
 *             - `false` no water covers it, or the database has no water
 */
native bool:A3D_GetWaterLevel(Float:x, Float:y, &Float:level);

/**
 * @brief      Whether a point lies in the water of the map.
 *
 * The point must be under the surface with nothing of the map between them, so the pool of an
 * interior placed above the city does not flood the streets below it.
 *
 * @param      x      Point on the X axis.
 * @param      y      Point on the Y axis.
 * @param      z      Point on the Z axis.
 * @param      depth  How far under the surface the point lies.
 *
 * @return     - `true` the point is in water
 *             - `false` it is not, or the database has no water
 */
native bool:A3D_IsPointInWater(Float:x, Float:y, Float:z, &Float:depth = 0.0);

/**
 * @brief      Find the box that holds a model, around its position.
 *
 * Models without collision, such as plants, still have the box the game gives them.
 *
 * @param      modelid  Model to read.
 * @param      minX     Lowest corner on the X axis.
 * @param      minY     Lowest corner on the Y axis.
 * @param      minZ     Lowest corner on the Z axis.
 * @param      maxX     Highest corner on the X axis.
 * @param      maxY     Highest corner on the Y axis.
 * @param      maxZ     Highest corner on the Z axis.
 *
 * @return     - `true` the box was written
 *             - `false` the database does not know the model
 */
native bool:A3D_GetModelBoundingBox(modelid, &Float:minX, &Float:minY, &Float:minZ, &Float:maxX, &Float:maxY, &Float:maxZ);

/**
 * @brief      Find the sphere that holds a model, around its position.
 *
 * @param      modelid  Model to read.
 * @param      x        Centre on the X axis.
 * @param      y        Centre on the Y axis.
 * @param      z        Centre on the Z axis.
 * @param      radius   Radius of the sphere.
 *
 * @return     - `true` the sphere was written
 *             - `false` the database does not know the model
 */
native bool:A3D_GetModelBoundingSphere(modelid, &Float:x, &Float:y, &Float:z, &Float:radius);

/**
 * @brief      Find how the game moves a model that can be knocked over, such as a bin or a crate,
 *             in the values the natives of this include take.
 *
 * The values come from object.dat. The buoyancy is `100.0` over the share of the object the game
 * keeps under water, and the damping matches the air resistance of the game at 50 frames a second.
 *
 * @param      modelid   Model to read.
 * @param      mass      Mass in kilograms, for `A3D_CreateObject`.
 * @param      bounce    Bounce, for `A3D_SetBounce`.
 * @param      buoyancy  Buoyancy, for `A3D_SetBuoyancy`.
 * @param      damping   Loss of speed, for the `linear` value of `A3D_SetDamping`.
 *
 * @return     - `true` the values were written
 *             - `false` the game never moves the model, or the database has no such data
 */
native bool:A3D_GetModelPhysics(modelid, &Float:mass, &Float:bounce, &Float:buoyancy, &Float:damping);

/**
 * @brief      Find the name the game gives a surface, such as `TARMAC` or `GRASS_SHORT_LUSH`.
 *
 * @param      material  Surface, as the natives that hit something give it.
 * @param      name      The name.
 * @param      size      Room in `name`.
 *
 * @return     - `true` the name was written
 *             - `false` there is no such surface, or the database has no surfaces
 */
native bool:A3D_GetMaterialName(material, name[], size = sizeof (name));

/**
 * @brief      Find what the game says of a surface, such as whether bullets pass through it or it
 *             is sand, glass or stairs.
 *
 * @param      material  Surface to read.
 *
 * @return     - the flags of the surface
 *             - `A3D_MATERIAL:0` there is no such surface, or the database has no surfaces
 */
native A3D_MATERIAL:A3D_GetMaterialFlags(material);

/**
 * @brief      Turn the rotation of an object into a quaternion, in the order of
 *             `GetVehicleRotationQuat`.
 *
 * @param      rx  Rotation on the X axis.
 * @param      ry  Rotation on the Y axis.
 * @param      rz  Rotation on the Z axis.
 * @param      w   Real part of the quaternion.
 * @param      x   Part of the X axis.
 * @param      y   Part of the Y axis.
 * @param      z   Part of the Z axis.
 *
 * @return     - `true` the quaternion was written
 */
native bool:A3D_EulerToQuaternion(Float:rx, Float:ry, Float:rz, &Float:w, &Float:x, &Float:y, &Float:z);

/**
 * @brief      Turn a quaternion, such as one from `GetVehicleRotationQuat`, into the rotation of an
 *             object.
 *
 * @param      w   Real part of the quaternion.
 * @param      x   Part of the X axis.
 * @param      y   Part of the Y axis.
 * @param      z   Part of the Z axis.
 * @param      rx  Rotation on the X axis.
 * @param      ry  Rotation on the Y axis.
 * @param      rz  Rotation on the Z axis.
 *
 * @return     - `true` the rotation was written
 */
native bool:A3D_QuaternionToEuler(Float:w, Float:x, Float:y, Float:z, &Float:rx, &Float:ry, &Float:rz);

/**
 * @brief      Find the rotation that stands an object on a surface, such as a crate on a slope.
 *
 * The object turns by `heading` around its own up axis first, as its facing angle, and its up axis
 * then follows the normal. `A3D_RayCastNormal` gives the normal of a surface.
 *
 * @param      normalX  Normal of the surface on the X axis.
 * @param      normalY  Normal on the Y axis.
 * @param      normalZ  Normal on the Z axis.
 * @param      rx       Rotation on the X axis.
 * @param      ry       Rotation on the Y axis.
 * @param      rz       Rotation on the Z axis.
 * @param      heading  Facing angle of the object, in degrees.
 *
 * @return     - `true` the rotation was written
 *             - `false` the normal has no length
 */
native bool:A3D_GetSurfaceRotation(Float:normalX, Float:normalY, Float:normalZ, &Float:rx, &Float:ry, &Float:rz, Float:heading = 0.0);

/**
 * @brief      Find the direction something takes after bouncing off a surface, such as a bullet
 *             that ricochets.
 *
 * @param      directionX  Direction before the bounce on the X axis.
 * @param      directionY  Direction on the Y axis.
 * @param      directionZ  Direction on the Z axis.
 * @param      normalX     Normal of the surface on the X axis.
 * @param      normalY     Normal on the Y axis.
 * @param      normalZ     Normal on the Z axis.
 * @param      x           Direction after the bounce on the X axis, as long as before.
 * @param      y           Direction on the Y axis.
 * @param      z           Direction on the Z axis.
 *
 * @return     - `true` the direction was written
 *             - `false` the normal has no length
 */
native bool:A3D_GetReflection(Float:directionX, Float:directionY, Float:directionZ, Float:normalX, Float:normalY, Float:normalZ, &Float:x, &Float:y, &Float:z);

/**
 * @brief      Find where a point given relative to an object lies in the world, such as a spot on a
 *             table or the end of a ramp.
 *
 * The offset follows the axes of the object, which turn with it.
 *
 * @param      x        Position of the object on the X axis.
 * @param      y        Position on the Y axis.
 * @param      z        Position on the Z axis.
 * @param      rx       Rotation of the object on the X axis.
 * @param      ry       Rotation on the Y axis.
 * @param      rz       Rotation on the Z axis.
 * @param      offsetX  Offset along the X axis of the object.
 * @param      offsetY  Offset along its Y axis.
 * @param      offsetZ  Offset along its Z axis.
 * @param      worldX   The point in the world on the X axis.
 * @param      worldY   The point on the Y axis.
 * @param      worldZ   The point on the Z axis.
 *
 * @return     - `true` the point was written
 */
native bool:A3D_GetOffsetPosition(Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, Float:offsetX, Float:offsetY, Float:offsetZ, &Float:worldX, &Float:worldY, &Float:worldZ);

/**
 * @brief      Find where a point of the world lies relative to an object, the reverse of
 *             `A3D_GetOffsetPosition`.
 *
 * @param      x        Position of the object on the X axis.
 * @param      y        Position on the Y axis.
 * @param      z        Position on the Z axis.
 * @param      rx       Rotation of the object on the X axis.
 * @param      ry       Rotation on the Y axis.
 * @param      rz       Rotation on the Z axis.
 * @param      worldX   Point in the world on the X axis.
 * @param      worldY   Point on the Y axis.
 * @param      worldZ   Point on the Z axis.
 * @param      offsetX  Offset along the X axis of the object.
 * @param      offsetY  Offset along its Y axis.
 * @param      offsetZ  Offset along its Z axis.
 *
 * @return     - `true` the offset was written
 */
native bool:A3D_GetPositionOffset(Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz, Float:worldX, Float:worldY, Float:worldZ, &Float:offsetX, &Float:offsetY, &Float:offsetZ);

/**
 * @brief      Find the rotation in the world of something turned relative to an object, such as an
 *             object attached to another.
 *
 * @param      rx        Rotation of the object on the X axis.
 * @param      ry        Rotation on the Y axis.
 * @param      rz        Rotation on the Z axis.
 * @param      offsetRX  Rotation relative to the object on the X axis.
 * @param      offsetRY  Relative rotation on the Y axis.
 * @param      offsetRZ  Relative rotation on the Z axis.
 * @param      worldRX   Rotation in the world on the X axis.
 * @param      worldRY   Rotation on the Y axis.
 * @param      worldRZ   Rotation on the Z axis.
 *
 * @return     - `true` the rotation was written
 */
native bool:A3D_GetOffsetRotation(Float:rx, Float:ry, Float:rz, Float:offsetRX, Float:offsetRY, Float:offsetRZ, &Float:worldRX, &Float:worldRY, &Float:worldRZ);

/**
 * @brief      Find the rotation that turns the front of an object, its Y axis, to a point, such as a
 *             camera or a turret.
 *
 * @param      x        Position of the object on the X axis.
 * @param      y        Position on the Y axis.
 * @param      z        Position on the Z axis.
 * @param      targetX  Point to face on the X axis.
 * @param      targetY  Point on the Y axis.
 * @param      targetZ  Point on the Z axis.
 * @param      rx       Rotation on the X axis.
 * @param      ry       Rotation on the Y axis.
 * @param      rz       Rotation on the Z axis.
 *
 * @return     - `true` the rotation was written
 *             - `false` both points are the same
 */
native bool:A3D_GetLookRotation(Float:x, Float:y, Float:z, Float:targetX, Float:targetY, Float:targetZ, &Float:rx, &Float:ry, &Float:rz);

/**
 * @brief      Find where a point given relative to a vehicle lies in the world, as the vehicle
 *             stands and leans now.
 *
 * X points to the right of the vehicle, Y to its front and Z up.
 *
 * @param      vehicleid  Vehicle the offset belongs to.
 * @param      offsetX    Offset along the X axis of the vehicle.
 * @param      offsetY    Offset along its Y axis.
 * @param      offsetZ    Offset along its Z axis.
 * @param      x          The point in the world on the X axis.
 * @param      y          The point on the Y axis.
 * @param      z          The point on the Z axis.
 *
 * @return     - `true` the point was written
 *             - `false` there is no such vehicle
 */
native bool:A3D_GetVehicleOffsetPosition(vehicleid, Float:offsetX, Float:offsetY, Float:offsetZ, &Float:x, &Float:y, &Float:z);

/**
 * @brief      Find where a point of the world lies relative to a vehicle, such as whether a player
 *             stands behind it.
 *
 * @param      vehicleid  Vehicle to measure from.
 * @param      x          Point in the world on the X axis.
 * @param      y          Point on the Y axis.
 * @param      z          Point on the Z axis.
 * @param      offsetX    Offset along the X axis of the vehicle, positive to its right.
 * @param      offsetY    Offset along its Y axis, positive to its front.
 * @param      offsetZ    Offset along its Z axis, positive up.
 *
 * @return     - `true` the offset was written
 *             - `false` there is no such vehicle
 */
native bool:A3D_GetVehiclePositionOffset(vehicleid, Float:x, Float:y, Float:z, &Float:offsetX, &Float:offsetY, &Float:offsetZ);

/**
 * @brief      Find where a part of a vehicle lies in the world, such as the boot for a player to
 *             open it or the petrol cap to refuel.
 *
 * Parts come from the size of the model and the places the game gives for its seats, wheels and
 * petrol cap. Bumpers lie at the ends of the box at the height the game gives them, doors on its
 * sides at the height of the seats, and the driver sits on the left.
 *
 * @param      vehicleid  Vehicle to read.
 * @param      part       Part to find.
 * @param      x          Position of the part on the X axis.
 * @param      y          Position on the Y axis.
 * @param      z          Position on the Z axis.
 *
 * @return     - `true` the position was written
 *             - `false` there is no such vehicle, or its model lacks the part
 */
native bool:A3D_GetVehiclePartPosition(vehicleid, A3D_PART:part, &Float:x, &Float:y, &Float:z);

/**
 * @brief      Find the best route between two points along the paths of the game.
 *
 * Each end goes to the nearest point of a path the route can use, within `reach`. Height counts
 * twice, so a road on a bridge overhead is farther than a road beside. When the nearest paths do
 * not connect, the ends go to the nearest paths of the main network instead. The route keeps its
 * points and its turns until `A3D_DestroyRoute`, or until the script unloads.
 *
 * `A3D_ROUTE_FASTEST` takes the least time and `A3D_ROUTE_SHORTEST` the least distance.
 * `A3D_ROUTE_ANY_WAY` takes the least time driving one way streets both ways, as the emergency
 * services do. `A3D_ROUTE_TRAFFIC` keeps to the roads the traffic of the game drives, which leaves
 * out dirt roads, tracks and alleys. `A3D_ROUTE_BOAT` keeps to the lanes for boats, and
 * `A3D_ROUTE_WALK` to the paths of pedestrians, or the roads where there are none. With the
 * navigation mesh, a walk shorter than 500 metres goes the shortest way on the mesh, and a longer
 * one walks on the mesh from its start to the paths and from the paths to its end.
 *
 * @param      fromX    Start on the X axis.
 * @param      fromY    Start on the Y axis.
 * @param      fromZ    Start on the Z axis.
 * @param      toX      End on the X axis.
 * @param      toY      End on the Y axis.
 * @param      toZ      End on the Z axis.
 * @param      type     How the route weighs the paths.
 * @param      heading  Direction the vehicle faces at the start, as `GetVehicleZAngle` gives it, or
 *                      `-1.0` for none. Starting against it costs a turn back.
 * @param      reach    How far from each end to look for a path, in metres.
 *
 * @return     - the route
 *             - `INVALID_ROUTE_ID` no path is within reach, or the ends are not connected
 */
native Route:A3D_FindRoute(Float:fromX, Float:fromY, Float:fromZ, Float:toX, Float:toY, Float:toZ, A3D_ROUTE:type = A3D_ROUTE_FASTEST, Float:heading = -1.0, Float:reach = 300.0);

/**
 * @brief      Destroy a route, and hide it from the players it is shown to.
 *
 * @param      routeid  Route to destroy.
 *
 * @return     - `true` the route was destroyed
 *             - `false` the route does not exist
 */
native bool:A3D_DestroyRoute(Route:routeid);

/**
 * @brief      Whether a route exists.
 *
 * @param      routeid  Route to check.
 */
native bool:A3D_IsValidRoute(Route:routeid);

/**
 * @brief      Length of a route, in metres.
 *
 * @param      routeid  Route to measure.
 *
 * @return     - the length
 *             - `0.0` the route does not exist
 */
native Float:A3D_GetRouteLength(Route:routeid);

/**
 * @brief      Time a route takes, in seconds.
 *
 * Vehicles drive at 36 metres a second on highways, 22 on other roads and 14 on the roads the
 * traffic leaves alone, and lose the time the turns cost. Boats sail at 15 metres a second and
 * pedestrians run at 5.
 *
 * @param      routeid  Route to measure.
 *
 * @return     - the time
 *             - `0.0` the route does not exist
 */
native Float:A3D_GetRouteTime(Route:routeid);

/**
 * @brief      Number of points of a route, from its start to its end.
 *
 * @param      routeid  Route to read.
 *
 * @return     - the number of points
 *             - `0` the route does not exist
 */
native A3D_GetRoutePointCount(Route:routeid);

/**
 * @brief      Read a point of a route.
 *
 * @param      routeid  Route to read.
 * @param      index    Index of the point, from `0`.
 * @param      x        Point on the X axis.
 * @param      y        Point on the Y axis.
 * @param      z        Point on the Z axis.
 *
 * @return     - `true` the point was read
 *             - `false` the route or the point does not exist
 */
native bool:A3D_GetRoutePoint(Route:routeid, index, &Float:x, &Float:y, &Float:z);

/**
 * @brief      Copy points of a route into three arrays.
 *
 * @param      routeid  Route to read.
 * @param      x        Points on the X axis.
 * @param      y        Points on the Y axis.
 * @param      z        Points on the Z axis.
 * @param      start    Index of the first point to copy.
 * @param      size     Room in each array.
 *
 * @return     - the number of points copied
 */
native A3D_GetRoutePoints(Route:routeid, Float:x[], Float:y[], Float:z[], start = 0, size = sizeof (x));

/**
 * @brief      Number of turns of a route.
 *
 * @param      routeid  Route to read.
 *
 * @return     - the number of turns
 *             - `0` the route does not exist
 */
native A3D_GetRouteTurnCount(Route:routeid);

/**
 * @brief      Read a turn of a route, at a junction where more than two ways meet.
 *
 * Turns a few metres apart count as one, so crossing a divided road is a single turn. A route that
 * starts against `heading` begins with `A3D_TURN_BACK` at its first point.
 *
 * @param      routeid   Route to read.
 * @param      index     Index of the turn, from `0`.
 * @param      x         Junction on the X axis.
 * @param      y         Junction on the Y axis.
 * @param      z         Junction on the Z axis.
 * @param      distance  Distance from the start of the route to the junction, in metres.
 *
 * @return     - the turn
 *             - `A3D_TURN_NONE` the route or the turn does not exist
 */
native A3D_TURN:A3D_GetRouteTurn(Route:routeid, index, &Float:x, &Float:y, &Float:z, &Float:distance = 0.0);

/**
 * @brief      Find where a point is along a route: the distance left, the distance from the route
 *             and the next turn.
 *
 * The point goes to the nearest part of the route, and where the route passes a place twice, to the
 * part nearer where the last call put it. A distance from the route that keeps growing means the
 * player took another way, and a new route from where they are brings them back.
 *
 * @param      routeid       Route to follow.
 * @param      x             Point on the X axis.
 * @param      y             Point on the Y axis.
 * @param      z             Point on the Z axis.
 * @param      remaining     Distance left to the end of the route, in metres.
 * @param      away          Distance from the point to the route.
 * @param      next          Index of the next point of the route.
 * @param      turn          The next turn, or `A3D_TURN_NONE` when none is left.
 * @param      turnDistance  Distance to the next turn, or to the end when none is left.
 *
 * @return     - `true` the route exists
 *             - `false` it does not
 */
native bool:A3D_GetRouteProgress(Route:routeid, Float:x, Float:y, Float:z, &Float:remaining, &Float:away = 0.0, &next = 0, &A3D_TURN:turn = A3D_TURN_NONE, &Float:turnDistance = 0.0);

/**
 * @brief      Measure the best route between two points without keeping it, which is faster than
 *             finding it.
 *
 * @param      fromX    Start on the X axis.
 * @param      fromY    Start on the Y axis.
 * @param      fromZ    Start on the Z axis.
 * @param      toX      End on the X axis.
 * @param      toY      End on the Y axis.
 * @param      toZ      End on the Z axis.
 * @param      length   Length of the route, in metres.
 * @param      time     Time the route takes, in seconds.
 * @param      type     How the route weighs the paths.
 * @param      heading  Direction the vehicle faces at the start, or `-1.0` for none.
 * @param      reach    How far from each end to look for a path, in metres.
 *
 * @return     - `true` a route was found
 *             - `false` no path is within reach, or the ends are not connected
 */
native bool:A3D_MeasureRoute(Float:fromX, Float:fromY, Float:fromZ, Float:toX, Float:toY, Float:toZ, &Float:length, &Float:time, A3D_ROUTE:type = A3D_ROUTE_FASTEST, Float:heading = -1.0, Float:reach = 300.0);

/**
 * @brief      Find which of several points the best route from a point reaches first, such as the
 *             nearest hospital by road rather than in a straight line.
 *
 * Every point is weighed in one search, which takes about as long as a few routes.
 *
 * @param      x        Start on the X axis.
 * @param      y        Start on the Y axis.
 * @param      z        Start on the Z axis.
 * @param      targetX  Points on the X axis.
 * @param      targetY  Points on the Y axis.
 * @param      targetZ  Points on the Z axis.
 * @param      count    Number of points.
 * @param      type     How the routes weigh the paths.
 * @param      heading  Direction the vehicle faces at the start, or `-1.0` for none.
 * @param      reach    How far from each point to look for a path, in metres.
 *
 * @return     - the index of the point reached first
 *             - `-1` none can be reached
 */
native A3D_FindNearestByRoute(Float:x, Float:y, Float:z, const Float:targetX[], const Float:targetY[], const Float:targetZ[], count = sizeof (targetX), A3D_ROUTE:type = A3D_ROUTE_FASTEST, Float:heading = -1.0, Float:reach = 300.0);

/**
 * @brief      Find the nearest point of a path a route can use, and the direction of the path there.
 *
 * On a one way street the angle points the way the traffic goes, which places a vehicle on the road
 * facing the right way.
 *
 * @param      x        Point on the X axis.
 * @param      y        Point on the Y axis.
 * @param      z        Point on the Z axis.
 * @param      pathX    Point of the path on the X axis.
 * @param      pathY    Point of the path on the Y axis.
 * @param      pathZ    Point of the path on the Z axis.
 * @param      angle    Direction of the path, as `SetVehicleZAngle` takes it.
 * @param      type     Which paths count.
 * @param      reach    How far to look, in metres.
 *
 * @return     - `true` a path is within reach
 *             - `false` none is
 */
native bool:A3D_GetNearestPathPoint(Float:x, Float:y, Float:z, &Float:pathX, &Float:pathY, &Float:pathZ, &Float:angle = 0.0, A3D_ROUTE:type = A3D_ROUTE_FASTEST, Float:reach = 100.0);

/**
 * @brief      Show a route on the radar and the map of a player as a line, which moves on as they go.
 *
 * The line is made of gang zones sent straight to the client of the player. They are not gang zones
 * of the server, so they take no memory or id there, only room among the 1024 gang zones the client
 * shows. The line from the player to the end is whole when it takes no more than `squares` gang
 * zones as wide as the line. Otherwise half of them draw the route ahead of the player without a
 * gap, and the rest mark the way beyond as dots, down to a dot at the end. Only the gang zones that
 * change are sent as the player goes.
 *
 * A player is shown one route at a time. A route shown in place of another of the same colour and
 * width keeps the gang zones of the way the two share, so a route found again from where the player
 * strayed sends little. Show the new route before destroying the old one, or the line is sent whole
 * again. `A3D_OnRouteArrived`, `A3D_OnRouteLeft` and `A3D_OnRouteAdvanced` tell the script that found
 * the route when the player reaches its end, strays from it or moves along it.
 *
 * @param      playerid  Player to show the route to.
 * @param      routeid   Route to show.
 * @param      colour    Colour of the line, as `0xRRGGBBAA`.
 * @param      width     Width of the line, in metres.
 * @param      squares   Most gang zones the line may use, up to `512`. With a width of 8 metres, 128
 *                       draw a whole line a kilometre long and 512 four kilometres long.
 *
 * @return     - `true` the route is shown
 *             - `false` the player or the route does not exist
 */
native bool:A3D_ShowRouteForPlayer(playerid, Route:routeid, colour = 0x9966FFFF, Float:width = 8.0, squares = 128);

/**
 * @brief      Hide the route shown to a player.
 *
 * @param      playerid  Player to hide it from.
 *
 * @return     - `true` the route was hidden
 *             - `false` no route is shown to the player
 */
native bool:A3D_HideRouteForPlayer(playerid);

/**
 * @brief      The route shown to a player.
 *
 * @param      playerid  Player to check.
 *
 * @return     - the route
 *             - `INVALID_ROUTE_ID` no route is shown to the player
 */
native Route:A3D_GetPlayerRoute(playerid);

/**
 * @brief      Whether the navigation mesh was read. A call made while the server starts waits for it.
 */
native bool:A3D_IsWalkMeshLoaded();

/**
 * @brief      Find the nearest point of the navigation mesh, where a pedestrian can stand.
 *
 * @param      x      Point on the X axis.
 * @param      y      Point on the Y axis.
 * @param      z      Point on the Z axis.
 * @param      walkX  Point of the mesh on the X axis.
 * @param      walkY  Point of the mesh on the Y axis.
 * @param      walkZ  Point of the mesh on the Z axis.
 * @param      reach  How far to look across, in metres. Twice as far is looked up and down.
 *
 * @return     - `true` the mesh is within reach
 *             - `false` it is not, or there is no navigation mesh
 */
native bool:A3D_GetNearestWalkPoint(Float:x, Float:y, Float:z, &Float:walkX, &Float:walkY, &Float:walkZ, Float:reach = 3.0);

/**
 * @brief      Find a random point of the navigation mesh within a radius, which can be walked to from
 *             the centre, such as where an actor wanders next.
 *
 * @param      x       Centre on the X axis.
 * @param      y       Centre on the Y axis.
 * @param      z       Centre on the Z axis.
 * @param      radius  Distance from the centre, in metres.
 * @param      walkX   Point found on the X axis.
 * @param      walkY   Point found on the Y axis.
 * @param      walkZ   Point found on the Z axis.
 *
 * @return     - `true` a point was found
 *             - `false` the centre is off the mesh, or there is no navigation mesh
 */
native bool:A3D_GetRandomWalkPoint(Float:x, Float:y, Float:z, Float:radius, &Float:walkX, &Float:walkY, &Float:walkZ);

/**
 * @brief      Find where a straight walk between two points is stopped by a wall, a building, a
 *             ledge or the water.
 *
 * The walk follows the ground, up and down slopes and stairs. A start off the mesh stops it where it
 * starts.
 *
 * @param      fromX  Start on the X axis.
 * @param      fromY  Start on the Y axis.
 * @param      fromZ  Start on the Z axis.
 * @param      toX    End on the X axis.
 * @param      toY    End on the Y axis.
 * @param      toZ    End on the Z axis.
 * @param      x      Where the walk stops on the X axis.
 * @param      y      Where the walk stops on the Y axis.
 * @param      z      Where the walk stops on the Z axis.
 *
 * @return     - `true` the walk is stopped
 *             - `false` the way is clear
 */
native bool:A3D_WalkRayCast(Float:fromX, Float:fromY, Float:fromZ, Float:toX, Float:toY, Float:toZ, &Float:x = 0.0, &Float:y = 0.0, &Float:z = 0.0);

/**
 * @brief      Create four walls around a rectangle on the ground.
 *
 * @param      minX     West edge.
 * @param      minY     South edge.
 * @param      maxX     East edge.
 * @param      maxY     North edge.
 * @param      minZ     Bottom of the walls.
 * @param      maxZ     Top of the walls.
 * @param      walls    The four walls created, south, north, west and east.
 * @param      worldid  Virtual world they collide in, or `-1` for every world.
 * @param      width    Thickness of each wall, outside the rectangle.
 *
 * @return     - `true` the four walls were created
 *             - `false` the rectangle is empty, or the world is full
 */
stock bool:A3D_CreateArea(Float:minX, Float:minY, Float:maxX, Float:maxY, Float:minZ, Float:maxZ, Physic:walls[4], worldid = -1, Float:width = 0.2) {
    if (maxX <= minX || maxY <= minY || maxZ <= minZ) {
        return false;
    }

    new const
        Float:sizeX = maxX - minX + width * 2.0,
        Float:sizeY = maxY - minY,
        Float:height = maxZ - minZ,
        Float:centreX = (minX + maxX) / 2.0,
        Float:centreY = (minY + maxY) / 2.0,
        Float:centreZ = (minZ + maxZ) / 2.0
    ;

    walls[0] = A3D_CreateStaticBox(sizeX, width, height, centreX, minY - width / 2.0, centreZ, 0.0, 0.0, 0.0, worldid);
    walls[1] = A3D_CreateStaticBox(sizeX, width, height, centreX, maxY + width / 2.0, centreZ, 0.0, 0.0, 0.0, worldid);
    walls[2] = A3D_CreateStaticBox(width, sizeY, height, minX - width / 2.0, centreY, centreZ, 0.0, 0.0, 0.0, worldid);
    walls[3] = A3D_CreateStaticBox(width, sizeY, height, maxX + width / 2.0, centreY, centreZ, 0.0, 0.0, 0.0, worldid);

    for (new i; i != 4; ++i) {
        if (walls[i] == INVALID_PHYSIC_ID) {
            for (new j; j != 4; ++j) {
                A3D_DestroyObject(walls[j]);
            }

            return false;
        }
    }

    return true;
}

/**
 * # Events
 */

/**
 * @brief      A moving physics object hit something faster than its collision speed.
 *
 * @param      physicid  Physics object that hit.
 * @param      type      What it hit.
 * @param      hitid     Model of the map object, or the physics object, static object, player,
 *                       vehicle or actor it hit.
 * @param      x         Point of contact on the X axis.
 * @param      y         Point of contact on the Y axis.
 * @param      z         Point of contact on the Z axis.
 * @param      speed     Speed of the impact, in metres per second.
 */
forward A3D_OnCollided(Physic:physicid, A3D_HIT:type, hitid, Float:x, Float:y, Float:z, Float:speed);

/**
 * @brief      A moving physics object came to rest, or fell below `3dandreas.floor` and was
 *             stopped there.
 *
 * @param      physicid  Physics object that stopped.
 * @param      x         Position on the X axis.
 * @param      y         Position on the Y axis.
 * @param      z         Position on the Z axis.
 */
forward A3D_OnStopped(Physic:physicid, Float:x, Float:y, Float:z);

/**
 * @brief      A moving physics object entered a zone.
 *
 * @param      zoneid    Zone it entered.
 * @param      physicid  Physics object that entered.
 */
forward A3D_OnEntered(Physic:zoneid, Physic:physicid);

/**
 * @brief      A moving physics object left a zone.
 *
 * @param      zoneid    Zone it left.
 * @param      physicid  Physics object that left.
 */
forward A3D_OnLeft(Physic:zoneid, Physic:physicid);

/**
 * @brief      A player reached the end of the route shown to them. The route stays on the map until it
 *             is hidden or destroyed.
 *
 * @param      playerid  Player who arrived.
 * @param      routeid   Route they followed.
 */
forward A3D_OnRouteArrived(playerid, Route:routeid);

/**
 * @brief      A player strayed from the route shown to them.
 *
 * A player strays when they are 30 metres farther from the route than the nearest they have been to
 * it, so a route shown to a player who is away from it, as in a car park beside the road, waits
 * until they move off. It is not called again until they come back within 15 metres of that
 * distance.
 *
 * @param      playerid  Player who strayed.
 * @param      routeid   Route they left.
 * @param      distance  Distance from the player to the route.
 */
forward A3D_OnRouteLeft(playerid, Route:routeid, Float:distance);

/**
 * @brief      A player moved along the route shown to them: the next turn or the distance to it
 *             changed.
 *
 * It comes at most five times a second, when the next turn changes or the distance to it passes a
 * multiple of ten metres, and not while the player is away from the route. It suits a text that
 * tells the next turn without a timer.
 *
 * @param      playerid      Player shown the route.
 * @param      routeid       Route they follow.
 * @param      remaining     Distance left to the end of the route, in metres.
 * @param      turn          The next turn, or `A3D_TURN_NONE` when the end comes next.
 * @param      turnDistance  Distance to the next turn, or to the end when none is left.
 */
forward A3D_OnRouteAdvanced(playerid, Route:routeid, Float:remaining, A3D_TURN:turn, Float:turnDistance);

/**
 * # Calls
 */

#if defined _streamer_included
    /**
     * Moves the dynamic object of a body for the component. The streamer starts
     * a move from the position it stored last, which trails an object that is
     * still moving, so the position is read first to bring it up to date.
     */
    forward @A3D_MoveDynamicObject(objectid, Float:x, Float:y, Float:z, Float:speed, Float:rx, Float:ry, Float:rz);
    public @A3D_MoveDynamicObject(objectid, Float:x, Float:y, Float:z, Float:speed, Float:rx, Float:ry, Float:rz) {
        if (speed > 0.0) {
            new
                Float:atX,
                Float:atY,
                Float:atZ
            ;

            GetDynamicObjectPos(objectid, atX, atY, atZ);
            MoveDynamicObject(objectid, x, y, z, speed, rx, ry, rz);

            return 1;
        }

        if (IsDynamicObjectMoving(objectid)) {
            StopDynamicObject(objectid);
        }

        SetDynamicObjectPos(objectid, x, y, z);
        SetDynamicObjectRot(objectid, rx, ry, rz);

        return 1;
    }
#endif
