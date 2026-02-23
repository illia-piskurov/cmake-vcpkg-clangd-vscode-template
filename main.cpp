#include <SDL3/SDL.h>
#include <cpr/cpr.h>
#include <filesystem>
#include <future>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

struct WeatherData {
  std::string temp = "N/A";
  std::string wind = "N/A";
  bool loading = false;
};

std::string find_system_font() {
  std::vector<std::string> paths = {
      "C:\\Windows\\Fonts\\arial.ttf", "C:\\Windows\\Fonts\\segoeui.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
      "/usr/share/fonts/google-roboto/Roboto-Regular.ttf"};
  for (const auto &path : paths) {
    if (std::filesystem::exists(path))
      return path;
  }
  return "";
}

int main(int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO))
    return -1;

  SDL_Window *window;
  SDL_Renderer *renderer;
  if (!SDL_CreateWindowAndRenderer("Weather Station", 1280, 720,
                                   SDL_WINDOW_RESIZABLE, &window, &renderer))
    return -1;

  float display_scale = SDL_GetWindowDisplayScale(window);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGuiIO &io = ImGui::GetIO();
  std::string font_path = find_system_font();

  static const ImWchar ranges[] = {0x0020, 0x00FF, 0x0400, 0x044F, 0};
  if (!font_path.empty()) {
    io.Fonts->AddFontFromFileTTF(font_path.c_str(), 18.0f * display_scale,
                                 nullptr, ranges);
  } else {
    io.Fonts->AddFontDefault();
  }

  ImGui::GetStyle().ScaleAllSizes(display_scale);

  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  WeatherData weather;
  std::future<void> fetch_task;

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT)
        running = false;
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("MainPanel", nullptr, flags);

    ImGui::Text("Station Location: Sumy, UA");
    ImGui::Separator();

    if (weather.loading) {
      ImGui::Text("Fetching weather data...");
      if (fetch_task.valid() && fetch_task.wait_for(std::chrono::seconds(0)) ==
                                    std::future_status::ready) {
        weather.loading = false;
      }
    } else {
      ImGui::Text("Current Temperature: %s", weather.temp.c_str());
      ImGui::Text("Wind Condition: %s", weather.wind.c_str());

      if (ImGui::Button("Update Weather", ImVec2(200, 40))) {
        weather.loading = true;
        fetch_task = std::async(std::launch::async, [&weather]() {
          cpr::Response r =
              cpr::Get(cpr::Url{"https://api.open-meteo.com/v1/forecast"},
                       cpr::Parameters{{"latitude", "50.91"},
                                       {"longitude", "34.80"},
                                       {"current_weather", "true"}});
          if (r.status_code == 200) {
            auto j = json::parse(r.text);
            weather.temp =
                std::to_string(
                    j["current_weather"]["temperature"].get<double>()) +
                " C";
            weather.wind =
                std::to_string(
                    j["current_weather"]["windspeed"].get<double>()) +
                " km/h";
          }
        });
      }
    }

    if (ImGui::Button("Close Application", ImVec2(200, 40)))
      running = false;

    ImGui::End();

    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
  }

  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}