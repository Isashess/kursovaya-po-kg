#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#ifdef APIENTRY
#undef APIENTRY
#endif

#include <windows.h>

#ifndef APIENTRY
#define APIENTRY __stdcall
#endif

#include <GL/gl.h>
#include <GL/glu.h>

#include "Render.h"
#include "GUItextRectangle.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>
#include <array>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _DEBUG
#include <Debugapi.h>
struct debug_print
{
    template <class C> debug_print& operator<<(const C& a)
    {
        OutputDebugStringA((std::stringstream() << a).str().c_str());
        return *this;
    }
} debout;
#else
struct debug_print
{
    template <class C> debug_print& operator<<(const C& a)
    {
        return *this;
    }
} debout;
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "MyOGL.h"
extern OpenGL gl;
#include "Light.h"
Light light;
#include "Camera.h"
Camera camera;

bool texturing = true;
bool lightning = true;
bool alpha = false;

void switchModes(OpenGL* sender, KeyEventArg arg)
{
    auto key = LOWORD(MapVirtualKeyA(arg.key, MAPVK_VK_TO_CHAR));
    switch (key)
    {
    case 'L': lightning = !lightning; break;
    case 'T': texturing = !texturing; break;
    case 'A': alpha = !alpha; break;
    }
}

GuiTextRectangle text;

GLuint texId;       // текстура для верха и боковых граней
GLuint texIdBottom; // текстура для нижней крышки

struct Point { double x, y, z; Point() : x(0), y(0), z(0) {} Point(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {} };

void initRender()
{
    //============== НАСТРОЙКА ТЕКСТУР ================
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);

    int x, y, n;
    unsigned char* data = stbi_load("texture.png", &x, &y, &n, 4);
    if (data)
    {
        unsigned char* _tmp = new unsigned char[x * 4];
        for (int i = 0; i < y / 2; ++i)
        {
            std::memcpy(_tmp, data + i * x * 4, x * 4);
            std::memcpy(data + i * x * 4, data + (y - 1 - i) * x * 4, x * 4);
            std::memcpy(data + (y - 1 - i) * x * 4, _tmp, x * 4);
        }
        delete[] _tmp;

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);

        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    texIdBottom = texId; // Если захочешь другую текстуру на дно, загрузи ее сюда

    //============== НАСТРОЙКА КАМЕРЫ И СВЕТА ==============
    camera.caclulateCameraPos();
    gl.WheelEvent.reaction(&camera, &Camera::Zoom);
    gl.MouseMovieEvent.reaction(&camera, &Camera::MouseMovie);
    gl.MouseLeaveEvent.reaction(&camera, &Camera::MouseLeave);
    gl.MouseLdownEvent.reaction(&camera, &Camera::MouseStartDrag);
    gl.MouseLupEvent.reaction(&camera, &Camera::MouseStopDrag);

    gl.MouseMovieEvent.reaction(&light, &Light::MoveLight);
    gl.KeyDownEvent.reaction(&light, &Light::StartDrug);
    gl.KeyUpEvent.reaction(&light, &Light::StopDrug);

    gl.KeyDownEvent.reaction(switchModes);
    text.setSize(512, 180);
    camera.setPosition(2, 1.5, 1.5);
}

