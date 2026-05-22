#pragma once
#include "graph.h"
#include "raylib.h"

// Draw a power-plant (pink) or substation (blue) node.
void DrawNodeA(graph::Vertex_A* node);

// Draw a consumer node, coloured by type.
void DrawNodeB(graph::Vertex_B* node);

// Draw a line-with-arrowhead that stops at the surface of the target circle.
void DrawArrow(Vector2 start, Vector2 end, float endRadius,
               float thickness, Color color);
