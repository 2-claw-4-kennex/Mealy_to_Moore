#include <bits/stdc++.h>
#include <cmath>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imnodes.h"

using namespace std;

struct MealyTransition {
    string next_state;
    string output;
};

struct MealyTableRow {
    char state[16] = "";
    char next_a[16] = "", out_a[16] = "";
    char next_b[16] = "", out_b[16] = "";
};

// Global variables for automata mapping
map<string, map<string, MealyTransition>> mealy;
vector<string> alphabet = {"a", "b"};
map<string, map<string, string>> moore_transitions;
map<string, string> moore_outputs;
queue<string> bfs_queue;
set<string> visited;
map<string, int> state_layer;

// UI State Variables
vector<MealyTableRow> ui_table_rows;
char ui_initial_state[32] = "q0";
bool algorithm_started = false;
bool trigger_auto_layout = false;

// String Tester Variables
char ui_test_string[128] = "";
string simulation_result = "";
string simulation_path = "";

void processNextStep() {
    if (bfs_queue.empty()) return;

    string current_moore = bfs_queue.front();
    bfs_queue.pop();

    string current_mealy = current_moore.substr(0, current_moore.length() - 1);

    for (string symbol : alphabet) {
        if (mealy[current_mealy].count(symbol) == 0) continue;

        MealyTransition trans = mealy[current_mealy][symbol];
        string next_moore = trans.next_state + trans.output;

        moore_transitions[current_moore][symbol] = next_moore;

        if (visited.find(next_moore) == visited.end()) {
            visited.insert(next_moore);
            moore_outputs[next_moore] = trans.output;
            state_layer[next_moore] = state_layer[current_moore] + 1;
            bfs_queue.push(next_moore);
        }
    }
}

