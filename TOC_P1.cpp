#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
static double getDPR()     { return EM_ASM_DOUBLE({ return window.devicePixelRatio || 1.0; }); }
static int    getCanvasW() { return EM_ASM_INT({ return Math.floor(window.innerWidth  * (window.devicePixelRatio || 1.0)); }); }
static int    getCanvasH() { return EM_ASM_INT({ return Math.floor(window.innerHeight * (window.devicePixelRatio || 1.0)); }); }
#endif

using namespace std;

constexpr float PI = 3.14159265359f;

inline ImVec2 vAdd(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }
inline ImVec2 vSub(ImVec2 a, ImVec2 b) { return ImVec2(a.x - b.x, a.y - b.y); }
inline ImVec2 vMul(ImVec2 a, float s)  { return ImVec2(a.x * s, a.y * s); }
inline float  vLen(ImVec2 a)           { return sqrtf(a.x * a.x + a.y * a.y); }
inline ImVec2 vNorm(ImVec2 a) {
    float l = vLen(a);
    return (l > 1e-5f) ? ImVec2(a.x / l, a.y / l) : ImVec2(1.0f, 0.0f);
}

string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

struct MealyTransition {
    string next_state;
    string output;
};

struct MealyTableRow {
    char state[16] = "";
    char next_0[16] = "", out_0[16] = "";
    char next_1[16] = "", out_1[16] = "";
};

struct MooreState {
    string name;
    string mealy_state;
    string output;
    bool processed = false;
    ImVec2 pos = ImVec2(0.0f, 0.0f);
};

static GLFWwindow* g_window = nullptr;

vector<string> alphabet = {"0", "1"};
vector<string> mealy_state_order;
map<string, map<string, MealyTransition>> mealy;
map<pair<string, string>, string> mealy_to_moore;

vector<MooreState> moore_states;
map<string, int> moore_name_to_idx;
map<string, map<string, string>> moore_transitions;
queue<int> pending_queue;
string moore_start_state = "";

vector<MealyTableRow> ui_table_rows;
char ui_sym0[8] = "0";
char ui_sym1[8] = "1";
char ui_initial_state[32] = "A";
bool algorithm_started = false;

char ui_test_string[128] = "0111";
string sim_mealy_out = "";
string sim_moore_out = "";
string sim_moore_path = "";
set<string> sim_visited_states;

ImVec2 canvas_pan = ImVec2(0.0f, 0.0f);
float canvas_zoom = 1.0f;
bool request_fit_view = false;
int dragging_node = -1;

void loadBinaryPreset() {
    strcpy(ui_sym0, "0");
    strcpy(ui_sym1, "1");
    strcpy(ui_initial_state, "A");
    strcpy(ui_test_string, "0111");

    ui_table_rows.assign(3, MealyTableRow());
    strcpy(ui_table_rows[0].state, "A");
    strcpy(ui_table_rows[0].next_0, "B"); strcpy(ui_table_rows[0].out_0, "a");
    strcpy(ui_table_rows[0].next_1, "C"); strcpy(ui_table_rows[0].out_1, "a");

    strcpy(ui_table_rows[1].state, "B");
    strcpy(ui_table_rows[1].next_0, "B"); strcpy(ui_table_rows[1].out_0, "b");
    strcpy(ui_table_rows[1].next_1, "C"); strcpy(ui_table_rows[1].out_1, "a");

    strcpy(ui_table_rows[2].state, "C");
    strcpy(ui_table_rows[2].next_0, "B"); strcpy(ui_table_rows[2].out_0, "a");
    strcpy(ui_table_rows[2].next_1, "C"); strcpy(ui_table_rows[2].out_1, "b");
}

