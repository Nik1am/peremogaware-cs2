#include "render.h"
#include "settings.h"
#include <thread>
#include <string>
#include "sharedData.h"

#include "busr.h"

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

DWORD64 ImVec4toABGR(ImVec4 color) {
    UCHAR r = (UCHAR)(color.x * 255);
    UCHAR g = (UCHAR)(color.y * 255);
    UCHAR b = (UCHAR)(color.z * 255);
    UCHAR a = 0xFF; (UCHAR)(color.w * 255);
    UCHAR outc[4] = { r,g,b,a };
    return 0xFF000000 + (r << 0x0) + (g << 0x8) + (b << 0x10);
}

struct text_color {
    ImColor black = ImColor(0.f, 0.f, 0.f);
    ImColor red = ImColor(1.f, 0.f, 0.f);
    ImColor green = ImColor(0.f, 1.f, 0.f);
    ImColor blue = ImColor(0.f, 0.f, 1.f);
    ImColor yellow = ImColor(1.f, 1.f, 0.f);
    ImColor cyan = ImColor(0.f, 1.f, 1.f);
    ImColor magenta = ImColor(1.f, 0.f, 1.f);
    ImColor white = ImColor(1.f, 1.f, 1.f);
} text_color;

ImVec2 Vector3_to_ImVec2(Vector3 v, float* depth = nullptr) {
    Vector3 vec = v.WTS(dwViewMatrix, 1920.f, 1080.f);
    return ImVec2(vec.x, vec.y);
}

