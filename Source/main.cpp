#include <stdio.h>
#include <cstdint>
#include <iostream>
#include <array>
#include <filesystem>
#include <string>
#include <optional>

#include <glm/glm.hpp>

#include "SOIS/ImGuiSample.hpp"
#include "SOIS/ApplicationContext.hpp"
#include "SOIS/Renderer.hpp"

#include "SDL.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h"

#include "nfd.h"

// Returns float between 0 and 1, inclusive;
float Rand()
{
  return ((float)rand() / (RAND_MAX));
}

ImVec2 ToImgui(glm::vec2 vec)
{
    return ImVec2(vec.x, vec.y);
}

class FancyPoint
{
public:
    glm::vec2 mPos;
    glm::vec2 mVelocity;
    glm::vec3 mColor;
    float mRadius = 2.f;

    FancyPoint(glm::vec2 pos, float r = 2.f, glm::vec3 c = glm::vec3(1.f, 1.f, 1.f), glm::vec2 velocity = glm::vec2(1, 1))
    {
        mPos = pos;
        mRadius = r;
        mColor = c;
        mVelocity = velocity;
    }

    bool IsOutCanvas()
    {
        ImGuiIO& io = ImGui::GetIO();
        glm::vec2 p = mPos;
        float w = io.DisplaySize.x;
        float h = io.DisplaySize.y;
        return (p.x > w || p.y > h || p.x < 0 || p.y < 0);
    }

    void update()
    {
        mPos.x += mVelocity.x;
        mPos.y += mVelocity.y;

        //mVelocity.x *= (Rand() > .01) ? 1 : -1;
        //mVelocity.y *= (Rand() > .01) ? 1 : -1;
        if (IsOutCanvas())
        {
            ImGuiIO& io = ImGui::GetIO();

            mPos = glm::vec2(Rand() * io.DisplaySize.x, Rand() * io.DisplaySize.y);
            mVelocity.x *= Rand() > .5 ? 1 : -1;
            mVelocity.y *= Rand() > .5 ? 1 : -1;
        }
    }

    void draw() const
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddCircleFilled(ImVec2(mPos.x, mPos.y), mRadius, ImColor{mColor.r, mColor.g, mColor.b});
    }

};

constexpr float cMinDistance = 73.f;

std::vector<FancyPoint> InitPoints()
{
    std::vector<FancyPoint> points;
    

    ImGuiIO& io = ImGui::GetIO();
    float canvasWidth = io.DisplaySize.x;
    float canvasHeight = io.DisplaySize.y;

    for (size_t i = 0; i < 100; i++)
    {
        points.emplace_back(glm::vec2(Rand() * canvasWidth, Rand() * canvasHeight), 3.4f, glm::vec3(1,0,0), glm::vec2(Rand()>.5?Rand():-Rand(), Rand()>.5?Rand():-Rand()));
    }

    return std::move(points);
}

void DrawPoints(std::vector<FancyPoint>& points)
{
    for (auto const& point1 : points)
    {
        for (auto const& point2 : points)
        {
            if ((&point1 != &point2))
            {
                auto distance = glm::distance(point1.mPos, point2.mPos);

                if (distance <=  cMinDistance)
                {
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    draw_list->AddLine(ToImgui(point1.mPos), ToImgui(point2.mPos), ImColor(1.f, 1.f, 1.f, 1 - (distance / cMinDistance)));
                }
            }
        }
    }
    
    // Draw the points separately to make them draw on top
    for (auto const& point1 : points)
    {
        point1.draw();
    }
}

void UpdatePoints(std::vector<FancyPoint>& points)
{
    ImGuiIO& io = ImGui::GetIO();

    glm::vec2 mouse = glm::vec2(io.MousePos.x, io.MousePos.y);

    for (auto& point : points)
    {
        point.update();

        if (glm::distance(mouse, point.mPos) < 100.f)
        {
            auto direction = glm::normalize(point.mPos - mouse);
        
            point.mPos = mouse + (direction * 100.f);
        }
    }
}

std::string GetImGuiIniPath()
{
    auto sdlIniPath = SDL_GetPrefPath("PlaymerTools", "PadInput");

    std::filesystem::path path{ sdlIniPath };
    SDL_free(sdlIniPath);

    path /= "imgui.ini";

    return path.u8string();
}

std::string PickImageFile()
{
    std::string out;
    nfdchar_t *outPath = nullptr;
    nfdresult_t result = NFD_OpenDialog("png,jpg", NULL, &outPath);
        
    if ( result == NFD_OKAY ) {
        out = outPath;
        free(outPath);
    }

    return out;
}

static bool gShowMainWindow = true;
static std::string gBingoCard;
static std::string gBingoChip;

std::unique_ptr<SOIS::Texture> gBingoCardTexture;
std::unique_ptr<SOIS::Texture> gBingoChipTexture;

bool FileUpdate(char const* aButtonLabel, char const* aLabel, std::string& file)
{
    bool changed = false;
    if (ImGui::Button(aButtonLabel))
    {
        file = PickImageFile();
        changed = changed || !file.empty();
    }
    ImGui::SameLine();
    return changed || ImGui::InputText(aLabel, &file);
}


struct ImageDisplay
{
    ImVec2 Dimensions;
    ImVec2 Position;
};

