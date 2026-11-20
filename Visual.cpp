#define IMGUI_DEFINE_MATH_OPERATORS
#include "Visual.hpp"
#include <cstdio>
#include <chrono>
#include <examples/example_win32_directx11/src/Globals.hpp>

// DIAG-ONLY stub — no Diag in Globals
static void EspDiag_VisualSkip(uint32_t /*reason*/, uint32_t /*addr*/, const char* /*detail*/) {}

#include <examples/example_win32_directx11/src/Fonts/Fonts.hpp>
#include "Namegun.h"
#include <Windows.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#ifdef min
#undef min
#endif
#include <examples/example_win32_directx11/Esp Line/Math/Vector/Vector2.hpp>
#include <examples/example_win32_directx11/Esp Line/Esp Ranks/RankIcons.hpp>
#ifdef max
#undef max
#endif
#include <examples/example_win32_directx11/Esp Line/Math/WordToScreen.hpp>
#include <examples/example_win32_directx11/Esp Line/Memory/Memory.hpp>
#include <examples/example_win32_directx11/Esp Line/Offsets.hpp>
#include <examples/example_win32_directx11/Esp Line/Math/TMatrix.hpp>

#include <examples/example_win32_directx11/Esp Line/Data/Data.hpp>
#include <examples/example_win32_directx11/src/Overlay/Overlay.hpp>
#include <examples/example_win32_directx11/Yorzen/Dudas/ui_settings.hpp>

namespace texture {
    extern ID3D11ShaderResourceView* custom_logo;
}

bool IsTeam = false;

namespace font
{
    inline ImFont* WeaponsIco = nullptr;
    inline ImFont* icomoon = nullptr;
    inline ImFont* lexend_bold = nullptr;
    inline ImFont* lexend_regular = nullptr;
    inline ImFont* lexend_general_bold = nullptr;
    inline ImFont* icomoon_widget = nullptr;
    inline ImFont* icomoon_widget2 = nullptr;
}

namespace {
    bool IsTrainingBotTarget(const Player& player) {
        if (player.Name.empty())
            return true;
        std::string lower = player.Name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        return lower.find("training") != std::string::npos;
    }

    static inline bool EffectsDisabled() {
        return g_Globals.General.DisableAllEffects;
    }

    static ImU32 FillBoxColorU32(bool isKnocked) {
        if (isKnocked)
            return IM_COL32(255, 0, 0, 45);
        ImVec4 col = g_Globals.Visuals.fillBoxColor.Value;
        // Ensure translucent see-through alpha tint for whichever color is selected in the UI
        float alpha = (col.w > 0.05f && col.w < 0.95f) ? col.w : 0.22f;
        return ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, alpha));
    }

    static void DrawGlowLine(ImDrawList* dl, ImVec2 a, ImVec2 b, ImColor color, float thickness, float glowRadius, float feather)
    {
        if (EffectsDisabled() || !g_Globals.Visuals.LineGlow || glowRadius <= 0.f) {
            dl->AddLine(a, b, color, thickness);
            return;
        }
        for (float i = glowRadius; i > 0.f; i -= feather) {
            const int alpha = (int)(color.Value.w * 255.f * (i / glowRadius) * 0.15f);
            dl->AddLine(a, b, IM_COL32(
                (int)(color.Value.x * 255.f), (int)(color.Value.y * 255.f),
                (int)(color.Value.z * 255.f), alpha), thickness + i * 0.12f);
        }
        dl->AddLine(a, b, color, thickness);
    }

    static void DrawGlowBoneLine(ImDrawList* dl, ImVec2 a, ImVec2 b, ImColor color, float glowRadius, float feather)
    {
        if (EffectsDisabled() || !g_Globals.Visuals.SkeletonGlow || glowRadius <= 0.f) {
            dl->AddLine(a, b, color, 1.0f);
            return;
        }
        for (float i = glowRadius; i > 0.f; i -= feather) {
            const int alpha = (int)(color.Value.w * 255.f * (i / glowRadius) * 0.12f);
            dl->AddLine(a, b, IM_COL32(
                (int)(color.Value.x * 255.f), (int)(color.Value.y * 255.f),
                (int)(color.Value.z * 255.f), alpha), 1.0f + i * 0.25f);
        }
        dl->AddLine(a, b, color, 1.0f);
    }

    static void DrawGlowText(ImDrawList* dl, ImFont* font, float font_size,
        const ImVec2& pos, ImU32 col, const char* text, float glowRadius = 0.f)
    {
        if (!dl || !text || !text[0]) return;
        const ImU32 outline = IM_COL32(0, 0, 0, 220);
        dl->AddText(font, font_size, ImVec2(pos.x + 1.f, pos.y + 1.f), outline, text);
        dl->AddText(font, font_size, pos, col, text);
    }

    static ImVec4 GetHealthColor(float healthPercentage, bool isKnocked) {
        if (isKnocked)
            return g_Globals.Visuals.KnockedEnemiesColor.Value;
        const ImVec4 Green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        const ImVec4 Yellow = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        const ImVec4 Red = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        if (healthPercentage > 0.5f)
            return ImLerp(Green, Yellow, (1.0f - healthPercentage) * 2.0f);
        else
            return ImLerp(Yellow, Red, (0.5f - healthPercentage) * 2.0f);
    }

    static bool EspIsValidScreenPos(float x, float y, float offX, float offY, int screenW, int screenH)
    {
        if (!std::isfinite(x) || !std::isfinite(y))
            return false;
        if (std::abs(x) > 100000.f || std::abs(y) > 100000.f)
            return false;
        return (x > offX - 50.f && y > offY - 50.f &&
            x < offX + (float)screenW + 50.f && y < offY + (float)screenH + 50.f);
    }
}

ImColor GetAnimatedRGBColor(float speedMultiplier = 1.0f) {
    float time = ImGui::GetTime() * speedMultiplier;
    float r = (sinf(time + 0.0f) * 0.5f) + 0.5f;
    float g = (sinf(time + 2.0f) * 0.5f) + 0.5f;
    float b = (sinf(time + 4.0f) * 0.5f) + 0.5f;
    return ImColor(r, g, b, 1.0f);
}

void DrawHealthBarVertical(ImDrawList* vList, float x, float y, float w, float h, float healthPercentage, bool isKnocked, float radius = 0.0f, float glowStrength = 6.0f, float border = 0.5f)
{
    healthPercentage = std::clamp(healthPercentage, 0.0f, 1.0f);
    float filledHeight = h * healthPercentage;
    float emptyHeight = h - filledHeight;
    ImVec2 barTopLeft = ImVec2(x, y);
    ImVec2 barBottomRight = ImVec2(x + w, y + h);
    ImVec2 fillTopLeft = ImVec2(x, y + emptyHeight);
    ImVec2 fillBottomRight = ImVec2(x + w, y + h);
    ImColor topColor, bottomColor;
    if (isKnocked) {
        ImVec4 knockCol = g_Globals.Visuals.KnockedEnemiesColor;
        topColor = bottomColor = ImColor(knockCol);
    }
    else if (healthPercentage > 0.75f) {
        topColor = ImColor(34, 112, 18, 255);
        bottomColor = ImColor(51, 160, 27, 255);
    }
    else if (healthPercentage > 0.3f) {
        topColor = ImColor(130, 110, 0, 255);
        bottomColor = ImColor(200, 180, 0, 255);
    }
    else {
        topColor = ImColor(120, 15, 10, 255);
        bottomColor = ImColor(191, 20, 12, 255);
    }
    vList->AddRectFilled(barTopLeft, barBottomRight, IM_COL32(0, 0, 0, 180), radius);
    vList->AddRectFilledMultiColor(fillTopLeft, fillBottomRight, topColor, topColor, bottomColor, bottomColor);
    if (!EffectsDisabled() && g_Globals.Visuals.BoxGlow) {
        float glowSize = 4.0f;
        for (int i = 1; i <= 2; ++i) {
            float offset = glowSize * i;
            ImU32 glowCol = ImColor(bottomColor.Value.x, bottomColor.Value.y, bottomColor.Value.z, 0.05f / i);
            vList->AddRectFilled(
                ImVec2(fillTopLeft.x - offset, fillTopLeft.y - offset),
                ImVec2(fillBottomRight.x + offset, fillBottomRight.y + offset),
                glowCol, radius);
        }
    }
    vList->AddRect(barTopLeft, barBottomRight, IM_COL32(0, 0, 0, 255), radius, 0, border);
}

void DrawGlowCorneredBox(float x, float y, float w, float h, ImColor color, float thickness = 1.5f, float glowRadius = 12.0f, float feather = 2.0f, bool fillEnabled = true, bool isKnocked = false) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;
    float newWidth = w;
    float newHeight = h;
    float newY = y;
    if (EffectsDisabled() || !g_Globals.Visuals.BoxGlow || glowRadius <= 0.f) {
        if (fillEnabled && g_Globals.Visuals.fillBox)
            drawList->AddRectFilled(ImVec2(x, newY), ImVec2(x + newWidth, newY + newHeight), FillBoxColorU32(isKnocked));
        const float lineW = newWidth / 6.0f;
        const float lineH = newHeight / 6.0f;
        drawList->AddLine(ImVec2(x, newY), ImVec2(x + lineW, newY), color, thickness);
        drawList->AddLine(ImVec2(x, newY), ImVec2(x, newY + lineH), color, thickness);
        drawList->AddLine(ImVec2(x + newWidth, newY), ImVec2(x + newWidth - lineW, newY), color, thickness);
        drawList->AddLine(ImVec2(x + newWidth, newY), ImVec2(x + newWidth, newY + lineH), color, thickness);
        drawList->AddLine(ImVec2(x, newY + newHeight), ImVec2(x + lineW, newY + newHeight), color, thickness);
        drawList->AddLine(ImVec2(x, newY + newHeight), ImVec2(x, newY + newHeight - lineH), color, thickness);
        drawList->AddLine(ImVec2(x + newWidth, newY + newHeight), ImVec2(x + newWidth - lineW, newY + newHeight), color, thickness);
        drawList->AddLine(ImVec2(x + newWidth, newY + newHeight), ImVec2(x + newWidth, newY + newHeight - lineH), color, thickness);
        return;
    }
    if (fillEnabled && g_Globals.Visuals.fillBox)
        drawList->AddRectFilled(ImVec2(x, newY), ImVec2(x + newWidth, newY + newHeight), FillBoxColorU32(isKnocked));
    for (int i = (int)glowRadius; i > 0; --i) {
        float alphaFactor = 25.0f * (1.0f - ((float)i / glowRadius));
        ImColor glowColor = isKnocked
            ? ImColor(1.f, 0.f, 0.f, alphaFactor / 255.0f)
            : ImColor(color.Value.x, color.Value.y, color.Value.z, alphaFactor / 255.0f);
        float offset = i * 0.5f;
        float sixthW = newWidth / 6.0f;
        float sixthH = newHeight / 6.0f;
        drawList->AddLine(ImVec2(x - offset, newY - offset), ImVec2(x + sixthW + offset, newY - offset), glowColor, thickness);
        drawList->AddLine(ImVec2(x - offset, newY - offset), ImVec2(x - offset, newY + sixthH + offset), glowColor, thickness);
        drawList->AddLine(ImVec2(x + newWidth + offset, newY - offset), ImVec2(x + newWidth - sixthW - offset, newY - offset), glowColor, thickness);
        drawList->AddLine(ImVec2(x + newWidth + offset, newY - offset), ImVec2(x + newWidth + offset, newY + sixthH + offset), glowColor, thickness);
        drawList->AddLine(ImVec2(x - offset, newY + newHeight + offset), ImVec2(x + sixthW + offset, newY + newHeight + offset), glowColor, thickness);
        drawList->AddLine(ImVec2(x - offset, newY + newHeight + offset), ImVec2(x - offset, newY + newHeight - sixthH - offset), glowColor, thickness);
        drawList->AddLine(ImVec2(x + newWidth + offset, newY + newHeight + offset), ImVec2(x + newWidth - sixthW - offset, newY + newHeight + offset), glowColor, thickness);
        drawList->AddLine(ImVec2(x + newWidth + offset, newY + newHeight + offset), ImVec2(x + newWidth + offset, newY + newHeight - sixthH - offset), glowColor, thickness);
    }
    float lineW = newWidth / 6.0f;
    float lineH = newHeight / 6.0f;
    drawList->AddLine(ImVec2(x, newY), ImVec2(x + lineW, newY), color, thickness);
    drawList->AddLine(ImVec2(x, newY), ImVec2(x, newY + lineH), color, thickness);
    drawList->AddLine(ImVec2(x + newWidth, newY), ImVec2(x + newWidth - lineW, newY), color, thickness);
    drawList->AddLine(ImVec2(x + newWidth, newY), ImVec2(x + newWidth, newY + lineH), color, thickness);
    drawList->AddLine(ImVec2(x, newY + newHeight), ImVec2(x + lineW, newY + newHeight), color, thickness);
    drawList->AddLine(ImVec2(x, newY + newHeight), ImVec2(x, newY + newHeight - lineH), color, thickness);
    drawList->AddLine(ImVec2(x + newWidth, newY + newHeight), ImVec2(x + newWidth - lineW, newY + newHeight), color, thickness);
    drawList->AddLine(ImVec2(x + newWidth, newY + newHeight), ImVec2(x + newWidth, newY + newHeight - lineH), color, thickness);
}

