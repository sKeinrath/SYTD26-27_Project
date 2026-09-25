#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


// ============================================================
// BASIC TYPES
// ============================================================

typedef struct
{
    float x;
    float y;
} Vec2;


typedef struct
{
    float x;
    float y;
    float z;
} Vec3;


// ============================================================
// VECTOR FUNCTIONS
// ============================================================

Vec2 vec2_create(float x, float y)
{
    Vec2 v;

    v.x = x;
    v.y = y;

    return v;
}


Vec3 vec3_create(float x, float y, float z)
{
    Vec3 v;

    v.x = x;
    v.y = y;
    v.z = z;

    return v;
}


Vec2 vec2_add(Vec2 a, Vec2 b)
{
    return vec2_create(
        a.x + b.x,
        a.y + b.y
    );
}


Vec2 vec2_sub(Vec2 a, Vec2 b)
{
    return vec2_create(
        a.x - b.x,
        a.y - b.y
    );
}


Vec2 vec2_mul(Vec2 v, float scalar)
{
    return vec2_create(
        v.x * scalar,
        v.y * scalar
    );
}


Vec2 vec2_div(Vec2 v, float scalar)
{
    if (scalar == 0.0f)
        return vec2_create(0.0f, 0.0f);

    return vec2_create(
        v.x / scalar,
        v.y / scalar
    );
}


float vec2_length(Vec2 v)
{
    return sqrtf(
        v.x * v.x +
        v.y * v.y
    );
}


Vec2 vec2_normalize(Vec2 v)
{
    float len = vec2_length(v);

    if (len == 0.0f)
        return vec2_create(0.0f, 0.0f);

    return vec2_div(v, len);
}


bool vec3_equal(Vec3 a, Vec3 b)
{
    return
        a.x == b.x &&
        a.y == b.y &&
        a.z == b.z;
}


// ============================================================
// VARIABLES
// ============================================================

float orbitDistance = 15.0f;


// ============================================================
// ENGINE
// ============================================================

typedef struct
{
    GLFWwindow* window;

    int WIDTH;
    int HEIGHT;

} Engine;


Engine engine;


void engine_init(void)
{
    engine.WIDTH = 800;
    engine.HEIGHT = 600;


    // --------------------------------------------------------
    // Init GLFW
    // --------------------------------------------------------

    if (!glfwInit())
    {
        fprintf(
            stderr,
            "failed to init glfw, LOL\n"
        );

        exit(EXIT_FAILURE);
    }


    // --------------------------------------------------------
    // Create Window
    // --------------------------------------------------------

    engine.window =
        glfwCreateWindow(
            engine.WIDTH,
            engine.HEIGHT,
            "2D atom sim by kavan",
            NULL,
            NULL
        );


    if (!engine.window)
    {
        fprintf(
            stderr,
            "failed to create window, LOLOLOL\n"
        );

        glfwTerminate();

        exit(EXIT_FAILURE);
    }


    glfwMakeContextCurrent(
        engine.window
    );


    int fbWidth;
    int fbHeight;

    glfwGetFramebufferSize(
        engine.window,
        &fbWidth,
        &fbHeight
    );


    glViewport(
        0,
        0,
        fbWidth,
        fbHeight
    );
}


void engine_run(void)
{
    glClear(
        GL_COLOR_BUFFER_BIT
    );


    glMatrixMode(
        GL_PROJECTION
    );

    glLoadIdentity();


    // Set origin to centre

    double halfWidth =
        engine.WIDTH / 2.0;

    double halfHeight =
        engine.HEIGHT / 2.0;


    glOrtho(
        -halfWidth,
        halfWidth,
        -halfHeight,
        halfHeight,
        -1.0,
        1.0
    );


    glMatrixMode(
        GL_MODELVIEW
    );

    glLoadIdentity();
}


// ============================================================
// WAVE POINT
// ============================================================

typedef struct
{
    Vec2 localPos;
    Vec2 dir;

} WavePoint;


// ============================================================
// WAVE
// ============================================================

typedef struct
{
    float energy;

    float sigma;
    float k;
    float phase;
    float a;

    float angleR;

    Vec2 pos;
    Vec2 dir;

    Vec3 col;

    WavePoint* points;
    size_t pointCount;

} Wave;


