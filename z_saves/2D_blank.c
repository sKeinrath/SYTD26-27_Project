#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const float c = 299792458.0f / 100000000.0f;
const float k = 8.9875517923e9f;

// ================= Engine ================= //

typedef struct {
    GLFWwindow* window;
    int WIDTH;
    int HEIGHT;
} Engine;

Engine engine = { NULL, 800, 600 };

void engine_init(void)
{
    if (!glfwInit()) {
        fprintf(stderr, "failed to init glfw, PANIC!\n");
        exit(EXIT_FAILURE);
    }

    engine.window = glfwCreateWindow(
        engine.WIDTH,
        engine.HEIGHT,
        "Atom Sim",
        NULL,
        NULL
    );

    if (!engine.window) {
        fprintf(stderr, "failed to create window, PANIC!\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(engine.window);

    glViewport(
        0,
        0,
        engine.WIDTH,
        engine.HEIGHT
    );
}

void engine_run(void)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    double left   = -engine.WIDTH;
    double right  =  engine.WIDTH;
    double bottom = -engine.HEIGHT;
    double top    =  engine.HEIGHT;

    glOrtho(
        left,
        right,
        bottom,
        top,
        -1.0,
        1.0
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void drawCircle(float x, float y, float r, int segments)
{
    glBegin(GL_LINE_LOOP);

    for (int i = 0; i < segments; i++) {

        float angle =
            2.0f * (float)M_PI * (float)i / (float)segments;

        float dx = r * cosf(angle);
        float dy = r * sinf(angle);

        glVertex2f(
            x + dx,
            y + dy
        );
    }

    glEnd();
}

void drawFilledCircle(float x, float y, float r, int segments)
{
    glBegin(GL_TRIANGLE_FAN);

    // Mittelpunkt
    glVertex2f(x, y);

    for (int i = 0; i <= segments; i++) {

        float angle =
            2.0f * (float)M_PI * (float)i / (float)segments;

        float dx = r * cosf(angle);
        float dy = r * sinf(angle);

        glVertex2f(
            x + dx,
            y + dy
        );
    }

    glEnd();
}

// ================= Main ================= //

int main(void)
{
    engine_init();

    while (!glfwWindowShouldClose(engine.window)) {

        engine_run();

        glfwSwapBuffers(engine.window);
        glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}