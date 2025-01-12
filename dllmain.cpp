// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <iostream>
#include <d3d9.h>
#include <d3dx9.h>

#include "detours.h"
#include "offsets.h"


#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "detours.lib")


#define MAX_LIST_SIZE 10000//lol
#define CONST_FOV 1.736801744f//use fov 75
#define STEP_DEFAULT 0.1f

HINSTANCE DllHandle;
DWORD BASE;
HWND wnd;

float MIN_W = 0.1f;


typedef HRESULT(__stdcall* endScene)(IDirect3DDevice9* pDevice);
endScene pEndScene;

typedef unsigned int(__stdcall* loopTop)(int a1, int a2, int start);
loopTop pLoopTop;

typedef int* __cdecl sub_60072B70(float* a1, float* a2, float* a3);
sub_60072B70* MatrixCalc = (sub_60072B70*)0x60072B70;

int ww, wh;//height and width of the window
const char* credits = "Made by skyrimfus. Thanks to Heebo for helping me figure out rotations and view matrix hell. D3D hook by CasualGamer";



struct sSettings {
    bool bDraw;//controls whether the triggers should be drawn or not
    bool bDrawOffscreen;//should we draw points that go offscreen?
    bool bDebug; //enable debug info, limit to 1 line render on only 1 trigger(the first)
    int line_width;
    float maxRenderDistance;
};

sSettings settings = {
    true,//bDraw
    true,//bDrawOffscreen
    false,//bDebug
    5,//line_width
    -STEP_DEFAULT,//maxRenderDistance
};
LPD3DXFONT font;
ID3DXLine* line;

IDirect3DDevice9* GLOBAL_pDevice;



//Implies 75 fov:
D3DXMATRIX matrix_constant = { 0.5904204249f, 0.000000000f, 0.00000000000f, 0.00000000f,
                               0.00000000f, CONST_FOV, 0.00000000000f, 0.00000000f,
                               0.00000000f, 0.000000000f, 1.00010001700f, 1.00000000f,
                               0.00000000f, 0.000000000f, -0.1200120002f, 0.00000000f };



D3DXMATRIX* matrix_test = (D3DXMATRIX*)0x60989FC8;

struct vec4 {
    float x, y, z, w;
};

struct vec3 {
    float x, y, z;
};

vec3 operator+(vec3 a, vec3 b) {
    vec3 r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    return r;
}

vec3 operator-(vec3 a, vec3 b) {
    vec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}





struct vec3_scalar {
    float x, y, z, w, h, d;
};

struct vec2 {
    float x, y;
};

struct exodus_trigger {
    vec3 pos;
    vec3 scale;
    float rotation;
    float distance;
  
    int32_t triggered;
  
    char ID[100] = "UNKOWN_ID";
    char type[100] = "UNKOWN_TYPE";

};

struct position_matrix { //This could be made into a 4x4 matrix for cleaner code
    vec3 w;
    float notUsed1;
    vec3 h;
    float notUsed2;
    vec3 d;
    float notUsed3;
    vec3 origin;
};

struct exodus_naitive_sens {

    DWORD unkown1[20];
    char* string_id;//0x50
    DWORD unkown2[13];
    char* string_type;//0x88
};

struct exodus_naitive_trigger {
    /*+0x000*/ DWORD unkown1[3];
    /*+0x00C*/ position_matrix position;//from 0xC to 0x48
    /*+0x048*/ DWORD unknown2[39];
    /*+0x0E4*/ int32_t triggered;
    /*+0x0E8*/ DWORD unknown3[11];
    /*+0x114*/ exodus_naitive_sens** sens;
};

int trigger_count = 0;
struct exodus_trigger trigger_list[MAX_LIST_SIZE];



float Get3Ddistance(vec3* a, vec3* b) {
    vec3 d = *a - *b;
    return sqrtf((d.x * d.x) + (d.y * d.y) + (d.z * d.z));
}