void loadAlphaPreset() {
    strcpy(ui_sym0, "a");
    strcpy(ui_sym1, "b");
    strcpy(ui_initial_state, "q0");
    strcpy(ui_test_string, "aba");

    ui_table_rows.assign(4, MealyTableRow());
    strcpy(ui_table_rows[0].state, "q0");
    strcpy(ui_table_rows[0].next_0, "q3"); strcpy(ui_table_rows[0].out_0, "0");
    strcpy(ui_table_rows[0].next_1, "q1"); strcpy(ui_table_rows[0].out_1, "1");

    strcpy(ui_table_rows[1].state, "q1");
    strcpy(ui_table_rows[1].next_0, "q0"); strcpy(ui_table_rows[1].out_0, "1");
    strcpy(ui_table_rows[1].next_1, "q3"); strcpy(ui_table_rows[1].out_1, "0");

    strcpy(ui_table_rows[2].state, "q2");
    strcpy(ui_table_rows[2].next_0, "q2"); strcpy(ui_table_rows[2].out_0, "1");
    strcpy(ui_table_rows[2].next_1, "q2"); strcpy(ui_table_rows[2].out_1, "0");

    strcpy(ui_table_rows[3].state, "q3");
    strcpy(ui_table_rows[3].next_0, "q1"); strcpy(ui_table_rows[3].out_0, "0");
    strcpy(ui_table_rows[3].next_1, "q0"); strcpy(ui_table_rows[3].out_1, "1");
}

void arrangeNodesInPolygon() {
    int n = (int)moore_states.size();
    if (n == 0) return;

    float radius = max(180.0f, n * 48.0f);
    for (int i = 0; i < n; i++) {
        float angle = -PI / 2.0f + (2.0f * PI * i) / n;
        moore_states[i].pos = ImVec2(radius * cosf(angle), radius * sinf(angle));
    }
    request_fit_view = true;
}

void resetSimulator() {
    mealy.clear();
    mealy_state_order.clear();
    mealy_to_moore.clear();
    moore_states.clear();
    moore_name_to_idx.clear();
    moore_transitions.clear();
    while (!pending_queue.empty()) pending_queue.pop();
    moore_start_state = "";
    sim_mealy_out = "";
    sim_moore_out = "";
    sim_moore_path = "";
    sim_visited_states.clear();
    algorithm_started = false;
}

void initializeConversion() {
    resetSimulator();

    string s0 = trim(ui_sym0);
    string s1 = trim(ui_sym1);
    if (s0.empty()) { s0 = "0"; strcpy(ui_sym0, "0"); }
    if (s1.empty()) { s1 = "1"; strcpy(ui_sym1, "1"); }
    alphabet = {s0, s1};

    set<string> seen_states;
    map<string, set<string>> incoming_outputs;

    for (const auto& row : ui_table_rows) {
        string st = trim(row.state);
        if (st.empty()) continue;

        if (!seen_states.count(st)) {
            seen_states.insert(st);
            mealy_state_order.push_back(st);
        }

        string n0 = trim(row.next_0), o0 = trim(row.out_0);
        string n1 = trim(row.next_1), o1 = trim(row.out_1);

        if (!n0.empty()) {
            mealy[st][s0] = {n0, o0};
            incoming_outputs[n0].insert(o0);
            if (!seen_states.count(n0)) {
                seen_states.insert(n0);
                mealy_state_order.push_back(n0);
            }
        }
        if (!n1.empty()) {
            mealy[st][s1] = {n1, o1};
            incoming_outputs[n1].insert(o1);
            if (!seen_states.count(n1)) {
                seen_states.insert(n1);
                mealy_state_order.push_back(n1);
            }
        }
    }

    if (mealy_state_order.empty()) return;

    string init_mealy = trim(ui_initial_state);
    if (!seen_states.count(init_mealy)) {
        init_mealy = mealy_state_order[0];
        strncpy(ui_initial_state, init_mealy.c_str(), sizeof(ui_initial_state) - 1);
        ui_initial_state[sizeof(ui_initial_state) - 1] = '\0';
    }

    auto addMooreState = [&](const string& name, const string& orig, const string& out) {
        int idx = (int)moore_states.size();
        moore_states.push_back({name, orig, out, false, ImVec2(0, 0)});
        moore_name_to_idx[name] = idx;
        pending_queue.push(idx);
    };

    if (incoming_outputs[init_mealy].size() > 1) {
        moore_start_state = init_mealy;
        addMooreState(init_mealy, init_mealy, "-");
    } else {
        moore_start_state = init_mealy;
    }

    for (const string& q : mealy_state_order) {
        const auto& outs = incoming_outputs[q];
        if (outs.empty()) {
            addMooreState(q, q, "-");
        } else if (outs.size() == 1) {
            string only_out = *outs.begin();
            mealy_to_moore[{q, only_out}] = q;
            addMooreState(q, q, only_out);
        } else {
            for (const string& y : outs) {
                string split_name = q + y;
                mealy_to_moore[{q, y}] = split_name;
                addMooreState(split_name, q, y);
            }
        }
    }

    arrangeNodesInPolygon();
    algorithm_started = true;
}

