#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-narrowing-conversions"
// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui.h"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <cstdio>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <iostream>
#include <fstream>
#include <map>
#include <ImGuiFileBrowser.h>
#include <DataLoader.hpp>
#include <cmath>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif
int MAXIMUM_SAMPLES_TO_RENDER = 5000;
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
unsigned long long getTotalSystemMemory()
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return status.ullTotalPhys;
}

#else

namespace ImGui { extern ImGuiKeyData* GetKeyData(ImGuiKey key); }
std::vector<int> last_keys;

unsigned long long getTotalSystemMemory()
{
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}


const static std::map<int, const char*> popup_ids{
        {0, "Open File"},
        {1, "Save File"},
        {2, "Parameter Error"}
};


struct CurrentState {
    // Windows
    bool show_demo_window = false;
    bool show_data_view_window = true;
    bool show_data_converter_window = false;
    bool show_file_selector = false;

    // Data View
    bool load_the_file = false;
    py::array_t<int> data;
    bool data_loaded = false;
    uint64_t data_min = 0;
    uint64_t data_max = 0;

    // Data Converter
    bool should_convert_data = false;

    // Misc
    std::string error_message;
};

struct Config {
    uint64_t samples = 0;
    uint64_t  start_token = 0;
    uint64_t  end_token = 0;
    uint64_t  sample_length = 0;
    uint64_t  padding_value = 0;
    std::string file_path;
    std::string tokenizer_name;
};

enum PopUpIDs {
    OpenFileSelect = 0,
    SaveFileSelect = 1,
    ParameterError = 2
};

struct ParameterErrors {
    bool tokenizer_name = false;
    bool samples = false;
    bool start_token = false;
    bool end_token = false;
    bool sample_length = false;
    bool padding_value = false;
    bool any = false;
};

int PADDING_SIZE = 2;


void TurnOffViewData(CurrentState *state) {
    state->show_data_view_window = false;
    state->data = py::array_t<int>();
    state->data_loaded = false;
}


Config LoadConfig() {
    Config config;
    std::fstream config_file;
    config_file.open("config.ini", std::ios::in);
    if (config_file) {
        std::string line;
        while (std::getline(config_file, line)) {
            std::string key;
            std::string value;
            for (int i=0; i <= line.length(); i++) {
                if (line[i] == '=') {
                    key.append(line, 0, i);
                    value.append(line, i+1, std::string::npos);
                }
            }
            if (key == "tokenizer_name") {
                config.tokenizer_name = value;
            }
            else if (key == "file_path") {
                config.file_path = value;
            }
            else if (key == "samples") {
                try {
                    config.samples = std::stoi(value);
                }
                catch (std::invalid_argument& e) {
                    std::cout << "Invalid argument samples: " << e.what() << value << std::endl;
                    config.samples = 0;
                }
            }
            else if (key == "start_token") {
                try {
                    config.start_token = std::stoi(value);
                }
                catch (std::invalid_argument& e) {
                    std::cout << "Invalid argument start_token: " << e.what() << value << std::endl;
                    config.start_token = 0;
                }
            }
            else if (key == "end_token") {
                try {
                    config.end_token = std::stoi(value);
                }
                catch (std::invalid_argument& e) {
                    std::cout << "Invalid argument end_token: " << e.what() << value << std::endl;
                    config.end_token = 0;
                }
            }
            else if (key == "sample_length") {
                try {
                    config.sample_length = std::stoi(value);
                }
                catch (std::invalid_argument& e) {
                    std::cout << "Invalid argument sample_length: " << e.what() << value << std::endl;
                    config.sample_length = 0;
                }
            }
            else if (key == "padding_value") {
                try {
                    config.padding_value = std::stoi(value);
                }
                catch (std::invalid_argument& e) {
                    std::cout << "Invalid argument padding_value: " << e.what() << value << std::endl;
                    config.padding_value = 0;
                }
            }
        }
    }
    else {
        std::cout << "Config file not found" << std::endl;
    }
    config_file.close();
    return config;
}


