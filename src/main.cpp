// Interpolation playground: lerp, easing, color lerp, 2D slerp, quaternion slerp.
// SDL3 draws the four demo panels, Dear ImGui drives all the values.

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include <cmath>
#include <cstdio>

static constexpr float PI = 3.14159265358979f;
static constexpr float DEG2RAD = PI / 180.0f;

// ----------------------------------------------------------------------------
// Math helpers
// ----------------------------------------------------------------------------
struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Quat { float w, x, y, z; };

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static Vec2  lerp(Vec2 a, Vec2 b, float t) { return { lerpf(a.x, b.x, t), lerpf(a.y, b.y, t) }; }

// Slerp for 2D unit vectors: constant angular speed along the arc.
static Vec2 slerp(Vec2 a, Vec2 b, float t)
{
    float d = clampf(a.x * b.x + a.y * b.y, -1.0f, 1.0f);
    float omega = acosf(d);
    float so = sinf(omega);
    if (so < 1e-4f) return lerp(a, b, t);          // (anti)parallel: slerp undefined
    float s0 = sinf((1.0f - t) * omega) / so;
    float s1 = sinf(t * omega) / so;
    return { a.x * s0 + b.x * s1, a.y * s0 + b.y * s1 };
}

static Quat  qmul(Quat a, Quat b)
{
    return { a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
             a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
             a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
             a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w };
}
static Quat  qnormalize(Quat q)
{
    float n = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return { q.w / n, q.x / n, q.y / n, q.z / n };
}
static float qdot(Quat a, Quat b) { return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z; }
static Quat  qneg(Quat q) { return { -q.w, -q.x, -q.y, -q.z }; }
static Quat  axisAngle(float ax, float ay, float az, float deg)
{
    float h = deg * DEG2RAD * 0.5f, s = sinf(h);
    return { cosf(h), ax * s, ay * s, az * s };
}
static Quat  fromEuler(Vec3 e) // x = pitch, y = yaw, z = roll (degrees)
{
    return qnormalize(qmul(axisAngle(0, 1, 0, e.y), qmul(axisAngle(1, 0, 0, e.x), axisAngle(0, 0, 1, e.z))));
}
static Quat nlerp(Quat a, Quat b, float t, bool shortest)
{
    if (shortest && qdot(a, b) < 0.0f) b = qneg(b);
    return qnormalize({ lerpf(a.w, b.w, t), lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t) });
}
static Quat qslerp(Quat a, Quat b, float t, bool shortest)
{
    float d = qdot(a, b);
    if (shortest && d < 0.0f) { b = qneg(b); d = -d; }
    if (d > 0.9995f) return nlerp(a, b, t, false);  // nearly identical: avoid dividing by ~0
    float theta = acosf(clampf(d, -1.0f, 1.0f));
    float s = sinf(theta);
    if (fabsf(s) < 1e-5f) return nlerp(a, b, t, false);
    float w0 = sinf((1.0f - t) * theta) / s;
    float w1 = sinf(t * theta) / s;
    return { a.w * w0 + b.w * w1, a.x * w0 + b.x * w1, a.y * w0 + b.y * w1, a.z * w0 + b.z * w1 };
}
static Vec3 rotate(Quat q, Vec3 v)
{
    // v' = v + 2w(q x v) + 2 q x (q x v)
    Vec3 u{ q.x, q.y, q.z };
    Vec3 t{ 2 * (u.y * v.z - u.z * v.y), 2 * (u.z * v.x - u.x * v.z), 2 * (u.x * v.y - u.y * v.x) };
    Vec3 c{ u.y * t.z - u.z * t.y, u.z * t.x - u.x * t.z, u.x * t.y - u.y * t.x };
    return { v.x + q.w * t.x + c.x, v.y + q.w * t.y + c.y, v.z + q.w * t.z + c.z };
}