void DrawFullBox(float x, float y, float w, float h, ImColor color, float thickness = 1.5f, bool isKnocked = false) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;
    float newWidth = w;
    float newHeight = h;
    float newX = x;
    float newY = y;
    if (g_Globals.Visuals.fillBox)
        drawList->AddRectFilled(ImVec2(newX, newY), ImVec2(newX + newWidth, newY + newHeight), FillBoxColorU32(isKnocked));

    if (!EffectsDisabled() && g_Globals.Visuals.BoxGlow && g_Globals.Visuals.BoxGlowRadius > 0.f) {
        float glowRadius = g_Globals.Visuals.BoxGlowRadius;
        for (int i = (int)glowRadius; i > 0; i -= 2) {
            float alphaFactor = 20.0f * (1.0f - ((float)i / glowRadius));
            ImColor glowColor = isKnocked
                ? ImColor(1.f, 0.f, 0.f, alphaFactor / 255.0f)
                : ImColor(color.Value.x, color.Value.y, color.Value.z, alphaFactor / 255.0f);
            float offset = i * 0.4f;
            drawList->AddRect(ImVec2(newX - offset, newY - offset), ImVec2(newX + newWidth + offset, newY + newHeight + offset), glowColor, 0.9f, ImDrawFlags_None, thickness);
        }
    }

    drawList->AddRect(ImVec2(newX, newY), ImVec2(newX + newWidth, newY + newHeight), color, 0.9f, ImDrawFlags_None, thickness * 0.5f);
}

void DrawVerticalHealthBar(short CurrentHealth, short MaxHealth, ImVec2 Position, float TotalHeight, bool isKnocked = false, int GlowRadius = 10) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;
    const float HealthPercentage = std::clamp(static_cast<float>(CurrentHealth) / MaxHealth, 0.f, 1.f);
    const float FilledBarHeight = TotalHeight * HealthPercentage;
    const float BarWidth = 2.5f;
    ImVec4 HealthBarColor = GetHealthColor(HealthPercentage, isKnocked);
    ImVec4 GlowColor = HealthBarColor;
    const ImU32 BarColorU32 = ImGui::ColorConvertFloat4ToU32(HealthBarColor);
    drawList->AddRectFilled(ImVec2(Position.x, Position.y), ImVec2(Position.x + BarWidth, Position.y + TotalHeight),
        isKnocked ? IM_COL32(80, 0, 0, 180) : IM_COL32(0, 0, 0, 128));
    if (!EffectsDisabled() && g_Globals.Visuals.BoxGlow) {
        for (int i = GlowRadius; i > 0; --i) {
            const float Alpha = 15.0f * (1.0f - ((float)i / GlowRadius));
            drawList->AddRectFilled(
                ImVec2(Position.x - i, Position.y + (TotalHeight - FilledBarHeight) - i),
                ImVec2(Position.x + BarWidth + i, Position.y + TotalHeight + i),
                IM_COL32((int)(GlowColor.x * 255), (int)(GlowColor.y * 255), (int)(GlowColor.z * 255), (int)Alpha));
        }
    }
    const float fillTop = isKnocked ? Position.y : (Position.y + (TotalHeight - FilledBarHeight));
    const float fillH = isKnocked ? TotalHeight : FilledBarHeight;
    drawList->AddRectFilled(ImVec2(Position.x, fillTop), ImVec2(Position.x + BarWidth, fillTop + fillH), BarColorU32);
}

inline bool IsOnScreen(const Vector2& screenPos, int screenWidth, int screenHeight) {
    return screenPos.X >= 0 && screenPos.Y >= 0 && screenPos.X <= screenWidth && screenPos.Y <= screenHeight;
}

namespace ESP {

    static float RadarGetCameraYaw()
    {
        const auto& m = g_Globals.EspConfig.ViewMatrix;
        const float fx = m.m02;
        const float fz = m.m22;
        return atan2f(fx, fz);
    }

