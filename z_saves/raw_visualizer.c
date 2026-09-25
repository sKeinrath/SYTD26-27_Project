#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cjson/cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const float c = 299792458.0f / 100000000.0f;    // speed of light in m/s
const float eu = 2.71828182845904523536f;        // Euler's number
const float k = 8.9875517923e9f;                 // Coulomb's constant
const float a0 = 52.9f;                          // Bohr radius in pm
const float electron_r = 5.0f;
const float fieldRes = 25.0f;


/* =========================================================
   Ersatz für glm::vec3
   ========================================================= */

typedef struct {
    float x;
    float y;
    float z;
} vec3;


/* =========================================================
   Ersatz für glm::vec4
   ========================================================= */

typedef struct {
    float r;
    float g;
    float b;
    float a;
} vec4;


/* =========================================================
   Ersatz für glm::mat4
   ========================================================= */

typedef struct {
    float m[16];
} mat4;


/* =========================================================
   Hilfsfunktionen für Vektoren
   ========================================================= */

vec3 vec3_create(float x, float y, float z)
{
    vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}


/* =========================================================
   Hilfsfunktionen für Matrizen
   ========================================================= */

mat4 mat4_identity(void)
{
    mat4 result = {0};

    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;

    return result;
}


mat4 mat4_multiply(mat4 a, mat4 b)
{
    mat4 result = {0};

    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            result.m[col * 4 + row] =
                a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }

    return result;
}


mat4 mat4_translate(mat4 matrix, vec3 position)
{
    mat4 translation = mat4_identity();

    translation.m[12] = position.x;
    translation.m[13] = position.y;
    translation.m[14] = position.z;

    return mat4_multiply(matrix, translation);
}


mat4 mat4_lookAt(vec3 eye, vec3 center, vec3 up)
{
    vec3 f;
    vec3 s;
    vec3 u;

    float fx = center.x - eye.x;
    float fy = center.y - eye.y;
    float fz = center.z - eye.z;

    float flen = sqrtf(fx * fx + fy * fy + fz * fz);

    f.x = fx / flen;
    f.y = fy / flen;
    f.z = fz / flen;

    s.x = f.y * up.z - f.z * up.y;
    s.y = f.z * up.x - f.x * up.z;
    s.z = f.x * up.y - f.y * up.x;

    float slen = sqrtf(s.x * s.x + s.y * s.y + s.z * s.z);

    s.x /= slen;
    s.y /= slen;
    s.z /= slen;

    u.x = s.y * f.z - s.z * f.y;
    u.y = s.z * f.x - s.x * f.z;
    u.z = s.x * f.y - s.y * f.x;

    mat4 result = mat4_identity();

    result.m[0] = s.x;
    result.m[1] = u.x;
    result.m[2] = -f.x;

    result.m[4] = s.y;
    result.m[5] = u.y;
    result.m[6] = -f.y;

    result.m[8] = s.z;
    result.m[9] = u.z;
    result.m[10] = -f.z;

    result.m[12] = -(s.x * eye.x + s.y * eye.y + s.z * eye.z);
    result.m[13] = -(u.x * eye.x + u.y * eye.y + u.z * eye.z);
    result.m[14] = f.x * eye.x + f.y * eye.y + f.z * eye.z;

    return result;
}