void SaveConfig(const Config& config) {
    std::fstream config_file;
    config_file.open("config.ini", std::ios::out);
    if (config_file) {
        config_file << "tokenizer_name=" << config.tokenizer_name << std::endl;
        config_file << "file_path=" << config.file_path << std::endl;
        config_file << "samples=" << config.samples << std::endl;
        config_file << "start_token=" << config.start_token << std::endl;
        config_file << "end_token=" << config.end_token << std::endl;
        config_file << "sample_length=" << config.sample_length << std::endl;
        config_file << "padding_value=" << config.padding_value << std::endl;
    }
    else {
        std::cout << "Config file not found" << std::endl;
        std::ofstream config_file("config.ini");
        config_file.close();
        SaveConfig(config);
    }
    config_file.close();
}


ImVec2 CreateMenuBar(CurrentState *state, Config *config) {
    ImVec2 size;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Configuration")) {
                SaveConfig(*config);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Data")) {
            if (ImGui::MenuItem("View Data")) {
                state->show_data_view_window = true;
                state->show_demo_window = false;
            }
            if (ImGui::MenuItem("Close View Data")) {
                TurnOffViewData(state);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Convert Data")) {
                state->show_data_converter_window = true;
                state->show_demo_window = false;
                TurnOffViewData(state);
            }
            if (ImGui::MenuItem("Close Convert Data")) {
                state->show_data_converter_window = false;
                state->show_data_view_window = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                state->show_demo_window = true;
                TurnOffViewData(state);
            }
            ImGui::EndMenu();
        }
        size = ImGui::GetWindowSize();
        ImGui::EndMainMenuBar();
    }
    return size;
}

ParameterErrors verify_data_view_parameters(const std::string& file_path, const std::string& tokenizer_name, uint64_t sample_size,
                                            uint64_t start_token, uint64_t end_token, uint64_t sample_length,
                                            uint64_t padding_value) {
    ParameterErrors errors = ParameterErrors();
    if (tokenizer_name.empty()) {
        errors.tokenizer_name = true;
        errors.any = true;
    }
    else if (sample_size <= 0) {
        errors.samples = true;
        errors.any = true;
    }
    else if (start_token == padding_value) {
        errors.start_token = true;
        errors.any = true;
    }
    else if (start_token == end_token) {
        errors.end_token = true;
        errors.any = true;
    }
    else if (64 <= sample_length || sample_length <= 0) {
        errors.sample_length = true;
        errors.any = true;
    }
    else if (padding_value < 0) {
        errors.padding_value = true;
        errors.any = true;
    }
    return errors;
}


float lerp(float a, float b, float p) {
    return a + (b - a)*p;
}

float ulerp (float n, float m, float M) {
    return (n - m)/(M - m);
}

float remap(float n, float a, float b, float x, float y) {
    return lerp(x, y, ulerp(n, a, b));
}


void linear_gradient(float value, float min_value, float max_value, float *start_color, float *end_color, float *out) {
    for (int i = 0; i < 4; i++) {
        float percent = ulerp(value, min_value, max_value);
        out[i] = lerp(start_color[i], end_color[i], percent);
    }
}


