/core
  - Time, math, logging, config
/platform
  - SDL window, input, timing
/renderer
  - Framebuffer
  - Rasterizer (walls, flats, sprites)
  - Clipping, projection
/world
  - Map (sectors, linedefs, sidedefs)
  - BSP
  - Collision
/game
  - Player
  - Enemies
  - Weapons
/assets
  - WAD loader



struct GameState {
    WorldState world;
    PlayerState player;
    std::vector<EntityState> entities;
    uint32_t rng_seed;
    GameTime time;
};