int main() {
    // 1. Initialize hardware window
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Mealy to Moore Simulator", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }

    glfwMaximizeWindow(window);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // 2. Initialize GUI context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // 3. High-Resolution Font Loading & Geometry Scaling
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 38.0f);

    ImGui::GetStyle().ScaleAllSizes(2.5f);
    ImNodes::GetStyle().NodePadding = ImVec2(24.0f, 24.0f);
    ImGui::StyleColorsDark();

    // 4. Hardcode the requested Mealy transition table
    ui_table_rows.resize(4);

    strcpy(ui_table_rows[0].state, "q0");
    strcpy(ui_table_rows[0].next_a, "q3"); strcpy(ui_table_rows[0].out_a, "0");
    strcpy(ui_table_rows[0].next_b, "q1"); strcpy(ui_table_rows[0].out_b, "1");

    strcpy(ui_table_rows[1].state, "q1");
    strcpy(ui_table_rows[1].next_a, "q0"); strcpy(ui_table_rows[1].out_a, "1");
    strcpy(ui_table_rows[1].next_b, "q3"); strcpy(ui_table_rows[1].out_b, "0");

    strcpy(ui_table_rows[2].state, "q2");
    strcpy(ui_table_rows[2].next_a, "q2"); strcpy(ui_table_rows[2].out_a, "1");
    strcpy(ui_table_rows[2].next_b, "q2"); strcpy(ui_table_rows[2].out_b, "0");

    strcpy(ui_table_rows[3].state, "q3");
    strcpy(ui_table_rows[3].next_a, "q1"); strcpy(ui_table_rows[3].out_a, "0");
    strcpy(ui_table_rows[3].next_b, "q0"); strcpy(ui_table_rows[3].out_b, "1");

    // 5. Main Application Render Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- WINDOW 1: Tabular Input & Controls ---
        ImGui::Begin("Simulator Controls & Input");

        ImGui::Text("Mealy Transition Table");

        if (algorithm_started) ImGui::BeginDisabled();

        if (ImGui::BeginTable("MealyInput", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn("Input a");
            ImGui::TableSetupColumn("Input b");
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < ui_table_rows.size(); i++) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(i * 10 + 0);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText("##state", ui_table_rows[i].state, 16);
                ImGui::PopID();

                ImGui::TableSetColumnIndex(1);
                ImGui::PushID(i * 10 + 1);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
                ImGui::InputText("##na", ui_table_rows[i].next_a, 16);
                ImGui::SameLine(); ImGui::Text(","); ImGui::SameLine();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText("##oa", ui_table_rows[i].out_a, 16);
                ImGui::PopID();

                ImGui::TableSetColumnIndex(2);
                ImGui::PushID(i * 10 + 2);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
                ImGui::InputText("##nb", ui_table_rows[i].next_b, 16);
                ImGui::SameLine(); ImGui::Text(","); ImGui::SameLine();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText("##ob", ui_table_rows[i].out_b, 16);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        if (ImGui::Button("+ Add Row")) {
            ui_table_rows.push_back(MealyTableRow());
        }

        ImGui::Separator();
        ImGui::Text("Initial State:"); ImGui::SameLine();
        ImGui::InputText("##init_state", ui_initial_state, IM_ARRAYSIZE(ui_initial_state));

        if (algorithm_started) ImGui::EndDisabled();

        if (!algorithm_started) {
            if (ImGui::Button("Lock Input & Start Conversion")) {
                mealy.clear();
                for (auto& row : ui_table_rows) {
                    if (strlen(row.state) > 0) {
                        mealy[row.state]["a"] = {row.next_a, row.out_a};
                        mealy[row.state]["b"] = {row.next_b, row.out_b};
                    }
                }
                string init_moore = string(ui_initial_state) + "-";
                bfs_queue.push(init_moore);
                visited.insert(init_moore);
                moore_outputs[init_moore] = "-";
                state_layer[init_moore] = 0;
                algorithm_started = true;
            }
        } else {
            if (ImGui::Button("Execute Next Step")) {
                processNextStep();
                trigger_auto_layout = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset Simulator")) {
                mealy.clear(); moore_transitions.clear();
                moore_outputs.clear(); visited.clear(); state_layer.clear();
                while(!bfs_queue.empty()) bfs_queue.pop();
                simulation_result = ""; simulation_path = ""; ui_test_string[0] = '\0';
                algorithm_started = false;
            }

            ImGui::Text("Pending states in BFS Queue (%d)", (int)bfs_queue.size());
            if (!bfs_queue.empty()) {
                queue<string> temp_q = bfs_queue;
                while (!temp_q.empty()) {
                    ImGui::BulletText("%s", temp_q.front().c_str());
                    temp_q.pop();
                }
            }

            // --- String Tester UI ---
            ImGui::Separator();
            ImGui::Text("Moore Machine String Tester");
            ImGui::InputText("Input String (e.g. aba)", ui_test_string, IM_ARRAYSIZE(ui_test_string));

            if (ImGui::Button("Simulate Input")) {
                simulation_result = "";
                simulation_path = "";
                string curr_state = string(ui_initial_state) + "-";

                if (moore_outputs.count(curr_state) == 0) {
                    simulation_result = "Error: Machine not initialized.";
                } else {
                    simulation_result = moore_outputs[curr_state];
                    simulation_path = curr_state;

                    for (char c : string(ui_test_string)) {
                        string sym(1, c);
                        if (moore_transitions[curr_state].count(sym)) {
                            curr_state = moore_transitions[curr_state][sym];
                            simulation_result += moore_outputs[curr_state];
                            simulation_path += " -> " + curr_state;
                        } else {
                            simulation_result += " [HALT: Unmapped transition '" + sym + "']";
                            break;
                        }
                    }
                }
            }

            if (!simulation_result.empty()) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "Output String: %s", simulation_result.c_str());
                ImGui::TextWrapped("State Path: %s", simulation_path.c_str());
            }
        }
        ImGui::End();

        // --- WINDOW 2: Dedicated Moore Output Table ---
        ImGui::Begin("Output Moore Machine Table");
        if (algorithm_started) {
            if (ImGui::BeginTable("MooreOutput", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("State");
                ImGui::TableSetupColumn("a");
                ImGui::TableSetupColumn("b");
                ImGui::TableSetupColumn("output");
                ImGui::TableHeadersRow();

                for (const auto& pair : moore_outputs) {
                    ImGui::TableNextRow();
                    string state = pair.first;
                    string out = pair.second;

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", state.c_str());

                    ImGui::TableSetColumnIndex(1);
                    if (moore_transitions[state].count("a")) ImGui::Text("%s", moore_transitions[state]["a"].c_str());
                    else ImGui::Text("-");

                    ImGui::TableSetColumnIndex(2);
                    if (moore_transitions[state].count("b")) ImGui::Text("%s", moore_transitions[state]["b"].c_str());
                    else ImGui::Text("-");

                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%s", out.c_str());
                }
                ImGui::EndTable();
            }
        } else {
            ImGui::Text("Table will generate after starting conversion.");
        }
        ImGui::End();

        // --- WINDOW 3: Node Graph Visualizer UI ---
        ImGui::Begin("State Visualizer");

        if (ImGui::Button("Force Align Hierarchical Layout")) {
            trigger_auto_layout = true;
        }

        ImNodes::BeginNodeEditor();

        int node_id = 1;
        int link_id = 1000;
        map<string, int> state_to_id;

        for (auto const& pair : moore_outputs) {
            state_to_id[pair.first] = node_id++;
        }

        map<int, int> nodes_in_layer;

        for (auto const& pair : moore_outputs) {
            string state = pair.first;
            string out = pair.second;
            int current_node_id = state_to_id[state];

            if (trigger_auto_layout) {
                int layer = state_layer[state];
                float x = 50.0f + layer * 550.0f;
                float y = 50.0f + nodes_in_layer[layer] * 350.0f;
                ImNodes::SetNodeGridSpacePos(current_node_id, ImVec2(x, y));
                nodes_in_layer[layer]++;
            }

            ImNodes::BeginNode(current_node_id);

            ImNodes::BeginNodeTitleBar();
            ImGui::Text("%s / %s", state.c_str(), out.c_str());
            ImNodes::EndNodeTitleBar();

            ImNodes::BeginInputAttribute(current_node_id << 8);
            ImGui::Dummy(ImVec2(1.0f, 20.0f));
            ImNodes::EndInputAttribute();

            int symbol_idx = 1;
            for (const string& sym : alphabet) {
                if (moore_transitions[state].count(sym)) {
                    ImNodes::BeginOutputAttribute((current_node_id << 8) + symbol_idx);

                    ImGui::Dummy(ImVec2(120.0f, 0.0f));
                    ImGui::SameLine();
                    ImGui::Text("[ %s ] --->", sym.c_str());

                    ImNodes::EndOutputAttribute();
                }

                if (symbol_idx == 1 && alphabet.size() > 1) {
                    ImGui::Dummy(ImVec2(0, 80.0f));
                }
                symbol_idx++;
            }
            ImNodes::EndNode();
        }

        trigger_auto_layout = false;

        for (auto const& state_pair : moore_transitions) {
            string state = state_pair.first;
            int current_node_id = state_to_id[state];

            int symbol_idx = 1;
            for (const string& sym : alphabet) {
                if (state_pair.second.count(sym)) {
                    string next_state = state_pair.second.at(sym);
                    int next_node_id = state_to_id[next_state];
                    ImNodes::Link(link_id++, (current_node_id << 8) + symbol_idx, (next_node_id << 8));
                }
                symbol_idx++;
            }
        }

        ImNodes::EndNodeEditor();
        ImGui::End();

        // 6. Render frame to screen
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // 7. Memory cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}