unsigned int __stdcall hookedLoopTop(int a1, int a2, int a3) {//a3 is what we need
    int v4, size;


    if (!settings.bDraw || !a3)
        return pLoopTop(a1, a2, a3);//call original

    v4 = a3;

    size = ((*(int*)(a3 + 8)) - (*(int*)(a3 + 4))) >> 2;
    exodus_naitive_trigger** naitive_list = (exodus_naitive_trigger**)*(int*)(a3 + 4);//fuck you c++
    if (!naitive_list)
        return pLoopTop(a1, a2, a3);//call original

    if (settings.bDebug)
        size = 0;

    trigger_count = 0;
    if (size > MAX_LIST_SIZE) {
        while (true) {
            std::cout << "error calculated size (" << size << ") is bigger than the array \n";
        }
    }
  
    vec3 playerpos = {0, 0, 0};
    if (*(int*)a_player_pos && **(int**)a_player_pos) {
        float* playerpos_tmp = (float*)((**(int**)a_player_pos) + 0x228);

        playerpos.x = playerpos_tmp[0];
        playerpos.y = playerpos_tmp[4];
        playerpos.z = playerpos_tmp[8];

    }

    for (int i = 0; i <= size; i++) {
        exodus_naitive_trigger* sensor = naitive_list[i];
        if (!sensor)
            continue;



        trigger_list[i].distance = Get3Ddistance(&playerpos, &(sensor->position.origin));


        trigger_list[i].pos.x = sensor->position.origin.x;
        trigger_list[i].pos.y = sensor->position.origin.y;
        trigger_list[i].pos.z = sensor->position.origin.z;




        trigger_list[i].scale.x = sqrtf(pow(sensor->position.w.x, 2) + pow(sensor->position.w.y, 2) + pow(sensor->position.w.z, 2));
        trigger_list[i].scale.y = sqrtf(pow(sensor->position.h.x, 2) + pow(sensor->position.h.y, 2) + pow(sensor->position.h.z, 2));
        trigger_list[i].scale.z = sqrtf(pow(sensor->position.d.x, 2) + pow(sensor->position.d.y, 2) + pow(sensor->position.d.z, 2));

        trigger_list[i].rotation = atan2f(sensor->position.w.z, sensor->position.w.x);
        trigger_list[i].triggered = sensor->triggered;


        if (sensor->sens) {
            exodus_naitive_sens* sens = *(sensor->sens);
            if (sens) {
                strcpy_s(trigger_list[i].ID  , 99, sens->string_id);
                strcpy_s(trigger_list[i].type, 99, sens->string_type);                

            }
        }



        trigger_count++;
    }


    return pLoopTop(a1, a2, a3);//call original
}





bool WorldToScreen(vec3 pos, float* matrix, vec2& out, int windowWidth, int windowHeight) {


    D3DXMATRIX matrix_temp;
    memcpy(matrix_temp, matrix_test, sizeof(D3DXMATRIX));
    MatrixCalc((float*)matrix_temp, (float*)matrix_temp, (float*)matrix_constant);



    matrix = (float*)matrix_temp;
    struct vec4 clipCoords;
    /*
    clipCoords.x = pos.x * matrix[0]  + pos.y * matrix[1]  + pos.z * matrix[2]  + matrix[3];
    clipCoords.y = pos.x * matrix[4]  + pos.y * matrix[5]  + pos.z * matrix[6]  + matrix[7];
    clipCoords.z = pos.x * matrix[8]  + pos.y * matrix[9]  + pos.z * matrix[10] + matrix[11];
    clipCoords.w = pos.x * matrix[12] + pos.y * matrix[13] + pos.z * matrix[14] + matrix[15];
    */

    clipCoords.x = pos.x * matrix[0] + pos.y * matrix[4] + pos.z * matrix[8] + matrix[12];
    clipCoords.y = pos.x * matrix[1] + pos.y * matrix[5] + pos.z * matrix[9] + matrix[13];
    clipCoords.z = pos.x * matrix[2] + pos.y * matrix[6] + pos.z * matrix[10] + matrix[14];
    clipCoords.w = pos.x * matrix[3] + pos.y * matrix[7] + pos.z * matrix[11] + matrix[15];

    vec3 NDC;
    NDC.x = clipCoords.x / clipCoords.w;
    NDC.y = clipCoords.y / clipCoords.w;
    NDC.z = clipCoords.z / clipCoords.w;

    out.x = (windowWidth / 2 * NDC.x) + (NDC.x + windowWidth / 2);
    out.y = -(windowHeight / 2 * NDC.y) + (NDC.y + windowHeight / 2);

    if (clipCoords.w < MIN_W) {
        if (settings.bDebug) {
            printf("W2S FALSE -- X:%.2f, %Y:.%2f NDC.Z = %.2f\n", out.x, out.y, NDC.z);
            return true;

        }
        return false;
    }

    return true;
}

bool WorldToScreenW(vec3 pos, vec2& out) {
    return WorldToScreen(pos, (float*)a_view_matrix, out, ww, wh);
}

void rotate_y(vec3& point, vec3 pivot, float rotation) {
    vec3 n;
    vec3 d = point - pivot;

    float c = cosf(rotation);
    float s = sinf(rotation);

    n.x = d.x * c - d.z * s;
    n.z = d.z * c + d.x * s;

    n = n + pivot;

    point.x = n.x;
    point.z = n.z;
}