int main(int, char**)
{
    imgui_addons::ImGuiFileBrowser file_dialog; // As a class member or globally
    py::scoped_interpreter guard{};

    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
    CurrentState MainState = *new CurrentState;
    Config config = LoadConfig();


    int width = 1280;
    int height = 720;
    ImVec2 screen_size = ImVec2(1280, 720);
    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(width, height, "Gavin Dataset Editor", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    ImVec4 window_background_colour = ImVec4(0.f, 0.f, 0.f, 1.00f);


    // Values used later
    std::string data_view_file_path = config.file_path;
    uint64_t data_view_samples = config.samples;
    uint64_t data_view_start_token = config.start_token;
    uint64_t data_view_end_token = config.end_token;
    uint64_t data_view_sample_length = config.sample_length;
    uint64_t data_view_padding_value = config.padding_value;
    std::string data_view_tokenizer_name = config.tokenizer_name;

    // Main loop
    while (!glfwWindowShouldClose(window))
    {

        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        for (ImGuiKey key = 32; key < GLFW_KEY_LAST; key++)
        {
            if ((key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) && glfwGetKey(window, key) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);
            else if ((key == GLFW_KEY_RIGHT_CONTROL || key == GLFW_KEY_LEFT_CONTROL) && glfwGetKey(window, key) == GLFW_PRESS)
                MainState.show_data_view_window = !MainState.show_data_view_window;
        }


        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int h, w;
        glfwGetWindowSize(window, &w, &h);
        screen_size = ImVec2((float)w, (float)h);

        ImVec2 main_menu_bar_size = CreateMenuBar(&MainState, &config);

        if (MainState.show_data_view_window) {

            ImVec2 data_view_window_size = ImVec2(std::ceil(screen_size.x),
                                              std::floor(screen_size.y - main_menu_bar_size.y)*.33f);
            ImGui::SetNextWindowSize(data_view_window_size);
            ImGui::SetNextWindowPos(ImVec2(0, main_menu_bar_size.y));
            ImGui::Begin("GavinBackendDatasetUtils-Config", &MainState.show_data_view_window,
                         ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoTitleBar
                         |ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);

            ImGui::InputText("File Path: ", const_cast<char *>(data_view_file_path.c_str()), 50000);
            ImGui::InputText("File Name", const_cast<char *>(data_view_tokenizer_name.c_str()), 50000);
            ImGui::InputInt("Number of Samples", reinterpret_cast<int *>(&data_view_samples));
            ImGui::InputInt("Start Token", reinterpret_cast<int *>(&data_view_start_token));
            ImGui::InputInt("End Token", reinterpret_cast<int *>(&data_view_end_token));
            ImGui::InputInt("Sample Length", reinterpret_cast<int *>(&data_view_sample_length));
            ImGui::InputInt("Padding Value", reinterpret_cast<int *>(&data_view_padding_value));
            ImGui::Checkbox("Load: ", &MainState.load_the_file);
            ImGui::Text("%s", MainState.error_message.c_str());
            ImGui::End();
            ImGui::SetNextWindowSize({
                std::ceil(screen_size.x),
                std::floor(screen_size.y - (main_menu_bar_size.y + data_view_window_size.y + PADDING_SIZE))
            });
            ImGui::SetNextWindowPos({
                0,
                (main_menu_bar_size.y + data_view_window_size.y) + PADDING_SIZE
            });
            ImGui::Begin("GavinBackendDatasetUtils-Data", &MainState.show_data_view_window,
                         ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoTitleBar
                         |ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
            if (MainState.load_the_file) {
                ParameterErrors errors = verify_data_view_parameters(
                        data_view_file_path,
                        data_view_tokenizer_name,
                        data_view_samples,
                        data_view_start_token,
                        data_view_end_token,
                        data_view_sample_length,
                        data_view_padding_value
                );
                if (errors.any) {
                    MainState.error_message = "Invalid Configuration";
                    if (errors.tokenizer_name) {
                        MainState.error_message += ": Tokenizer Name";
                    }
                    if (errors.samples) {
                        MainState.error_message += ": Number of Samples";
                    }
                    if (errors.start_token) {
                        MainState.error_message += ": Start Token";
                    }
                    if (errors.end_token) {
                        MainState.error_message += ": End Token";
                    }
                    if (errors.sample_length) {
                        MainState.error_message += ": Sample Length";
                    }
                    if (errors.padding_value) {
                        MainState.error_message += ": Padding Value";
                    }
                }
                else {
                    MainState.error_message = "";
                    config.file_path = data_view_file_path;
                    config.tokenizer_name = data_view_tokenizer_name;
                    config.samples = data_view_samples;
                    config.start_token = data_view_start_token;
                    config.end_token = data_view_end_token;
                    config.sample_length = data_view_sample_length;
                    config.padding_value = data_view_padding_value;
                    MainState.load_the_file = false;
                    SaveConfig(config);
                    if (data_view_samples * data_view_sample_length * sizeof(uint64_t) > getTotalSystemMemory()) {
                        MainState.error_message = "Not enough memory to load dataset";
                        MainState.data_loaded = false;
                    }
                    else {
                        if (data_view_samples > MAXIMUM_SAMPLES_TO_RENDER) {
                            data_view_samples = MAXIMUM_SAMPLES_TO_RENDER;
                            MainState.error_message = "Maximum data_view_samples to render is " + std::to_string(MAXIMUM_SAMPLES_TO_RENDER);
                        }
                        py::array_t<int> data = LoadTrainDataMT(
                                data_view_samples,
                                data_view_file_path,
                                data_view_tokenizer_name,
                                data_view_start_token,
                                data_view_end_token,
                                data_view_sample_length,
                                data_view_padding_value
                        );
                        MainState.data = data;
                        MainState.data_loaded = true;
                        MainState.data_max = data_view_end_token;
                        MainState.data_min = data_view_padding_value;
                    }
                    }
                }
            else if (MainState.data_loaded) {
                static ImGuiTableFlags flags =
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti
                        | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody
                        | ImGuiTableFlags_ScrollY;

                if (ImGui::BeginTable("Data table", MainState.data.shape()[1]), flags) {
                    for (int i = 0; i<MainState.data.shape()[1]; i++) {
                        std::ostringstream oss;
                        oss << i;
                        ImGui::TableSetupColumn(oss.str().c_str());
                    }
                    ImGui::TableHeadersRow();
                    int max_samples;
                    if (MainState.data.shape()[0] > MAXIMUM_SAMPLES_TO_RENDER) {
                        max_samples = MAXIMUM_SAMPLES_TO_RENDER;
                        MainState.error_message = "Maximum Samples reached";
                    }
                    else {
                        max_samples = MainState.data.shape()[0];
                    }
                    for (int row = 0; row < max_samples; row++) {
                        ImGui::TableNextRow();
                        for (int col = 0; col < MainState.data.shape()[1]; col++) {
                            ImGui::TableSetColumnIndex(col);
                            int cell_value = MainState.data.at(row, col);
                            char buf[512];
                            std::sprintf(buf, "%d", cell_value);
                            ImGui::TextUnformatted(buf);

                            float colour[4];
                            float start_color[] = {0.19, 0.36, 0.87, 1.0};  // Blue
                            float end_color[] = {0.87, 0.19, 0.28, 1.0};  // Red
                            linear_gradient((float)cell_value, (float)MainState.data_min,
                                            (float)MainState.data_max, start_color, end_color, colour);
                            ImU32 colour_cast = ImColor(colour[0], colour[1], colour[2], colour[3]);
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, colour_cast);
                        }
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::End();
            }
        else if (!MainState.show_data_view_window) {
            MainState.data = py::array_t<int>();
            MainState.data_loaded = false;
        }

        if (MainState.show_data_converter_window) {
            uint64_t samples_to_convert;
            const char* file_convert_path;
            const char* file_save_path;
            ImVec2 data_converter_window_size = ImVec2(std::ceil(screen_size.x),
                                                       std::floor(screen_size.y-main_menu_bar_size.y)
                    );
            ImGui::SetNextWindowSize(data_converter_window_size);
            ImGui::SetNextWindowPos(ImVec2(0, main_menu_bar_size.y));
            ImGui::Begin("GavinBackendDatasetUtils-Config", &MainState.show_data_view_window,
                         ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoTitleBar
                         |ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
            ImGui::InputInt("Number of Samples", reinterpret_cast<int *>(&samples_to_convert));
            ImGui::InputText("File to Convert", const_cast<char *>(file_convert_path), 50000);
            ImGui::InputText("File to Save", const_cast<char *>(file_save_path), 50000);
            ImGui::Text("%s", MainState.error_message.c_str());
            ImGui::Checkbox("Convert data", &MainState.should_convert_data);
            ImGui::End();
            if (MainState.should_convert_data) {
                try {
                    ConvertToBinFormat(samples_to_convert, file_convert_path, file_save_path);
                }
                catch (const std::exception& ex) {
                    MainState.error_message = ex.what();
                }
            }
        }


        if (MainState.show_demo_window) {
            ImGui::ShowDemoWindow(&MainState.show_demo_window);
        }

        if (MainState.show_file_selector) {
            ImGui::OpenPopup("Open File");
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(window_background_colour.x * window_background_colour.w, window_background_colour.y * window_background_colour.w, window_background_colour.z * window_background_colour.w, window_background_colour.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