void render_menu() {
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(
        WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED,
        wc.lpszClassName, 
        L"Peremogaware", 
        WS_POPUP, 
        0, 
        0, 
        1920, 
        1080, 
        nullptr, 
        nullptr, 
        wc.hInstance, 
        nullptr
    );
    SetLayeredWindowAttributes(hwnd, RGB(0,0,0), BYTE(255), LWA_ALPHA);


    {
        RECT client_area{};
        GetClientRect(hwnd, &client_area);

        RECT window_area{};
        GetClientRect(hwnd, &window_area);

        POINT diff{};

        ClientToScreen(hwnd, &diff);

        const MARGINS margins{
            window_area.left + (diff.x - window_area.left),
            window_area.top + (diff.y - window_area.top),
            client_area.right,
            client_area.bottom
        };

        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->GetGlyphRangesCyrillic();
    ImFont* arial = io.Fonts->AddFontFromFileTTF("arial.ttf", 16, nullptr, io.Fonts->GetGlyphRangesCyrillic()); 
    ImFont* squares = io.Fonts->AddFontFromFileTTF("squares.otf", 48, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);


    // Our state
    ImVec4 imvec4_glow_color_aim_targer = ImVec4(1.f, 1.f, 0.0f, 1.00f);

    ImColor ESPaimTracerColor = ImColor(1.f, 1.f, 0.f);

    ImColor ESPenemyTracerColor = ImColor(1.f, 0.f, 0.f);
    ImColor ESPenemyTextColor = ImColor(1.f, 1.f, 1.f);
    ImColor ESPenemySkeletonColor = ImColor(1.f, 1.f, 1.f);

    ImColor ESPfriendlyTracerColor = ImColor(0.f, 1.f, 0.f);
    ImColor ESPfriendlyTextColor = ImColor(1.f, 1.f, 1.f);
    ImColor ESPfriendlySkeletonColor = ImColor(1.f, 1.f, 1.f);
     
    ImColor meshfillcolor = ImColor(1.f, 1.f, 1.f, 0.1f);
    // Main loop
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        LONG_PTR windowStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        if (d_showmenu)
        {
            windowStyle &= ~WS_EX_TRANSPARENT;
            SetForegroundWindow(hwnd);
            static float f = 0.0f;
            static int counter = 0;
            ImGui::Begin("Peremogaware");

            ImGui::PushFont(squares);
            ImGui::Text("Peremogaware v1");
            ImGui::PopFont();

            ImGui::Checkbox("bhop", &bhop);
            ImGui::Checkbox("autostrafe", &autostrafe);
            ImGui::SliderFloat("strafe factor", &factor, 0.0f, 1.0f);
            ImGui::Checkbox("aim", &aim);
            ImGui::Checkbox("aim safe mode", &aimsafemode);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("Safe mode uses mouse_event\nUnsafe writes angles directly to memory");
            }
            const char* aimmethodsname[] = { "no prediction", "linear", "3 point" };
            ImGui::Combo("aim prediction method", &aimpredictionmethod, aimmethodsname, IM_ARRAYSIZE(aimmethodsname));
            ImGui::Checkbox("trigger", &trigger);
            ImGui::Checkbox("antiflash", &antiflash);
            ImGui::Checkbox("fovchanger", &fovchanger);

            ImGui::SliderFloat("flash_opacity", &flash_opacity, 0.0f, 1.0f);
            ImGui::SliderInt("fov", &fov, 0, 180);
            ImGui::SliderInt("d_boneoffset", &d_boneoffset, 0, 124);
            ImGui::ColorEdit3("glow_color_aim_targerr", (float*)&imvec4_glow_color_aim_targer);
            ImGui::ColorEdit4("meshfillcolor", (float*)&meshfillcolor);
            glow_color_aim_targer = ImVec4toABGR(imvec4_glow_color_aim_targer); 
            ImGui::SliderFloat("aim_smooth_factor", &aim_smooth_factor, 0.0f, 1.0f);
            ImGui::SliderFloat("aim_predict", &aim_predict, 0.0f, 4.0f);
            ImGui::Checkbox("d_aimalways", &d_aimalways);
            ImGui::Checkbox("aim_ignore_if_spotted", &aim_ignore_if_spotted);
            ImGui::Checkbox("aim_ignore_team", &aim_ignore_team);
            ImGui::ColorEdit3("ESPaimTracerColor", (float*)&ESPaimTracerColor);
            ImGui::ColorEdit3("ESPenemyTracerColor", (float*)&ESPenemyTracerColor);
            ImGui::ColorEdit3("ESPenemyTextColor", (float*)&ESPenemyTextColor);
            ImGui::ColorEdit3("ESPenemySkeletonColor", (float*)&ESPenemySkeletonColor);
            ImGui::ColorEdit3("ESPfriendlyTracerColor", (float*)&ESPfriendlyTracerColor);
            ImGui::ColorEdit3("ESPfriendlyTextColor", (float*)&ESPfriendlyTextColor);
            ImGui::ColorEdit3("ESPfriendlySkeletonColor", (float*)&ESPfriendlySkeletonColor);
            ImGui::Checkbox("show console", &d_showconsole);

            ImGui::Checkbox("vis thread sleep", &visthreadsleep);
            ImGui::Checkbox("fake angle", &fakeangle);
            ImGui::SliderFloat("fakeangleyaw", &fakeangleyaw, -180.0f, 180.0f);

            //if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            //    counter++;
            //ImGui::SameLine();
            //ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

            if (ImGui::Button("Close")) {
                isrunning = 0;
                done = 1;
            }
            ImGui::End();
        }
        else {
            windowStyle |= WS_EX_TRANSPARENT;
        }
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, windowStyle);

        if (GetAsyncKeyState(VK_INSERT) & 1) {
            d_showmenu = !d_showmenu;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // overlay
        ImGui::GetBackgroundDrawList()->AddText({0, 0}, ImColor(1.f, 1.f, 1.f), map_name);
        for (int i = 0; i < 64; i++) {
            player plr = plrList[i];
            Vector3 boneScreenPos = plr.bonePosPred.WTS(dwViewMatrix, 1920.f, 1080.f);
            Vector3 originScreenPos = plr.origin.WTS(dwViewMatrix, 1920.f, 1080.f);

            //that fucked up locator
            if (!plr.isLocalPlayer && plr.m_iHealth > 0) {
                ImGui::GetBackgroundDrawList()->AddText(ImVec2((1920.f / 2) + sin(plr.deltaYaw) * 64, (1080.f / 2) + cos(plr.deltaYaw) * -64), text_color.cyan, "+");
            }

            if (!plr.isLocalPlayer && originScreenPos.z >= 0.f && !plr.bonePos.IsZero() && (plr.m_iHealth > 0)) {
                ImColor SkeletonColor;
                if (player_team != plr.m_iTeamNum) {
                    SkeletonColor = ESPenemySkeletonColor;
                    if (plr.isAimTarget) {
                        ImGui::GetBackgroundDrawList()->AddLine({ 1920 / 2,1080 / 2 }, ImVec2(boneScreenPos.x, boneScreenPos.y), ESPaimTracerColor);
                    }
                    else {
                        ImGui::GetBackgroundDrawList()->AddLine({ 1920 / 2,1080 / 2 }, ImVec2(boneScreenPos.x, boneScreenPos.y), ESPenemyTracerColor);
                    }
                    ImGui::GetBackgroundDrawList()->AddText(ImVec2(originScreenPos.x+1, originScreenPos.y+1), text_color.black, std::to_string(plr.m_iHealth).c_str());
                    ImGui::GetBackgroundDrawList()->AddText(ImVec2(originScreenPos.x, originScreenPos.y), ESPenemyTextColor, std::to_string(plr.m_iHealth).c_str());

                    ImGui::GetBackgroundDrawList()->AddText(ImVec2(originScreenPos.x + 1, originScreenPos.y + 1 + 0x10), text_color.black, plr.name);
                    ImGui::GetBackgroundDrawList()->AddText(ImVec2(originScreenPos.x, originScreenPos.y + 0x10), ESPenemyTextColor, plr.name);

                    ImGui::GetBackgroundDrawList()->AddText(ImVec2(originScreenPos.x + 1, originScreenPos.y + 1 + 0x20), text_color.black, plr.m_szLastPlaceName);
                    ImGui::GetBackgroundDrawList()->AddText(ImVec2(originScreenPos.x, originScreenPos.y + 0x20), ESPenemyTextColor, plr.m_szLastPlaceName);
                    

                    //pelvis to head
                    for (int j = 0; j < 6; j++) {
                        ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[j].origin), Vector3_to_ImVec2(plr.bonelist[j + 1].origin), SkeletonColor);
                    }
                    //hands
                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[7].origin), Vector3_to_ImVec2(plr.bonelist[8].origin), SkeletonColor);
                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[8].origin), Vector3_to_ImVec2(plr.bonelist[9].origin), SkeletonColor);
                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[9].origin), Vector3_to_ImVec2(plr.bonelist[10].origin), SkeletonColor);
                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[10].origin), Vector3_to_ImVec2(plr.bonelist[11].origin), SkeletonColor);

                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[12].origin), Vector3_to_ImVec2(plr.bonelist[13].origin), SkeletonColor);
                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[13].origin), Vector3_to_ImVec2(plr.bonelist[14].origin), SkeletonColor);
                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[14].origin), Vector3_to_ImVec2(plr.bonelist[15].origin), SkeletonColor);
                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[15].origin), Vector3_to_ImVec2(plr.bonelist[16].origin), SkeletonColor);
                    //legs
                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[0].origin), Vector3_to_ImVec2(plr.bonelist[22].origin), SkeletonColor);
                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[22].origin), Vector3_to_ImVec2(plr.bonelist[23].origin), SkeletonColor);
                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[23].origin), Vector3_to_ImVec2(plr.bonelist[24].origin), SkeletonColor);

                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[0].origin), Vector3_to_ImVec2(plr.bonelist[25].origin), SkeletonColor);
                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[25].origin), Vector3_to_ImVec2(plr.bonelist[26].origin), SkeletonColor);
                    ImGui::GetBackgroundDrawList()->AddLine(Vector3_to_ImVec2(plr.bonelist[26].origin), Vector3_to_ImVec2(plr.bonelist[27].origin), SkeletonColor);

                }
                else {
                    SkeletonColor = ESPfriendlySkeletonColor;
                }


            }
        }
        //speedometer
        char veltext[32];
        strcpy_s(veltext, std::to_string(local_velocity.length2d()).c_str());
        float veltext_w = ImGui::CalcTextSize(veltext).x;
        if (local_velocity.length2d() <= 250.f) {
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(960 - (veltext_w / 2), 500), text_color.white, veltext);
        }
        else {
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(960 - (veltext_w / 2), 500), text_color.green, veltext);
        }

        //scope
        float m_fAccuracyPenalty_w = ImGui::CalcTextSize(std::to_string(m_fAccuracyPenalty).c_str()).x;
        ImGui::GetBackgroundDrawList()->AddText(ImVec2(960 - (m_fAccuracyPenalty_w / 2), 580), text_color.green, std::to_string(m_fAccuracyPenalty).c_str());

        float size = 8.f * local_velocity.length() * m_fAccuracyPenalty;
        ImGui::GetBackgroundDrawList()->AddLine({ 1920 / 2 - size,1080 / 2 }, { 1920 / 2 + size,1080 / 2 }, ESPenemyTextColor,2.f);
        ImGui::GetBackgroundDrawList()->AddLine({ 1920 / 2,1080 / 2 -size  }, { 1920 / 2,1080 / 2 +size }, ESPenemyTextColor, 2.f);
        ImGui::GetBackgroundDrawList()->AddRect({ 1920 / 2 - size, 1080 / 2 - size }, { 1920 / 2 + size, 1080 / 2 + size }, ESPenemyTextColor, 0.f, 0, 2.f);

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.f, 0.f, 0.f, 0.f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    isrunning = 0;
}


bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}