// ============================================================
// WAVE ARRAY
// ============================================================

typedef struct
{
    Wave* data;
    size_t count;

} WaveArray;


WaveArray waves = {
    NULL,
    0
};


// ============================================================
// ADD WAVE
// ============================================================

void waves_add(Wave wave)
{
    Wave* newData =
        realloc(
            waves.data,
            (waves.count + 1) * sizeof(Wave)
        );


    if (!newData)
    {
        fprintf(
            stderr,
            "Failed to allocate wave memory\n"
        );

        exit(EXIT_FAILURE);
    }


    waves.data = newData;

    waves.data[waves.count] = wave;

    waves.count++;
}


// ============================================================
// FREE WAVE
// ============================================================

void wave_free(Wave* wave)
{
    free(wave->points);

    wave->points = NULL;
    wave->pointCount = 0;
}


// ============================================================
// CREATE WAVE
// ============================================================

Wave wave_create(
    float energy,
    Vec2 pos,
    Vec2 dir,
    Vec3 col
)
{
    Wave wave;


    wave.energy = energy;

    wave.sigma = 40.0f;
    wave.k = 0.4f;
    wave.phase = 0.0f;
    wave.a = 10.0f;

    wave.angleR = 0.0f;

    wave.pos = pos;
    wave.dir = vec2_normalize(dir);

    wave.col = col;

    wave.points = NULL;
    wave.pointCount = 0;


    // --------------------------------------------------------
    // Create wave points
    // --------------------------------------------------------

    for (
        float x = -wave.sigma;
        x <= wave.sigma;
        x += 0.1f
    )
    {
        WavePoint* newPoints =
            realloc(
                wave.points,
                (wave.pointCount + 1)
                    * sizeof(WavePoint)
            );


        if (!newPoints)
        {
            fprintf(
                stderr,
                "Failed to allocate wave point memory\n"
            );

            wave_free(&wave);

            exit(EXIT_FAILURE);
        }


        wave.points = newPoints;


        Vec2 offset =
            vec2_mul(
                wave.dir,
                x
            );


        wave.points[wave.pointCount].localPos =
            vec2_add(
                pos,
                offset
            );


        wave.points[wave.pointCount].dir =
            vec2_mul(
                wave.dir,
                200.0f
            );


        wave.pointCount++;
    }


    wave.angleR =
        atan2f(
            wave.dir.y,
            wave.dir.x
        );


    return wave;
}


// ============================================================
// DRAW WAVE
// ============================================================

void wave_draw(Wave* wave)
{
    glColor3f(
        wave->col.x,
        wave->col.y,
        wave->col.z
    );


    glBegin(
        GL_LINE_STRIP
    );


    for (
        size_t i = 0;
        i < wave->pointCount;
        i++
    )
    {
        WavePoint* p =
            &wave->points[i];


        // Perpendicular vector

        Vec2 perp =
            vec2_create(
                -p->dir.y,
                p->dir.x
            );


        perp =
            vec2_normalize(perp);


        // Global phase

        float y_disp =
            wave->a *
            sinf(
                wave->k *
                vec2_length(
                    p->localPos
                )
                -
                wave->phase
            );


        Vec2 drawPos =
            vec2_add(
                p->localPos,
                vec2_mul(
                    perp,
                    y_disp
                )
            );


        glVertex2f(
            drawPos.x,
            drawPos.y
        );
    }


    glEnd();
}


// ============================================================
// UPDATE WAVE
// ============================================================

bool wave_update(
    Wave* wave,
    float dt
)
{
    wave->phase +=
        30.0f * dt;


    for (
        size_t i = 0;
        i < wave->pointCount;
        i++
    )
    {
        WavePoint* p =
            &wave->points[i];


        // Move along velocity

        p->localPos =
            vec2_add(
                p->localPos,
                vec2_mul(
                    p->dir,
                    dt
                )
            );


        // Check boundaries

        if (
            p->localPos.x <
                -engine.WIDTH / 2.0f
            ||
            p->localPos.x >
                engine.WIDTH / 2.0f
            ||
            p->localPos.y <
                -engine.HEIGHT / 2.0f
            ||
            p->localPos.y >
                engine.HEIGHT / 2.0f
        )
        {
            return true;
        }
    }


    return false;
}