void processNextStep() {
    if (pending_queue.empty()) return;

    int idx = pending_queue.front();
    pending_queue.pop();

    MooreState& ms = moore_states[idx];
    for (const string& sym : alphabet) {
        if (mealy[ms.mealy_state].count(sym) == 0) continue;

        MealyTransition tr = mealy[ms.mealy_state][sym];
        auto key = make_pair(tr.next_state, tr.output);
        if (mealy_to_moore.count(key)) {
            moore_transitions[ms.name][sym] = mealy_to_moore[key];
        }
    }
    ms.processed = true;
}

void processAllSteps() {
    while (!pending_queue.empty()) {
        processNextStep();
    }
}

void runStringSimulation() {
    processAllSteps();

    sim_mealy_out = "";
    sim_moore_out = "";
    sim_moore_path = "";
    sim_visited_states.clear();

    string input = trim(ui_test_string);
    if (moore_start_state.empty() || !moore_name_to_idx.count(moore_start_state)) {
        sim_moore_out = "Error: Machine not initialized.";
        return;
    }

    string cur_mealy = moore_states[moore_name_to_idx[moore_start_state]].mealy_state;
    for (char c : input) {
        string sym(1, c);
        if (mealy[cur_mealy].count(sym)) {
            MealyTransition tr = mealy[cur_mealy][sym];
            sim_mealy_out += tr.output;
            cur_mealy = tr.next_state;
        } else {
            sim_mealy_out += " [HALT:'" + sym + "' not in {" + alphabet[0] + "," + alphabet[1] + "}]";
            break;
        }
    }

    string cur_moore = moore_start_state;
    sim_moore_out = "[" + moore_states[moore_name_to_idx[cur_moore]].output + "] ";
    sim_moore_path = cur_moore;
    sim_visited_states.insert(cur_moore);

    for (char c : input) {
        string sym(1, c);
        if (moore_transitions[cur_moore].count(sym)) {
            cur_moore = moore_transitions[cur_moore][sym];
            sim_moore_out += moore_states[moore_name_to_idx[cur_moore]].output;
            sim_moore_path += " --(" + sym + ")--> " + cur_moore;
            sim_visited_states.insert(cur_moore);
        } else {
            sim_moore_out += " [HALT:'" + sym + "' not in {" + alphabet[0] + "," + alphabet[1] + "}]";
            break;
        }
    }
}

void drawArrowhead(ImDrawList* dl, ImVec2 tip, ImVec2 dir, float size, ImU32 col) {
    ImVec2 u = vNorm(dir);
    ImVec2 perp(-u.y, u.x);
    ImVec2 base = vSub(tip, vMul(u, size));
    ImVec2 left = vAdd(base, vMul(perp, size * 0.48f));
    ImVec2 right = vSub(base, vMul(perp, size * 0.48f));
    dl->AddTriangleFilled(tip, left, right, col);
}

