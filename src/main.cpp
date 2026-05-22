// main.cpp
// Responsibility: application entry point, Raylib window, game-loop state machine.
#include "globals.h"
#include "graph.h"
#include "renderer.h"

#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

enum GameState { STATE_INPUT, STATE_VISUALIZATION };

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // -----------------------------------------------------------------------
    // 1. Window
    // -----------------------------------------------------------------------
    const int screenWidth  = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "Power Grid Visualizer");
    SetTargetFPS(60);

    GameState currentState = STATE_INPUT;

    // -----------------------------------------------------------------------
    // 2. Input-screen variables
    // -----------------------------------------------------------------------
    const int  inputFieldCount = 3;
    const char* labels[3] = { "Power Plants:", "Substations:", "Consumers:" };
    Rectangle  textBoxes[3];
    std::string inputStrings[3] = { "5", "50", "500" };
    int  activeTextBox  = -1;
    bool showCursor     = false;
    int  framesCounter  = 0;

    const int startY      = 100;
    const int inputHeight = 40;
    const int inputWidth  = 200;
    const int labelWidth  = 250;
    const int padding     = 15;
    const int fontSize    = 20;

    for (int i = 0; i < inputFieldCount; ++i)
        textBoxes[i] = { (float)screenWidth / 2 - inputWidth / 2,
                         (float)(startY + i * (inputHeight + padding)),
                         (float)inputWidth, (float)inputHeight };

    Rectangle startButton = {
        (float)screenWidth / 2 - inputWidth / 2,
        (float)(startY + inputFieldCount * (inputHeight + padding) + 20),
        (float)inputWidth, (float)(inputHeight + 10) };

    // -----------------------------------------------------------------------
    // 3. Visualisation variables
    // -----------------------------------------------------------------------
    graph G;
    Camera2D camera = { {0,0}, {0,0}, 0.f, 1.f };
    const float ZOOM_LOD_SUBSTATION = 0.5f;
    const float ZOOM_LOD_CONSUMER   = 1.0f;

    graph::Vertex_A* selected_a_node = nullptr;
    graph::Vertex_B* selected_b_node = nullptr;
    graph::Edge*     selected_edge   = nullptr;

    std::unordered_set<void*>     visited_nodes;
    std::vector<graph::Vertex_A*> a_nodes_to_draw;
    std::vector<graph::Vertex_B*> b_nodes_to_draw;

    enum FixVizState { VIZ_IDLE, VIZ_SHOWING_OVERLOAD, VIZ_SHOWING_FIX };
    FixVizState fix_viz_state         = VIZ_IDLE;
    double      fix_highlight_start   = 0.0;
    bool        show_no_overload_msg  = false;
    double      no_overload_msg_time  = 0.0;
    bool        auto_loop_active      = false;
    double      last_auto_advance_time= 0.0;

    // -----------------------------------------------------------------------
    // 4. Main game loop
    // -----------------------------------------------------------------------
    while (!WindowShouldClose()) {
        switch (currentState) {

        // -------------------------------------------------------------------
        case STATE_INPUT:
        // -------------------------------------------------------------------
        {
            ++framesCounter;
            showCursor = ((framesCounter / 30) % 2 == 0);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                activeTextBox = -1;
                for (int i = 0; i < inputFieldCount; ++i)
                    if (CheckCollisionPointRec(GetMousePosition(), textBoxes[i]))
                    { activeTextBox = i; framesCounter = 0; break; }
            }

            if (activeTextBox >= 0) {
                for (int ch = GetCharPressed(); ch > 0; ch = GetCharPressed())
                    if (ch >= '0' && ch <= '9' && inputStrings[activeTextBox].size() < 9)
                        inputStrings[activeTextBox] += static_cast<char>(ch);
                if (IsKeyPressed(KEY_BACKSPACE) && !inputStrings[activeTextBox].empty())
                    inputStrings[activeTextBox].pop_back();
            }

            // Start button
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                CheckCollisionPointRec(GetMousePosition(), startButton))
            {
                try {
                    int num_pp  = std::stoi(inputStrings[0]);
                    int num_sub = std::stoi(inputStrings[1]);
                    int num_con = std::stoi(inputStrings[2]);

                    // BUG FIX: original allowed num_substation == 0 which
                    // causes a divide-by-zero in distributor().
                    if (num_pp <= 0 || num_sub <= 0 || num_con <= 0) {
                        std::cout << "Error: all fields must be > 0.\n";
                    } else {
                        resident_demand  = 25;
                        hospital_demand  = 90;
                        industry_demand  = 80;
                        institute_demand = 15;

                        std::cout << "Building graph...\n";
                        G = graph();
                        G.mapping(num_pp, num_sub, num_con);
                        G.layout_graph();
                        G.add_realistic_connections(2);
                        std::cout << "Graph ready.\n";

                        camera.target   = { 1000.f, 1000.f };
                        camera.offset   = { screenWidth / 2.f, screenHeight / 2.f };
                        camera.rotation = 0.f;
                        camera.zoom     = 0.25f;

                        selected_a_node = nullptr;
                        selected_b_node = nullptr;
                        selected_edge   = nullptr;
                        fix_viz_state   = VIZ_IDLE;
                        auto_loop_active= false;
                        currentState    = STATE_VISUALIZATION;
                    }
                } catch (const std::invalid_argument&) {
                    std::cout << "Error: please enter numbers only.\n";
                } catch (const std::out_of_range&) {
                    std::cout << "Error: number too large.\n";
                }
            }

            // Draw input screen
            BeginDrawing();
            ClearBackground(DARKGRAY);
            DrawText("Power Grid Simulation Setup",
                     screenWidth / 2 - MeasureText("Power Grid Simulation Setup", 30) / 2,
                     30, 30, WHITE);

            for (int i = 0; i < inputFieldCount; ++i) {
                DrawText(labels[i],
                         (int)(textBoxes[i].x - labelWidth),
                         (int)(textBoxes[i].y + (inputHeight - fontSize) / 2.f),
                         fontSize, LIGHTGRAY);
                DrawRectangleRec(textBoxes[i], LIGHTGRAY);
                DrawRectangleLinesEx(textBoxes[i], (activeTextBox == i) ? 2.f : 1.f,
                                     (activeTextBox == i) ? RED : DARKGRAY);
                DrawText(inputStrings[i].c_str(),
                         (int)(textBoxes[i].x + 5),
                         (int)(textBoxes[i].y + (inputHeight - fontSize) / 2.f),
                         fontSize, BLACK);
                if (activeTextBox == i && showCursor) {
                    int tw = MeasureText(inputStrings[i].c_str(), fontSize);
                    DrawRectangle((int)(textBoxes[i].x + 5 + tw),
                                  (int)(textBoxes[i].y + 4), 2, inputHeight - 8, MAROON);
                }
            }
            DrawRectangleRec(startButton, MAROON);
            DrawText("START SIMULATION",
                     (int)(startButton.x + startButton.width / 2 -
                           MeasureText("START SIMULATION", fontSize) / 2),
                     (int)(startButton.y + (startButton.height - fontSize) / 2),
                     fontSize, WHITE);
            EndDrawing();
        } break;

        // -------------------------------------------------------------------
        case STATE_VISUALIZATION:
        // -------------------------------------------------------------------
        {
            // Rebuild draw lists
            visited_nodes.clear(); a_nodes_to_draw.clear(); b_nodes_to_draw.clear();
            for (auto& [n, ch] : G.adj_power_substation) {
                if (!visited_nodes.count(n)) { a_nodes_to_draw.push_back(n); visited_nodes.insert(n); }
                for (auto& [c, _] : ch)
                    if (!visited_nodes.count(c)) { a_nodes_to_draw.push_back(c); visited_nodes.insert(c); }
            }
            for (auto& [n, ch] : G.adj_substion_consumer) {
                if (!visited_nodes.count(n)) { a_nodes_to_draw.push_back(n); visited_nodes.insert(n); }
                for (auto& [c, _] : ch)
                    if (!visited_nodes.count(c)) { b_nodes_to_draw.push_back(c); visited_nodes.insert(c); }
            }

            // Apply pending throttles every frame
            G.apply_demand_reduction_updates();

            // Camera
            camera.zoom = std::clamp(camera.zoom + GetMouseWheelMove() * 0.05f, 0.1f, 3.f);
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                Vector2 d = Vector2Scale(GetMouseDelta(), -1.f / camera.zoom);
                camera.target = Vector2Add(camera.target, d);
            }

            // Keys
            if (IsKeyPressed(KEY_I)) {
                auto_loop_active     = !auto_loop_active;
                last_auto_advance_time = GetTime();
                std::cout << (auto_loop_active ? "Auto-loop ON\n" : "Auto-loop OFF\n");
            }
            if (IsKeyPressed(KEY_U)) { G.upgrade_selected_node_limit(selected_a_node); G.check_node_overloads(); }
            if (IsKeyPressed(KEY_C)) {
                G.overloaded_edges.clear(); G.edge_being_fixed = nullptr;
                G.fix_path.clear(); G.throttled_nodes_visual.clear();
                G.node_overloads_visual.clear();
                fix_viz_state = VIZ_IDLE; show_no_overload_msg = false;
                std::cout << "Highlights cleared.\n";
            }
            if (IsKeyPressed(KEY_BACKSPACE)) { G = graph(); currentState = STATE_INPUT; break; }

            // Auto-loop: advance one hour per second
            if (auto_loop_active && GetTime() - last_auto_advance_time >= 1.0) {
                last_auto_advance_time = GetTime();
                fix_viz_state = VIZ_IDLE;
                G.edge_being_fixed = nullptr; G.fix_path.clear(); G.throttled_nodes_visual.clear();
                G.advance_hour_demand();
                G.check_node_overloads();
                if (G.detect_overloads())
                { fix_viz_state = VIZ_SHOWING_OVERLOAD; fix_highlight_start = GetTime(); show_no_overload_msg = false; }
                else
                { show_no_overload_msg = true; no_overload_msg_time = GetTime(); }
            }

            // Continuous re-detect while idle and overloads still exist
            if (fix_viz_state == VIZ_IDLE &&
                (!G.node_overloads_visual.empty() || !G.overloaded_edges.empty()))
                if (G.detect_overloads())
                { fix_viz_state = VIZ_SHOWING_OVERLOAD; fix_highlight_start = GetTime(); }

            // After 0.5 s red highlight → apply fixes
            if (fix_viz_state == VIZ_SHOWING_OVERLOAD && GetTime() - fix_highlight_start >= 0.5) {
                G.apply_overload_fixes();
                G.apply_demand_reduction_updates();
                G.check_node_overloads();
                for (auto* n : G.node_overloads_visual) G.upgrade_selected_node_limit(n);
                G.check_node_overloads();
                fix_viz_state = VIZ_SHOWING_FIX;
                std::cout << "Switching to fix-path visualisation.\n";
            }

            // Click selection
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 wp = GetScreenToWorld2D(GetMousePosition(), camera);
                selected_a_node = nullptr; selected_b_node = nullptr; selected_edge = nullptr;
                bool hit = false;

                // B-nodes
                if (camera.zoom >= ZOOM_LOD_CONSUMER)
                    for (auto* n : b_nodes_to_draw) {
                        float r = (n->node_type == "residential") ? 5.f : 7.f;
                        if (CheckCollisionPointCircle(wp, {(float)n->x,(float)n->y}, r))
                        { selected_b_node = n; hit = true; break; }
                    }

                // A-nodes
                if (!hit)
                    for (auto* n : a_nodes_to_draw) {
                        float r = 0.f;
                        if (n->node_type == "power_plant") r = 15.f;
                        else if (n->node_type == "substation" && camera.zoom >= ZOOM_LOD_SUBSTATION) r = 10.f;
                        if (r > 0.f && CheckCollisionPointCircle(wp, {(float)n->x,(float)n->y}, r))
                        { selected_a_node = n; hit = true; break; }
                    }

                const int CLICK_T = 5;

                // A→A edges
                if (!hit && camera.zoom >= ZOOM_LOD_SUBSTATION)
                    for (auto& [p, ch] : G.adj_power_substation)
                        for (auto& [c, e] : ch)
                            if (CheckCollisionPointLine(wp,
                                    {(float)p->x,(float)p->y},{(float)c->x,(float)c->y},
                                    CLICK_T / camera.zoom))
                            { selected_edge = e; hit = true; break; }

                // A→B edges
                if (!hit && camera.zoom >= ZOOM_LOD_CONSUMER)
                    for (auto& [p, ch] : G.adj_substion_consumer)
                        for (auto& [c, e] : ch)
                            if (CheckCollisionPointLine(wp,
                                    {(float)p->x,(float)p->y},{(float)c->x,(float)c->y},
                                    CLICK_T / camera.zoom))
                            { selected_edge = e; hit = true; break; }
            }

            // -----------------------------------------------------------
            // Draw
            // -----------------------------------------------------------
            BeginDrawing();
            ClearBackground(DARKGRAY);
            BeginMode2D(camera);

            // Collect fix-path edge set for green highlight
            std::unordered_set<graph::Edge*> fix_edges;
            if (fix_viz_state == VIZ_SHOWING_FIX)
                for (std::size_t i = 1; i < G.fix_path.size(); ++i)
                    if (G.fix_path[i].second) fix_edges.insert(G.fix_path[i].second);

            // A→A edges
            if (camera.zoom >= ZOOM_LOD_SUBSTATION)
                for (auto& [parent, ch] : G.adj_power_substation)
                    for (auto& [child, edge] : ch) {
                        Vector2 sv  = {(float)parent->x,(float)parent->y};
                        Vector2 ev  = {(float)child->x, (float)child->y};
                        float   er  = (child->node_type == "power_plant") ? 15.f : 10.f;
                        Color   col = (parent->node_type == "power_plant") ? YELLOW : SKYBLUE;
                        float   thk = 1.f;

                        bool is_fix  = (fix_viz_state == VIZ_SHOWING_FIX) &&
                                       (edge == G.edge_being_fixed || fix_edges.count(edge));
                        bool is_ol   = (fix_viz_state == VIZ_SHOWING_OVERLOAD) && (edge == G.edge_being_fixed);
                        bool is_red  = G.overloaded_edges.count(edge) != 0;

                        if (is_fix) {
                            // Determine correct direction along the fix path
                            Vector2 ps = sv, pe = ev; float pr = er;
                            for (std::size_t i = 1; i < G.fix_path.size(); ++i)
                                if (G.fix_path[i].second == edge && G.fix_path[i].first == parent)
                                { std::swap(ps, pe); pr = (parent->node_type == "power_plant") ? 15.f : 10.f; break; }
                            DrawArrow(ps, pe, pr, 4.f / camera.zoom, GREEN);
                        } else if (is_ol) {
                            DrawArrow(sv, ev, er, 4.f / camera.zoom, ORANGE);
                        } else if (is_red) {
                            thk = 3.f + sinf((float)GetTime() * 10.f) * 1.5f;
                            DrawArrow(sv, ev, er, thk / camera.zoom, RED);
                        } else {
                            if (edge->current_load < 0.f) {
                                float rr = (parent->node_type == "power_plant") ? 15.f : 10.f;
                                DrawArrow(ev, sv, rr, thk / camera.zoom, col);
                            } else {
                                DrawArrow(sv, ev, er, thk / camera.zoom, col);
                            }
                        }
                        if (edge == selected_edge)
                            DrawLineEx(sv, ev, (thk + 2.f) / camera.zoom, YELLOW);
                    }

            // A→B edges
            if (camera.zoom >= ZOOM_LOD_CONSUMER)
                for (auto& [parent, ch] : G.adj_substion_consumer)
                    for (auto& [child, edge] : ch) {
                        Vector2 sv  = {(float)parent->x,(float)parent->y};
                        Vector2 ev  = {(float)child->x, (float)child->y};
                        float   er  = (child->node_type == "residential") ? 5.f : 7.f;
                        DrawArrow(sv, ev, er, 1.f / camera.zoom, LIGHTGRAY);
                        if (edge == selected_edge)
                            DrawLineEx(sv, ev, 4.f / camera.zoom, YELLOW);
                    }

            // A-nodes
            for (auto* node : a_nodes_to_draw) {
                if (node->node_type == "power_plant") {
                    DrawNodeA(node);
                    if (node == selected_a_node) DrawCircleLines(node->x, node->y, 17.f, YELLOW);
                } else if (node->node_type == "substation" && camera.zoom >= ZOOM_LOD_SUBSTATION) {
                    DrawNodeA(node);
                    if (G.throttled_nodes_visual.count(node)) {
                        DrawCircle(node->x, node->y, 10.f, RAYWHITE);
                        DrawCircleLines(node->x, node->y, 10.f, WHITE);
                    }
                    if (G.node_overloads_visual.count(node)) {
                        DrawCircle(node->x, node->y, 10.f, BLACK);
                        DrawCircleLines(node->x, node->y, 10.f, DARKGRAY);
                    }
                    if (node == selected_a_node) DrawCircleLines(node->x, node->y, 12.f, YELLOW);
                }
            }

            // B-nodes
            if (camera.zoom >= ZOOM_LOD_CONSUMER)
                for (auto* node : b_nodes_to_draw) {
                    DrawNodeB(node);
                    float r = (node->node_type == "residential") ? 5.f : 7.f;
                    if (node == G.last_demand_increase_consumer) {
                        float pulse = r + 2.f + sinf((float)GetTime() * 10.f) * 1.5f;
                        DrawRingLines({(float)node->x,(float)node->y},
                                      pulse - 2.f / camera.zoom, pulse, 0, 360, 36,
                                      Fade(YELLOW, 0.8f));
                    }
                    if (node == selected_b_node)
                        DrawCircleLines(node->x, node->y, r + 2.f, YELLOW);
                }

            EndMode2D();

            // HUD legend
            DrawText("Power Grid Visualizer", 10, 10, 20, WHITE);
            const char* leg_labels[] = {"Power Plant","Substation","Resident","Hospital","Institute","Industry"};
            Color       leg_colors[] = {PINK, BLUE, BROWN, DARKBLUE, PURPLE, ORANGE};
            for (int i = 0; i < 6; ++i) {
                DrawText(leg_labels[i], 10, 35 + i * 20, 18, WHITE);
                DrawCircle(175, 44 + i * 20, 7.f, leg_colors[i]);
            }
            DrawFPS(screenWidth - 100, 10);

            // Clock / control panel
            std::string month_str = std::to_string(G.get_month_number(G.sim_day)) + ". " +
                                    G.get_month_name(G.sim_day) +
                                    " | Day " + std::to_string((G.sim_day % 30) + 1);
            std::string hour_str  = "Hour: " + std::to_string(G.sim_hour) + ":00";
            std::string loop_str  = auto_loop_active ? "[ON]  Auto-loop every 1s" : "[OFF] Press I to start";

            DrawRectangle(screenWidth - 380, 5, 375, 155, Fade(BLACK, 0.7f));
            DrawText(month_str.c_str(), screenWidth - 375, 10,  16, YELLOW);
            DrawText(hour_str.c_str(),  screenWidth - 375, 30,  16, LIGHTGRAY);
            DrawText(loop_str.c_str(),  screenWidth - 375, 50,  14,
                     auto_loop_active ? Fade(LIME, 1.f) : Fade(RED, 1.f));
            DrawText("[I] Toggle Auto-loop",      screenWidth - 375, 70,  14, WHITE);
            DrawText("[U] Upgrade Selected Node", screenWidth - 375, 88,  14, WHITE);
            DrawText("[C] Clear Highlights",      screenWidth - 375, 105, 14, WHITE);
            DrawText("[BACKSPACE] Main Menu",      screenWidth - 375, 122, 14, WHITE);

            // "Grid OK" toast
            if (show_no_overload_msg) {
                double el = GetTime() - no_overload_msg_time;
                if (el < 2.0) {
                    float a = (el > 1.5) ? (float)((2.0 - el) / 0.5) : 1.f;
                    DrawRectangle(screenWidth/2-120, screenHeight-60, 240, 40, Fade(DARKGREEN, a * 0.85f));
                    DrawText("Grid OK - No Overload",
                             screenWidth/2 - MeasureText("Grid OK - No Overload",18)/2,
                             screenHeight-50, 18, Fade(WHITE, a));
                } else show_no_overload_msg = false;
            }

            // Overload banner
            if (!G.node_overloads_visual.empty() || !G.overloaded_edges.empty()) {
                DrawRectangle(screenWidth/2-140, screenHeight-60, 280, 40, Fade(RED, 0.85f));
                DrawText("! OVERLOAD - Auto-fixing...",
                         screenWidth/2 - MeasureText("! OVERLOAD - Auto-fixing...",18)/2,
                         screenHeight-50, 18, WHITE);
            }

            // Inspector panel
            if (selected_a_node) {
                bool ol = G.node_overloads_visual.count(selected_a_node);
                DrawRectangle(10, 170, 270, ol ? 140 : 80, Fade(BLACK, ol ? 0.8f : 0.7f));
                DrawText("--- SELECTED NODE ---", 15, 175, 16, YELLOW);
                DrawText(("Type: " + selected_a_node->node_type).c_str(), 15, 195, 18, WHITE);
                if (ol) {
                    DrawText("STATUS: NODE OVERLOADED", 15, 215, 18, RED);
                    DrawText(("Load: "  + std::to_string(selected_a_node->current_downstream_demand)).c_str(), 15, 235, 18, ORANGE);
                    DrawText(("Limit: " + std::to_string(selected_a_node->max_limit)).c_str(), 15, 255, 18, WHITE);
                    DrawText("Press 'U' to upgrade limit", 15, 275, 18, WHITE);
                } else {
                    DrawText(("Load: "  + std::to_string(selected_a_node->current_downstream_demand)).c_str(), 15, 215, 18, WHITE);
                    DrawText(("Limit: " + std::to_string(selected_a_node->max_limit)).c_str(), 15, 235, 18, WHITE);
                }
            } else if (selected_b_node) {
                DrawRectangle(10, 170, 250, 60, Fade(BLACK, 0.7f));
                DrawText("--- SELECTED NODE ---", 15, 175, 16, YELLOW);
                DrawText(("Type: "   + selected_b_node->node_type).c_str(), 15, 195, 18, WHITE);
                DrawText(("Demand: " + std::to_string(selected_b_node->demand)).c_str(), 15, 215, 18, WHITE);
            } else if (selected_edge) {
                DrawRectangle(10, 170, 250, 60, Fade(BLACK, 0.7f));
                DrawText("--- SELECTED EDGE ---", 15, 175, 16, YELLOW);
                DrawText(("Load: "     + std::to_string(selected_edge->current_load)).c_str(),  15, 195, 18, WHITE);
                DrawText(("Max Load: " + std::to_string(selected_edge->max_load)).c_str(), 15, 215, 18, WHITE);
            }

            EndDrawing();
        } break;
        }   // switch
    }       // while

    CloseWindow();
    return 0;
}