void findScreenPosition(vec3 good, vec3 bad, vec2 &out) {
    vec2 sc;
    if (WorldToScreenW(bad, sc)) {
        out.x = sc.x;
        out.y = sc.y;
        return;
    }

    vec3 d;
    d = bad - good;
    bad.x = good.x + d.x / 2;
    bad.y = good.y + d.y / 2;
    bad.z = good.z + d.z / 2;
    findScreenPosition(good, bad, out);


}

void drawLine(vec2 a, vec2 b, D3DCOLOR color) {
    if (settings.bDebug && !(a.x >= 0 && a.x <= ww && a.y >= 0 && a.y <= wh && b.x >= 0 && b.x <= ww && b.y >= 0 && b.y <= wh))
        printf("x1: %.2f, y1: %.2f  ---  x2: %.2f, y2: %.2f  --- W:%i, %H:%i\n", a.x, a.y, b.x, b.y, ww, wh);
    if (!settings.bDrawOffscreen && !(a.x >= 0 && a.x <= ww && a.y >= 0 && a.y <= wh && b.x >= 0 && b.x <= ww && b.y >= 0 && b.y <= wh))
        return;

    D3DXVECTOR2  vert[2] = { D3DXVECTOR2{(float)a.x,(float)a.y}, D3DXVECTOR2{(float)b.x, (float)b.y} };
    line->Draw(vert, 2, color);
}

void drawLine3D(vec3 a, vec3 b, D3DCOLOR color) {
    vec2 a_out, b_out;
    bool w2s_a, w2s_b;
    
    w2s_a = WorldToScreenW(a, a_out);
    w2s_b = WorldToScreenW(b, b_out);

    if (!w2s_a && !w2s_b)
        return;
    if (w2s_a && w2s_b) {
        drawLine(a_out, b_out, color);
        return;
    }

    vec3 good_point, bad_point;
    vec2 screen, good_screen;
    if (w2s_a) {
        good_point = a;
        good_screen = a_out;
        bad_point = b;
    }
    else {
        good_point = b;
        good_screen = b_out;
        bad_point = a;
    }

    findScreenPosition(good_point, bad_point, screen);

    drawLine(screen, good_screen, color);
}




void drawBoxRotated(vec3 RightTopFront, vec3 LeftBottomBack, vec3 pivot, vec3 scale, float rotation, D3DCOLOR color) {
    vec3 points[8];

    vec3 delta = RightTopFront - LeftBottomBack;

    for (int i = 0; i < 8; i++) {
        points[i].x = LeftBottomBack.x;
        points[i].y = LeftBottomBack.y;
        points[i].z = LeftBottomBack.z;
    }

    points[0].y += delta.y;

    points[1].x += delta.x;
    points[1].y += delta.y;

    points[2].x += delta.x;

    points[3].z += delta.z;

    points[4].x += delta.x;
    points[4].z += delta.z;

    points[5].y += delta.y;
    points[5].z += delta.z;

    points[6] = RightTopFront;
    points[7] = LeftBottomBack;




    vec2 sc[8];
    bool w2s[8];
    for (int i = 0; i < 8; i++) {
        vec3 delta2 = points[i] - pivot;
        points[i].x = pivot.x + delta2.x * scale.x * 2;
        points[i].y = pivot.y + delta2.y * scale.y * 2;
        points[i].z = pivot.z + delta2.z * scale.z * 2;


        rotate_y(points[i], pivot, rotation);
        if (!settings.bDebug)
            w2s[i] = WorldToScreenW(points[i], sc[i]);
    }

    if (settings.bDebug) {
        w2s[6] = true;
        WorldToScreenW(points[6], sc[6]);

        w2s[1] = true;
        WorldToScreenW(points[1], sc[1]);
    }

    D3DXVECTOR2  vert[2];

    //D3DCOLOR color = D3DCOLOR_ARGB(255, 0, 255, 0);

    /*
    if (!settings.bDebug) {

        if (w2s[0] && w2s[1]) { drawLine(sc[0], sc[1], color); }
        if (w2s[2] && w2s[1]) { drawLine(sc[2], sc[1], color); }
        if (w2s[2] && w2s[7]) { drawLine(sc[2], sc[7], color); }
        if (w2s[7] && w2s[0]) { drawLine(sc[7], sc[0], color); }



        if (w2s[7] && w2s[3]) { drawLine(sc[7], sc[3], color); }
        if (w2s[4] && w2s[3]) { drawLine(sc[4], sc[3], color); }
        if (w2s[4] && w2s[2]) { drawLine(sc[4], sc[2], color); }

        if (w2s[6] && w2s[4]) { drawLine(sc[6], sc[4], color); }
        if (w2s[6] && w2s[5]) { drawLine(sc[6], sc[5], color); }
        if (w2s[3] && w2s[5]) { drawLine(sc[3], sc[5], color); }

        if (w2s[5] && w2s[0]) { drawLine(sc[5], sc[0], color); }
    }
    if (w2s[6] && w2s[1]) { drawLine(sc[6], sc[1], color); }

    */

    if (!settings.bDebug) {
        drawLine3D(points[0], points[1], color);
        drawLine3D(points[2], points[1], color);
        drawLine3D(points[2], points[7], color);
        drawLine3D(points[7], points[0], color);
             
             
             
        drawLine3D(points[7], points[3], color);
        drawLine3D(points[4], points[3], color);
        drawLine3D(points[4], points[2], color);
                
        drawLine3D(points[6], points[4], color);
        drawLine3D(points[6], points[5], color);
        drawLine3D(points[3], points[5], color);

        drawLine3D(points[5], points[0], color);
    }
    drawLine3D(points[6], points[1], color);
                                                            








}