// https://codereview.stackexchange.com/a/70916
ImageDisplay StretchToFit(ImVec2 aImageResolution, ImVec2 aWindowResolution)
{
    float scaleHeight = aWindowResolution.y / aImageResolution.y;
    float scaleWidth = aWindowResolution.x / aImageResolution.x;
    float scale = std::min(scaleHeight, scaleWidth);
    
    auto dimensions = ImVec2(scale * aImageResolution.x, scale * aImageResolution.y);
    auto position = ImVec2(0.f, 0.f);
    
    position = ImVec2((aWindowResolution.x - dimensions.x)/2, (aWindowResolution.y - dimensions.y)/2);

    return ImageDisplay{ dimensions, position };
}

ImageDisplay GetRenderDimensions(ImVec2 aImageResolution)
{
    ImGuiIO& io = ImGui::GetIO();
    return StretchToFit(aImageResolution, io.DisplaySize);
}

void RenderBingoCard()
{
    auto& io = ImGui::GetIO();

    auto dimensions = GetRenderDimensions(ImVec2( gBingoCardTexture->Width, gBingoCardTexture->Height));
            
    ImGui::SetCursorPos(ImVec2{ dimensions.Position.x + 10, dimensions.Position.y + 10 });

    ImGui::Image((void*)gBingoCardTexture->GetTextureId(), dimensions.Dimensions/*, uv1, uv2*/);
}


std::vector<ImVec2> gChipPositions;// {ImVec2{ 10.0f, 10.0f }};
void RenderBingoChips()
{
    std::optional<size_t> toBeDeleted;
    auto& io = ImGui::GetIO();
    float scale = 100;
    
    bool isAnythingClicked = false;
    size_t i = 0;
    for (auto& chipPosition : gChipPositions)
    {
        ImGui::SetCursorPos(chipPosition);
        ImGui::IsItemClicked();
        
        ImGui::PushID((size_t)gBingoChipTexture->GetTextureId() + i);
        if (ImGui::ImageButton(gBingoChipTexture->GetTextureId(), ImVec2{ scale * 1, scale * 1 }, ImVec2{ 0,0 }, ImVec2{ 1,1 }, 0)
            || ImGui::IsItemActive())
        {
            ImGui::PopID();
            chipPosition = ImVec2{ chipPosition.x + io.MouseDelta.x, chipPosition.y + io.MouseDelta.y};

            isAnythingClicked = true;
        }
        else if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            toBeDeleted = i;
            isAnythingClicked = true;
            ImGui::PopID();
        }
        else
            ImGui::PopID();

        ++i;
    }

    if (toBeDeleted.has_value())
    {
        gChipPositions.erase(gChipPositions.begin() + *toBeDeleted);
    }
    

    if (!isAnythingClicked && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowFocused())
    {
        ImVec2 mousePosition = ImGui::GetMousePos();
        float toSubtract = scale * .5f;
        ImVec2 position{mousePosition.x - toSubtract, mousePosition.y - toSubtract };
        gChipPositions.push_back(position);
    }
}

bool gDisableFancyPoints = false;

void MainWindow(SOIS::ApplicationContext& aContext)
{
    auto& io = ImGui::GetIO();

    if (gBingoCardTexture)
        RenderBingoCard();

    if (io.KeysDown[SDL_SCANCODE_ESCAPE] && (io.KeysDownDuration[SDL_SCANCODE_ESCAPE] == 0.f))
        gShowMainWindow = !gShowMainWindow;

    if (false == gShowMainWindow)
        return;

    if (ImGui::Begin("Settings Window"))
    {
        if (FileUpdate("Open Bingo Card", "Bingo Card", gBingoCard))
            gBingoCardTexture = aContext.GetRenderer()->LoadTextureFromFile(gBingoCard);
        
        if (FileUpdate("Open Bingo Chip", "Bingo Chip", gBingoChip))
            gBingoChipTexture = aContext.GetRenderer()->LoadTextureFromFile(gBingoChip);

        if (ImGui::Button("Clear Chips"))
            gChipPositions.clear();

        if (ImGui::Button("Toggle Fancy Points"))
            gDisableFancyPoints = !gDisableFancyPoints;

        ImGui::End();
    }
    
    if (gBingoChipTexture)
        RenderBingoChips();
}

int main(int, char**)
{
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    SOIS::ApplicationInitialization();
  
    auto iniPath = GetImGuiIniPath();

    SOIS::ApplicationContextConfig config;
    config.aBlocking = false;
    config.aIniFile = iniPath.c_str();
    config.aWindowName = "SOIS Template";

    SOIS::ApplicationContext context{config};
    //SOIS::ImGuiSample sample;

    std::vector<FancyPoint> points;

    while (context.Update())
    {
        ImGui::Begin(
            "Canvas", 
            nullptr, 
            ImGuiWindowFlags_NoBackground | 
            ImGuiWindowFlags_NoBringToFrontOnFocus | 
            ImGuiWindowFlags_NoCollapse |  
            ImGuiWindowFlags_NoDecoration | 
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoMove);
        {
            static bool firstRun = true;

            if (firstRun)
            {
                points = InitPoints();
                firstRun = false;
            }

            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
            ImGui::SetWindowPos(ImVec2(0, 0));

            if (!gDisableFancyPoints)
                DrawPoints(points);

            MainWindow(context);
        }
        ImGui::End();

        UpdatePoints(points);

        //sample.Update();
    }

    return 0;
}