// ----------------------------------------------------------------------------
// Easing
// ----------------------------------------------------------------------------
static const char* kEaseNames[] = {
    "Linear", "Smoothstep", "Smootherstep", "Ease in (quad)", "Ease out (quad)",
    "Ease in-out (cubic)", "Ease out (bounce)", "Ease out (back, overshoot)"
};
static float ease(int mode, float t)
{
    switch (mode) {
    case 1: return t * t * (3.0f - 2.0f * t);
    case 2: return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    case 3: return t * t;
    case 4: return 1.0f - (1.0f - t) * (1.0f - t);
    case 5: return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    case 6: {
        const float n1 = 7.5625f, d1 = 2.75f;
        if (t < 1.0f / d1)        return n1 * t * t;
        else if (t < 2.0f / d1) { t -= 1.5f / d1;   return n1 * t * t + 0.75f; }
        else if (t < 2.5f / d1) { t -= 2.25f / d1;  return n1 * t * t + 0.9375f; }
        else { t -= 2.625f / d1; return n1 * t * t + 0.984375f; }
    }
    case 7: { const float c1 = 1.70158f, c3 = c1 + 1.0f; float u = t - 1.0f; return 1.0f + c3 * u * u * u + c1 * u * u; }
    default: return t;
    }
}

// ----------------------------------------------------------------------------
// Drawing helpers
// ----------------------------------------------------------------------------
static void setColor(SDL_Renderer* r, float R, float G, float B, float A = 1.0f) { SDL_SetRenderDrawColorFloat(r, R, G, B, A); }

static void fillSquare(SDL_Renderer* r, float cx, float cy, float size)
{
    SDL_FRect rc{ cx - size * 0.5f, cy - size * 0.5f, size, size };
    SDL_RenderFillRect(r, &rc);
}

static void label(SDL_Renderer* r, float x, float y, const char* text)
{
    SDL_RenderDebugText(r, x, y, text);
}

static void drawCircle(SDL_Renderer* r, float cx, float cy, float radius)
{
    SDL_FPoint pts[73];
    for (int i = 0; i <= 72; ++i) {
        float a = i * (2.0f * PI / 72.0f);
        pts[i] = { cx + cosf(a) * radius, cy - sinf(a) * radius };
    }
    SDL_RenderLines(r, pts, 73);
}

