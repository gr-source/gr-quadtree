#pragma once

#include <cstddef>
#include <cstdint>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGTH 600

typedef uint32_t QuadtreeID;
typedef uint32_t PlayerID;

static constexpr QuadtreeID INVALID_QUADTREEID = UINT32_MAX;
static constexpr PlayerID INVALID_PLAYERID = UINT32_MAX;

template <typename T, size_t MAX_DEPTH>
class Quadtree;