// ============================================================
// PARTICLE
// ============================================================

typedef struct
{
    Vec2 pos;

    int charge;

    float angle;

    int n;

    float excitedTimer;

} Particle;


// ============================================================
// PARTICLE INITIALIZATION
// ============================================================

Particle particle_create(
    Vec2 pos,
    int charge
)
{
    Particle p;

    p.pos = pos;
    p.charge = charge;

    p.angle = 0.0f;

    p.n = 1;

    p.excitedTimer = 0.0f;

    return p;
}


// ============================================================
// PARTICLE DRAW
// ============================================================

void particle_draw(
    Particle* p,
    Vec2 centre,
    int segments
)
{
    // --------------------------------------------------------
    // Draw orbit
    // --------------------------------------------------------

    if (p->charge == -1)
    {
        glLineWidth(0.4f);

        glBegin(
            GL_LINE_LOOP
        );


        glColor3f(
            0.4f,
            0.4f,
            0.4f
        );


        for (
            int i = 0;
            i <= segments;
            i++
        )
        {
            float angle =
                2.0f *
                (float)M_PI *
                (float)i /
                (float)segments;


            float x =
                cosf(angle) *
                p->n *
                orbitDistance;


            float y =
                sinf(angle) *
                p->n *
                orbitDistance;


            glVertex2f(
                x + centre.x,
                y + centre.y
            );
        }


        glEnd();
    }


    // --------------------------------------------------------
    // Particle color / radius
    // --------------------------------------------------------

    float r;


    if (p->charge == -1)
    {
        r = 2.0f;

        glColor3f(
            0.0f,
            1.0f,
            1.0f
        );
    }
    else if (p->charge == 1)
    {
        r = 5.0f;

        glColor3f(
            1.0f,
            0.0f,
            0.0f
        );
    }
    else
    {
        r = 5.0f;

        glColor3f(
            0.5f,
            0.5f,
            0.5f
        );
    }


    // --------------------------------------------------------
    // Draw particle
    // --------------------------------------------------------

    glBegin(
        GL_TRIANGLE_FAN
    );


    glVertex2f(
        p->pos.x,
        p->pos.y
    );


    for (
        int i = 0;
        i <= segments;
        i++
    )
    {
        float angle =
            2.0f *
            (float)M_PI *
            (float)i /
            (float)segments;


        float x =
            cosf(angle) * r;


        float y =
            sinf(angle) * r;


        glVertex2f(
            x + p->pos.x,
            y + p->pos.y
        );
    }


    glEnd();
}


// ============================================================
// PARTICLE UPDATE
// ============================================================

void particle_update(
    Particle* p,
    Vec2 centre
)
{
    // Set radius

    float r =
        p->n *
        orbitDistance;


    p->angle +=
        0.05f;


    // Update position

    p->pos =
        vec2_create(
            cosf(p->angle) *
                r +
                centre.x,

            sinf(p->angle) *
                r +
                centre.y
        );


    // --------------------------------------------------------
    // Electron falls to lower energy level
    // --------------------------------------------------------

    if (
        p->excitedTimer <= 0.0f &&
        p->n > 1
    )
    {
        p->n--;


        p->excitedTimer +=
            0.003f;


        float waveDirX =
            ((float)rand() /
             (float)RAND_MAX)
            * 2.0f
            - 1.0f;


        float waveDirY =
            ((float)rand() /
             (float)RAND_MAX)
            * 2.0f
            - 1.0f;


        float energyDiff =
            -13.6f /
            ((p->n + 1) *
             (p->n + 1))

            -

            (
                -13.6f /
                (p->n * p->n)
            );


        Wave wave =
            wave_create(
                energyDiff,
                p->pos,
                vec2_create(
                    waveDirX,
                    waveDirY
                ),
                vec3_create(
                    1.0f,
                    1.0f,
                    0.0f
                )
            );


        waves_add(wave);
    }
}


// ============================================================
// ATOM
// ============================================================

