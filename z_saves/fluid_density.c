#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


// ============================================================
// Konstanten
// ============================================================

const float c = 299792458.0f / 100000000.0f;
const float k = 8.9875517923e9f;


// ============================================================
// Vec2
// ============================================================

typedef struct {
    float x;
    float y;
} Vec2;


// ============================================================
// Vec3
// ============================================================

typedef struct {
    float x;
    float y;
    float z;
} Vec3;


// ============================================================
// Engine
// ============================================================

typedef struct {
    GLFWwindow* window;
    int WIDTH;
    int HEIGHT;
} Engine;

Engine engine;


// ============================================================
// Engine Funktionen
// ============================================================

void engine_init(void)
{
    engine.WIDTH = 800;
    engine.HEIGHT = 600;

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
    double right  = engine.WIDTH;
    double bottom = -engine.HEIGHT;
    double top    = engine.HEIGHT;

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


void engine_drawCircle(
    float x,
    float y,
    float r,
    int segments
)
{
    glBegin(GL_LINE_LOOP);

    for (int i = 0; i < segments; i++) {

        float angle =
            2.0f * (float)M_PI * i / segments;

        float dx = r * cosf(angle);
        float dy = r * sinf(angle);

        glVertex2f(
            x + dx,
            y + dy
        );
    }

    glEnd();
}


void engine_drawFilledCircle(
    float x,
    float y,
    float r,
    int segments
)
{
    glBegin(GL_TRIANGLE_FAN);

    glVertex2f(x, y);

    for (int i = 0; i <= segments; i++) {

        float angle =
            2.0f * (float)M_PI * i / segments;

        float dx = r * cosf(angle);
        float dy = r * sinf(angle);

        glVertex2f(
            x + dx,
            y + dy
        );
    }

    glEnd();
}


// ============================================================
// Density Map
// ============================================================

#define GRID_W 800
#define GRID_H 600

#define CELL_W 1.0f
#define CELL_H 1.0f

float density[GRID_W * GRID_H];

float maxDensity = 1.0f;


// ============================================================
// Density -> Farbe
// ============================================================

Vec3 densityToColour(
    float d,
    float maxD
)
{
    if (maxD <= 0.0f)
        maxD = 1.0f;

    float t = d / maxD;

    if (t < 0.0f)
        t = 0.0f;

    if (t > 1.0f)
        t = 1.0f;

    float r;
    float g;
    float b;

    if (t < 0.33f) {

        // schwarz -> rot

        float value = t / 0.33f;

        r = value;
        g = 0.0f;
        b = 0.0f;

    }
    else if (t < 0.66f) {

        // rot -> gelb

        float value =
            (t - 0.33f) / 0.33f;

        r = 1.0f;
        g = value;
        b = 0.0f;

    }
    else {

        // gelb -> weiß

        float value =
            (t - 0.66f) / 0.34f;

        r = 1.0f;
        g = 1.0f;
        b = value;
    }

    Vec3 result;

    result.x = r;
    result.y = g;
    result.z = b;

    return result;
}


// ============================================================
// Density Map berechnen
// ============================================================

void calculateDensityMap(
    Vec2* particles,
    int particleCount
)
{
    // Density zurücksetzen

    for (int i = 0; i < GRID_W * GRID_H; i++)
        density[i] = 0.0f;

    maxDensity = 0.0f;


    const float INFLUENCE_RADIUS = 200.0f;

    const float INFLUENCE_RADIUS_SQ =
        INFLUENCE_RADIUS *
        INFLUENCE_RADIUS;


    const float DENSITY_SCALE = 1.0f;


    // Alle Partikel

    for (int pIndex = 0;
         pIndex < particleCount;
         pIndex++)
    {
        Vec2 p = particles[pIndex];


        // Partikel -> Grid

        int center_i =
            (int)roundf(
                p.x + GRID_W
            ) / 2;

        int center_j =
            (int)roundf(
                p.y + GRID_H
            ) / 2;


        // Begrenzung

        int radius =
            (int)ceilf(
                INFLUENCE_RADIUS
            );


        int min_i = center_i - radius;
        int max_i = center_i + radius;

        int min_j = center_j - radius;
        int max_j = center_j + radius;


        if (min_i < 0)
            min_i = 0;

        if (max_i >= GRID_W)
            max_i = GRID_W - 1;

        if (min_j < 0)
            min_j = 0;

        if (max_j >= GRID_H)
            max_j = GRID_H - 1;


        // Grid durchlaufen

        for (int j = min_j;
             j <= max_j;
             j++)
        {
            for (int i = min_i;
                 i <= max_i;
                 i++)
            {

                // Grid -> OpenGL Koordinaten

                float cell_x =
                    i * 2.0f - GRID_W;

                float cell_y =
                    j * 2.0f - GRID_H;


                // Abstand

                float dx =
                    p.x - cell_x;

                float dy =
                    p.y - cell_y;

                float distSq =
                    dx * dx +
                    dy * dy;


                if (distSq <
                    INFLUENCE_RADIUS_SQ)
                {
                    float dist =
                        sqrtf(distSq);


                    float influence =
                        (INFLUENCE_RADIUS - dist)
                        / INFLUENCE_RADIUS;


                    float kernel_weight =
                        influence *
                        influence *
                        DENSITY_SCALE;


                    int index =
                        j * GRID_W + i;


                    density[index] +=
                        kernel_weight;


                    if (density[index] >
                        maxDensity)
                    {
                        maxDensity =
                            density[index];
                    }
                }
            }
        }
    }
}


// ============================================================
// Density Map zeichnen
// ============================================================

void drawDensityMap(void)
{
    glBegin(GL_QUADS);

    for (int j = 0;
         j < GRID_H;
         j++)
    {
        for (int i = 0;
             i < GRID_W;
             i++)
        {
            int index =
                j * GRID_W + i;


            float d =
                density[index];


            Vec3 color =
                densityToColour(
                    d,
                    maxDensity
                );


            glColor3f(
                color.x,
                color.y,
                color.z
            );


            float x0 =
                i * 2.0f - GRID_W;

            float y0 =
                j * 2.0f - GRID_H;

            float x1 =
                (i + 1) * 2.0f - GRID_W;

            float y1 =
                (j + 1) * 2.0f - GRID_H;


            glVertex2f(x0, y0);
            glVertex2f(x1, y0);
            glVertex2f(x1, y1);
            glVertex2f(x0, y1);
        }
    }

    glEnd();
}


// ============================================================
// Zufallszahl
// ============================================================

float randomFloat(
    float min,
    float max
)
{
    float r =
        (float)rand() /
        (float)RAND_MAX;

    return min + r * (max - min);
}


// ============================================================
// Partikel generieren
// ============================================================

void generateParticles(
    Vec3* particles,
    int numParticles,
    int numClusters,
    float clusterRadius
)
{
    float minX = -engine.WIDTH;
    float maxX = engine.WIDTH;

    float minY = -engine.HEIGHT;
    float maxY = engine.HEIGHT;

    float minZ = -500.0f;
    float maxZ = 500.0f;


    // Cluster-Zentren

    Vec3* clusterCenters =
        malloc(
            numClusters *
            sizeof(Vec3)
        );

    if (!clusterCenters) {
        fprintf(
            stderr,
            "Failed to allocate cluster centers\n"
        );

        exit(EXIT_FAILURE);
    }


    for (int i = 0;
         i < numClusters;
         i++)
    {
        clusterCenters[i].x =
            randomFloat(minX, maxX);

        clusterCenters[i].y =
            randomFloat(minY, maxY);

        clusterCenters[i].z =
            randomFloat(minZ, maxZ);
    }


    // 1/3 globale Partikel

    int globalParticles =
        numParticles / 3;

    int clusteredParticles =
        numParticles -
        globalParticles;


    int particleIndex = 0;


    // ========================================================
    // Globale zufällige Partikel
    // ========================================================

    for (int i = 0;
         i < globalParticles;
         i++)
    {
        particles[particleIndex].x =
            randomFloat(minX, maxX);

        particles[particleIndex].y =
            randomFloat(minY, maxY);

        particles[particleIndex].z =
            randomFloat(minZ, maxZ);

        particleIndex++;
    }


    // ========================================================
    // Cluster-Partikel
    // ========================================================

    for (int i = 0;
         i < clusteredParticles;
         i++)
    {
        int clusterIndex =
            rand() % numClusters;

        Vec3 center =
            clusterCenters[clusterIndex];


        // Zufällige Richtung auf Kugel

        float u =
            randomFloat(-1.0f, 1.0f);

        float phi =
            randomFloat(
                0.0f,
                2.0f * (float)M_PI
            );


        float sqrt1MinusU2 =
            sqrtf(
                1.0f - u * u
            );


        float dx =
            sqrt1MinusU2 *
            cosf(phi);

        float dy =
            sqrt1MinusU2 *
            sinf(phi);

        float dz = u;


        // Gleichmäßiger Radius

        float randomValue =
            randomFloat(0.0f, 1.0f);

        float radius =
            clusterRadius *
            cbrtf(randomValue);


        particles[particleIndex].x =
            center.x + dx * radius;

        particles[particleIndex].y =
            center.y + dy * radius;

        particles[particleIndex].z =
            center.z + dz * radius;


        particleIndex++;
    }


    free(clusterCenters);
}


// ============================================================
// 3D -> 2D Projektion
// ============================================================

void project_2d(
    Vec3* particles_3d,
    int particleCount,
    Vec2* particles_2d
)
{
    for (int i = 0;
         i < particleCount;
         i++)
    {
        particles_2d[i].x =
            particles_3d[i].x;

        particles_2d[i].y =
            particles_3d[i].y;
    }
}


// ============================================================
// Main
// ============================================================

int main(void)
{
    srand(
        (unsigned int)time(NULL)
    );


    // Engine starten

    engine_init();


    // ========================================================
    // Partikel
    // ========================================================

    const int NUM_PARTICLES = 10000;

    const int NUM_CLUSTERS = 5;

    const float CLUSTER_RADIUS = 300.0f;


    Vec3* particles_3d =
        malloc(
            NUM_PARTICLES *
            sizeof(Vec3)
        );


    Vec2* particles =
        malloc(
            NUM_PARTICLES *
            sizeof(Vec2)
        );


    if (!particles_3d ||
        !particles)
    {
        fprintf(
            stderr,
            "Failed to allocate particles\n"
        );

        free(particles_3d);
        free(particles);

        glfwDestroyWindow(
            engine.window
        );

        glfwTerminate();

        return EXIT_FAILURE;
    }


    // 3D Partikel erzeugen

    generateParticles(
        particles_3d,
        NUM_PARTICLES,
        NUM_CLUSTERS,
        CLUSTER_RADIUS
    );


    // 3D -> 2D

    project_2d(
        particles_3d,
        NUM_PARTICLES,
        particles
    );


    // 3D Array wird danach nicht mehr benötigt

    free(particles_3d);


    // ========================================================
    // Density Map
    // ========================================================

    calculateDensityMap(
        particles,
        NUM_PARTICLES
    );


    // ========================================================
    // Hauptloop
    // ========================================================

    while (
        !glfwWindowShouldClose(
            engine.window
        )
    )
    {
        engine_run();


        // ----------------------------------------------------
        // Density Map zeichnen
        // ----------------------------------------------------

        drawDensityMap();


        // ----------------------------------------------------
        // Partikel zeichnen
        // ----------------------------------------------------

        glColor4f(
            1.0f,
            1.0f,
            1.0f,
            0.3f
        );


        for (int i = 0;
             i < NUM_PARTICLES;
             i++)
        {
            engine_drawFilledCircle(
                particles[i].x,
                particles[i].y,
                1.0f,
                50
            );
        }


        // ----------------------------------------------------
        // Frame anzeigen
        // ----------------------------------------------------

        glfwSwapBuffers(
            engine.window
        );

        glfwPollEvents();
    }


    // ========================================================
    // Cleanup
    // ========================================================

    free(particles);

    glfwDestroyWindow(
        engine.window
    );

    glfwTerminate();


    return 0;
}