static void drawCube(SDL_Renderer* r, Quat q, float cx, float cy, float size, bool axes,
    float R, float G, float B, float A)
{
    auto project = [&](Vec3 v) {
        Vec3 p = rotate(q, v);
        float persp = 4.0f / (4.0f - p.z);   // simple perspective, camera on +Z
        return SDL_FPoint{ cx + p.x * size * persp, cy - p.y * size * persp };
        };
    SDL_FPoint pv[8];
    for (int i = 0; i < 8; ++i)
        pv[i] = project({ (i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f });

    setColor(r, R, G, B, A);
    const int bits[3] = { 1, 2, 4 };
    for (int i = 0; i < 8; ++i) {
        for (int k = 0; k < 3; ++k) {
            const int bit = bits[k];
            if (!(i & bit)) SDL_RenderLine(r, pv[i].x, pv[i].y, pv[i | bit].x, pv[i | bit].y);
        }
    }

    if (axes) {
        SDL_FPoint o = project({ 0, 0, 0 });
        SDL_FPoint ax = project({ 1.6f, 0, 0 }), ay = project({ 0, 1.6f, 0 }), az = project({ 0, 0, 1.6f });
        setColor(r, 1.0f, 0.3f, 0.3f); SDL_RenderLine(r, o.x, o.y, ax.x, ax.y);
        setColor(r, 0.3f, 1.0f, 0.3f); SDL_RenderLine(r, o.x, o.y, ay.x, ay.y);
        setColor(r, 0.4f, 0.6f, 1.0f); SDL_RenderLine(r, o.x, o.y, az.x, az.y);
    }
}

// ----------------------------------------------------------------------------
int main(int, char**)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    TTF_Init();
    MIX_Init();

    SDL_Window* window = SDL_CreateWindow("Interpolation playground - SDL3 + Dear ImGui", 1280, 720, 0);
    if (!window) { SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError()); return 1; }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) { SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError()); return 1; }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // ---- Tweakable state (everything below is edited through ImGui) ----
    // Time / easing
    float t = 0.0f, phase = 0.0f, speed = 0.4f;
    bool  animate = true, pingPong = true;
    int   easeMode = 1;

    // Panel 1: position lerp (coordinates are local to the panel)
    float posA[2] = { 40.0f, 300.0f };
    float posB[2] = { 390.0f, 90.0f };

    // Panel 2: color lerp
    float colA[3] = { 0.10f, 0.45f, 0.95f };
    float colB[3] = { 1.00f, 0.40f, 0.10f };

    // Panel 3: 2D lerp vs slerp on the unit circle
    float angA = 20.0f, angB = 160.0f;

    // Panel 4: quaternion slerp between two orientations (pitch, yaw, roll in degrees)
    float eulA[3] = { 0.0f, 0.0f, 0.0f };
    float eulB[3] = { 40.0f, 140.0f, 60.0f };
    bool  shortestPath = true, compareNlerp = false, showGhosts = true;

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT ||
                (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && e.window.windowID == SDL_GetWindowID(window)))
                running = false;
        }

        // ---- Advance time ----
        if (animate) {
            phase += io.DeltaTime * speed;
            float period = pingPong ? 2.0f : 1.0f;
            float p = fmodf(phase, period);
            t = pingPong ? (p < 1.0f ? p : 2.0f - p) : p;
        }
        float te = ease(easeMode, t);   // eased t used by every demo

        // ---- ImGui UI ----
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(870, 10), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(400, 700), ImGuiCond_Once);
        ImGui::Begin("Interpolation controls");

        if (ImGui::CollapsingHeader("Time & easing", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Animate", &animate);
            ImGui::SameLine();
            ImGui::Checkbox("Ping-pong", &pingPong);
            ImGui::SliderFloat("Speed", &speed, 0.05f, 2.0f);
            ImGui::SliderFloat("t", &t, 0.0f, 1.0f);
            if (ImGui::IsItemActive()) { animate = false; }   // grabbing the slider pauses playback
            ImGui::Combo("Easing", &easeMode, kEaseNames, IM_ARRAYSIZE(kEaseNames));
            ImGui::Text("t = %.3f   eased t = %.3f", t, te);

            float curve[100];
            for (int i = 0; i < 100; ++i) curve[i] = ease(easeMode, i / 99.0f);
            ImGui::PlotLines("##curve", curve, 100, 0, "easing curve", -0.2f, 1.2f, ImVec2(0, 70));
        }

        if (ImGui::CollapsingHeader("1. Position lerp", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat2("Start (A)", posA, 1.0f, 0.0f, 430.0f, "%.0f");
            ImGui::DragFloat2("End (B)", posB, 1.0f, 0.0f, 430.0f, "%.0f");
            ImGui::TextDisabled("lerp(a, b, t) = a + (b - a) * t");
        }

        if (ImGui::CollapsingHeader("2. Color lerp", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ColorEdit3("Color A", colA);
            ImGui::ColorEdit3("Color B", colB);
        }

        if (ImGui::CollapsingHeader("3. Lerp vs slerp (2D)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Angle A", &angA, 0.0f, 360.0f, "%.0f deg");
            ImGui::SliderFloat("Angle B", &angB, 0.0f, 360.0f, "%.0f deg");
            ImGui::TextDisabled("Red = lerp (cuts the chord), green = slerp (follows the arc)");
        }

        if (ImGui::CollapsingHeader("4. Quaternion slerp", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat3("Orientation A", eulA, -180.0f, 180.0f, "%.0f");
            ImGui::SliderFloat3("Orientation B", eulB, -180.0f, 180.0f, "%.0f");
            ImGui::TextDisabled("(pitch, yaw, roll) in degrees");
            ImGui::Checkbox("Take shortest path", &shortestPath);
            ImGui::Checkbox("Compare with nlerp", &compareNlerp);
            ImGui::Checkbox("Show start/end ghosts", &showGhosts);
        }
        ImGui::End();
        ImGui::Render();

        // ---- Scene ----
        setColor(renderer, 0.08f, 0.09f, 0.11f);
        SDL_RenderClear(renderer);

        const float PW = 430.0f, PH = 360.0f;
        const float ox[4] = { 0.0f, PW, 0.0f, PW };
        const float oy[4] = { 0.0f, 0.0f, PH, PH };
        char buf[128];

        // Panel borders
        setColor(renderer, 0.25f, 0.27f, 0.32f);
        for (int i = 0; i < 4; ++i) {
            SDL_FRect rc{ ox[i] + 2, oy[i] + 2, PW - 4, PH - 4 };
            SDL_RenderRect(renderer, &rc);
        }

        // --- Panel 1: position lerp ---
        {
            Vec2 a{ ox[0] + posA[0], oy[0] + posA[1] }, b{ ox[0] + posB[0], oy[0] + posB[1] };
            setColor(renderer, 1, 1, 1);
            label(renderer, ox[0] + 10, oy[0] + 10, "1. Position lerp");

            setColor(renderer, 0.35f, 0.35f, 0.4f);
            SDL_RenderLine(renderer, a.x, a.y, b.x, b.y);

            // Dots at evenly spaced *input* t: their spacing shows what the easing does
            setColor(renderer, 0.8f, 0.8f, 0.3f);
            for (int i = 0; i <= 20; ++i) {
                Vec2 p = lerp(a, b, ease(easeMode, i / 20.0f));
                fillSquare(renderer, p.x, p.y, 4.0f);
            }
            setColor(renderer, 0.3f, 0.6f, 1.0f); fillSquare(renderer, a.x, a.y, 12.0f);
            setColor(renderer, 1.0f, 0.4f, 0.3f); fillSquare(renderer, b.x, b.y, 12.0f);

            Vec2 p = lerp(a, b, te);
            setColor(renderer, 1.0f, 0.65f, 0.15f);
            fillSquare(renderer, p.x, p.y, 26.0f);

            setColor(renderer, 0.7f, 0.7f, 0.7f);
            std::snprintf(buf, sizeof(buf), "pos = (%.0f, %.0f)", p.x - ox[0], p.y - oy[0]);
            label(renderer, ox[0] + 10, oy[0] + PH - 18, buf);
        }

        // --- Panel 2: color lerp ---
        {
            setColor(renderer, 1, 1, 1);
            label(renderer, ox[1] + 10, oy[1] + 10, "2. Color lerp");

            float cr = lerpf(colA[0], colB[0], te), cg = lerpf(colA[1], colB[1], te), cb = lerpf(colA[2], colB[2], te);
            setColor(renderer, cr, cg, cb);
            SDL_FRect big{ ox[1] + 40, oy[1] + 50, PW - 80, 170 };
            SDL_RenderFillRect(renderer, &big);

            // Gradient strip: one slice per pixel column, marker shows the current eased t
            const float sx = ox[1] + 40, sw = PW - 80, sy = oy[1] + 250;
            for (int i = 0; i < (int)sw; ++i) {
                float s = i / (sw - 1.0f);
                setColor(renderer, lerpf(colA[0], colB[0], s), lerpf(colA[1], colB[1], s), lerpf(colA[2], colB[2], s));
                SDL_RenderLine(renderer, sx + i, sy, sx + i, sy + 40);
            }
            float mx = sx + clampf(te, 0.0f, 1.0f) * (sw - 1.0f);
            setColor(renderer, 1, 1, 1);
            SDL_RenderLine(renderer, mx, sy - 8, mx, sy + 48);
            fillSquare(renderer, mx, sy - 10, 8.0f);

            setColor(renderer, 0.7f, 0.7f, 0.7f);
            std::snprintf(buf, sizeof(buf), "rgb = (%.2f, %.2f, %.2f)", cr, cg, cb);
            label(renderer, ox[1] + 10, oy[1] + PH - 18, buf);
        }

        // --- Panel 3: lerp vs slerp on the unit circle ---
        {
            const float cx = ox[2] + PW * 0.5f, cy = oy[2] + PH * 0.5f + 8.0f, R = 125.0f;
            setColor(renderer, 1, 1, 1);
            label(renderer, ox[2] + 10, oy[2] + 10, "3. Lerp vs slerp");

            setColor(renderer, 0.3f, 0.32f, 0.38f);
            drawCircle(renderer, cx, cy, R);

            Vec2 a{ cosf(angA * DEG2RAD), sinf(angA * DEG2RAD) };
            Vec2 b{ cosf(angB * DEG2RAD), sinf(angB * DEG2RAD) };
            auto toScreen = [&](Vec2 v) { return Vec2{ cx + v.x * R, cy - v.y * R }; };

            // Sample dots at eased steps for both methods
            for (int i = 0; i <= 24; ++i) {
                float s = ease(easeMode, i / 24.0f);
                Vec2 pl = toScreen(lerp(a, b, s)), ps = toScreen(slerp(a, b, s));
                setColor(renderer, 0.9f, 0.35f, 0.3f, 0.6f); fillSquare(renderer, pl.x, pl.y, 3.0f);
                setColor(renderer, 0.3f, 0.9f, 0.4f, 0.6f);  fillSquare(renderer, ps.x, ps.y, 3.0f);
            }

            Vec2 vl = lerp(a, b, te), vs = slerp(a, b, te);
            Vec2 pl = toScreen(vl), ps = toScreen(vs);
            setColor(renderer, 0.9f, 0.35f, 0.3f); SDL_RenderLine(renderer, cx, cy, pl.x, pl.y); fillSquare(renderer, pl.x, pl.y, 12.0f);
            setColor(renderer, 0.3f, 0.9f, 0.4f);  SDL_RenderLine(renderer, cx, cy, ps.x, ps.y); fillSquare(renderer, ps.x, ps.y, 12.0f);

            Vec2 sa = toScreen(a), sb = toScreen(b);
            setColor(renderer, 0.3f, 0.6f, 1.0f); fillSquare(renderer, sa.x, sa.y, 10.0f);
            setColor(renderer, 1.0f, 0.8f, 0.2f); fillSquare(renderer, sb.x, sb.y, 10.0f);

            setColor(renderer, 0.9f, 0.5f, 0.45f);
            std::snprintf(buf, sizeof(buf), "lerp length = %.3f", sqrtf(vl.x * vl.x + vl.y * vl.y));
            label(renderer, ox[2] + 10, oy[2] + PH - 30, buf);
            setColor(renderer, 0.45f, 0.9f, 0.55f);
            std::snprintf(buf, sizeof(buf), "slerp length = %.3f", sqrtf(vs.x * vs.x + vs.y * vs.y));
            label(renderer, ox[2] + 10, oy[2] + PH - 18, buf);
        }

        // --- Panel 4: quaternion slerp ---
        {
            const float cx = ox[3] + PW * 0.5f, cy = oy[3] + PH * 0.5f;
            setColor(renderer, 1, 1, 1);
            label(renderer, ox[3] + 10, oy[3] + 10, "4. Quaternion slerp");

            Quat qa = fromEuler({ eulA[0], eulA[1], eulA[2] });
            Quat qb = fromEuler({ eulB[0], eulB[1], eulB[2] });

            if (showGhosts) {
                drawCube(renderer, qa, cx, cy, 55.0f, false, 0.3f, 0.6f, 1.0f, 0.35f);
                drawCube(renderer, qb, cx, cy, 55.0f, false, 1.0f, 0.8f, 0.2f, 0.35f);
            }
            if (compareNlerp)
                drawCube(renderer, nlerp(qa, qb, te, shortestPath), cx, cy, 55.0f, false, 0.9f, 0.35f, 0.3f, 0.9f);
            drawCube(renderer, qslerp(qa, qb, te, shortestPath), cx, cy, 55.0f, true, 0.3f, 0.9f, 0.4f, 1.0f);

            setColor(renderer, 0.7f, 0.7f, 0.7f);
            std::snprintf(buf, sizeof(buf), "dot(qA, qB) = %.3f%s", qdot(qa, qb),
                (qdot(qa, qb) < 0 ? "  (long way unless shortest)" : ""));
            label(renderer, ox[3] + 10, oy[3] + PH - 30, buf);
            setColor(renderer, 0.45f, 0.9f, 0.55f);
            label(renderer, ox[3] + 10, oy[3] + PH - 18, compareNlerp ? "green = slerp, red = nlerp" : "green = slerp");
        }

        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    MIX_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}