void drawLabelPill(ImDrawList* dl, ImVec2 center, const string& text, float font_size, ImU32 border_col) {
    ImFont* font = ImGui::GetFont();
    float scale = font_size / ImGui::GetFontSize();
    ImVec2 raw_sz = ImGui::CalcTextSize(text.c_str());
    ImVec2 txt_sz(raw_sz.x * scale, raw_sz.y * scale);
    ImVec2 pad(6.0f * scale, 3.0f * scale);

    ImVec2 p0(center.x - txt_sz.x * 0.5f - pad.x, center.y - txt_sz.y * 0.5f - pad.y);
    ImVec2 p1(center.x + txt_sz.x * 0.5f + pad.x, center.y + txt_sz.y * 0.5f + pad.y);

    dl->AddRectFilled(p0, p1, IM_COL32(22, 24, 30, 235), 6.0f * scale);
    dl->AddRect(p0, p1, border_col, 6.0f * scale, 0, max(1.0f, 1.5f * scale));
    dl->AddText(font, font_size, ImVec2(center.x - txt_sz.x * 0.5f, center.y - txt_sz.y * 0.5f),
                IM_COL32(245, 245, 245, 255), text.c_str());
}

void drawStateCanvas() {
    ImGui::Begin("State Visualizer");

    if (ImGui::Button("Zoom -")) { canvas_zoom = clamp(canvas_zoom * 0.8f, 0.2f, 3.0f); }
    ImGui::SameLine();
    if (ImGui::Button("Zoom +")) { canvas_zoom = clamp(canvas_zoom * 1.25f, 0.2f, 3.0f); }
    ImGui::SameLine();
    if (ImGui::Button("Fit to View")) { request_fit_view = true; }
    ImGui::SameLine();
    if (ImGui::Button("Auto-Arrange")) { arrangeNodesInPolygon(); }
    ImGui::SameLine();
    ImGui::Text("Zoom: %d%%  |  Scroll: Zoom  |  Drag BG: Pan  |  Drag Node: Move", (int)(canvas_zoom * 100.0f));

    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
    ImVec2 canvas_p1 = vAdd(canvas_p0, canvas_sz);

    if (request_fit_view && !moore_states.empty() && canvas_sz.x > 60.0f && canvas_sz.y > 60.0f) {
        float min_x = 1e9f, max_x = -1e9f, min_y = 1e9f, max_y = -1e9f;
        for (const auto& st : moore_states) {
            min_x = min(min_x, st.pos.x); max_x = max(max_x, st.pos.x);
            min_y = min(min_y, st.pos.y); max_y = max(max_y, st.pos.y);
        }
        float pad = 160.0f;
        float world_w = max(200.0f, (max_x - min_x) + pad * 2.0f);
        float world_h = max(200.0f, (max_y - min_y) + pad * 2.0f);
        canvas_zoom = clamp(min(canvas_sz.x / world_w, canvas_sz.y / world_h), 0.25f, 1.5f);

        ImVec2 world_center((min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f);
        canvas_pan = ImVec2(canvas_sz.x * 0.5f - world_center.x * canvas_zoom,
                            canvas_sz.y * 0.5f - world_center.y * canvas_zoom);
        request_fit_view = false;
    }

    auto worldToScreen = [&](ImVec2 w) {
        return ImVec2(canvas_p0.x + canvas_pan.x + w.x * canvas_zoom,
                      canvas_p0.y + canvas_pan.y + w.y * canvas_zoom);
    };

    ImGui::InvisibleButton("fsm_canvas", canvas_sz,
                           ImGuiButtonFlags_MouseButtonLeft |
                           ImGuiButtonFlags_MouseButtonRight |
                           ImGuiButtonFlags_MouseButtonMiddle);
    bool is_hovered = ImGui::IsItemHovered();
    bool is_active  = ImGui::IsItemActive();
    ImGuiIO& io = ImGui::GetIO();

    const float base_node_radius = 48.0f;
    float node_r = base_node_radius * canvas_zoom;

    if (is_hovered && io.MouseWheel != 0.0f) {
        ImVec2 mouse = io.MousePos;
        ImVec2 world_before((mouse.x - canvas_p0.x - canvas_pan.x) / canvas_zoom,
                            (mouse.y - canvas_p0.y - canvas_pan.y) / canvas_zoom);
        float factor = (io.MouseWheel > 0.0f) ? 1.15f : 0.87f;
        canvas_zoom = clamp(canvas_zoom * factor, 0.2f, 3.0f);
        canvas_pan = ImVec2(mouse.x - canvas_p0.x - world_before.x * canvas_zoom,
                            mouse.y - canvas_p0.y - world_before.y * canvas_zoom);
        node_r = base_node_radius * canvas_zoom;
    }

    if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        dragging_node = -1;
        for (int i = (int)moore_states.size() - 1; i >= 0; i--) {
            if (vLen(vSub(io.MousePos, worldToScreen(moore_states[i].pos))) <= node_r) {
                dragging_node = i;
                break;
            }
        }
    }
    if (dragging_node != -1) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            moore_states[dragging_node].pos = vAdd(moore_states[dragging_node].pos,
                                                   vMul(io.MouseDelta, 1.0f / canvas_zoom));
        } else {
            dragging_node = -1;
        }
    } else if (is_active && (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) ||
                             ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) ||
                             ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))) {
        canvas_pan = vAdd(canvas_pan, io.MouseDelta);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(canvas_p0, canvas_p1, true);
    dl->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(26, 28, 34, 255));
    dl->AddRect(canvas_p0, canvas_p1, IM_COL32(70, 75, 90, 255));

    float grid_step = 64.0f * canvas_zoom;
    if (grid_step >= 12.0f) {
        for (float x = fmodf(canvas_pan.x, grid_step); x < canvas_sz.x; x += grid_step)
            dl->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p1.y), IM_COL32(45, 48, 58, 120));
        for (float y = fmodf(canvas_pan.y, grid_step); y < canvas_sz.y; y += grid_step)
            dl->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p1.x, canvas_p0.y + y), IM_COL32(45, 48, 58, 120));
    }

    if (moore_states.empty()) {
        dl->PopClipRect();
        ImGui::End();
        return;
    }

    ImVec2 centroid(0, 0);
    for (const auto& st : moore_states) centroid = vAdd(centroid, st.pos);
    centroid = vMul(centroid, 1.0f / (float)moore_states.size());

    map<pair<int, int>, vector<string>> merged_edges;
    for (const auto& st : moore_states) {
        int u = moore_name_to_idx[st.name];
        for (const string& sym : alphabet) {
            if (moore_transitions[st.name].count(sym)) {
                string target = moore_transitions[st.name][sym];
                if (moore_name_to_idx.count(target)) {
                    merged_edges[{u, moore_name_to_idx[target]}].push_back(sym);
                }
            }
        }
    }

    float edge_thick = max(1.5f, 2.8f * canvas_zoom);
    float arrow_sz   = max(7.0f, 14.0f * canvas_zoom);
    float lbl_font   = max(10.0f, ImGui::GetFontSize() * 0.85f * canvas_zoom);

    for (const auto& kv : merged_edges) {
        int u = kv.first.first;
        int v = kv.first.second;
        const auto& syms = kv.second;

        string label;
        for (size_t i = 0; i < syms.size(); i++) {
            if (i > 0) label += ", ";
            label += syms[i];
        }

        ImU32 col = IM_COL32(180, 140, 255, 255);
        if (syms.size() == 1 && syms[0] == alphabet[0]) col = IM_COL32(80, 190, 255, 255);
        if (syms.size() == 1 && syms[0] == alphabet[1]) col = IM_COL32(255, 165, 80, 255);

        ImVec2 posU = worldToScreen(moore_states[u].pos);
        ImVec2 posV = worldToScreen(moore_states[v].pos);

        if (u == v) {
            ImVec2 out_dir = vSub(moore_states[u].pos, centroid);
            if (vLen(out_dir) < 1.0f) out_dir = ImVec2(0.0f, -1.0f);
            out_dir = vNorm(out_dir);
            ImVec2 perp(-out_dir.y, out_dir.x);

            float base_ang = atan2f(out_dir.y, out_dir.x);
            float a1 = base_ang - 0.45f;
            float a2 = base_ang + 0.45f;

            ImVec2 p0 = vAdd(posU, ImVec2(cosf(a1) * node_r, sinf(a1) * node_r));
            ImVec2 p3 = vAdd(posU, ImVec2(cosf(a2) * node_r, sinf(a2) * node_r));
            ImVec2 cp1 = vAdd(vAdd(posU, vMul(out_dir, node_r * 2.7f)), vMul(perp, -node_r * 1.3f));
            ImVec2 cp2 = vAdd(vAdd(posU, vMul(out_dir, node_r * 2.7f)), vMul(perp,  node_r * 1.3f));

            dl->AddBezierCubic(p0, cp1, cp2, p3, col, edge_thick);
            drawArrowhead(dl, p3, vSub(p3, cp2), arrow_sz, col);

            ImVec2 mid = vAdd(vAdd(vMul(p0, 0.125f), vMul(cp1, 0.375f)),
                              vAdd(vMul(cp2, 0.375f), vMul(p3, 0.125f)));
            drawLabelPill(dl, mid, label, lbl_font, col);
        } else {
            bool has_reverse = merged_edges.count({v, u}) > 0;
            ImVec2 d = vSub(posV, posU);
            float dist = vLen(d);
            if (dist < 1e-3f) continue;
            ImVec2 unit = vMul(d, 1.0f / dist);
            ImVec2 perp(-unit.y, unit.x);

            if (has_reverse) {
                float bend = clamp(dist * 0.22f, 35.0f * canvas_zoom, 110.0f * canvas_zoom);
                ImVec2 mid_straight = vMul(vAdd(posU, posV), 0.5f);
                ImVec2 ctrl = vAdd(mid_straight, vMul(perp, bend));

                ImVec2 p0 = vAdd(posU, vMul(vNorm(vSub(ctrl, posU)), node_r));
                ImVec2 p2 = vAdd(posV, vMul(vNorm(vSub(ctrl, posV)), node_r));

                dl->AddBezierQuadratic(p0, ctrl, p2, col, edge_thick);
                drawArrowhead(dl, p2, vSub(p2, ctrl), arrow_sz, col);

                ImVec2 curve_mid = vAdd(vAdd(vMul(p0, 0.25f), vMul(ctrl, 0.5f)), vMul(p2, 0.25f));
                drawLabelPill(dl, curve_mid, label, lbl_font, col);
            } else {
                ImVec2 p0 = vAdd(posU, vMul(unit, node_r));
                ImVec2 p1 = vSub(posV, vMul(unit, node_r));

                dl->AddLine(p0, p1, col, edge_thick);
                drawArrowhead(dl, p1, unit, arrow_sz, col);

                ImVec2 mid = vMul(vAdd(p0, p1), 0.5f);
                drawLabelPill(dl, mid, label, lbl_font, col);
            }
        }
    }

    if (!moore_start_state.empty() && moore_name_to_idx.count(moore_start_state)) {
        ImVec2 start_pos = worldToScreen(moore_states[moore_name_to_idx[moore_start_state]].pos);
        ImVec2 tip  = ImVec2(start_pos.x - node_r, start_pos.y);
        ImVec2 tail = ImVec2(start_pos.x - node_r * 2.3f, start_pos.y);
        ImU32 start_col = IM_COL32(80, 230, 120, 255);
        dl->AddLine(tail, tip, start_col, edge_thick * 1.2f);
        drawArrowhead(dl, tip, ImVec2(1, 0), arrow_sz, start_col);
        drawLabelPill(dl, ImVec2(tail.x - 16.0f * canvas_zoom, tail.y), "START", lbl_font * 0.85f, start_col);
    }

    ImFont* font = ImGui::GetFont();
    float node_font = max(10.0f, ImGui::GetFontSize() * canvas_zoom);
    float font_ratio = node_font / ImGui::GetFontSize();

    for (const auto& st : moore_states) {
        ImVec2 c = worldToScreen(st.pos);
        bool is_start  = (st.name == moore_start_state);
        bool is_traced = sim_visited_states.count(st.name) > 0;

        ImU32 fill_col   = st.processed ? IM_COL32(36, 44, 62, 255) : IM_COL32(55, 45, 35, 255);
        ImU32 border_col = st.processed ? IM_COL32(100, 175, 255, 255) : IM_COL32(240, 180, 70, 255);
        if (is_traced) border_col = IM_COL32(80, 230, 120, 255);

        dl->AddCircleFilled(c, node_r, fill_col, 48);
        dl->AddCircle(c, node_r, border_col, 48, max(2.0f, (is_start ? 4.0f : 2.5f) * canvas_zoom));

        dl->AddLine(ImVec2(c.x - node_r * 0.78f, c.y), ImVec2(c.x + node_r * 0.78f, c.y),
                    IM_COL32(140, 155, 180, 180), max(1.0f, 1.5f * canvas_zoom));

        ImVec2 name_sz = vMul(ImGui::CalcTextSize(st.name.c_str()), font_ratio);
        dl->AddText(font, node_font,
                    ImVec2(c.x - name_sz.x * 0.5f, c.y - name_sz.y - 3.0f * canvas_zoom),
                    IM_COL32(255, 255, 255, 255), st.name.c_str());

        ImVec2 out_sz = vMul(ImGui::CalcTextSize(st.output.c_str()), font_ratio);
        dl->AddText(font, node_font,
                    ImVec2(c.x - out_sz.x * 0.5f, c.y + 3.0f * canvas_zoom),
                    IM_COL32(130, 235, 160, 255), st.output.c_str());
    }

    dl->PopClipRect();
    ImGui::End();
}

