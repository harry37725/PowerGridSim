// renderer.cpp
// Responsibility: all Raylib drawing helpers (nodes and edges).
#include "renderer.h"
#include "raymath.h"

void DrawNodeA(graph::Vertex_A* node) {
    if (node->node_type == "power_plant") {
        DrawCircle(node->x, node->y, 15.f, PINK);
        DrawCircleLines(node->x, node->y, 15.f, MAROON);
    } else if (node->node_type == "substation") {
        DrawCircle(node->x, node->y, 10.f, BLUE);
        DrawCircleLines(node->x, node->y, 10.f, DARKBLUE);
    }
}

void DrawNodeB(graph::Vertex_B* node) {
    float radius = 5.f;
    Color color  = BROWN;  // residential default
    if      (node->node_type == "hospital")   { color = DARKBLUE; radius = 7.f; }
    else if (node->node_type == "industrial") { color = ORANGE;   radius = 7.f; }
    else if (node->node_type == "institute")  { color = PURPLE;   radius = 7.f; }
    DrawCircle(node->x, node->y, radius, color);
}

void DrawArrow(Vector2 start, Vector2 end, float endRadius,
               float thickness, Color color)
{
    Vector2 dir        = Vector2Normalize(Vector2Subtract(end, start));
    Vector2 arrowPoint = Vector2Subtract(end, Vector2Scale(dir, endRadius));

    DrawLineEx(start, arrowPoint, thickness, color);

    float   arrowSize = 8.f;
    float   scale     = arrowSize / (thickness > 2.f ? 1.5f : 1.f);
    Vector2 leftDir   = Vector2Rotate(dir, -150.f * DEG2RAD);
    Vector2 rightDir  = Vector2Rotate(dir,  150.f * DEG2RAD);

    DrawLineEx(arrowPoint, Vector2Add(arrowPoint, Vector2Scale(leftDir,  scale)), thickness, color);
    DrawLineEx(arrowPoint, Vector2Add(arrowPoint, Vector2Scale(rightDir, scale)), thickness, color);
}