mat4 mat4_perspective(float fov, float aspect, float nearPlane, float farPlane)
{
    mat4 result = {0};

    float tanHalfFovy = tanf(fov / 2.0f);

    result.m[0] = 1.0f / (aspect * tanHalfFovy);
    result.m[5] = 1.0f / tanHalfFovy;
    result.m[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    result.m[11] = -1.0f;
    result.m[14] = -(2.0f * farPlane * nearPlane) /
                   (farPlane - nearPlane);

    return result;
}


mat4 mat4_ortho(
    float left,
    float right,
    float bottom,
    float top,
    float nearPlane,
    float farPlane)
{
    mat4 result = mat4_identity();

    result.m[0] = 2.0f / (right - left);
    result.m[5] = 2.0f / (top - bottom);
    result.m[10] = -2.0f / (farPlane - nearPlane);

    result.m[12] = -(right + left) / (right - left);
    result.m[13] = -(top + bottom) / (top - bottom);
    result.m[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);

    return result;
}


float radians(float degrees)
{
    return degrees * (float)M_PI / 180.0f;
}


float clamp_float(float value, float minValue, float maxValue)
{
    if (value < minValue)
        return minValue;

    if (value > maxValue)
        return maxValue;

    return value;
}


/* =========================================================
   Forward declaration
   ========================================================= */

typedef struct Particle Particle;


/* =========================================================
   Camera
   ========================================================= */

typedef struct Camera {

    vec3 target;
    float radius;

    float azimuth;
    float elevation;

    float orbitSpeed;
    float panSpeed;
    double zoomSpeed;

    int dragging;
    int panning;
    int moving;

    double lastX;
    double lastY;

} Camera;


Camera camera = {
    {0.0f, 0.0f, 0.0f},
    500.0f,

    0.0f,
    M_PI / 2.0f,

    0.01f,
    0.01f,
    125.0,

    0,
    0,
    0,

    0.0,
    0.0
};


vec3 camera_position(const Camera* cam)
{
    float clampedElevation =
        clamp_float(
            cam->elevation,
            0.01f,
            (float)M_PI - 0.01f
        );

    return vec3_create(
        cam->radius *
            sinf(clampedElevation) *
            cosf(cam->azimuth),

        cam->radius *
            cosf(clampedElevation),

        cam->radius *
            sinf(clampedElevation) *
            sinf(cam->azimuth)
    );
}


void camera_update(Camera* cam)
{
    cam->target = vec3_create(0.0f, 0.0f, 0.0f);

    if (cam->dragging || cam->panning) {
        cam->moving = 1;
    }
    else {
        cam->moving = 0;
    }
}


void camera_processMouseMove(
    Camera* cam,
    double x,
    double y)
{
    float dx = (float)(x - cam->lastX);
    float dy = (float)(y - cam->lastY);

    if (cam->dragging && cam->panning) {
        /*
         * Pan: Shift + Left or Middle Mouse
         * Disable panning to keep camera centered on black hole
         */
    }
    else if (cam->dragging && !cam->panning) {

        cam->azimuth += dx * cam->orbitSpeed;

        cam->elevation -= dy * cam->orbitSpeed;

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


void camera_processMouseButton(
    Camera* cam,
    int button,
    int action,
    int mods,
    GLFWwindow* win)
{
    (void)mods;

    if (button == GLFW_MOUSE_BUTTON_LEFT ||
        button == GLFW_MOUSE_BUTTON_MIDDLE) {

        if (action == GLFW_PRESS) {

            cam->dragging = 1;

            /*
             * Disable panning so camera always orbits center
             */
            cam->panning = 0;

            glfwGetCursorPos(
                win,
                &cam->lastX,
                &cam->lastY
            );
        }
        else if (action == GLFW_RELEASE) {

            cam->dragging = 0;
            cam->panning = 0;
        }
    }
}


void camera_processScroll(
    Camera* cam,
    double xoffset,
    double yoffset)
{
    (void)xoffset;

    cam->radius -=
        yoffset * cam->zoomSpeed;

    camera_update(cam);
}


/* =========================================================
   Engine
   ========================================================= */

typedef struct Engine {

    GLFWwindow* window;
    GLuint shaderProgram;

    int WIDTH;
    int HEIGHT;

    float width;
    float height;

} Engine;


Engine engine;


/* Forward declarations */
GLuint CreateShaderProgram(void);


void Engine_init(Engine* e)
{
    e->WIDTH = 800;
    e->HEIGHT = 600;

    e->width = 1000.0f;
    e->height = 750.0f;

    if (!glfwInit()) {

        fprintf(stderr, "GLFW init failed\n");

        exit(EXIT_FAILURE);
    }


    e->window =
        glfwCreateWindow(
            e->WIDTH,
            e->HEIGHT,
            "Quantum Simulation by kavan G",
            NULL,
            NULL
        );


    if (!e->window) {

        fprintf(
            stderr,
            "Failed to create GLFW window\n"
        );

        glfwTerminate();

        exit(EXIT_FAILURE);
    }


    glViewport(
        0,
        0,
        e->WIDTH,
        e->HEIGHT
    );

    glClearColor(
        0.0f,
        0.0f,
        0.0f,
        1.0f
    );


    glfwMakeContextCurrent(e->window);

    glEnable(GL_DEPTH_TEST);


    /*
     * Enable alpha blending for transparent objects
     */

    glEnable(GL_BLEND);

    /*
     * glBlendFunc(
     *     GL_SRC_ALPHA,
     *     GL_ONE_MINUS_SRC_ALPHA
     * );
     */

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE
    );


    glewExperimental = GL_TRUE;

    GLenum glewErr = glewInit();

    if (glewErr != GLEW_OK) {

        fprintf(
            stderr,
            "Failed to initialize GLEW: %s\n",
            (const char*)glewGetErrorString(glewErr)
        );

        glfwTerminate();

        exit(EXIT_FAILURE);
    }


    e->shaderProgram =
        CreateShaderProgram();
}


void Engine_run(Engine* e)
{
    glClear(
        GL_COLOR_BUFFER_BIT |
        GL_DEPTH_BUFFER_BIT
    );

    glUseProgram(e->shaderProgram);


    vec3 cameraPos =
        camera_position(&camera);

    mat4 view =
        mat4_lookAt(
            cameraPos,
            camera.target,
            vec3_create(0.0f, 1.0f, 0.0f)
        );


    mat4 projection =
        mat4_perspective(
            radians(45.0f),
            (float)e->WIDTH /
                (float)e->HEIGHT,
            0.1f,
            10000.0f
        );


    glUniformMatrix4fv(
        glGetUniformLocation(
            e->shaderProgram,
            "view"
        ),
        1,
        GL_FALSE,
        view.m
    );


    glUniformMatrix4fv(
        glGetUniformLocation(
            e->shaderProgram,
            "projection"
        ),
        1,
        GL_FALSE,
        projection.m
    );
}


/* =========================================================
   Shader
   ========================================================= */

GLuint CreateShaderProgram(void)
{
    const char* vertexShaderSource =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "uniform vec4 objectColor;\n"
        "\n"
        "out vec4 FragColorVS;\n"
        "\n"
        "void main() {\n"
        "    gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
        "    FragColorVS = objectColor;\n"
        "}";


    const char* fragmentShaderSource =
        "#version 330 core\n"
        "in vec4 FragColorVS;\n"
        "out vec4 FragColor;\n"
        "\n"
        "void main() {\n"
        "    FragColor = FragColorVS;\n"
        "}";


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


/* =========================================================
   VBO / VAO
   ========================================================= */

void Engine_CreateVBOVAO(
    Engine* e,
    GLuint* VAO,
    GLuint* VBO,
    const float* vertices,
    size_t vertexCount)
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


/* =========================================================
   Spherical -> Cartesian
   ========================================================= */

vec3 Engine_sphericalToCartesian(
    float r,
    float theta,
    float phi)
{
    float x =
        r * sinf(theta) * cosf(phi);

    float y =
        r * cosf(theta);

    float z =
        r * sinf(theta) * sinf(phi);

    return vec3_create(x, y, z);
}


/* =========================================================
   2D Rendering
   ========================================================= */

void Engine_SwitchTo2DRendering(Engine* e)
{
    mat4 ortho =
        mat4_ortho(
            0.0f,
            (float)e->WIDTH,
            0.0f,
            (float)e->HEIGHT,
            -1.0f,
            1.0f
        );


    GLuint projLoc =
        glGetUniformLocation(
            e->shaderProgram,
            "projection"
        );


    glUniformMatrix4fv(
        projLoc,
        1,
        GL_FALSE,
        ortho.m
    );


    mat4 view2D =
        mat4_identity();


    GLuint viewLoc =
        glGetUniformLocation(
            e->shaderProgram,
            "view"
        );


    glUniformMatrix4fv(
        viewLoc,
        1,
        GL_FALSE,
        view2D.m
    );
}


/* =========================================================
   Camera Callbacks
   ========================================================= */

void mouse_button_callback(
    GLFWwindow* win,
    int button,
    int action,
    int mods)
{
    Camera* cam =
        (Camera*)glfwGetWindowUserPointer(win);

    camera_processMouseButton(
        cam,
        button,
        action,
        mods,
        win
    );
}


void cursor_position_callback(
    GLFWwindow* win,
    double x,
    double y)
{
    Camera* cam =
        (Camera*)glfwGetWindowUserPointer(win);

    camera_processMouseMove(
        cam,
        x,
        y
    );
}


void scroll_callback(
    GLFWwindow* win,
    double xoffset,
    double yoffset)
{
    Camera* cam =
        (Camera*)glfwGetWindowUserPointer(win);

    camera_processScroll(
        cam,
        xoffset,
        yoffset
    );
}


void setupCameraCallbacks(GLFWwindow* window)
{
    glfwSetWindowUserPointer(
        window,
        &camera
    );


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


/* =========================================================
   Particle
   ========================================================= */

struct Particle {

    GLuint VAO;
    GLuint VBO;

    float radius;

    vec4 color;

    vec3 position;

    float* vertices;

    size_t vertexCount;
};


void Particle_DrawSphere(
    Particle* p,
    Engine* e)
{
    int stacks = 25;
    int sectors = 25;

    size_t maxVertices =
        (size_t)stacks *
        (size_t)sectors *
        6 *
        3;

    p->vertices =
        (float*)malloc(
            maxVertices *
            sizeof(float)
        );

    if (!p->vertices) {
        fprintf(
            stderr,
            "Failed to allocate sphere vertices\n"
        );
        exit(EXIT_FAILURE);
    }


    size_t index = 0;


    for (float i = 0.0f;
         i <= stacks;
         ++i)
    {
        float theta1 =
            (i / stacks) *
            (float)M_PI;

        float theta2 =
            ((i + 1.0f) / stacks) *
            (float)M_PI;


        for (float j = 0.0f;
             j < sectors;
             ++j)
        {
            float phi1 =
                (j / sectors) *
                2.0f *
                (float)M_PI;

            float phi2 =
                ((j + 1.0f) / sectors) *
                2.0f *
                (float)M_PI;


            vec3 v1 =
                Engine_sphericalToCartesian(
                    p->radius,
                    theta1,
                    phi1
                );

            vec3 v2 =
                Engine_sphericalToCartesian(
                    p->radius,
                    theta1,
                    phi2
                );

            vec3 v3 =
                Engine_sphericalToCartesian(
                    p->radius,
                    theta2,
                    phi1
                );

            vec3 v4 =
                Engine_sphericalToCartesian(
                    p->radius,
                    theta2,
                    phi2
                );


            /*
             * Triangle 1: v1-v2-v3
             */

            p->vertices[index++] = v1.x;
            p->vertices[index++] = v1.y;
            p->vertices[index++] = v1.z;

            p->vertices[index++] = v2.x;
            p->vertices[index++] = v2.y;
            p->vertices[index++] = v2.z;

            p->vertices[index++] = v3.x;
            p->vertices[index++] = v3.y;
            p->vertices[index++] = v3.z;


            /*
             * Triangle 2: v2-v4-v3
             */

            p->vertices[index++] = v2.x;
            p->vertices[index++] = v2.y;
            p->vertices[index++] = v2.z;

            p->vertices[index++] = v4.x;
            p->vertices[index++] = v4.y;
            p->vertices[index++] = v4.z;

            p->vertices[index++] = v3.x;
            p->vertices[index++] = v3.y;
            p->vertices[index++] = v3.z;
        }
    }


    p->vertexCount =
        index;
}


void Particle_init(
    Particle* p,
    float r,
    vec4 col,
    vec3 pos)
{
    p->radius = r;
    p->color = col;
    p->position = pos;

    Particle_DrawSphere(
        p,
        &engine
    );


    Engine_CreateVBOVAO(
        &engine,
        &p->VAO,
        &p->VBO,
        p->vertices,
        p->vertexCount
    );
}


void Particle_Draw(
    Particle* p,
    GLint objectColorLoc,
    GLint modelLoc)
{
    glUniform4f(
        objectColorLoc,
        p->color.r,
        p->color.g,
        p->color.b,
        p->color.a
    );


    mat4 model =
        mat4_identity();


    model =
        mat4_translate(
            model,
            p->position
        );


    glUniformMatrix4fv(
        modelLoc,
        1,
        GL_FALSE,
        model.m
    );


    glUniform1i(
        glGetUniformLocation(
            engine.shaderProgram,
            "GLOW"
        ),
        0
    );


    glBindVertexArray(
        p->VAO
    );
}


/* =========================================================
   JSON / Wavefunction
   ========================================================= */

typedef struct {
    Particle* data;
    size_t size;
    size_t capacity;
} ParticleArray;


void ParticleArray_init(ParticleArray* array)
{
    array->data = NULL;
    array->size = 0;
    array->capacity = 0;
}


void ParticleArray_push(
    ParticleArray* array,
    Particle particle)
{
    if (array->size >= array->capacity) {

        size_t newCapacity =
            array->capacity == 0
                ? 16
                : array->capacity * 2;


        Particle* newData =
            (Particle*)realloc(
                array->data,
                newCapacity *
                sizeof(Particle)
            );


        if (!newData) {
            fprintf(
                stderr,
                "Failed to allocate particles\n"
            );
            exit(EXIT_FAILURE);
        }


        array->data = newData;
        array->capacity = newCapacity;
    }


    array->data[array->size++] =
        particle;
}


ParticleArray LoadWavefunction(
    const char* filename)
{
    ParticleArray pts;

    ParticleArray_init(&pts);


    char path[1024];

    snprintf(
        path,
        sizeof(path),
        "orbitals/%s",
        filename
    );


    FILE* file =
        fopen(path, "r");


    if (!file) {

        fprintf(
            stderr,
            "Failed to open JSON file: %s\n",
            filename
        );

        return pts;
    }


    fseek(file, 0, SEEK_END);

    long fileSize =
        ftell(file);

    fseek(file, 0, SEEK_SET);


    char* fileContent =
        (char*)malloc(
            (size_t)fileSize + 1
        );


    if (!fileContent) {

        fclose(file);

        fprintf(
            stderr,
            "Failed to allocate JSON buffer\n"
        );

        return pts;
    }


    fread(
        fileContent,
        1,
        (size_t)fileSize,
        file
    );


    fileContent[fileSize] =
        '\0';


    fclose(file);


    cJSON* j =
        cJSON_Parse(fileContent);


    free(fileContent);


    if (!j) {

        fprintf(
            stderr,
            "Failed to parse JSON file: %s\n",
            filename
        );

        return pts;
    }


    cJSON* points =
        cJSON_GetObjectItem(
            j,
            "points"
        );


    if (!cJSON_IsArray(points)) {

        cJSON_Delete(j);

        return pts;
    }


    const float bohr_to_pm =
        5.29f;


    cJSON* point = NULL;


    cJSON_ArrayForEach(
        point,
        points)
    {
        cJSON* xValue =
            cJSON_GetArrayItem(
                point,
                0
            );

        cJSON* yValue =
            cJSON_GetArrayItem(
                point,
                1
            );

        cJSON* zValue =
            cJSON_GetArrayItem(
                point,
                2
            );


        if (!cJSON_IsNumber(xValue) ||
            !cJSON_IsNumber(yValue) ||
            !cJSON_IsNumber(zValue))
        {
            continue;
        }


        float x =
            (float)xValue->valuedouble *
            bohr_to_pm;

        float y =
            (float)yValue->valuedouble *
            bohr_to_pm;

        float z =
            (float)zValue->valuedouble *
            bohr_to_pm;


        /*
         * Particle radius
         * (small for electrons)
         */

        float radius =
            1.0f;


        /*
         * Color: blue for electron
         */

        /*
         * vec4 color =
         *     vec4_create(
         *         0.2f,
         *         0.5f,
         *         1.0f,
         *         1.0f
         *     );
         */


        vec4 color = {
            1.0f,
            0.0f,
            1.0f,
            1.0f
        };


        Particle p;


        Particle_init(
            &p,
            radius,
            color,
            vec3_create(
                x,
                y,
                z
            )
        );


        ParticleArray_push(
            &pts,
            p
        );
    }


    cJSON_Delete(j);


    return pts;
}


/* =========================================================
   Grid
   ========================================================= */

typedef struct {

    GLuint gridVAO;
    GLuint gridVBO;

    float* vertices;

    size_t vertexCount;

} Grid;


void Grid_CreateGridVertices(
    Grid* grid,
    float size,
    int divisions)
{
    size_t capacity = 1024;

    grid->vertices =
        (float*)malloc(
            capacity *
            sizeof(float)
        );


    if (!grid->vertices) {
        fprintf(
            stderr,
            "Failed to allocate grid vertices\n"
        );
        exit(EXIT_FAILURE);
    }


    grid->vertexCount = 0;


    float step =
        size / divisions;

    float halfSize =
        size / 2.0f;


    /*
     * amount to extend the central X-axis line
     */

    float extra =
        step * 3.0f;

    int midZ =
        divisions / 2;


    #define GRID_PUSH(value)                                      \
        do {                                                      \
            if (grid->vertexCount >= capacity) {                 \
                capacity *= 2;                                   \
                grid->vertices = (float*)realloc(               \
                    grid->vertices,                             \
                    capacity * sizeof(float)                    \
                );                                               \
                if (!grid->vertices) {                          \
                    fprintf(stderr, "Grid allocation failed\n"); \
                    exit(EXIT_FAILURE);                         \
                }                                                \
            }                                                     \
            grid->vertices[grid->vertexCount++] = (value);       \
        } while (0)


    /*
     * x axis
     */

    for (int yStep = 3;
         yStep <= 3;
         ++yStep)
    {
        float y = 0;


        for (int zStep = 0;
             zStep <= divisions;
             ++zStep)
        {
            float z =
                -halfSize +
                zStep * step;


            for (int xStep = 0;
                 xStep < divisions;
                 ++xStep)
            {
                float xStart =
                    -halfSize +
                    xStep * step;

                float xEnd =
                    xStart + step;


                /*
                 * If this is the central line,
                 * extend first and last segment.
                 */

                if (zStep == midZ) {

                    if (xStep == 0) {
                        xStart -= extra;
                    }

                    if (xStep == divisions - 1) {
                        xEnd += extra;
                    }
                }


                GRID_PUSH(xStart);
                GRID_PUSH(y);
                GRID_PUSH(z);

                GRID_PUSH(xEnd);
                GRID_PUSH(y);
                GRID_PUSH(z);
            }
        }
    }


    /*
     * z axis
     */

    for (int xStep = 0;
         xStep <= divisions;
         ++xStep)
    {
        float x =
            -halfSize +
            xStep * step;


        for (int yStep = 3;
             yStep <= 3;
             ++yStep)
        {
            float y = 0;


            for (int zStep = 0;
                 zStep < divisions;
                 ++zStep)
            {
                float zStart =
                    -halfSize +
                    zStep * step;

                float zEnd =
                    zStart + step;


                GRID_PUSH(x);
                GRID_PUSH(y);
                GRID_PUSH(zStart);

                GRID_PUSH(x);
                GRID_PUSH(y);
                GRID_PUSH(zEnd);
            }
        }
    }


    #undef GRID_PUSH
}


void Grid_init(Grid* grid)
{
    Grid_CreateGridVertices(
        grid,
        500.0f,
        2
    );


    Engine_CreateVBOVAO(
        &engine,
        &grid->gridVAO,
        &grid->gridVBO,
        grid->vertices,
        grid->vertexCount
    );
}


void Grid_DrawGrid(
    Grid* grid,
    GLuint shaderProgram,
    GLuint gridVAO,
    size_t vertexCount)
{
    glUseProgram(shaderProgram);


    mat4 model =
        mat4_identity();


    GLint modelLoc =
        glGetUniformLocation(
            shaderProgram,
            "model"
        );


    glUniformMatrix4fv(
        modelLoc,
        1,
        GL_FALSE,
        model.m
    );


    glBindVertexArray(
        gridVAO
    );


    glPointSize(2.0f);


    glDrawArrays(
        GL_LINES,
        0,
        (GLsizei)(vertexCount / 3)
    );


    glBindVertexArray(0);
}


void Grid_Draw(
    Grid* grid,
    GLint objectColorLoc)
{
    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );


    glUseProgram(
        engine.shaderProgram
    );


    glUniform4f(
        objectColorLoc,
        1.0f,
        1.0f,
        1.0f,
        0.05f
    );


    glBindBuffer(
        GL_ARRAY_BUFFER,
        grid->gridVBO
    );


    glBufferData(
        GL_ARRAY_BUFFER,
        grid->vertexCount *
            sizeof(float),
        grid->vertices,
        GL_DYNAMIC_DRAW
    );


    Grid_DrawGrid(
        grid,
        engine.shaderProgram,
        grid->gridVAO,
        grid->vertexCount
    );
}


Grid grid;


/* =========================================================
   Main
   ========================================================= */

int main(void)
{
    /*
     * Engine constructor
     */

    Engine_init(&engine);


    setupCameraCallbacks(
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
     * Grid constructor
     */

    Grid_init(&grid);


    /*
     * Load wavefunction
     */

    ParticleArray particles =
        LoadWavefunction(
            "orbital_n7_l4_m0.json"
        );


    /*
     * Rendering loop
     */

    while (
        !glfwWindowShouldClose(
            engine.window
        )
    )
    {
        Engine_run(&engine);


        /*
         * DRAW GRID
         */

        Grid_Draw(
            &grid,
            objectColorLoc
        );


        /*
         * DRAW PARTICLES
         */

        for (size_t i = 0;
             i < particles.size;
             ++i)
        {
            Particle* p =
                &particles.data[i];


            Particle_Draw(
                p,
                objectColorLoc,
                modelLoc
            );


            glDrawArrays(
                GL_TRIANGLES,
                0,
                (GLsizei)(p->vertexCount / 3)
            );


            glBindVertexArray(0);
        }


        glfwSwapBuffers(
            engine.window
        );


        glfwPollEvents();
    }


    /*
     * CLEAN UP
     */

    for (size_t i = 0;
         i < particles.size;
         ++i)
    {
        glDeleteVertexArrays(
            1,
            &particles.data[i].VAO
        );


        glDeleteBuffers(
            1,
            &particles.data[i].VBO
        );


        free(
            particles.data[i].vertices
        );
    }


    free(
        particles.data
    );


    free(
        grid.vertices
    );


    glfwDestroyWindow(
        engine.window
    );


    glfwTerminate();


    return 0;
}