    static ImU32 RadarCol4(const float c[4], float aMul = 1.f)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3] * aMul));
    }

    static Vector3 RadarGetHead(const Player& p)
    {
        if (p.Head.X != 0.f || p.Head.Y != 0.f || p.Head.Z != 0.f)
            return p.Head;
        return p.Hip;
    }

    static ImVec4 RadarHealthColor(float healthPercentage, bool isKnocked)
    {
        if (isKnocked)
            return g_Globals.Visuals.KnockedEnemiesColor.Value;
        const ImVec4 Green(0.0f, 1.0f, 0.0f, 1.0f);
        const ImVec4 Yellow(1.0f, 1.0f, 0.0f, 1.0f);
        const ImVec4 Red(1.0f, 0.0f, 0.0f, 1.0f);
        healthPercentage = std::clamp(healthPercentage, 0.f, 1.f);
        if (healthPercentage > 0.5f)
            return ImLerp(Green, Yellow, (1.0f - healthPercentage) * 2.0f);
        return ImLerp(Yellow, Red, (0.5f - healthPercentage) * 2.0f);
    }

    static void DrawAnimatedDetectText(ImDrawList* dl, ImFont* font, float font_size,
        const ImVec2& textPos, const char* buffer, bool hasEnemies)
    {
        if (!font || !buffer || !buffer[0]) return;

        const ImVec4 redBase(0.82f, 0.12f, 0.16f, 1.0f);
        const ImVec4 greenBase(0.02f, 0.82f, 0.40f, 1.0f);
        const ImVec4 whiteHi(1.0f, 1.0f, 1.0f, 1.0f);
        const float t = static_cast<float>(ImGui::GetTime());
        const float phase = std::fmod(t * (hasEnemies ? 1.15f : 0.9f), 1.0f);
        const ImVec4 base = hasEnemies ? redBase : greenBase;

        const ImVec2 textSize = font->CalcTextSizeA(font_size, FLT_MAX, -1.0f, buffer, nullptr, nullptr);
        float charX = textPos.x;
        for (const char* p = buffer; *p; ++p) {
            char ch[2] = { *p, '\0' };
            const ImVec2 chSize = font->CalcTextSizeA(font_size, FLT_MAX, -1.0f, ch, ch + 1, nullptr);
            const float centerX = charX + chSize.x * 0.5f;
            const float denom = (textSize.x > 1.0f) ? textSize.x : 1.0f;
            const float rel = (centerX - textPos.x) / denom;
            float g = std::fmod(rel + phase, 1.0f);
            float band = 1.0f - std::fabs(g - 0.5f) * 2.0f;
            band = std::clamp(band, 0.0f, 1.0f);
            const float whiteMix = band * 0.85f;
            ImVec4 col;
            col.x = base.x + (whiteHi.x - base.x) * whiteMix;
            col.y = base.y + (whiteHi.y - base.y) * whiteMix;
            col.z = base.z + (whiteHi.z - base.z) * whiteMix;
            col.w = 1.0f;
            const ImU32 glowCol = IM_COL32(0, 0, 0, 200);
            const ImU32 mainCol = ImGui::ColorConvertFloat4ToU32(col);
            const ImVec2 chPos(charX, textPos.y);
            dl->AddText(font, font_size, ImVec2(chPos.x + 1.0f, chPos.y + 1.0f), glowCol, ch);
            dl->AddText(font, font_size, chPos, mainCol, ch);
            charX += chSize.x;
        }
    }

    static bool EspIsValidScreenPos(const Vector2& sp, int w, int h)
    {
        if (!std::isfinite(sp.X) || !std::isfinite(sp.Y)) return false;
        if (sp.X < 1.f && sp.Y < 1.f) return false;
        const float margin = 4000.f;
        if (sp.X < -margin || sp.Y < -margin || sp.X >(float)w + margin || sp.Y >(float)h + margin)
            return false;
        return true;
    }

    void DrawRadar()
    {
        if (!ui.esp.RadarEnabled) return;
        if (g_Globals.GameMode.MatchStatus != 1) return;

        if (!g_Globals.EspConfig.Matrix) {
            EspDiag_VisualSkip(5, 0, "matrix_false_radar_soft");
        }

        const float detectionRange = (float)ui.esp.RadarRange;
        const float minimapRadius = std::clamp(ui.esp.RadarSize, 35.f, 120.f);
        const float margin = 14.f;
        const float windowSize = minimapRadius * 2.f + 12.f;

        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        const float sw = (displaySize.x > 0.f) ? displaySize.x : (float)GetSystemMetrics(SM_CXSCREEN);
        const float sh = (displaySize.y > 0.f) ? displaySize.y : (float)GetSystemMetrics(SM_CYSCREEN);

        ImVec2 radarPos;
        if (ui.esp.RadarPosX < 0.f || ui.esp.RadarPosY < 0.f)
            radarPos = ImVec2(margin, sh - windowSize - margin * 2.f);
        else
            radarPos = ImVec2(ui.esp.RadarPosX, ui.esp.RadarPosY);

        radarPos.x = ImClamp(radarPos.x, 0.f, sw - windowSize);
        radarPos.y = ImClamp(radarPos.y, 0.f, sh - windowSize);

        ImGui::SetNextWindowPos(radarPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(windowSize, windowSize));
        ImGui::SetNextWindowBgAlpha(0.f);
        if (ImGui::Begin("##RadarDrag", nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoFocusOnAppearing))
        {
            ImGui::InvisibleButton("##radar_drag", ImVec2(windowSize, windowSize));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                radarPos.x += ImGui::GetIO().MouseDelta.x;
                radarPos.y += ImGui::GetIO().MouseDelta.y;
                radarPos.x = ImClamp(radarPos.x, 0.f, sw - windowSize);
                radarPos.y = ImClamp(radarPos.y, 0.f, sh - windowSize);
            }
        }
        ImGui::End();

        ui.esp.RadarPosX = radarPos.x;
        ui.esp.RadarPosY = radarPos.y;

        const ImVec2 center(radarPos.x + minimapRadius + 6.f,
            radarPos.y + minimapRadius + 6.f);

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const float* ac = ui.esp.RadarColor;
        const ImU32 accent = RadarCol4(ac);
        const ImU32 grid = RadarCol4(ac, 0.22f);
        const ImU32 bg = IM_COL32(8, 6, 8, 200);

        dl->AddCircleFilled(center, minimapRadius, bg, 64);
        for (int i = 1; i <= 3; ++i)
            dl->AddCircle(center, minimapRadius * (float)i / 3.f, grid, 64, 0.7f);

        dl->AddLine(ImVec2(center.x - minimapRadius, center.y), ImVec2(center.x + minimapRadius, center.y), grid, 1.0f);
        dl->AddLine(ImVec2(center.x, center.y - minimapRadius), ImVec2(center.x, center.y + minimapRadius), grid, 1.0f);

        for (int i = 4; i >= 1; --i)
            dl->AddCircle(center, minimapRadius + (float)i * 0.7f, RadarCol4(ac, 0.07f * i), 64, 1.8f);
        dl->AddCircle(center, minimapRadius, accent, 64, 1.5f);

        const auto& m = g_Globals.EspConfig.ViewMatrix;
        float cameraYaw = -atan2f(m.m20, m.m22);
        float cosYaw = cosf(cameraYaw);
        float sinYaw = sinf(cameraYaw);

        static const char* labels[] = { "N", "E", "S", "W" };
        for (int i = 0; i < 4; ++i)
        {
            float angle = -IM_PI * 0.5f + (float)i * IM_PI * 0.5f + cameraYaw;
            ImVec2 p(center.x + cosf(angle) * (minimapRadius - 10.f),
                center.y + sinf(angle) * (minimapRadius - 10.f));
            ImVec2 ts = ImGui::CalcTextSize(labels[i]);
            dl->AddText(ImVec2(p.x - ts.x * 0.5f, p.y - ts.y * 0.5f), accent, labels[i]);
        }

        if (ui.esp.RadarSweep)
        {
            const float t = (float)ImGui::GetTime();
            const float sweepAng = fmodf(t * ui.esp.RadarSweepSpeed, IM_PI * 2.f);
            ImVec2 tip(center.x + cosf(sweepAng) * minimapRadius,
                center.y + sinf(sweepAng) * minimapRadius);
            for (int i = 3; i >= 1; --i)
                dl->AddLine(center, tip, RadarCol4(ac, 0.12f * i), 1.0f + (float)i * 0.7f);
            dl->AddLine(center, tip, accent, 1.5f);
        }

        dl->AddCircleFilled(center, 5.0f, accent, 14);
        dl->AddCircle(center, 7.0f, RadarCol4(ac, 0.40f), 14, 1.3f);
        {
            const float arrowLen = 18.f;
            ImVec2 tip(center.x, center.y - arrowLen);
            dl->AddLine(center, tip, accent, 2.4f);
            float ang = -IM_PI * 0.5f;
            ImVec2 p1(tip.x + cosf(ang + 2.55f) * 7.5f, tip.y + sinf(ang + 2.55f) * 7.5f);
            ImVec2 p2(tip.x + cosf(ang - 2.55f) * 7.5f, tip.y + sinf(ang - 2.55f) * 7.5f);
            dl->AddTriangleFilled(tip, p1, p2, accent);
        }

        const Vector3 cam = g_Globals.EspConfig.MainCamera;
        int detected = 0;
        const float scale = minimapRadius / detectionRange;

        std::shared_lock radarLock(g_Globals.EspConfig.EntitiesMutex);
        for (const auto& kv : g_Globals.EspConfig.Entities)
        {
            const Player& e = kv.second;
            if (!e.IsKnown || e.IsDead) continue;
            if (e.IsTeam == Bool3::True && g_Globals.GameMode.MatchStatus == 1) continue;
            if (!g_Globals.EspConfig.showOnlyVisible && !e.IsVisible) continue;

            Vector3 head = RadarGetHead(e);
            float dist = Vector3::Distance(cam, head);
            if (dist > detectionRange || dist < 0.5f) continue;
            ++detected;

            float rx = head.X - cam.X;
            float rz = head.Z - cam.Z;
            float rotatedX = rx * cosYaw - rz * sinYaw;
            float rotatedY = rx * sinYaw + rz * cosYaw;

            ImVec2 pos(center.x + rotatedX * scale, center.y - rotatedY * scale);
            float dx = pos.x - center.x, dy = pos.y - center.y;
            if (sqrtf(dx * dx + dy * dy) > minimapRadius - 1.f)
                continue;

            ImU32 dot = e.IsKnocked ? IM_COL32(255, 220, 40, 230) : accent;
            dl->AddCircleFilled(pos, ui.esp.RadarEnemyDotSize, dot, 10);
        }

        if (ui.esp.RadarShowCount)
        {
            char buf[40];
            snprintf(buf, sizeof(buf), "Detected: %d", detected);
            ImFont* font = ImGui::GetFont();
            const float font_size = font ? font->FontSize * 0.90f : 13.f;
            const ImVec2 textSize = font->CalcTextSizeA(font_size, FLT_MAX, -1.0f, buf);
            ImVec2 tp(center.x - textSize.x * 0.5f, center.y - minimapRadius - textSize.y - 4.f);
            DrawAnimatedDetectText(dl, font, font_size, tp, buf, detected > 0);
        }
    }

    void DrawDetectionArrows()
    {
        if (!ui.esp.DetectArrowEnabled) return;
        // Alive whole match incl. death + spectator; off only in lobby
        if (g_Globals.GameMode.MatchStatus != 1) return;
        // Need some camera basis for world angles; use last matrix if current is off
        if (!g_Globals.EspConfig.Matrix && !g_Globals.EspConfig.ViewMatrix.m00)
            return;

        const float range = (float)ui.esp.DetectArrowRange;
        const float baseR = ui.esp.DetectArrowRadius;
        const float pulse = ui.esp.DetectArrowPulse
            ? (0.85f + 0.15f * sinf((float)ImGui::GetTime() * ui.esp.DetectArrowPulseSpeed))
            : 1.f;
        const float ringR = baseR * pulse;

        const int w = g_Globals.EspConfig.Width;
        const int h = g_Globals.EspConfig.Height;
        if (w < 10 || h < 10) return;

        const bool outside = (g_Globals.General.OverlayMode == 1);
        const float offX = outside ? (float)FWork::Overlay::GetEmuLeft() : 0.f;
        const float offY = outside ? (float)FWork::Overlay::GetEmuTop() : 0.f;

        const ImVec2 screenCenter((float)w * 0.5f + offX, (float)h * 0.5f + offY);
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const float* ac = ui.esp.DetectArrowColor;
        const float infoScale = ui.esp.DetectArrowInfoScale;
        // Gun icon scale (menu: DetectArrowWeaponIconScale, default ~1.35)
        const float iconScale = (ui.esp.DetectArrowWeaponIconScale > 0.1f)
            ? ui.esp.DetectArrowWeaponIconScale : 1.35f;

        // Camera axes from view matrix (row-major Unity style used in project)
        const auto& vm = g_Globals.EspConfig.ViewMatrix;
        const Vector3 cam = g_Globals.EspConfig.MainCamera;
        // Right / Up / Forward from view matrix columns commonly used with this W2S
        Vector3 camRight(vm.m00, vm.m10, vm.m20);
        Vector3 camUp(vm.m01, vm.m11, vm.m21);
        Vector3 camFwd(vm.m02, vm.m12, vm.m22);

        auto len3 = [](const Vector3& v) {
            return sqrtf(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
            };
        auto norm3 = [&](Vector3& v) {
            float L = len3(v);
            if (L > 1e-5f) { v.X /= L; v.Y /= L; v.Z /= L; }
            };
        norm3(camRight); norm3(camUp); norm3(camFwd);

        bool drewAny = false;

        for (const auto& kv : g_Globals.EspConfig.Entities) {
            const Player& e = kv.second;
            if (!e.IsKnown || e.IsDead) continue;
            if (e.IsTeam == Bool3::True && g_Globals.GameMode.MatchStatus == 1) continue;
            // ===== WUKONG MODE FILTER (Detection Arrows) =====
            // Wukong OFF = visible only ; Wukong ON = show all
            if (!g_Globals.EspConfig.showOnlyVisible && !e.IsVisible) continue;
            // =================================================

            Vector3 head = RadarGetHead(e);
            if (head.X == 0.f && head.Y == 0.f && head.Z == 0.f) continue;

            float dist3d = Vector3::Distance(cam, head);
            if (dist3d > range || dist3d < 1.f) continue;

            // World-relative direction (works for behind camera too)
            Vector3 rel(head.X - cam.X, head.Y - cam.Y, head.Z - cam.Z);
            const float rdot = rel.X * camRight.X + rel.Y * camRight.Y + rel.Z * camRight.Z;
            const float udot = rel.X * camUp.X + rel.Y * camUp.Y + rel.Z * camUp.Z;
            const float fdot = rel.X * camFwd.X + rel.Y * camFwd.Y + rel.Z * camFwd.Z;

            // Screen direction from camera basis (Y flipped for screen space)
            ImVec2 dir(rdot, -udot);
            float dlen = sqrtf(dir.x * dir.x + dir.y * dir.y);
            // If almost centered on axes (rare), push by forward side
            if (dlen < 1e-3f) {
                dir.x = (fdot < 0.f) ? 0.f : 0.f;
                dir.y = (fdot < 0.f) ? 1.f : -1.f;
                dlen = 1.f;
            }
            dir.x /= dlen; dir.y /= dlen;

            // On-screen only if IN FRONT of camera AND W2S lands inside viewport
            bool hideArrow = false;
            if (fdot > 0.35f) {
                Vector2 sp = W2S::WorldToScreen(vm, head, w, h);
                if (EspIsValidScreenPos(sp, w, h)) {
                    const float edgePad = 12.f;
                    if (sp.X >= edgePad && sp.X <= (float)w - edgePad &&
                        sp.Y >= edgePad && sp.Y <= (float)h - edgePad) {
                        hideArrow = true; // clearly visible on screen
                    }
                }
            }
            if (hideArrow)
                continue;

            ImVec2 arrowPos(screenCenter.x + dir.x * ringR, screenCenter.y + dir.y * ringR);
            float ang = atan2f(dir.y, dir.x);
            const float asz = ui.esp.DetectArrowSize;
            ImVec2 p0(arrowPos.x + cosf(ang) * asz, arrowPos.y + sinf(ang) * asz);
            ImVec2 p1(arrowPos.x + cosf(ang + 2.55f) * asz * 0.55f, arrowPos.y + sinf(ang + 2.55f) * asz * 0.55f);
            ImVec2 p2(arrowPos.x + cosf(ang - 2.55f) * asz * 0.55f, arrowPos.y + sinf(ang - 2.55f) * asz * 0.55f);

            ImU32 mainCol = e.IsKnocked ? IM_COL32(255, 220, 40, 255) : RadarCol4(ac);
            for (int g = 2; g >= 1; --g)
                dl->AddTriangleFilled(p0, p1, p2, RadarCol4(ac, 0.16f * g));
            dl->AddTriangleFilled(p0, p1, p2, mainCol);
            dl->AddTriangle(p0, p1, p2, IM_COL32(0, 0, 0, 200), 1.3f);
            drewAny = true;

            if (!ui.esp.DetectArrowShowInfo)
                continue;

            float fontSz = ImGui::GetFontSize() * 0.78f * infoScale;
            float yOff = -fontSz - 2.f;

            auto addLine = [&](const char* txt, ImU32 col, ImFont* useFont = nullptr, float useSize = 0.f) {
                ImFont* f = useFont ? useFont : ImGui::GetFont();
                float fs = useSize > 0.f ? useSize : fontSz;
                ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0.f, txt);
                ImVec2 tp(arrowPos.x - ts.x * 0.5f + dir.x * (asz + 6.f),
                    arrowPos.y - ts.y * 0.5f + dir.y * (asz + 6.f) + yOff);
                dl->AddText(f, fs, ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 160), txt);
                dl->AddText(f, fs, tp, col, txt);
                yOff += fs + 1.f;
                };

            if (ui.esp.DetectArrowShowName && !e.Name.empty()) {
                ImFont* nameF = FWork::Fonts::NotoSansRegular ? FWork::Fonts::NotoSansRegular
                    : (FWork::Fonts::ArialUnicode ? FWork::Fonts::ArialUnicode : ImGui::GetFont());
                addLine(e.Name.c_str(), IM_COL32(240, 240, 245, 255), nameF, fontSz);
            }

            if (ui.esp.DetectArrowShowHealth) {
                char hb[24];
                int hpShow = (int)e.Health;
                if (hpShow < 0) hpShow = 0;
                if (hpShow > 200) hpShow = 200;
                const float hpPct = std::clamp(static_cast<float>(hpShow) / 200.f, 0.f, 1.f);
                snprintf(hb, sizeof(hb), "%d%%", hpShow);
                ImVec4 hc = RadarHealthColor(hpPct, e.IsKnocked);
                addLine(hb, ImGui::ColorConvertFloat4ToU32(hc));
            }

            if (ui.esp.DetectArrowShowDist) {
                char db[24];
                snprintf(db, sizeof(db), "%.0fm", dist3d);
                addLine(db, IM_COL32(200, 200, 210, 230));
            }

            // Weapon ICON — larger, skip broken/empty glyphs
            if (ui.esp.DetectArrowShowWeapon && Namegun::ShouldShowWeapon(e.Gun) && Namegun::HasIcon(e.Gun)) {
                std::string iconText = Namegun::GetGunIcon(e.Gun);
                // Filter glitched / placeholder icons (single X-like junk)
                bool badIcon = iconText.empty();
                if (!badIcon) {
                    // pure "?" or "X" single char often means missing glyph
                    if (iconText.size() <= 2) {
                        unsigned char c0 = (unsigned char)iconText[0];
                        // skip ASCII X/x/? only — private-use icons are multi-byte UTF-8
                        if (iconText == "X" || iconText == "x" || iconText == "?" || iconText == "Unknown")
                            badIcon = true;
                    }
                    if (iconText == "Unknown") badIcon = true;
                }
                if (!badIcon) {
                    ImFont* iconFont = FWork::Fonts::IconWeapon ? FWork::Fonts::IconWeapon : ImGui::GetFont();
                    float ifs = fontSz * iconScale;
                    addLine(iconText.c_str(), IM_COL32(230, 235, 255, 250), iconFont, ifs);
                }
            }
        }

        if (drewAny && ui.esp.DetectArrowShowRing)
            dl->AddCircle(screenCenter, ringR, RadarCol4(ac, 0.20f), 64, 1.0f);
    }

    namespace
    {
        struct LocalBones
        {
            Vector3 Root{}, Hip{}, Head{}, Neck{};
            Vector3 LShoulder{}, RShoulder{}, LElbow{}, RElbow{};
            Vector3 LWrist{}, RWrist{}, LHand{}, RHand{};
            Vector3 Groin{}, LAnkle{}, RAnkle{}, LFoot{}, RFoot{};
            bool valid = false;
        };

        static LocalBones s_frozenBones;
        static Vector3    s_frozenCenter = Vector3::Zero();
        static bool       s_effectActive = false;
        static bool       s_wasActive = false;
        static uint32_t   s_frozenLocalPlayer = 0;

        static void ClearNetLagEffect()
        {
            s_effectActive = false;
            s_frozenBones = LocalBones{};
            s_frozenBones.valid = false;
            s_frozenCenter = Vector3::Zero();
            s_frozenLocalPlayer = 0;
        }

        bool ReadLocalBones(uint32_t localPlayer, LocalBones& out)
        {
            if (localPlayer < 0x10000) return false;

            bool isDead = false;
            if (!Mem.Read(localPlayer + Offsets::Player_IsDead, isDead) || isDead)
                return false;

            auto ReadBone = [&](uintptr_t offset, Vector3& bone) -> bool
                {
                    uint32_t ptr = 0;
                    if (!Mem.Read(localPlayer + offset, ptr) || ptr < 0x10000) return false;
                    Vector3 tmp{};
                    if (!TransformUtils::GetNodePosition(ptr, tmp) || tmp == Vector3::Zero()) return false;
                    if (!std::isfinite(tmp.X) || !std::isfinite(tmp.Y) || !std::isfinite(tmp.Z)) return false;
                    if (std::fabs(tmp.X) > 8000.f || std::fabs(tmp.Y) > 8000.f || std::fabs(tmp.Z) > 8000.f) return false;
                    bone = tmp;
                    return true;
                };

            LocalBones b{};
            bool ok = true;
            ok &= ReadBone(Offsets::Bones::Root, b.Root);
            ok &= ReadBone(Offsets::Bones::Hip, b.Hip);
            ok &= ReadBone(Offsets::Bones::Head, b.Head);
            ReadBone(Offsets::Bones::Neck, b.Neck);
            ReadBone(Offsets::Bones::LeftShoulder, b.LShoulder);
            ReadBone(Offsets::Bones::RightShoulder, b.RShoulder);
            ReadBone(Offsets::Bones::LeftElbow, b.LElbow);
            ReadBone(Offsets::Bones::RightElbow, b.RElbow);
            ReadBone(Offsets::Bones::LeftWrist, b.LWrist);
            ReadBone(Offsets::Bones::RightWrist, b.RWrist);
            ReadBone(Offsets::Bones::LeftHand, b.LHand);
            ReadBone(Offsets::Bones::RightHand, b.RHand);
            ReadBone(Offsets::Bones::Groin, b.Groin);
            ReadBone(Offsets::Bones::LeftAnkle, b.LAnkle);
            ReadBone(Offsets::Bones::RightAnkle, b.RAnkle);
            ReadBone(Offsets::Bones::LeftFoot, b.LFoot);
            ReadBone(Offsets::Bones::RightFoot, b.RFoot);

            if (!ok || b.Root == Vector3::Zero()) return false;
            b.valid = true;
            out = b;
            return true;
        }

        void FlattenToGround(LocalBones& b, Vector3& center)
        {
            float groundY = b.Root.Y;
            if (b.LFoot != Vector3::Zero()) groundY = (std::min)(groundY, b.LFoot.Y);
            if (b.RFoot != Vector3::Zero()) groundY = (std::min)(groundY, b.RFoot.Y);
            if (b.LAnkle != Vector3::Zero()) groundY = (std::min)(groundY, b.LAnkle.Y);
            if (b.RAnkle != Vector3::Zero()) groundY = (std::min)(groundY, b.RAnkle.Y);
            groundY -= 0.02f;

            float dy = groundY - b.Root.Y;
            auto Shift = [&](Vector3& v) { if (v != Vector3::Zero()) v.Y += dy; };

            Shift(b.Root); Shift(b.Hip); Shift(b.Head); Shift(b.Neck);
            Shift(b.LShoulder); Shift(b.RShoulder); Shift(b.LElbow); Shift(b.RElbow);
            Shift(b.LWrist); Shift(b.RWrist); Shift(b.LHand); Shift(b.RHand);
            Shift(b.Groin); Shift(b.LAnkle); Shift(b.RAnkle); Shift(b.LFoot); Shift(b.RFoot);

            if (b.LFoot != Vector3::Zero() && b.RFoot != Vector3::Zero())
            {
                center.X = (b.LFoot.X + b.RFoot.X) * 0.5f;
                center.Y = groundY;
                center.Z = (b.LFoot.Z + b.RFoot.Z) * 0.5f;
            }
            else
            {
                center = b.Root;
                center.Y = groundY;
            }
        }

        inline bool IsGoodScreen(const ImVec2& p, int w, int h)
        {
            if (!std::isfinite(p.x) || !std::isfinite(p.y)) return false;
            if (p.x < -800.f || p.y < -800.f) return false;
            if (p.x > (float)w + 800.f || p.y > (float)h + 800.f) return false;
            if (p.x < 1.f && p.y < 1.f) return false;
            return true;
        }

        inline bool IsSaneSegment(const ImVec2& a, const ImVec2& b, int w, int h)
        {
            float dx = a.x - b.x;
            float dy = a.y - b.y;
            float len2 = dx * dx + dy * dy;
            float maxLen = (float)(std::max)(w, h) * 0.85f;
            return len2 < maxLen * maxLen;
        }

        void DrawGroundSeal(ImDrawList* dl, const Vector3& center, float worldRadius,
            float angle, const Matrix4x4& vm, int w, int h,
            float offX, float offY, ImU32 col, bool glow)
        {
            auto Project = [&](float x, float y, float z) -> ImVec2
                {
                    Vector2 s = W2S::WorldToScreen(vm, Vector3(x, y, z), w, h);
                    if (!std::isfinite(s.X) || !std::isfinite(s.Y)) return ImVec2(-1.f, -1.f);
                    if (s.X < 1.f && s.Y < 1.f) return ImVec2(-1.f, -1.f);
                    return ImVec2(s.X + offX, s.Y + offY);
                };

            const float cy = center.Y;
            const int SEG = 48;

            auto WPt = [&](float a, float rad) -> Vector3
                {
                    return Vector3(
                        center.X + cosf(a) * rad,
                        cy,
                        center.Z + sinf(a) * rad);
                };

            auto DrawRing = [&](float rad, float thick, ImU32 c)
                {
                    ImVec2 prev;
                    bool hasPrev = false;
                    for (int i = 0; i <= SEG; ++i)
                    {
                        float a = angle + (i / (float)SEG) * 6.2831853f;
                        Vector3 wp = WPt(a, rad);
                        ImVec2 sp = Project(wp.X, wp.Y, wp.Z);
                        if (IsGoodScreen(sp, w, h))
                        {
                            if (hasPrev && IsSaneSegment(prev, sp, w, h))
                                dl->AddLine(prev, sp, c, thick);
                            prev = sp;
                            hasPrev = true;
                        }
                        else
                        {
                            hasPrev = false;
                        }
                    }
                };

            if (glow)
            {
                for (int g = 3; g >= 1; --g)
                {
                    float expand = g * 0.08f * worldRadius;
                    ImU32 gcol = (col & 0x00FFFFFF) | ((20 + g * 15) << 24);
                    DrawRing(worldRadius + expand, 1.6f, gcol);
                }
            }

            DrawRing(worldRadius, 2.3f, col);
            DrawRing(worldRadius * 0.82f, 1.6f, col);
            DrawRing(worldRadius * 0.58f, 1.4f, col);
            DrawRing(worldRadius * 0.32f, 1.2f, col);

            for (int i = 0; i < 24; ++i)
            {
                float a = angle + (i / 24.f) * 6.2831853f;
                Vector3 p1 = WPt(a, worldRadius * 0.92f);
                Vector3 p2 = WPt(a, worldRadius * 1.06f);
                ImVec2 s1 = Project(p1.X, p1.Y, p1.Z);
                ImVec2 s2 = Project(p2.X, p2.Y, p2.Z);
                if (IsGoodScreen(s1, w, h) && IsGoodScreen(s2, w, h) && IsSaneSegment(s1, s2, w, h))
                    dl->AddLine(s1, s2, col, 1.3f);
            }

            for (int i = 0; i < 6; ++i)
            {
                float a1 = angle + (i / 6.f) * 6.2831853f;
                float a2 = angle + ((i + 2) / 6.f) * 6.2831853f;
                Vector3 p1 = WPt(a1, worldRadius * 0.82f);
                Vector3 p2 = WPt(a2, worldRadius * 0.82f);
                ImVec2 s1 = Project(p1.X, p1.Y, p1.Z);
                ImVec2 s2 = Project(p2.X, p2.Y, p2.Z);
                if (IsGoodScreen(s1, w, h) && IsGoodScreen(s2, w, h) && IsSaneSegment(s1, s2, w, h))
                    dl->AddLine(s1, s2, col, 1.1f);
            }

            ImVec2 pts[5];
            bool ok[5] = {};
            for (int i = 0; i < 5; ++i)
            {
                float a = angle - 1.5707963f + (i * 2 * 6.2831853f / 5.f);
                Vector3 wp = WPt(a, worldRadius * 0.55f);
                pts[i] = Project(wp.X, wp.Y, wp.Z);
                ok[i] = IsGoodScreen(pts[i], w, h);
            }
            for (int i = 0; i < 5; ++i)
            {
                int j = (i + 2) % 5;
                if (ok[i] && ok[j] && IsSaneSegment(pts[i], pts[j], w, h))
                    dl->AddLine(pts[i], pts[j], col, 1.8f);
            }
            for (int i = 0; i < 5; ++i)
            {
                if (ok[i])
                    dl->AddCircleFilled(pts[i], 2.8f, col, 10);
            }
        }

        void DrawFrozenSkeleton(ImDrawList* dl, const LocalBones& b, const Matrix4x4& vm,
            int w, int h, float offX, float offY, ImU32 col, bool glow)
        {
            auto ToScreen = [&](const Vector3& world) -> ImVec2
                {
                    Vector2 s = W2S::WorldToScreen(vm, world, w, h);
                    if (!std::isfinite(s.X) || !std::isfinite(s.Y)) return ImVec2(-1.f, -1.f);
                    if (s.X < 1.f && s.Y < 1.f) return ImVec2(-1.f, -1.f);
                    return ImVec2(s.X + offX, s.Y + offY);
                };

            auto Line = [&](const Vector3& a, const Vector3& b)
                {
                    if (a == Vector3::Zero() || b == Vector3::Zero()) return;
                    ImVec2 p1 = ToScreen(a);
                    ImVec2 p2 = ToScreen(b);
                    if (!IsGoodScreen(p1, w, h) || !IsGoodScreen(p2, w, h)) return;
                    if (!IsSaneSegment(p1, p2, w, h)) return;

                    if (glow)
                    {
                        for (int g = 2; g >= 1; --g)
                        {
                            ImU32 gcol = (col & 0x00FFFFFF) | ((30 + g * 20) << 24);
                            dl->AddLine(p1, p2, gcol, 1.0f + g * 1.4f);
                        }
                    }
                    dl->AddLine(p1, p2, col, 1.6f);
                };

            Line(b.Head, b.Neck);
            Line(b.Neck, b.Hip);
            Line(b.Hip, b.Groin);

            Line(b.Neck, b.LShoulder);
            Line(b.LShoulder, b.LElbow);
            Line(b.LElbow, b.LWrist);
            Line(b.LWrist, b.LHand);

            Line(b.Neck, b.RShoulder);
            Line(b.RShoulder, b.RElbow);
            Line(b.RElbow, b.RWrist);
            Line(b.RWrist, b.RHand);

            Line(b.Groin, b.LAnkle);
            Line(b.LAnkle, b.LFoot);
            Line(b.Groin, b.RAnkle);
            Line(b.RAnkle, b.RFoot);

            if (b.Head != Vector3::Zero())
            {
                ImVec2 hp = ToScreen(b.Head);
                if (IsGoodScreen(hp, w, h))
                {
                    float headR = 6.0f;
                    if (glow)
                    {
                        for (int g = 2; g >= 1; --g)
                        {
                            ImU32 gcol = (col & 0x00FFFFFF) | ((25 + g * 18) << 24);
                            dl->AddCircle(hp, headR + g * 2.0f, gcol, 16, 1.2f);
                        }
                    }
                    dl->AddCircle(hp, headR, col, 16, 1.6f);
                }
            }
        }
    }

    void DrawNetLagOccultEffect()
    {
        try
        {
            const bool lagFlagsOn =
                g_Globals.Misc.FlushLagEnabled ||
                g_Globals.Misc.FakeLagEnabled ||
                g_Globals.Misc.GhostHack ||
                g_Globals.Misc.TellKillEnabled;

            const bool matchOk = FWork::Data::IsMatchActive() && g_Globals.EspConfig.Matrix;

            uint32_t trueLocal = 0;
            {
                if (Offsets::Il2Cpp != 0)
                {
                    uint32_t baseGameFacade = Mem.ReadS<uint32_t>(Offsets::Il2Cpp + Offsets::InitBase);
                    if (baseGameFacade)
                    {
                        uint32_t gameFacade = Mem.ReadS<uint32_t>(baseGameFacade);
                        if (gameFacade)
                        {
                            uint32_t staticGameFacade = Mem.ReadS<uint32_t>(gameFacade + Offsets::StaticClass);
                            if (staticGameFacade)
                            {
                                uint32_t currentGame = Mem.ReadS<uint32_t>(staticGameFacade);
                                if (currentGame)
                                {
                                    uint32_t currentMatch = Mem.ReadS<uint32_t>(currentGame + Offsets::CurrentMatch);
                                    if (currentMatch)
                                        trueLocal = Mem.ReadS<uint32_t>(currentMatch + Offsets::LocalPlayer);
                                }
                            }
                        }
                    }
                }
            }

            bool blockEffect = false;
            if (trueLocal < 0x10000)
            {
                blockEffect = true;
            }
            else
            {
                bool dead = false;
                if (Mem.Read(trueLocal + Offsets::Player_IsDead, dead) && dead)
                    blockEffect = true;

                uint32_t obs = Mem.ReadS<uint32_t>(trueLocal + Offsets::CurrentObserver);
                if (obs != 0)
                    blockEffect = true;
            }

            if (!lagFlagsOn || !matchOk || blockEffect)
            {
                if (s_effectActive || s_wasActive)
                    ClearNetLagEffect();
                s_wasActive = false;
                return;
            }

            if (lagFlagsOn && !s_wasActive)
            {
                LocalBones live{};
                if (ReadLocalBones(trueLocal, live))
                {
                    FlattenToGround(live, s_frozenCenter);
                    s_frozenBones = live;
                    s_frozenLocalPlayer = trueLocal;
                    s_effectActive = true;
                }
                else
                {
                    ClearNetLagEffect();
                }
            }

            if (!lagFlagsOn && s_wasActive)
                ClearNetLagEffect();

            s_wasActive = lagFlagsOn;

            if (s_effectActive && s_frozenLocalPlayer != trueLocal)
            {
                ClearNetLagEffect();
                return;
            }

            if (!s_effectActive || !s_frozenBones.valid)
                return;

            {
                const Vector3& cam = g_Globals.EspConfig.MainCamera;
                if (Vector3::Distance(cam, s_frozenCenter) > 45.f)
                    return;
            }

            const int w = g_Globals.EspConfig.Width;
            const int h = g_Globals.EspConfig.Height;
            if (w < 10 || h < 10) return;

            const bool outside = (g_Globals.General.OverlayMode == 1);
            const float offX = outside ? (float)FWork::Overlay::GetEmuLeft() : 0.f;
            const float offY = outside ? (float)FWork::Overlay::GetEmuTop() : 0.f;

            if (!lagFlagsOn || !s_effectActive || !s_frozenBones.valid)
                return;

            float worldRadius = 1.35f;

            ImDrawList* dl = ImGui::GetForegroundDrawList();
            if (!dl) return;

            if (outside) {
                dl->PushClipRect(
                    ImVec2(offX, offY),
                    ImVec2(offX + (float)w, offY + (float)h),
                    true);
            }

            float angle = ImGui::GetTime() * g_Globals.Visuals.NetLagRotateSpeed;

            ImU32 circleCol = ImGui::ColorConvertFloat4ToU32(g_Globals.Visuals.NetLagCircleColor.Value);
            ImU32 skelCol = ImGui::ColorConvertFloat4ToU32(g_Globals.Visuals.NetLagSkeletonColor.Value);

            DrawGroundSeal(dl, s_frozenCenter, worldRadius, angle,
                g_Globals.EspConfig.ViewMatrix, w, h, offX, offY,
                circleCol, g_Globals.Visuals.NetLagEffectGlow);

            DrawFrozenSkeleton(dl, s_frozenBones, g_Globals.EspConfig.ViewMatrix,
                w, h, offX, offY, skelCol, g_Globals.Visuals.NetLagEffectGlow);

            if (outside)
                dl->PopClipRect();
        }
        catch (...)
        {
            ClearNetLagEffect();
            s_wasActive = false;
        }
    }

    void DrawRadarAndArrows()
    {
        if (g_Globals.General.DisableAllEffects) return;
        if (!g_Globals.Visuals.Enabled) return;
        DrawRadar();
        DrawDetectionArrows();
    }

    static void DrawCustomLogoAlways()
    {
        if (g_Globals.General.DisableAllEffects) return;
        if (!g_Globals.Visuals.Enabled) return;
        if (g_Globals.Visuals.ShowLogo == 0) return;
        if (!texture::custom_logo) return;

        ImDrawList* const draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        int screenW = g_Globals.EspConfig.Width;
        int screenH = g_Globals.EspConfig.Height;
        if (screenW <= 0 || screenH <= 0) {
            const ImVec2 ds = ImGui::GetIO().DisplaySize;
            screenW = (int)ds.x;
            screenH = (int)ds.y;
        }
        if (screenW <= 0 || screenH <= 0) return;

        const bool outside = (g_Globals.General.OverlayMode == 1);
        const float offX = outside ? (float)FWork::Overlay::GetEmuLeft() : 0.f;
        const float offY = outside ? (float)FWork::Overlay::GetEmuTop() : 0.f;

        const float logoDim = std::max(16.f, g_Globals.Visuals.EspLineLogoSize);
        const ImVec2 logoSize(logoDim, logoDim);
        const float padding = 12.0f;
        ImVec2 logoPos;
        switch (g_Globals.Visuals.EspLines) {
        case 2:
            logoPos = ImVec2((screenW * 0.5f) - (logoSize.x * 0.5f), screenH - logoSize.y - padding);
            break;
        case 0:
        case 1:
        default:
            logoPos = ImVec2((screenW * 0.5f) - (logoSize.x * 0.5f), padding);
            break;
        }
        logoPos.x += offX;
        logoPos.y += offY;

        if (!EffectsDisabled()) {
            draw->AddImage((ImTextureID)texture::custom_logo,
                logoPos - ImVec2(2, 2), logoPos + logoSize + ImVec2(2, 2),
                ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 90));
        }
        draw->AddImage((ImTextureID)texture::custom_logo, logoPos, logoPos + logoSize);
    }

    void Players() {

        g_Globals.EspConfig.showOnlyVisible = ui.esp.WukongMode;

        DrawCustomLogoAlways();

        if (!g_Globals.Visuals.Enabled)
            return;

        if (g_Globals.GameMode.MatchStatus != 1)
            return;

        const bool canDrawEsp = g_Globals.EspConfig.Matrix;

        ImDrawList* const draw = ImGui::GetBackgroundDrawList();
        if (!draw)
            return;

        Namegun::Init();

        const int screenW = g_Globals.EspConfig.Width;
        const int screenH = g_Globals.EspConfig.Height;

        const bool outside = (g_Globals.General.OverlayMode == 1);
        const float offX = outside ? (float)FWork::Overlay::GetEmuLeft() : 0.f;
        const float offY = outside ? (float)FWork::Overlay::GetEmuTop() : 0.f;

        if (outside)
        {
            draw->PushClipRect(
                ImVec2(offX, offY),
                ImVec2(offX + (float)screenW, offY + (float)screenH),
                true);
        }

        const float logoDim = std::max(16.f, g_Globals.Visuals.EspLineLogoSize);
        const ImVec2 logoSize(logoDim, logoDim);
        const float padding = 12.0f;
        ImVec2 logoPos;
        switch (g_Globals.Visuals.EspLines) {
        case 2:
            logoPos = ImVec2((screenW * 0.5f) - (logoSize.x * 0.5f), screenH - logoSize.y - padding);
            break;
        case 0:
        case 1:
        default:
            logoPos = ImVec2((screenW * 0.5f) - (logoSize.x * 0.5f), padding);
            break;
        }
        logoPos.x += offX;
        logoPos.y += offY;
        const ImVec2 logoCenter(logoPos.x + logoSize.x * 0.5f, logoPos.y + logoSize.y * 0.5f);

        if (canDrawEsp) {
            std::shared_lock lock(g_Globals.EspConfig.EntitiesMutex);
            for (const auto& [entityID, player] : g_Globals.EspConfig.Entities) {
                if (entityID == 0) continue;
                if (entityID == g_Globals.EspConfig.LocalPlayer) continue;
                if (player.Address != 0 && player.Address == g_Globals.EspConfig.LocalPlayer) continue;
                if (player.IsTeam == Bool3::True && entityID == g_Globals.EspConfig.LocalPlayer) continue;

                if (player.Distance < 0.45f)
                    continue;

                if (!player.IsKnown) {
                    EspDiag_VisualSkip(3, (uint32_t)entityID, "not_known");
                    continue;
                }
                if (player.IsDead) {
                    EspDiag_VisualSkip(3, (uint32_t)entityID, "dead");
                    continue;
                }
                if (player.Address == 0) continue;

                if (player.IsTeam == Bool3::True &&
                    g_Globals.GameMode.MatchStatus == 1 &&
                    !false) {
                    EspDiag_VisualSkip(4, (uint32_t)entityID, "team");
                    continue;
                }

                if (!g_Globals.EspConfig.showOnlyVisible && !player.IsVisible) {
                    EspDiag_VisualSkip(5, (uint32_t)entityID, "classic_invisible");
                    continue;
                }

                auto isZero = [](const Vector3& v) {
                    return v.X == 0.f && v.Y == 0.f && v.Z == 0.f;
                    };

                auto MirrorY = [](const Vector3& bone, const Vector3& pivot) -> Vector3 {
                    if (bone.X == 0.f && bone.Y == 0.f && bone.Z == 0.f) return bone;
                    if (pivot.X == 0.f && pivot.Y == 0.f && pivot.Z == 0.f) return bone;
                    return Vector3(bone.X, 2.f * pivot.Y - bone.Y, bone.Z);
                    };

                const bool enemyIsFlipped = g_Globals.Misc.DownPlayer || g_Globals.Misc.EnemyRotate;

                Vector3 drawHead = player.Head;
                Vector3 drawRoot = player.Root;
                if (isZero(drawHead) && !isZero(player.LastHead))
                    drawHead = player.LastHead;
                if (isZero(drawRoot)) {
                    if (!isZero(player.Hip))
                        drawRoot = player.Hip;
                    else if (!isZero(player.LastHip))
                        drawRoot = player.LastHip;
                }

                Vector3 pivot = drawRoot;
                if (isZero(pivot)) pivot = player.Root;
                if (isZero(pivot)) pivot = player.Hip;
                if (isZero(pivot)) pivot = player.LastHip;

                if (enemyIsFlipped && !isZero(pivot) && !isZero(drawHead)) {
                    drawHead = MirrorY(drawHead, pivot);
                    if (!isZero(drawRoot))
                        drawRoot = MirrorY(drawRoot, pivot);
                }

                if (isZero(drawHead)) {
                    EspDiag_VisualSkip(1, (uint32_t)entityID, "drawHead_zero");
                    continue;
                }

                if (!isZero(drawRoot)) {
                    const float dx = drawHead.X - drawRoot.X;
                    const float dy = drawHead.Y - drawRoot.Y;
                    const float dz = drawHead.Z - drawRoot.Z;
                    const float sep3d = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (sep3d < 0.12f)
                        continue;
                }

                float dist = Vector3::Distance(g_Globals.EspConfig.MainCamera, drawHead);
                if (dist > g_Globals.Visuals.RenderDistance) continue;

                Vector2 headPos = W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, drawHead, screenW, screenH);
                Vector2 rootPos = isZero(drawRoot)
                    ? headPos
                    : W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, drawRoot, screenW, screenH);

                if (!std::isfinite(headPos.X) || !std::isfinite(headPos.Y))
                    continue;
                if (!std::isfinite(rootPos.X) || !std::isfinite(rootPos.Y))
                    rootPos = headPos;

                const bool headOk = (headPos.X > -120.f && headPos.X < screenW + 120.f &&
                    headPos.Y > -120.f && headPos.Y < screenH + 120.f);
                if (!headOk)
                    continue;

                const float hx = headPos.X + offX;
                const float hy = headPos.Y + offY;

                {
                    if (!std::isfinite(hx) || !std::isfinite(hy))
                        continue;
                    if (headPos.X < 1.f && headPos.Y < 1.f)
                        continue;
                    const float margin = -100.0f;
                    if (hx < offX + margin || hy < offY + margin ||
                        hx > offX + (float)screenW - margin ||
                        hy > offY + (float)screenH - margin)
                    {
                        continue;
                    }
                }

                auto ProjectSafe = [&](const Vector3& bone0) -> Vector2 {
                    if (isZero(bone0)) return Vector2(-1.f, -1.f);
                    Vector3 b = bone0;
                    if (enemyIsFlipped && !isZero(pivot))
                        b = MirrorY(b, pivot);
                    Vector2 s = W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, b, screenW, screenH);
                    if (!std::isfinite(s.X) || !std::isfinite(s.Y) || (s.X < 1.f && s.Y < 1.f))
                        return Vector2(-1.f, -1.f);
                    if (s.X < -150.f || s.Y < -150.f || s.X > screenW + 150.f || s.Y > screenH + 150.f)
                        return Vector2(-1.f, -1.f);
                    return s;
                    };

                // Dynamic Bone-Based Bounding Box Calculation from Head Bone and Root Bone
                // Fixed crawling oversized + faraway missing:
                // pure projected head-to-root span (no 850/dist fallback).
                // EnemyRotate / DownPlayer MirrorY is already applied to drawHead/drawRoot above.
                // Close-range / scoped projection behaviour unchanged.
                Vector2 shL = ProjectSafe(player.LeftShoulder);
                Vector2 shR = ProjectSafe(player.RightShoulder);

                float headY = headPos.Y;
                float rootY = rootPos.Y;

                // Soft root recovery only (tiny fixed offset, never distance-scaled)
                if (!std::isfinite(rootY) || rootY == 0.f || isZero(drawRoot)) {
                    rootY = headY + 42.0f;
                }

                float minY = std::min(headY, rootY);
                float maxY = std::max(headY, rootY);
                float spanHeight = maxY - minY;

                // Reject only insane projections (matrix glitch / behind camera)
                if (spanHeight > 2000.0f) {
                    continue;
                }
                // Allow real small spans (crawling / knocked). Only floor to a few pixels.
                if (spanHeight < 4.0f) {
                    spanHeight = 4.0f;
                }

                // Friend-style framing: 6% overhang + 12% scale so name/HP have room
                float boxHeight = spanHeight * 1.12f;
                const float rawTop = minY - (spanHeight * 0.06f);

                float boxWidth = boxHeight * 0.58f;

                if (shL.X > 0.f && shR.X > 0.f) {
                    float shoulderW = fabsf(shR.X - shL.X);
                    if (shoulderW > boxWidth * 0.30f && shoulderW < boxWidth * 2.0f)
                        boxWidth = std::max(boxWidth, shoulderW * 1.10f);
                }

                if (boxWidth > screenW * 0.50f) boxWidth = screenW * 0.38f;
                if (boxWidth < 6.0f)            boxWidth = 6.0f;
                if (boxHeight > (float)screenH * 0.90f) boxHeight = (float)screenH * 0.90f;
                if (boxHeight < 8.0f) boxHeight = 8.0f;

                // Midpoint of head + root (stays on body when leaning / EnemyRotate flipped / knocked)
                const float rootScreenX = rootPos.X + offX;
                const float boxCenterX = (hx + rootScreenX) * 0.5f;
                const float boxLeft = boxCenterX - boxWidth * 0.5f;
                const float boxTop = rawTop + offY;

                const ImColor knockColor = ImColor(g_Globals.Visuals.KnockedEnemiesColor);
                const bool isKnocked = player.IsKnocked;
                const ImColor rainbow = (!EffectsDisabled() && g_Globals.Visuals.RainbowESP)
                    ? GetAnimatedRGBColor(1.0f) : ImColor(255, 255, 255, 255);

                auto espColor = [&](ImColor normal) -> ImColor {
                    if (isKnocked) return knockColor;
                    if (!EffectsDisabled() && g_Globals.Visuals.RainbowESP) return rainbow;
                    return normal;
                    };

                ImColor currecntlinecolor = espColor(ImColor(g_Globals.Visuals.SnapLinesColor));

                if (g_Globals.EspConfig.showOnlyVisible && !player.IsVisible) {
                    currecntlinecolor = ImColor(
                        ui.esp.WukongLineColor[0],
                        ui.esp.WukongLineColor[1],
                        ui.esp.WukongLineColor[2],
                        ui.esp.WukongLineColor[3]);
                }

                ImColor currentBoxColor = espColor(ImColor(g_Globals.Visuals.BoxColor));
                ImColor currentSkeletonColor = espColor(ImColor(g_Globals.Visuals.SkeletonColor));
                ImColor currentNameColor = espColor(ImColor(g_Globals.Visuals.NameColor));
                ImColor currentDistanceColor = espColor(ImColor(g_Globals.Visuals.DistanceColor));
                ImColor currentRankColor = espColor(ImColor(g_Globals.Visuals.RankColor));
                ImColor weaponcolorxd = espColor(ImColor(g_Globals.Visuals.WeaponColor));

                const bool headOnScreen =
                    std::isfinite(hx) && std::isfinite(hy) &&
                    std::isfinite(boxWidth) && std::isfinite(boxHeight) &&
                    std::abs(hx) < 100000.f && std::abs(hy) < 100000.f &&
                    boxWidth > 4.f && boxWidth < screenW * 0.9f &&
                    boxHeight > 3.f && boxHeight < screenH * 1.1f &&
                    hx > offX - 50.f && hy > offY - 50.f &&
                    hx < offX + (float)screenW + 50.f && hy < offY + (float)screenH + 50.f;

                if (g_Globals.Visuals.Lines && headOnScreen && g_Globals.EspConfig.Matrix)
                {
                    const int linePos = g_Globals.Visuals.EspLines;

                    ImVec2 lineStart = logoCenter;
                    if (linePos == 1) {
                        lineStart = ImVec2(offX + (float)screenW * 0.5f,
                            offY + (float)screenH * 0.5f);
                    }

                    const bool useRoot = (linePos == 2);
                    const float lx = useRoot ? rootPos.X + offX : hx;
                    const float ly = useRoot ? rootPos.Y + offY : hy;

                    const float margin = 12.f;
                    bool safeForLine =
                        std::isfinite(lx) && std::isfinite(ly) &&
                        lx > offX + margin && ly > offY + margin &&
                        lx < offX + (float)screenW - margin &&
                        ly < offY + (float)screenH - margin;

                    if (safeForLine) {
                        const float dxLogo = lx - lineStart.x;
                        const float dyLogo = ly - lineStart.y;
                        const float distToStart = std::sqrt(dxLogo * dxLogo + dyLogo * dyLogo);
                        if (distToStart < 18.f && dist > 12.f)
                            safeForLine = false;
                    }

                    if (safeForLine)
                    {
                        if (g_Globals.Visuals.LineGlow && !EffectsDisabled()) {
                            DrawGlowLine(draw, ImVec2(lx, ly), lineStart, currecntlinecolor, 1.2f,
                                g_Globals.Visuals.LineGlowRadius, 2.f);
                        }
                        else {
                            draw->AddLine(ImVec2(lx, ly), lineStart, currecntlinecolor, 1.2f);
                        }
                    }
                }

                if (g_Globals.Visuals.Box) {
                    const float glowRadius = g_Globals.Visuals.BoxGlow ? g_Globals.Visuals.BoxGlowRadius : 0.f;
                    const float feather = 2.0f;
                    switch (g_Globals.Visuals.players_box) {
                    case 0:
                        DrawFullBox(boxLeft, boxTop, boxWidth, boxHeight, currentBoxColor, 0.5f, isKnocked);
                        break;
                    case 1:
                    default:
                        DrawGlowCorneredBox(boxLeft, boxTop, boxWidth, boxHeight, currentBoxColor, 0.5f,
                            glowRadius, feather, g_Globals.Visuals.fillBox, isKnocked);
                        break;
                    }
                }

                const float boxRight = boxLeft + boxWidth;
                const float boxBot = boxTop + boxHeight;

                const float gap = 3.0f;
                const float pad = 6.0f;

                float botY = boxBot + pad;

                if (g_Globals.Visuals.Distance) {
                    const std::string distanceText = std::to_string((int)std::round(dist)) + "m";
                    ImFont* distFont = FWork::Fonts::InterBold ? FWork::Fonts::InterBold : ImGui::GetFont();
                    if (distFont) ImGui::PushFont(distFont);
                    const ImVec2 textSize = ImGui::CalcTextSize(distanceText.c_str());
                    if (distFont) ImGui::PopFont();

                    ImVec2 dpos(hx - textSize.x * 0.5f, botY);
                    const float dfs = distFont ? distFont->FontSize : ImGui::GetFontSize();
                    DrawGlowText(draw, distFont, dfs, dpos, currentDistanceColor, distanceText.c_str(),
                        g_Globals.Visuals.LineGlowRadius);
                    botY += textSize.y + gap;
                }

                if (g_Globals.Visuals.WeaponName && Namegun::ShouldShowWeapon(player.Gun)) {
                    const std::string gunName = Namegun::GetGunName(player.Gun);
                    if (!gunName.empty()) {
                        ImFont* gunFont = FWork::Fonts::InterBold ? FWork::Fonts::InterBold : ImGui::GetFont();
                        if (gunFont) ImGui::PushFont(gunFont);
                        const ImVec2 textSize = ImGui::CalcTextSize(gunName.c_str());
                        if (gunFont) ImGui::PopFont();

                        ImVec2 gpos(hx - textSize.x * 0.5f, botY);
                        const float gfs = gunFont ? gunFont->FontSize : ImGui::GetFontSize();
                        DrawGlowText(draw, gunFont, gfs, gpos, weaponcolorxd, gunName.c_str(),
                            g_Globals.Visuals.LineGlowRadius);
                        botY += textSize.y + gap;
                    }
                }

                if (g_Globals.Visuals.WeaponIcon && Namegun::ShouldShowWeapon(player.Gun) && Namegun::HasIcon(player.Gun)) {
                    std::string iconText = Namegun::GetGunIcon(player.Gun);
                    const std::string fullName = Namegun::GetGunName(player.Gun);
                    const std::string baseName = Namegun::GetBaseName(fullName);
                    if (baseName != fullName && baseName.length() < fullName.length()) {
                        const std::string suffix = fullName.substr(baseName.length());
                        const size_t pos = iconText.find(suffix);
                        if (pos != std::string::npos)
                            iconText = iconText.substr(0, pos);
                    }
                    ImFont* iconFont = FWork::Fonts::IconWeapon ? FWork::Fonts::IconWeapon : ImGui::GetFont();
                    if (iconFont && !iconText.empty()) {
                        const float ifs = iconFont->FontSize;
                        const ImVec2 iconSize = iconFont->CalcTextSizeA(ifs, FLT_MAX, 0.f, iconText.c_str());
                        ImVec2 ipos(hx - iconSize.x * 0.5f, botY);
                        draw->AddText(iconFont, ifs, ImVec2(ipos.x + 1, ipos.y + 1), IM_COL32(0, 0, 0, 200), iconText.c_str());
                        draw->AddText(iconFont, ifs, ipos, weaponcolorxd, iconText.c_str());
                    }
                }

                if (g_Globals.Visuals.HealthBar) {
                    const float barX = boxLeft - 7.0f;
                    const short maxHP = 200;
                    const float hpPct = std::clamp(static_cast<float>(player.Health) / maxHP, 0.f, 1.f);
                    ImVec4 hpCol = GetHealthColor(hpPct, isKnocked);
                    ImColor hpTextColor = ImColor(hpCol);

                    DrawVerticalHealthBar(player.Health, maxHP, ImVec2(barX, boxTop), boxHeight, isKnocked);

                    char hpBuf[16];
                    snprintf(hpBuf, sizeof(hpBuf), "%d%%", (player.IsDead || player.Health <= 0) ? 0 : player.Health);

                    ImFont* hpFont = FWork::Fonts::InterBold ? FWork::Fonts::InterBold : ImGui::GetFont();
                    const float hpFs = (hpFont ? hpFont->FontSize : ImGui::GetFontSize()) * 0.78f;
                    ImVec2 hpSize = hpFont->CalcTextSizeA(hpFs, FLT_MAX, 0.f, hpBuf);

                    ImVec2 hpPos(
                        barX - hpSize.x - 3.0f,
                        boxTop + boxHeight - hpSize.y
                    );
                    draw->AddText(hpFont, hpFs, ImVec2(hpPos.x + 1, hpPos.y + 1), IM_COL32(0, 0, 0, 200), hpBuf);
                    draw->AddText(hpFont, hpFs, hpPos, hpTextColor, hpBuf);
                }

                float topY = boxTop - pad;
                const float cx = boxCenterX;

                if (g_Globals.Visuals.Name) {
                    std::string nameText = player.Name.empty()
                        ? (player.IsBot ? "BOT - Training" : "Player")
                        : player.Name;

                    ImFont* nameFont =
                        FWork::Fonts::NotoSansRegular ? FWork::Fonts::NotoSansRegular :
                        FWork::Fonts::ArialUnicode ? FWork::Fonts::ArialUnicode :
                        FWork::Fonts::NotoSansSymbolsRegular ? FWork::Fonts::NotoSansSymbolsRegular :
                        FWork::Fonts::NotoEmojiRegular ? FWork::Fonts::NotoEmojiRegular :
                        FWork::Fonts::LexendRegular ? FWork::Fonts::LexendRegular :
                        FWork::Fonts::InterBold ? FWork::Fonts::InterBold :
                        ImGui::GetFont();

                    if (nameFont) ImGui::PushFont(nameFont);
                    const ImVec2 textSize = ImGui::CalcTextSize(nameText.c_str());
                    if (nameFont) ImGui::PopFont();

                    topY -= textSize.y;
                    ImVec2 namePos(hx - textSize.x * 0.5f, topY);
                    const float nameFs = nameFont ? nameFont->FontSize : ImGui::GetFontSize();

                    DrawGlowText(draw, nameFont, nameFs, namePos, currentNameColor, nameText.c_str(),
                        g_Globals.Visuals.LineGlowRadius);

                    if ((ui.esp.LevelEnabled || g_Globals.Visuals.Level) && player.Level > 0) {
                        std::string levelText = std::to_string(player.Level);
                        ImFont* lvlFont = FWork::Fonts::InterBold ? FWork::Fonts::InterBold : nameFont;
                        const float lvlFs = lvlFont->FontSize * 0.75f;
                        ImVec2 levelTextSize = lvlFont->CalcTextSizeA(lvlFs, FLT_MAX, 0.f, levelText.c_str());
                        const float paddingX = 6.0f;
                        const float paddingY = 2.0f;
                        const float spacing = 8.0f;
                        ImVec2 boxSize(levelTextSize.x + 2 * paddingX, levelTextSize.y + 2 * paddingY);
                        ImVec2 levelPos(namePos.x - boxSize.x - spacing, namePos.y - paddingY);

                        ImColor lvlCol;
                        if (player.Level <= 10) lvlCol = ImColor(15, 200, 210, 255);
                        else if (player.Level <= 20) lvlCol = ImColor(100, 200, 50, 255);
                        else if (player.Level <= 30) lvlCol = ImColor(240, 180, 20, 255);
                        else if (player.Level <= 40) lvlCol = ImColor(230, 140, 20, 255);
                        else if (player.Level <= 50) lvlCol = ImColor(230, 90, 20, 255);
                        else if (player.Level <= 60) lvlCol = ImColor(210, 70, 30, 255);
                        else if (player.Level <= 70) lvlCol = ImColor(220, 30, 30, 255);
                        else if (player.Level <= 80) lvlCol = ImColor(180, 20, 20, 255);
                        else if (player.Level <= 90) lvlCol = ImColor(140, 10, 10, 255);
                        else                         lvlCol = ImColor(100, 0, 0, 255);

                        draw->AddRectFilled(levelPos,
                            ImVec2(levelPos.x + boxSize.x, levelPos.y + boxSize.y),
                            lvlCol, 4.0f);
                        draw->AddText(lvlFont, lvlFs,
                            ImVec2(levelPos.x + paddingX, levelPos.y + paddingY),
                            IM_COL32(255, 255, 255, 255), levelText.c_str());
                    }

                    topY -= gap;
                }

                {
                    const bool wantIcon = (g_Globals.Visuals.RankIcon || ui.esp.RankIconEnabled) && player.RankIconIndex >= 0;
                    const bool wantText = (g_Globals.Visuals.Rank || ui.esp.RankEnabled) && !player.RankText.empty();
                    const bool wantBar = (g_Globals.Visuals.RankScoreBar || ui.esp.RankScoreBarEnabled) && player.RankIconIndex >= 0;

                    if (wantIcon || wantText || wantBar)
                    {
                        const float iconSz = std::clamp(
                            (g_Globals.Visuals.RankIconSize > 0.f ? g_Globals.Visuals.RankIconSize : ui.esp.RankIconSize),
                            12.f, 48.f);
                        const float barH = 3.0f;

                        ImFont* rankFont = FWork::Fonts::InterBold ? FWork::Fonts::InterBold : ImGui::GetFont();
                        ImVec2 rankTextSize(0, 0);
                        if (wantText) {
                            if (rankFont) ImGui::PushFont(rankFont);
                            rankTextSize = ImGui::CalcTextSize(player.RankText.c_str());
                            if (rankFont) ImGui::PopFont();
                        }

                        float blockH = 0.f;
                        if (wantText) blockH += rankTextSize.y + gap;
                        if (wantIcon) blockH += iconSz + gap;
                        if (wantBar)  blockH += barH + gap;

                        topY -= blockH;
                        float curY = topY;

                        if (wantBar) {
                            const float barW = iconSz * 0.95f;
                            ImVec2 tl(cx - barW * 0.5f, curY);
                            ImVec2 br(tl.x + barW, tl.y + barH);
                            draw->AddRectFilled(tl, br, IM_COL32(0, 0, 0, 180), 1.5f);
                            float fill = barW * std::clamp(player.RankProgress, 0.f, 1.f);
                            if (fill > 0.5f)
                                draw->AddRectFilled(tl, ImVec2(tl.x + fill, br.y),
                                    ImGui::ColorConvertFloat4ToU32(g_Globals.Visuals.RankColor.Value), 1.5f);
                            draw->AddRect(tl, br, IM_COL32(0, 0, 0, 220), 1.5f, 0, 1.f);
                            curY += barH + gap;
                        }

                        if (wantIcon) {
                            if (auto* srv = RankIcons::Get(player.RankIconIndex)) {
                                ImVec2 p0(cx - iconSz * 0.5f, curY);
                                draw->AddImage((ImTextureID)srv, p0, ImVec2(p0.x + iconSz, p0.y + iconSz));
                            }
                            curY += iconSz + gap;
                        }

                        if (wantText) {
                            ImVec2 tpos(cx - rankTextSize.x * 0.5f, curY);
                            const float fs = rankFont ? rankFont->FontSize : ImGui::GetFontSize();
                            DrawGlowText(draw, rankFont, fs, tpos, currentRankColor, player.RankText.c_str(),
                                g_Globals.Visuals.LineGlowRadius);
                        }
                    }
                }

                if ((ui.esp.ShowIDEnabled || g_Globals.Visuals.ShowID) && player.ID != 0) {
                    std::string idText = "ID: " + std::to_string(player.ID);
                    ImFont* idFont = FWork::Fonts::InterBold ? FWork::Fonts::InterBold : ImGui::GetFont();
                    const float idFs = idFont->FontSize * 0.80f;
                    ImVec2 idSize = idFont->CalcTextSizeA(idFs, FLT_MAX, 0.f, idText.c_str());

                    topY -= idSize.y + 2.0f;
                    ImVec2 idPos(hx - idSize.x * 0.5f, topY);

                    DrawGlowText(draw, idFont, idFs, idPos, IM_COL32(255, 255, 255, 230), idText.c_str(),
                        g_Globals.Visuals.LineGlowRadius);
                }

                if (g_Globals.Visuals.Skeleton && (!player.BonesStale || enemyIsFlipped)) {
                    auto DrawBone = [&](const Vector3& from0, const Vector3& to0)
                        {
                            if (isZero(from0) || isZero(to0)) return;

                            Vector3 from = from0;
                            Vector3 to = to0;
                            if (enemyIsFlipped && !isZero(pivot)) {
                                from = MirrorY(from, pivot);
                                to = MirrorY(to, pivot);
                            }
                            if (isZero(from) || isZero(to)) return;

                            Vector2 screenFrom = W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, from, screenW, screenH);
                            Vector2 screenTo = W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, to, screenW, screenH);

                            if (!std::isfinite(screenFrom.X) || !std::isfinite(screenFrom.Y) ||
                                !std::isfinite(screenTo.X) || !std::isfinite(screenTo.Y))
                                return;

                            if (screenFrom.X < 0.f || screenFrom.Y < 0.f ||
                                screenFrom.X >(float)screenW || screenFrom.Y >(float)screenH)
                                return;

                            if (screenTo.X < 0.f || screenTo.Y < 0.f ||
                                screenTo.X >(float)screenW || screenTo.Y >(float)screenH)
                                return;

                            float dx = screenTo.X - screenFrom.X;
                            float dy = screenTo.Y - screenFrom.Y;
                            float len = sqrtf(dx * dx + dy * dy);
                            if (len > (float)screenH * 0.70f || len < 1.0f)
                                return;

                            DrawGlowBoneLine(draw,
                                ImVec2(screenFrom.X + offX, screenFrom.Y + offY),
                                ImVec2(screenTo.X + offX, screenTo.Y + offY),
                                currentSkeletonColor, g_Globals.Visuals.SkeletonGlowRadius, 2.f);
                        };

                    DrawBone(player.Head, player.Neck);
                    DrawBone(player.Neck, player.Hip);
                    DrawBone(player.Neck, player.LeftShoulder);
                    DrawBone(player.LeftShoulder, player.LeftElbow);
                    DrawBone(player.LeftElbow, player.LeftWrist);
                    DrawBone(player.Neck, player.RightShoulder);
                    DrawBone(player.RightShoulder, player.RightElbow);
                    DrawBone(player.RightElbow, player.RightWrist);
                    DrawBone(player.Hip, player.Groin);
                    DrawBone(player.Groin, player.LeftAnkle);
                    DrawBone(player.Groin, player.RightAnkle);
                    DrawBone(player.LeftAnkle, player.LeftFoot);
                    DrawBone(player.RightAnkle, player.RightFoot);

                    {
                        const float baseRadius = 3.5f;
                        const float scale = std::clamp(boxHeight / 80.0f, 0.35f, 1.25f);
                        const float dotRadius = baseRadius * scale;
                        const float outer = dotRadius + std::max(1.0f, 1.2f * scale);
                        draw->AddCircleFilled(ImVec2(hx, hy), outer, IM_COL32(0, 0, 0, 180), 16);
                        draw->AddCircleFilled(ImVec2(hx, hy), dotRadius, IM_COL32(255, 30, 30, 255), 16);
                    }
                }
            }
        }

        if (canDrawEsp && g_Globals.Visuals.SnapLines) {
            bool hasClosest = false;
            Player closestPlayer{};
            float closestCrossDist = FLT_MAX;
            const ImVec2 center(screenW * 0.5f + offX, screenH * 0.5f + offY);
            constexpr float kMaxWorldDist = 80.0f;

            auto MirrorYSnap = [](const Vector3& bone, const Vector3& pivot) -> Vector3 {
                if (bone.X == 0.f && bone.Y == 0.f && bone.Z == 0.f) return bone;
                if (pivot.X == 0.f && pivot.Y == 0.f && pivot.Z == 0.f) return bone;
                return Vector3(bone.X, 2.f * pivot.Y - bone.Y, bone.Z);
                };
            const bool anyFlipActive = g_Globals.Misc.DownPlayer || g_Globals.Misc.EnemyRotate;

            std::shared_lock snapLock(g_Globals.EspConfig.EntitiesMutex);
            for (const auto& [id, scan] : g_Globals.EspConfig.Entities) {
                if (!scan.IsKnown || scan.IsDead) continue;
                if (scan.IsTeam == Bool3::True) continue;
                if (!g_Globals.EspConfig.showOnlyVisible && !scan.IsVisible) continue;
                if (g_Globals.Visuals.IgnoreTrainingBots && IsTrainingBotTarget(scan)) continue;
                if (g_Globals.AimBot.IgnoreKnocked && scan.IsKnocked) continue;
                if (scan.Head.X == 0.f && scan.Head.Y == 0.f && scan.Head.Z == 0.f) continue;

                Vector3 aimHead = scan.Head;
                if (anyFlipActive) {
                    Vector3 pivot = scan.Root;
                    if (pivot.X == 0.f && pivot.Y == 0.f && pivot.Z == 0.f) pivot = scan.Hip;
                    if (!(pivot.X == 0.f && pivot.Y == 0.f && pivot.Z == 0.f))
                        aimHead = MirrorYSnap(aimHead, pivot);
                }

                const float worldDist = Vector3::Distance(g_Globals.EspConfig.MainCamera, aimHead);
                if (worldDist < 0.5f || worldDist > kMaxWorldDist) continue;

                const Vector2 head2D = W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, aimHead, screenW, screenH);
                if (!std::isfinite(head2D.X) || !std::isfinite(head2D.Y)) continue;
                if (head2D.X < 8.f || head2D.Y < 8.f) continue;
                if (head2D.X > (float)screenW - 8.f || head2D.Y > (float)screenH - 8.f) continue;

                const float dx = (head2D.X + offX) - center.x;
                const float dy = (head2D.Y + offY) - center.y;
                const float crossDist = std::sqrt(dx * dx + dy * dy);
                if (crossDist < closestCrossDist) {
                    closestCrossDist = crossDist;
                    closestPlayer = scan;
                    hasClosest = true;
                }
            }

            if (hasClosest) {
                Vector3 finalHead = closestPlayer.Head;
                if (anyFlipActive) {
                    Vector3 pivot = closestPlayer.Root;
                    if (pivot.X == 0.f && pivot.Y == 0.f && pivot.Z == 0.f) pivot = closestPlayer.Hip;
                    if (!(pivot.X == 0.f && pivot.Y == 0.f && pivot.Z == 0.f))
                        finalHead = MirrorYSnap(finalHead, pivot);
                }
                const Vector2 head2D = W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, finalHead, screenW, screenH);
                if (std::isfinite(head2D.X) && std::isfinite(head2D.Y) &&
                    head2D.X >= 8.f && head2D.Y >= 8.f &&
                    head2D.X <= (float)screenW - 8.f && head2D.Y <= (float)screenH - 8.f)
                {
                    const ImColor lineColor = closestPlayer.IsKnocked
                        ? ImColor(g_Globals.Visuals.KnockedEnemiesColor)
                        : ImColor(g_Globals.Visuals.OriginLineColor);
                    const ImVec2 snapA = center;
                    const ImVec2 snapB(head2D.X + offX, head2D.Y + offY);
                    if (g_Globals.Visuals.LineGlow && !EffectsDisabled()) {
                        DrawGlowLine(draw, snapA, snapB, lineColor, 1.0f,
                            g_Globals.Visuals.LineGlowRadius, 2.f);
                    }
                    else {
                        draw->AddLine(snapA, snapB, lineColor, 1.0f);
                    }
                }
            }
        }

        if (g_Globals.Visuals.InvalidTimer && !EffectsDisabled() &&
            g_Globals.GameMode.MatchStatus == 1)
        {
            const bool outside = (g_Globals.General.OverlayMode == 1);
            const float timerOffX = outside ? (float)FWork::Overlay::GetEmuLeft() : 0.f;
            const float timerOffY = outside ? (float)FWork::Overlay::GetEmuTop() : 0.f;
            FWork::Data::DrawInvalidMatchTimer(
                static_cast<float>(g_Globals.EspConfig.Width),
                static_cast<float>(g_Globals.EspConfig.Height),
                timerOffX, timerOffY);
        }

        DrawRadarAndArrows();
        DrawNetLagOccultEffect();
    }
} // namespace ESP