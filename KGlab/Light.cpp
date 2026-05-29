#include "Light.h"
#include "MyOGL.h"
#include "Vector3.h"

#include <algorithm>

#include <windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <tuple>

extern OpenGL gl;

std::tuple<double, double, double, double, double, double> getLookRay(int wndX, int wndY)
{
    GLint viewport[4];
    GLdouble projection[16];
    GLdouble modelview[16];
    GLdouble wx, wy, wz;

    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);

    double originX, originY, originZ;
    double directionX, directionY, directionZ;

    gluUnProject(wndX, wndY, 0, modelview, projection, viewport, &wx, &wy, &wz);
    originX = wx;
    originY = wy;
    originZ = wz;

    gluUnProject(wndX, wndY, 1, modelview, projection, viewport, &wx, &wy, &wz);
    directionX = wx;
    directionY = wy;
    directionZ = wz;

    directionX -= originX;
    directionY -= originY;
    directionZ -= originZ;

    double length = sqrt(directionX * directionX + directionY * directionY + directionZ * directionZ);
    directionX /= length;
    directionY /= length;
    directionZ /= length;

    return {originX, originY, originZ, directionX, directionY, directionZ};
}

void Light::SetPosition(double x, double y, double z)
{
    posX = x;
    posY = y;
    posZ = z;
}

void Light::StartDrug(OpenGL* sender, KeyEventArg arg)
{
    if (arg.key == 0x47)
        drag = true;
    if (arg.key == 0x46)
        from_camera = true;
}

void Light::StopDrug(OpenGL* sender, KeyEventArg arg)
{
    if (arg.key == 0x47)
        drag = false;
    if (arg.key == 0x46)
        from_camera = false;
}

void Light::MoveLight(OpenGL* sender, MouseEventArg arg)
{
    if (drag)
    {
        int _x = arg.x;
        int _y = gl.getHeight() - arg.y;

        auto [oX, oY, oZ, dX, dY, dZ] = getLookRay(_x, _y);

        if (!OpenGL::isKeyPressed(VK_LBUTTON))
        {
            double z = posZ;

            double k = 0, x = 0, y = 0;
            if (dZ == 0)
                k = 0;
            else
                k = (z - oZ) / dZ;

            x = k * dX + oX;
            y = k * dY + oY;

            if (x * x + y * y > 2500)
                return;

            posX = x;
            posY = y;
            posZ = z;
        }
        else
        {
            Vector3 o{oX, oY, oZ};
            Vector3 d{dX, dY, dZ};
            Vector3 z{0, 0, 1};

            Vector3 _top = d ^ Vector3(0, 0, 1) ^ d;
            double D = -_top.x() * oX - _top.y() * oY - _top.z() * oZ;

            if (_top.z() == 0)
                posZ = 0;
            else
                posZ = std::clamp(-(_top.x() * posX + _top.y() * posY + D) / _top.z(), -20.0, 20.0);
        }
    }
}

void Light::SetUpLight()
{
    GLfloat lamb[] = {0.2, 0.2, 0.2, 0};
    GLfloat ldif[] = {0.7, 0.7, 0.7, 0};
    GLfloat lspec[] = {1.0, 1.0, 1.0, 0};
    GLfloat lposition[] = {posX, posY, posZ, 1.};

    glLightfv(GL_LIGHT0, GL_POSITION, lposition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lamb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, ldif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lspec);
    glEnable(GL_LIGHT0);
}

void Light::DrawLightGizmo()
{
    GLfloat pointSize;
    glGetFloatv(GL_POINT_SIZE, &pointSize);
    glPointSize(10);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    glBegin(GL_POINTS);
    glColor3d(1, 0.7, 0.1);
    glVertex3d(posX, posY, posZ);
    glEnd();

    glPointSize(pointSize);

    if (!drag)
        return;

    GLfloat lineWidth;
    glGetFloatv(GL_LINE_WIDTH, &lineWidth);
    glLineWidth(3.0);

    glBegin(GL_LINES);
    glColor3d(0, 0, 0.8);
    glVertex3d(posX, posY, posZ);
    glVertex3d(posX, posY, 0);

    glColor3d(0.8, 0, 0);
    glVertex3d(posX - 1, posY, 0);
    glVertex3d(posX + 1, posY, 0);

    glColor3d(0, 0.8, 0);
    glVertex3d(posX, posY - 1, 0);
    glVertex3d(posX, posY + 1, 0);
    glEnd();

    glLineWidth(lineWidth);
}