typedef struct
{
    Vec2 pos;

    Vec2 v;

    Particle* particles;
    size_t particleCount;

} Atom;


// ============================================================
// CREATE ATOM
// ============================================================

Atom atom_create(Vec2 pos)
{
    Atom atom;


    atom.pos = pos;

    atom.v =
        vec2_create(
            0.0f,
            0.0f
        );


    atom.particles = NULL;
    atom.particleCount = 0;


    // --------------------------------------------------------
    // Proton
    // --------------------------------------------------------

    Particle proton =
        particle_create(
            pos,
            1
        );


    atom.particles =
        realloc(
            atom.particles,
            sizeof(Particle)
        );


    atom.particles[0] =
        proton;

    atom.particleCount = 1;


    // --------------------------------------------------------
    // Electron
    // --------------------------------------------------------

    Vec2 electronPos =
        vec2_create(
            pos.x - orbitDistance,
            pos.y
        );


    Particle electron =
        particle_create(
            electronPos,
            -1
        );


    Particle* newParticles =
        realloc(
            atom.particles,
            2 * sizeof(Particle)
        );


    if (!newParticles)
    {
        fprintf(
            stderr,
            "Failed to allocate particle memory\n"
        );

        exit(EXIT_FAILURE);
    }


    atom.particles =
        newParticles;


    atom.particles[1] =
        electron;

    atom.particleCount = 2;


    return atom;
}


// ============================================================
// ATOM ARRAY
// ============================================================

typedef struct
{
    Atom* data;
    size_t count;

} AtomArray;


AtomArray atoms = {
    NULL,
    0
};


// ============================================================
// ADD ATOM
// ============================================================

void atoms_add(Atom atom)
{
    Atom* newData =
        realloc(
            atoms.data,
            (atoms.count + 1) *
            sizeof(Atom)
        );


    if (!newData)
    {
        fprintf(
            stderr,
            "Failed to allocate atom memory\n"
        );

        exit(EXIT_FAILURE);
    }


    atoms.data =
        newData;

    atoms.data[atoms.count] =
        atom;

    atoms.count++;
}


// ============================================================
// FREE ATOM
// ============================================================

void atom_free(Atom* atom)
{
    free(atom->particles);

    atom->particles = NULL;

    atom->particleCount = 0;
}


// ============================================================
// MOUSE CALLBACK
// ============================================================

void mouseButtonCallback(
    GLFWwindow* window,
    int button,
    int action,
    int mods
)
{
    (void)mods;


    if (
        button !=
            GLFW_MOUSE_BUTTON_LEFT
        ||
        action !=
            GLFW_PRESS
    )
    {
        return;
    }


    double mx;
    double my;


    glfwGetCursorPos(
        window,
        &mx,
        &my
    );


    Engine* eng =
        (Engine*)
        glfwGetWindowUserPointer(
            window
        );


    // --------------------------------------------------------
    // Screen -> world
    // --------------------------------------------------------

    float worldX =
        (float)mx -
        eng->WIDTH / 2.0f;


    float worldY =
        eng->HEIGHT / 2.0f -
        (float)my;


    Vec2 spawnPos =
        vec2_create(
            worldX,
            worldY
        );


    // --------------------------------------------------------
    // Spawn 25 waves
    // --------------------------------------------------------

    float energyN1toN2 =
        -13.6f / (2.0f * 2.0f)
        -
        (-13.6f);


    for (
        int i = 0;
        i < 25;
        i++
    )
    {
        float angle =
            ((float)rand() /
             (float)RAND_MAX)
            *
            2.0f *
            (float)M_PI;


        Vec2 dir =
            vec2_create(
                cosf(angle),
                sinf(angle)
            );


        Wave wave =
            wave_create(
                energyN1toN2,
                spawnPos,
                dir,
                vec3_create(
                    0.0f,
                    1.0f,
                    1.0f
                )
            );


        waves_add(wave);
    }
}


// ============================================================
// MAIN
// ============================================================