void mainLoopFrame() {
#ifdef __EMSCRIPTEN__
    static int last_w = 0, last_h = 0;
    int target_w = getCanvasW(), target_h = getCanvasH();
    if (target_w > 0 && target_h > 0 && (target_w != last_w || target_h != last_h)) {
        glfwSetWindowSize(g_window, target_w, target_h);
        last_w = target_w;
        last_h = target_h;
    }
#endif

    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    int win_w, win_h;
    glfwGetWindowSize(g_window, &win_w, &win_h);
    float left_w = win_w * 0.36f;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(left_w - 15, win_h * 0.58f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Simulator Controls & Input");

    if (algorithm_started) ImGui::BeginDisabled();

    ImGui::Text("Input Alphabet:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.0f);
    ImGui::InputText("##sym0", ui_sym0, IM_ARRAYSIZE(ui_sym0));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.0f);
    ImGui::InputText("##sym1", ui_sym1, IM_ARRAYSIZE(ui_sym1));
    ImGui::SameLine();
    if (ImGui::Button("Preset {0,1}")) loadBinaryPreset();
    ImGui::SameLine();
    if (ImGui::Button("Preset {a,b}")) loadAlphaPreset();

    ImGui::Separator();
    ImGui::Text("Mealy Transition Table");

    string col0_lbl = "Input " + (trim(ui_sym0).empty() ? string("0") : trim(ui_sym0)) + " (Next, Out)";
    string col1_lbl = "Input " + (trim(ui_sym1).empty() ? string("1") : trim(ui_sym1)) + " (Next, Out)";

    if (ImGui::BeginTable("MealyInput", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("State");
        ImGui::TableSetupColumn(col0_lbl.c_str());
        ImGui::TableSetupColumn(col1_lbl.c_str());
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < ui_table_rows.size(); i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID((int)i * 10 + 0);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##state", ui_table_rows[i].state, 16);
            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID((int)i * 10 + 1);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.42f);
            ImGui::InputText("##n0", ui_table_rows[i].next_0, 16);
            ImGui::SameLine(); ImGui::Text(","); ImGui::SameLine();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##o0", ui_table_rows[i].out_0, 16);
            ImGui::PopID();

            ImGui::TableSetColumnIndex(2);
            ImGui::PushID((int)i * 10 + 2);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.42f);
            ImGui::InputText("##n1", ui_table_rows[i].next_1, 16);
            ImGui::SameLine(); ImGui::Text(","); ImGui::SameLine();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##o1", ui_table_rows[i].out_1, 16);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (ImGui::Button("+ Add Row")) ui_table_rows.push_back(MealyTableRow());
    ImGui::SameLine();
    if (ImGui::Button("- Remove Row") && ui_table_rows.size() > 1) ui_table_rows.pop_back();

    ImGui::Separator();
    ImGui::Text("Initial State:"); ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("##init_state", ui_initial_state, IM_ARRAYSIZE(ui_initial_state));

    if (algorithm_started) ImGui::EndDisabled();

    if (!algorithm_started) {
        if (ImGui::Button("Lock Input & Start Conversion")) {
            initializeConversion();
        }
    } else {
        if (ImGui::Button("Execute Next Step")) { processNextStep(); }
        ImGui::SameLine();
        if (ImGui::Button("Complete All Steps")) { processAllSteps(); }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) { resetSimulator(); }

        ImGui::Separator();
        ImGui::Text("Pending States Queue (%d):", (int)pending_queue.size());
        if (pending_queue.empty()) {
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "Conversion complete.");
        } else {
            queue<int> tmp = pending_queue;
            string q_str;
            while (!tmp.empty()) {
                if (!q_str.empty()) q_str += ", ";
                q_str += moore_states[tmp.front()].name;
                tmp.pop();
            }
            ImGui::TextWrapped("%s", q_str.c_str());
        }

        ImGui::Separator();
        ImGui::Text("String Tester (Mealy vs. Moore)");
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputText("##test_str", ui_test_string, IM_ARRAYSIZE(ui_test_string));
        ImGui::SameLine();
        if (ImGui::Button("Simulate")) {
            runStringSimulation();
        }

        if (!sim_moore_out.empty()) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Mealy Output: %s", sim_mealy_out.c_str());
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "Moore Output: %s", sim_moore_out.c_str());
            ImGui::TextWrapped("Moore Path: %s", sim_moore_path.c_str());
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(10, win_h * 0.60f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(left_w - 15, win_h * 0.38f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Output Moore Machine Table");

    if (algorithm_started) {
        if (ImGui::BeginTable("MooreOutput", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn(alphabet[0].c_str());
            ImGui::TableSetupColumn(alphabet[1].c_str());
            ImGui::TableSetupColumn("output");
            ImGui::TableHeadersRow();

            for (const auto& st : moore_states) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s%s", st.name.c_str(), (st.name == moore_start_state ? " (start)" : ""));

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", moore_transitions[st.name].count(alphabet[0]) ? moore_transitions[st.name][alphabet[0]].c_str() : "-");

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", moore_transitions[st.name].count(alphabet[1]) ? moore_transitions[st.name][alphabet[1]].c_str() : "-");

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", st.output.c_str());
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::Text("Click 'Lock Input & Start Conversion' to generate the Moore table.");
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(left_w + 5, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(win_w - left_w - 15, win_h - 20), ImGuiCond_FirstUseEver);
    drawStateCanvas();

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(g_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(g_window);
}

int main() {
    if (!glfwInit()) return -1;

#ifdef __EMSCRIPTEN__
    float dpr = (float)getDPR();
    const char* glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SAMPLES, 4);
    g_window = glfwCreateWindow(getCanvasW(), getCanvasH(), "Mealy to Moore Simulator", NULL, NULL);
#else
    const char* glsl_version = "#version 130";
    g_window = glfwCreateWindow(1280, 720, "Mealy to Moore Simulator", NULL, NULL);
    if (g_window) glfwMaximizeWindow(g_window);
#endif
    if (!g_window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(g_window);
#ifndef __EMSCRIPTEN__
    glfwSwapInterval(1);
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImGuiIO& io = ImGui::GetIO();
#ifdef __EMSCRIPTEN__
    if (!io.Fonts->AddFontFromFileTTF("fonts/segoeui.ttf", 20.0f * dpr)) io.Fonts->AddFontDefault();
    ImGui::GetStyle().ScaleAllSizes(1.15f * dpr);
#else
    if (!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 24.0f)) io.Fonts->AddFontDefault();
    ImGui::GetStyle().ScaleAllSizes(1.35f);
#endif
    ImGui::StyleColorsDark();

    loadBinaryPreset();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoopFrame, 0, false);
    return 0;
#else
    while (!glfwWindowShouldClose(g_window)) {
        mainLoopFrame();
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(g_window);
    glfwTerminate();
    return 0;
#endif
}