#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// CONSTANTS
// ============================================================

const float c = 299792458.0f / 100000000.0f;
const float eu = 2.71828182845904523536f;
const float k = 8.9875517923e9f;
const float a0 = 52.9f;
const float electron_r = 2.0f;
const float fieldRes = 25.0f;


// ============================================================
// BASIC TYPES
// ============================================================

typedef struct {
    float x;
    float y;
    float z;
} Vec3;

typedef struct {
    float m[4][4];
} Mat4;


// ============================================================
// VECTOR FUNCTIONS
// ============================================================

Vec3 vec3_create(float x, float y, float z)
{
    Vec3 v;

    v.x = x;
    v.y = y;
    v.z = z;

    return v;
}


// ============================================================
// CAMERA
// ============================================================

typedef struct {
    Vec3 target;

    float radius;

    float azimuth;
    float elevation;

    float orbitSpeed;
    float panSpeed;
    double zoomSpeed;

    bool dragging;
    bool panning;
    bool moving;

    double lastX;
    double lastY;

} Camera;


Camera camera = {
    .target = {0.0f, 0.0f, 0.0f},

    .radius = 500.0f,

    .azimuth = 0.0f,
    .elevation = M_PI / 2.0f,

    .orbitSpeed = 0.01f,
    .panSpeed = 0.01f,
    .zoomSpeed = 125.0,

    .dragging = false,
    .panning = false,
    .moving = false,

    .lastX = 0.0,
    .lastY = 0.0
};


// ------------------------------------------------------------
// Clamp
// ------------------------------------------------------------

float clamp_float(float value, float min, float max)
{
    if (value < min)
        return min;

    if (value > max)
        return max;

    return value;
}


// ------------------------------------------------------------
// Camera position
// ------------------------------------------------------------

Vec3 camera_position(Camera* cam)
{
    float elevation =
        clamp_float(
            cam->elevation,
            0.01f,
            (float)M_PI - 0.01f
        );

    float x =
        cam->radius *
        sinf(elevation) *
        cosf(cam->azimuth);

    float y =
        cam->radius *
        cosf(elevation);

    float z =
        cam->radius *
        sinf(elevation) *
        sinf(cam->azimuth);

    return vec3_create(x, y, z);
}


// ------------------------------------------------------------
// Camera update
// ------------------------------------------------------------

void camera_update(Camera* cam)
{
    cam->target = vec3_create(
        0.0f,
        0.0f,
        0.0f
    );

    if (cam->dragging || cam->panning)
        cam->moving = true;
    else
        cam->moving = false;
}


// ------------------------------------------------------------
// Mouse movement
// ------------------------------------------------------------

void camera_process_mouse_move(
    Camera* cam,
    double x,
    double y
)
{
    float dx =
        (float)(x - cam->lastX);

    float dy =
        (float)(y - cam->lastY);

    if (cam->dragging && cam->panning)
    {
        /*
         * Panning disabled.
         * Camera remains centered on black hole.
         */
    }
    else if (cam->dragging && !cam->panning)
    {
        cam->azimuth +=
            dx * cam->orbitSpeed;

        cam->elevation -=
            dy * cam->orbitSpeed;

        cam->elevation =
            clamp_float(
                cam->elevation,
                0.01f,
                (float)M_PI - 0.01f
            );
    }

    cam->lastX = x;
    cam->lastY = y;

    camera_update(cam);
}


// ------------------------------------------------------------
// Mouse button
// ------------------------------------------------------------

void camera_process_mouse_button(
    Camera* cam,
    int button,
    int action,
    int mods,
    GLFWwindow* window
)
{
    (void)mods;

    if (
        button == GLFW_MOUSE_BUTTON_LEFT ||
        button == GLFW_MOUSE_BUTTON_MIDDLE
    )
    {
        if (action == GLFW_PRESS)
        {
            cam->dragging = true;

            // Panning disabled
            cam->panning = false;

            glfwGetCursorPos(
                window,
                &cam->lastX,
                &cam->lastY
            );
        }
        else if (action == GLFW_RELEASE)
        {
            cam->dragging = false;
            cam->panning = false;
        }
    }
}


// ------------------------------------------------------------
// Scroll
// ------------------------------------------------------------

void camera_process_scroll(
    Camera* cam,
    double xoffset,
    double yoffset
)
{
    (void)xoffset;

    cam->radius -=
        yoffset * cam->zoomSpeed;

    camera_update(cam);
}


// ============================================================
// ENGINE
// ============================================================

typedef struct {
    GLFWwindow* window;

    GLuint shaderProgram;

    int WIDTH;
    int HEIGHT;

    float width;
    float height;

} Engine;


Engine engine;