int main(void)
{
    // --------------------------------------------------------
    // Initialize random number generator
    // --------------------------------------------------------

    srand(
        (unsigned int)time(NULL)
    );


    // --------------------------------------------------------
    // Initialize engine
    // --------------------------------------------------------

    engine_init();


    // --------------------------------------------------------
    // Initialize 20 atoms in a circle
    // --------------------------------------------------------

    int num_atoms = 20;

    float radius = 100.0f;


    for (
        int i = 0;
        i < num_atoms;
        i++
    )
    {
        float angle =
            2.0f *
            (float)M_PI *
            (float)i /
            (float)num_atoms;


        float x =
            cosf(angle) *
            radius;


        float y =
            sinf(angle) *
            radius;


        Atom atom =
            atom_create(
                vec2_create(
                    x,
                    y
                )
            );


        atoms_add(atom);
    }


    // --------------------------------------------------------
    // Callbacks
    // --------------------------------------------------------

    glfwSetWindowUserPointer(
        engine.window,
        &engine
    );


    glfwSetMouseButtonCallback(
        engine.window,
        mouseButtonCallback
    );


    // --------------------------------------------------------
    // Initial waves
    // --------------------------------------------------------

    float energyN1toN2 =
        -13.6f / (2.0f * 2.0f)
        -
        (-13.6f);


    for (
        int i = 0;
        i < 24;
        i++
    )
    {
        Wave wave =
            wave_create(
                energyN1toN2,

                vec2_create(
                    200.0f,
                    i * 20.0f - 200.0f
                ),

                vec2_create(
                    -1.0f,
                    0.0f
                ),

                vec3_create(
                    0.0f,
                    1.0f,
                    1.0f
                )
            );


        waves_add(wave);
    }


    // ========================================================
    // MAIN LOOP
    // ========================================================

    while (
        !glfwWindowShouldClose(
            engine.window
        )
    )
    {
        engine_run();


        // ----------------------------------------------------
        // Draw / update atoms
        // ----------------------------------------------------

        for (
            size_t ai = 0;
            ai < atoms.count;
            ai++
        )
        {
            Atom* a =
                &atoms.data[ai];


            // ------------------------------------------------
            // Atom-Atom repulsion
            // ------------------------------------------------

            for (
                size_t ai2 = 0;
                ai2 < atoms.count;
                ai2++
            )
            {
                if (ai2 == ai)
                    continue;


                Atom* a2 =
                    &atoms.data[ai2];


                Vec2 difference =
                    vec2_sub(
                        a->pos,
                        a2->pos
                    );


                float dist =
                    vec2_length(
                        difference
                    );


                if (dist > 0.0001f)
                {
                    Vec2 dir =
                        vec2_normalize(
                            difference
                        );


                    Vec2 force =
                        vec2_mul(
                            dir,
                            57.5f / dist
                        );


                    a->v =
                        vec2_add(
                            a->v,
                            force
                        );
                }
            }


            // ------------------------------------------------
            // Boundary repulsion
            // ------------------------------------------------

            const float boundary_stiffness =
                0.01f;

            const float boundary_threshold =
                200.0f;


            // Left

            float dist_left =
                a->pos.x +
                engine.WIDTH / 2.0f;


            if (
                dist_left <
                boundary_threshold
            )
            {
                a->v.x +=
                    (
                        boundary_threshold -
                        dist_left
                    )
                    *
                    boundary_stiffness;
            }


            // Right

            float dist_right =
                engine.WIDTH / 2.0f -
                a->pos.x;


            if (
                dist_right <
                boundary_threshold
            )
            {
                a->v.x -=
                    (
                        boundary_threshold -
                        dist_right
                    )
                    *
                    boundary_stiffness;
            }


            // Top

            float dist_top =
                engine.HEIGHT / 2.0f -
                a->pos.y;


            if (
                dist_top <
                boundary_threshold
            )
            {
                a->v.y -=
                    (
                        boundary_threshold -
                        dist_top
                    )
                    *
                    boundary_stiffness;
            }


            // Bottom

            float dist_bottom =
                a->pos.y +
                engine.HEIGHT / 2.0f;


            if (
                dist_bottom <
                boundary_threshold
            )
            {
                a->v.y +=
                    (
                        boundary_threshold -
                        dist_bottom
                    )
                    *
                    boundary_stiffness;
            }


            // ------------------------------------------------
            // Damping
            // ------------------------------------------------

            a->v =
                vec2_mul(
                    a->v,
                    0.99f
                );


            // ------------------------------------------------
            // Particles
            // ------------------------------------------------

            for (
                size_t pi = 0;
                pi < a->particleCount;
                pi++
            )
            {
                Particle* p =
                    &a->particles[pi];


                p->excitedTimer =
                    p->excitedTimer;


                // Draw particle

                particle_draw(
                    p,
                    a->pos,
                    50
                );


                // ------------------------------------------------
                // Proton
                // ------------------------------------------------

                if (p->charge == 1)
                {
                    p->pos =
                        a->pos;
                }


                // ------------------------------------------------
                // Electron
                // ------------------------------------------------

                if (p->charge == -1)
                {
                    if (
                        p->excitedTimer >
                        0.0f
                    )
                    {
                        p->excitedTimer -=
                            0.001f;
                    }


                    particle_update(
                        p,
                        a->pos
                    );


                    // --------------------------------------------
                    // Check wave interaction
                    // --------------------------------------------

                    for (
                        size_t wi = 0;
                        wi < waves.count;
                        wi++
                    )
                    {
                        Wave* wave =
                            &waves.data[wi];


                        for (
                            size_t wpi = 0;
                            wpi <
                            wave->pointCount;
                            wpi++
                        )
                        {
                            WavePoint* wp =
                                &wave->points[wpi];


                            Vec2 difference =
                                vec2_sub(
                                    p->pos,
                                    wp->localPos
                                );


                            float dist =
                                vec2_length(
                                    difference
                                );


                            float energyforUp =
                                -13.6f /
                                (
                                    (p->n + 1) *
                                    (p->n + 1)
                                )
                                -
                                (
                                    -13.6f /
                                    (
                                        p->n *
                                        p->n
                                    )
                                );


                            bool yellow =
                                vec3_equal(
                                    wave->col,
                                    vec3_create(
                                        1.0f,
                                        1.0f,
                                        0.0f
                                    )
                                );


                            if (
                                dist < 20.0f &&
                                wave->energy ==
                                    energyforUp &&
                                !yellow
                            )
                            {
                                wave->energy =
                                    0.0f;


                                p->n += 1;


                                p->excitedTimer +=
                                    0.003f;


                                break;
                            }
                        }
                    }
                }
            }
        }


        // ====================================================
        // DRAW / UPDATE WAVES
        // ====================================================

        size_t wi = 0;


        while (
            wi < waves.count
        )
        {
            Wave* wave =
                &waves.data[wi];


            if (
                wave->energy ==
                0.0f
            )
            {
                wi++;
                continue;
            }


            wave_draw(
                wave
            );


            if (
                wave_update(
                    wave,
                    0.01f
                )
            )
            {
                // --------------------------------------------
                // Delete wave
                // --------------------------------------------

                wave_free(
                    wave
                );


                for (
                    size_t j = wi;
                    j + 1 < waves.count;
                    j++
                )
                {
                    waves.data[j] =
                        waves.data[j + 1];
                }


                waves.count--;


                if (waves.count == 0)
                {
                    free(
                        waves.data
                    );

                    waves.data = NULL;
                }
                else
                {
                    Wave* newData =
                        realloc(
                            waves.data,
                            waves.count *
                            sizeof(Wave)
                        );


                    if (newData)
                    {
                        waves.data =
                            newData;
                    }
                }


                continue;
            }


            wi++;
        }


        // ----------------------------------------------------
        // Display
        // ----------------------------------------------------

        glfwSwapBuffers(
            engine.window
        );

        glfwPollEvents();
    }


    // ========================================================
    // CLEANUP
    // ========================================================

    for (
        size_t i = 0;
        i < waves.count;
        i++
    )
    {
        wave_free(
            &waves.data[i]
        );
    }


    free(
        waves.data
    );


    for (
        size_t i = 0;
        i < atoms.count;
        i++
    )
    {
        atom_free(
            &atoms.data[i]
        );
    }


    free(
        atoms.data
    );


    glfwDestroyWindow(
        engine.window
    );


    glfwTerminate();


    return 0;
}