HRESULT __stdcall hookedEndScene(IDirect3DDevice9* pDevice) {
    if (!settings.bDraw)
        return pEndScene(pDevice); // call original endScene 
    GLOBAL_pDevice = pDevice;

    D3DVIEWPORT9 Viewport;
    pDevice->GetViewport(&Viewport);
    int vpw = Viewport.Width;
    int vph = Viewport.Height;
    matrix_constant[0] = CONST_FOV / ((float)vpw / (float)vph);



    //now here we can create our own graphics
    int padding = 2;
    int rectx1 = 100, rectx2 = 300, recty1 = 50, recty2 = 100;
    D3DRECT rectangle = { rectx1, recty1, rectx2, recty2 };
    //pDevice->Clear(1, &rectangle, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 0.0f, 0); // this draws a rectangle
    if (!font)
        D3DXCreateFont(pDevice, 16, 0, FW_BOLD, 1, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &font);
    if (!line)
        D3DXCreateLine(pDevice, &line);

    RECT textRectangle;


    line->SetWidth(settings.line_width);

    vec3 size = { 0.5, 0.5, 0.5 };
    vec3 scale = { 1.93, 1.42, 2.61 };



    for (int i = 0; i <= trigger_count - 1; i++) {


        if (settings.maxRenderDistance > 0 && trigger_list[i].distance > settings.maxRenderDistance)
            continue;

        int x, y, w, h;
        D3DCOLOR color;
        vec2 sc;
        // vec2 sc2;
         //if (!WorldToScreen(trigger_list[i].pos, (float*)a_view_matrix, &sc, ww, wh))
        if (!WorldToScreenW(trigger_list[i].pos, sc))
            continue;

        x = sc.x;
        y = sc.y;
        w = 100;
        h = 20;

        if (trigger_list[i].triggered == 1)
            color = D3DCOLOR_ARGB(255, 0, 0, 255);
        else
            color = D3DCOLOR_ARGB(255, 0, 255, 0);

        //sc2.x = sc.x + 1;
       // sc2.y = sc.y + 1;
        //drawLine(sc,sc2, D3DCOLOR_ARGB(255, 0, 255,0));

        SetRect(&textRectangle, x, y, x + w, y + h);
        font->DrawText(NULL, trigger_list[i].ID, -1, &textRectangle, DT_NOCLIP | DT_LEFT, D3DCOLOR_ARGB(255, 255, 0, 66)); //draw text;

        y += 20;
        SetRect(&textRectangle, x, y, x + w, y + h);
        font->DrawText(NULL, trigger_list[i].type, -1, &textRectangle, DT_NOCLIP | DT_LEFT, D3DCOLOR_ARGB(255, 255, 0, 66)); //draw text;

        /*distance text
        y += 20;
        char tempDist[20];
        sprintf_s(tempDist,19, "%.2f", trigger_list[i].distance);
        SetRect(&textRectangle, x, y, x + w, y + h);
        font->DrawText(NULL, tempDist, -1, &textRectangle, DT_NOCLIP | DT_LEFT, D3DCOLOR_ARGB(255, 255, 0, 66)); //draw text;
        */

        //font->DrawText(NULL, "Press Numpad0 to Exit", -1, &textRectangle, DT_NOCLIP | DT_LEFT, D3DCOLOR_ARGB(255, 153, 255, 153)); //draw text;


        //darw boxes::
        vec3 c1 = trigger_list[i].pos - size;
        vec3 c2 = trigger_list[i].pos + size;
        drawBoxRotated(c1, c2, trigger_list[i].pos, trigger_list[i].scale, trigger_list[i].rotation, color);
    }



    //try to render in 3d space:


    return pEndScene(pDevice); // call original endScene 
}