// ============================================================
// MATRIX FUNCTIONS
// ============================================================

Mat4 mat4_identity(void)
{
    Mat4 result = {0};

    result.m[0][0] = 1.0f;
    result.m[1][1] = 1.0f;
    result.m[2][2] = 1.0f;
    result.m[3][3] = 1.0f;

    return result;
}


// ------------------------------------------------------------
// Perspective
// ------------------------------------------------------------

Mat4 mat4_perspective(
    float fov,
    float aspect,
    float nearPlane,
    float farPlane
)
{
    Mat4 result = {0};

    float f =
        1.0f /
        tanf(fov * 0.5f);

    result.m[0][0] = f / aspect;
    result.m[1][1] = f;

    result.m[2][2] =
        (farPlane + nearPlane) /
        (nearPlane - farPlane);

    result.m[2][3] = -1.0f;

    result.m[3][2] =
        (2.0f * farPlane * nearPlane) /
        (nearPlane - farPlane);

    return result;
}


// ============================================================
// ENGINE INITIALIZATION
// ============================================================

void engine_init(void)
{
    engine.WIDTH = 800;
    engine.HEIGHT = 600;

    engine.width = 1000.0f;
    engine.height = 750.0f;

    if (!glfwInit())
    {
        fprintf(
            stderr,
            "GLFW init failed\n"
        );

        exit(EXIT_FAILURE);
    }

    engine.window =
        glfwCreateWindow(
            engine.WIDTH,
            engine.HEIGHT,
            "Quantum Simulation by kavan G",
            NULL,
            NULL
        );

    if (!engine.window)
    {
        fprintf(
            stderr,
            "Failed to create GLFW window\n"
        );

        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(
        engine.window
    );

    glViewport(
        0,
        0,
        engine.WIDTH,
        engine.HEIGHT
    );

    glClearColor(
        0.0f,
        0.0f,
        0.0f,
        1.0f
    );

    glEnable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE
    );

    glewExperimental = GL_TRUE;

    GLenum glewErr = glewInit();

    if (glewErr != GLEW_OK)
    {
        fprintf(
            stderr,
            "Failed to initialize GLEW: %s\n",
            (const char*)glewGetErrorString(
                glewErr
            )
        );

        glfwTerminate();
        exit(EXIT_FAILURE);
    }
}


// ============================================================
// SHADER PROGRAM
// ============================================================

GLuint create_shader_program(void)
{
    const char* vertexShaderSource =
        "#version 330 core\n"

        "layout (location = 0) in vec3 aPos;\n"

        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "uniform vec4 objectColor;\n"

        "out vec4 FragColorVS;\n"

        "void main()\n"
        "{\n"
        "    gl_Position = "
        "projection * view * model * "
        "vec4(aPos, 1.0);\n"

        "    FragColorVS = objectColor;\n"
        "}\n";


    const char* fragmentShaderSource =
        "#version 330 core\n"

        "in vec4 FragColorVS;\n"

        "out vec4 FragColor;\n"

        "void main()\n"
        "{\n"
        "    FragColor = FragColorVS;\n"
        "}\n";


    GLuint vertexShader =
        glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(
        vertexShader,
        1,
        &vertexShaderSource,
        NULL
    );

    glCompileShader(vertexShader);


    GLuint fragmentShader =
        glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(
        fragmentShader,
        1,
        &fragmentShaderSource,
        NULL
    );

    glCompileShader(fragmentShader);


    GLuint shaderProgram =
        glCreateProgram();

    glAttachShader(
        shaderProgram,
        vertexShader
    );

    glAttachShader(
        shaderProgram,
        fragmentShader
    );

    glLinkProgram(shaderProgram);


    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}


// ============================================================
// ENGINE RUN
// ============================================================

void engine_run(void)
{
    glClear(
        GL_COLOR_BUFFER_BIT |
        GL_DEPTH_BUFFER_BIT
    );

    glUseProgram(
        engine.shaderProgram
    );

    /*
     * In the original C++ version:
     *
     * mat4 view =
     *     lookAt(
     *         camera.position(),
     *         camera.target,
     *         vec3(0,1,0)
     *     );
     *
     * Here the equivalent matrix has to be
     * calculated manually.
     */

    GLuint viewLoc =
        glGetUniformLocation(
            engine.shaderProgram,
            "view"
        );

    GLuint projectionLoc =
        glGetUniformLocation(
            engine.shaderProgram,
            "projection"
        );


    /*
     * Projection matrix.
     */

    Mat4 projection =
        mat4_perspective(
            45.0f * (float)M_PI / 180.0f,
            (float)engine.WIDTH /
                (float)engine.HEIGHT,
            0.1f,
            10000.0f
        );


    glUniformMatrix4fv(
        projectionLoc,
        1,
        GL_FALSE,
        &projection.m[0][0]
    );


    /*
     * View matrix is identity here.
     *
     * A full lookAt() implementation can be
     * added for the camera.
     */

    Mat4 view = mat4_identity();

    glUniformMatrix4fv(
        viewLoc,
        1,
        GL_FALSE,
        &view.m[0][0]
    );
}