void Render(double delta_time)
{
    glEnable(GL_DEPTH_TEST);

    if (gl.isKeyPressed('F'))
        light.SetPosition(camera.x(), camera.y(), camera.z());

    camera.SetUpCamera();
    light.SetUpLight();
    gl.DrawAxes();

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    if (lightning) glEnable(GL_LIGHTING);
    if (texturing) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, 0); }
    if (alpha) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }

    //============= НАСТРОЙКА МАТЕРИАЛА ==============
    float amb[] = { 0.2f, 0.2f, 0.1f, 1.0f };
    float dif[] = { 0.4f, 0.65f, 0.5f, 1.0f };
    float spec[] = { 0.9f, 0.8f, 0.3f, 1.0f };
    float sh = 0.2f * 256;
    glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT, GL_SHININESS, sh);
    glShadeModel(GL_SMOOTH);

    //============ ГЕОМЕТРИЯ ПРИЗМЫ ==============
    const double zb = 0.0, zt = 2.0;

    // Твои базовые точки
    std::vector<std::array<double, 2>> p = {
        {-1.0, 2.0},   // 0: A
        {2.0, 4.0},    // 1: B
        {3.5, 1.0},    // 2: C
        {0.5, -0.5},   // 3: D
        {2.0, -4.0},   // 4: E
        {-1.5, -4.5},  // 5: F
        {-3.5, -1.0},  // 6: G
        {-0.5, 0.0}    // 7: H
    };

    // Вспомогательные переменные (твоя логика)
    double mx = -2.0, my = -2.5;
    double cx = 0.5, cy = -0.5;

    // Расчеты для выпуклой стенки (A-B)
    double midx = (p[0][0] + p[1][0]) / 2.0;
    double midy = (p[0][1] + p[1][1]) / 2.0;
    double rad = hypot(p[1][0] - p[0][0], p[1][1] - p[0][1]) / 2.0;
    double ang = atan2(p[1][1] - midy, p[1][0] - midx);

    // --- АВТОМАТИЧЕСКАЯ UV-РАЗВЕРТКА (Определение границ) ---
    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    auto updateBounds = [&](double x, double y) {
        if (x < minX) minX = x; if (x > maxX) maxX = x;
        if (y < minY) minY = y; if (y > maxY) maxY = y;
        };
    for (auto& pt : p) updateBounds(pt[0], pt[1]);
    for (int s = 0; s <= 20; s++) {
        double t = s / 20.0;
        updateBounds((1 - t) * (1 - t) * p[5][0] + 2 * (1 - t) * t * mx + t * t * p[6][0],
            (1 - t) * (1 - t) * p[5][1] + 2 * (1 - t) * t * my + t * t * p[6][1]);
        updateBounds(midx + rad * cos(ang + t * M_PI), midy + rad * sin(ang + t * M_PI));
    }
    double scaleX = maxX - minX; if (scaleX == 0) scaleX = 1;
    double scaleY = maxY - minY; if (scaleY == 0) scaleY = 1;
    auto getUV = [&](double x, double y) -> std::pair<double, double> {
        return { (x - minX) / scaleX, (y - minY) / scaleY };
        };

    // ==========================================
    //            ОСНОВАНИЯ (ДНО И КРЫШКА)
    // ==========================================
    for (int cap = 0; cap < 2; cap++) {
        double z = (cap == 0) ? zb : zt;

        if (cap == 0) glNormal3d(0, 0, -1); // Нормаль дна смотрит вниз
        else glNormal3d(0, 0, 1);           // Нормаль крышки смотрит вверх

        if (texturing) glBindTexture(GL_TEXTURE_2D, (cap == 0) ? texIdBottom : texId);
        float difCap[] = { 0.4f, 0.65f, 0.5f, (cap == 0) ? 1.0f : 0.6f };
        glMaterialfv(GL_FRONT, GL_DIFFUSE, difCap);

        glBegin(GL_TRIANGLES);
        for (int i = 0; i < 8; i++) {
            int j = (i + 1) % 8;

            if (i == 5) { // Кривая Безье
                for (int s = 0; s < 20; s++) {
                    auto bez = [&](double t) {
                        return std::array<double, 2>{
                            (1 - t)* (1 - t)* p[5][0] + 2 * (1 - t) * t * mx + t * t * p[6][0],
                                (1 - t)* (1 - t)* p[5][1] + 2 * (1 - t) * t * my + t * t * p[6][1]
                        };
                        };
                    auto v1 = bez(s / 20.0), v2 = bez((s + 1) / 20.0);
                    auto uv0 = getUV(cx, cy), uv1 = getUV(v1[0], v1[1]), uv2 = getUV(v2[0], v2[1]);

                    if (cap == 0) { // Дно (обратный порядок вершин)
                        glTexCoord2d(uv0.first, uv0.second); glVertex3d(cx, cy, z);
                        glTexCoord2d(uv2.first, uv2.second); glVertex3d(v2[0], v2[1], z);
                        glTexCoord2d(uv1.first, uv1.second); glVertex3d(v1[0], v1[1], z);
                    }
                    else { // Крышка
                        glTexCoord2d(uv0.first, uv0.second); glVertex3d(cx, cy, z);
                        glTexCoord2d(uv1.first, uv1.second); glVertex3d(v1[0], v1[1], z);
                        glTexCoord2d(uv2.first, uv2.second); glVertex3d(v2[0], v2[1], z);
                    }
                }
            }
            else { // Плоские сегменты
                auto uv0 = getUV(cx, cy), uv1 = getUV(p[i][0], p[i][1]), uv2 = getUV(p[j][0], p[j][1]);
                if (cap == 0) { // Дно
                    glTexCoord2d(uv0.first, uv0.second); glVertex3d(cx, cy, z);
                    glTexCoord2d(uv2.first, uv2.second); glVertex3d(p[j][0], p[j][1], z);
                    glTexCoord2d(uv1.first, uv1.second); glVertex3d(p[i][0], p[i][1], z);
                }
                else { // Крышка
                    glTexCoord2d(uv0.first, uv0.second); glVertex3d(cx, cy, z);
                    glTexCoord2d(uv1.first, uv1.second); glVertex3d(p[i][0], p[i][1], z);
                    glTexCoord2d(uv2.first, uv2.second); glVertex3d(p[j][0], p[j][1], z);
                }
            }
        }
        glEnd();

        // Дорисовка ушка (торца полуцилиндра)
        glBegin(GL_TRIANGLE_FAN);
        auto uvMid = getUV(midx, midy);
        glTexCoord2d(uvMid.first, uvMid.second); glVertex3d(midx, midy, z);

        if (cap == 0) { // Дно
            for (int i = 20; i >= 0; i--) {
                double a = ang + i / 20.0 * M_PI;
                auto uv = getUV(midx + rad * cos(a), midy + rad * sin(a));
                glTexCoord2d(uv.first, uv.second); glVertex3d(midx + rad * cos(a), midy + rad * sin(a), z);
            }
        }
        else { // Крышка
            for (int i = 0; i <= 20; i++) {
                double a = ang + i / 20.0 * M_PI;
                auto uv = getUV(midx + rad * cos(a), midy + rad * sin(a));
                glTexCoord2d(uv.first, uv.second); glVertex3d(midx + rad * cos(a), midy + rad * sin(a), z);
            }
        }
        glEnd();
    }

    // ==========================================
    //               БОКОВЫЕ ГРАНИ
    // ==========================================
    if (texturing) glBindTexture(GL_TEXTURE_2D, texId);
    float difSide[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, difSide);

    double accumU = 0.0; // Для бесшовного обертывания текстуры

    for (int i = 0; i < 8; i++) {
        int j = (i + 1) % 8;

        if (i == 0) {
            // Выпуклая стенка (A-B)
            glBegin(GL_QUAD_STRIP);
            for (int s = 0; s <= 20; s++) {
                double a = ang + (s / 20.0) * M_PI;
                double x = midx + rad * cos(a);
                double y = midy + rad * sin(a);

                glNormal3d(cos(a), sin(a), 0.0); // Нормаль наружу по радиусу

                glTexCoord2d(accumU / 30.0, 0.0); glVertex3d(x, y, zb);
                glTexCoord2d(accumU / 30.0, 1.0); glVertex3d(x, y, zt);
                if (s < 20) accumU += (M_PI * rad) / 20.0;
            }
            glEnd();
        }
        else if (i == 5) {
            // Вогнутая стенка (F-G)
            glBegin(GL_QUAD_STRIP);
            for (int s = 0; s <= 20; s++) {
                double t = s / 20.0;
                double x = (1 - t) * (1 - t) * p[5][0] + 2 * (1 - t) * t * mx + t * t * p[6][0];
                double y = (1 - t) * (1 - t) * p[5][1] + 2 * (1 - t) * t * my + t * t * p[6][1];

                // Вектор касательной и расчет нормали внутрь (чтобы свет работал)
                double dxdt = 2 * (1 - t) * (mx - p[5][0]) + 2 * t * (p[6][0] - mx);
                double dydt = 2 * (1 - t) * (my - p[5][1]) + 2 * t * (p[6][1] - my);
                double len = hypot(dxdt, dydt);
                if (len > 1e-6) glNormal3d(dydt / len, -dxdt / len, 0.0);

                // Чтобы полигоны не вывернулись наизнанку, рисуем сначала верх, потом низ
                glTexCoord2d(accumU / 30.0, 1.0); glVertex3d(x, y, zt);
                glTexCoord2d(accumU / 30.0, 0.0); glVertex3d(x, y, zb);

                if (s < 20) {
                    double n_t = (s + 1) / 20.0;
                    double nx_pt = (1 - n_t) * (1 - n_t) * p[5][0] + 2 * (1 - n_t) * n_t * mx + n_t * n_t * p[6][0];
                    double ny_pt = (1 - n_t) * (1 - n_t) * p[5][1] + 2 * (1 - n_t) * n_t * my + n_t * n_t * p[6][1];
                    accumU += hypot(nx_pt - x, ny_pt - y);
                }
            }
            glEnd();
        }
        else {
            // Плоские стенки
            glBegin(GL_QUADS);
            double dx = p[j][0] - p[i][0];
            double dy = p[j][1] - p[i][1];
            double len = hypot(dx, dy);

            glNormal3d(-dy / len, dx / len, 0.0);

            double u1 = accumU;
            double u2 = accumU + len;
            accumU = u2;

            glTexCoord2d(u1 / 30.0, 0.0); glVertex3d(p[i][0], p[i][1], zb);
            glTexCoord2d(u2 / 30.0, 0.0); glVertex3d(p[j][0], p[j][1], zb);
            glTexCoord2d(u2 / 30.0, 1.0); glVertex3d(p[j][0], p[j][1], zt);
            glTexCoord2d(u1 / 30.0, 1.0); glVertex3d(p[i][0], p[i][1], zt);
            glEnd();
        }
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    //===============================================

    // Рисуем источник света
    light.DrawLightGizmo();

    //================ Сообщение в верхнем левом углу ================
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, gl.getWidth() - 1, 0, gl.getHeight() - 1, 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    std::wstringstream ss;
    ss << std::fixed << std::setprecision(3)
        << "T - " << (texturing ? L"[вкл]выкл" : L"вкл[выкл]") << L" текстур\n"
        << "L - " << (lightning ? L"[вкл]выкл" : L"вкл[выкл]") << L" освещение\n"
        << "A - " << (alpha ? L"[вкл]выкл" : L"вкл[выкл]") << L" альфа-наложение\n"
        << L"F - переместить свет в позицию камеры\n"
        << L"G - двигать свет по горизонтали\n"
        << L"G+ЛКМ - двигать свет по вертикали\n"
        << L"Координаты света: (" << std::setw(7) << light.x() << "," << std::setw(7) << light.y() << "," << std::setw(7) << light.z() << ")\n"
        << L"Координаты камеры: (" << std::setw(7) << camera.x() << "," << std::setw(7) << camera.y() << "," << std::setw(7) << camera.z() << ")\n"
        << L"Параметры камеры: R=" << std::setw(7) << camera.distance() << ", fi1=" << std::setw(7) << camera.fi1() << ", fi2=" << std::setw(7) << camera.fi2() << '\n'
        << L"delta_time: " << std::setprecision(5) << delta_time << std::endl;

    text.setPosition(10, gl.getHeight() - 10 - 180);
    text.setText(ss.str().c_str());
    text.Draw();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}