void hookEndScene() {
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION); // create IDirect3D9 object
    if (!pD3D)
        return;

    D3DPRESENT_PARAMETERS d3dparams = { 0 };
    d3dparams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dparams.hDeviceWindow = GetForegroundWindow();
    d3dparams.Windowed = true;

    IDirect3DDevice9* pDevice = nullptr;

    HRESULT result = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dparams.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dparams, &pDevice);
    if (FAILED(result) || !pDevice) {
        pD3D->Release();
        return;
    }
    //if device creation worked out -> lets get the virtual table:
    void** vTable = *reinterpret_cast<void***>(pDevice);

    //now detour:

    pEndScene = (endScene)DetourFunction((PBYTE)vTable[42], (PBYTE)hookedEndScene);

    //get window size:
    wnd = FindWindowA(NULL, "Exodus from the earth");
    RECT cl;
    GetClientRect(wnd, &cl);
    ww = cl.right - cl.left;
    wh = cl.bottom - cl.top;



    pDevice->Release();
    pD3D->Release();


    //detour top of func:
    pLoopTop = (loopTop)DetourFunction((PBYTE)a_hook_loop_top, (PBYTE)hookedLoopTop);
}


DWORD __stdcall EjectThread(LPVOID lpParameter) {
    Sleep(100);
    FreeLibraryAndExitThread(DllHandle, 0);
    return 0;
}

DWORD WINAPI Menue(HINSTANCE hModule) {

    BASE = (DWORD)GetModuleHandleA(NULL);
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout); //sets cout to be used with our newly created console

    hookEndScene();

    bool bVmPatched = false;

    while (true) {
        Sleep(50);
        if (GetAsyncKeyState(VK_NUMPAD0)) {
            DetourRemove((PBYTE)pEndScene, (PBYTE)hookedEndScene); //unhook to avoid game crash
            DetourRemove((PBYTE)pLoopTop, (PBYTE)hookedLoopTop); //unhook to avoid game crash
            break;
        }
        if (GetAsyncKeyState(VK_F1) & 1) {
            settings.bDraw = !settings.bDraw;
            std::cout << "Drawing is " << (settings.bDraw ? "enabled\n" : "disabled\n");
        }
        if (GetAsyncKeyState(VK_F2) & 1) {
            settings.bDrawOffscreen = !settings.bDrawOffscreen;
            std::cout << "Drawing of off-screen points  " << (settings.bDrawOffscreen ? "enabled\n" : "disabled\n");
        }
        if (GetAsyncKeyState(VK_F3) & 1) {
            settings.bDebug = !settings.bDebug;
            std::cout << "debug  " << (settings.bDebug ? "enabled\n" : "disabled\n");
        }
        if (GetAsyncKeyState(VK_UP) & 1) {
            settings.line_width++;
        }
        if (GetAsyncKeyState(VK_DOWN) & 1 && settings.line_width > 0) {
            settings.line_width--;
        }
        if (GetAsyncKeyState(VK_LEFT) & 1) {
            float step = -STEP_DEFAULT;
            if (GetAsyncKeyState(VK_CONTROL))
                step *= 10;
            if (GetAsyncKeyState(VK_SHIFT))
                step *= 100;

            settings.maxRenderDistance += step;
            printf("Render distance: %.2f\n", settings.maxRenderDistance);
        }
        if (GetAsyncKeyState(VK_RIGHT) & 1) {
            float step = STEP_DEFAULT;
            if (GetAsyncKeyState(VK_CONTROL))
                step *= 10;
            if (GetAsyncKeyState(VK_SHIFT))
                step *= 100;

            settings.maxRenderDistance += step;
            printf("Render distance: %.2f\n", settings.maxRenderDistance);
        }

        /*
        if (!bVmPatched && GetAsyncKeyState(VK_F2)) {
            std::cout << "This may fix the display, but I am not sure how many other things it will break\n";
            *(DWORD*)a_VM_FIX_PATCH = 0x0004B866;//mov ax,4
            bVmPatched = true;
        }
        */
    }
    std::cout << "ight imma head out" << std::endl;
    Sleep(1000);
    fclose(fp);
    FreeConsole();
    CreateThread(0, 0, EjectThread, 0, 0, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DllHandle = hModule;
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Menue, NULL, 0, NULL);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;

}