// ============================================================
// VBO / VAO
// ============================================================

void create_vbo_vao(
    GLuint* VAO,
    GLuint* VBO,
    const float* vertices,
    size_t vertexCount
)
{
    glGenVertexArrays(
        1,
        VAO
    );

    glGenBuffers(
        1,
        VBO
    );


    glBindVertexArray(*VAO);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        *VBO
    );


    glBufferData(
        GL_ARRAY_BUFFER,
        vertexCount * sizeof(float),
        vertices,
        GL_STATIC_DRAW
    );


    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        (void*)0
    );

    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}


// ============================================================
// SPHERICAL COORDINATES
// ============================================================

Vec3 spherical_to_cartesian(
    float r,
    float theta,
    float phi
)
{
    float x =
        r *
        sinf(theta) *
        cosf(phi);

    float y =
        r *
        cosf(theta);

    float z =
        r *
        sinf(theta) *
        sinf(phi);

    return vec3_create(
        x,
        y,
        z
    );
}


// ============================================================
// PARTICLES
// ============================================================

void draw_particle(
    Vec3 particle,
    GLint modelLoc,
    GLint objectColorLoc
)
{
    glUniform4f(
        objectColorLoc,
        1.0f,
        1.0f,
        1.0f,
        1.0f
    );


    /*
     * Translation matrix.
     */

    Mat4 model =
        mat4_identity();

    model.m[3][0] =
        particle.x;

    model.m[3][1] =
        particle.y;

    model.m[3][2] =
        particle.z;


    glUniformMatrix4fv(
        modelLoc,
        1,
        GL_FALSE,
        &model.m[0][0]
    );


    glPointSize(1.0f);

    glBegin(GL_POINTS);

    glVertex3f(
        0.0f,
        0.0f,
        0.0f
    );

    glEnd();
}


// ============================================================
// CAMERA CALLBACKS
// ============================================================

void mouse_button_callback(
    GLFWwindow* window,
    int button,
    int action,
    int mods
)
{
    camera_process_mouse_button(
        &camera,
        button,
        action,
        mods,
        window
    );
}


void cursor_position_callback(
    GLFWwindow* window,
    double x,
    double y
)
{
    (void)window;

    camera_process_mouse_move(
        &camera,
        x,
        y
    );
}


void scroll_callback(
    GLFWwindow* window,
    double xoffset,
    double yoffset
)
{
    (void)window;

    camera_process_scroll(
        &camera,
        xoffset,
        yoffset
    );
}


void setup_camera_callbacks(
    GLFWwindow* window
)
{
    glfwSetMouseButtonCallback(
        window,
        mouse_button_callback
    );

    glfwSetCursorPosCallback(
        window,
        cursor_position_callback
    );

    glfwSetScrollCallback(
        window,
        scroll_callback
    );
}


// ============================================================
// MAIN
// ============================================================

int main(void)
{
    engine_init();

    engine.shaderProgram =
        create_shader_program();


    setup_camera_callbacks(
        engine.window
    );


    GLint modelLoc =
        glGetUniformLocation(
            engine.shaderProgram,
            "model"
        );

    GLint objectColorLoc =
        glGetUniformLocation(
            engine.shaderProgram,
            "objectColor"
        );


    glUseProgram(
        engine.shaderProgram
    );


    /*
     * Example particle array.
     *
     * The original loads these from:
     *
     * orbitals/orbital_n6_l4_m1.json
     *
     * A JSON parser would need to be added
     * separately in C.
     */

    Vec3 particles[3];

    particles[0] =
        vec3_create(0.0f, 0.0f, 0.0f);

    particles[1] =
        vec3_create(20.0f, 0.0f, 0.0f);

    particles[2] =
        vec3_create(-20.0f, 0.0f, 0.0f);


    // ========================================================
    // RENDER LOOP
    // ========================================================

    while (
        !glfwWindowShouldClose(
            engine.window
        )
    )
    {
        engine_run();


        // Draw particles

        for (int i = 0; i < 3; i++)
        {
            draw_particle(
                particles[i],
                modelLoc,
                objectColorLoc
            );
        }


        glfwSwapBuffers(
            engine.window
        );

        glfwPollEvents();
    }


    glfwDestroyWindow(
        engine.window
    );

    glfwTerminate();

    return 0;
}