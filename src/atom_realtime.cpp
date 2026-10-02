/*
 * Converted from C++ to C99.
 *
 * Dependencies:
 *   - OpenGL / GLU
 *   - GLFW 3
 *   - GLEW
 *
 * Build example (Linux):
 *   cc atom_prob_flow.c -o atom_prob_flow -lglfw -lGLEW -lGL -lGLU -lm
 *
 * Note:
 *   The original program used GLM (C++ only). This C version replaces the
 *   required vector/matrix operations with small C structs and functions.
 */

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================= Small math types ================= */

typedef struct {
    float x;
    float y;
    float z;
} Vec3;

typedef struct {
    float r;
    float g;
    float b;
    float a;
} Vec4;

typedef struct {
    float m[16];
} Mat4;

static Vec3 vec3_make(float x, float y, float z) {
    Vec3 v = {x, y, z};
    return v;
}

static Vec3 vec3_zero(void) {
    return vec3_make(0.0f, 0.0f, 0.0f);
}

static Vec3 vec3_add(Vec3 a, Vec3 b) {
    return vec3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

static Vec3 vec3_scale(Vec3 v, float s) {
    return vec3_make(v.x * s, v.y * s, v.z * s);
}

static float vec3_length(Vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static Vec4 vec4_make(float r, float g, float b, float a) {
    Vec4 v = {r, g, b, a};
    return v;
}

/* Column-major 4x4 matrix, compatible with OpenGL glUniformMatrix4fv(). */
static Mat4 mat4_identity(void) {
    Mat4 out = {{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    }};
    return out;
}

static Mat4 mat4_multiply(Mat4 a, Mat4 b) {
    Mat4 out;
    int col, row, k;

    for (col = 0; col < 4; ++col) {
        for (row = 0; row < 4; ++row) {
            out.m[col * 4 + row] = 0.0f;
            for (k = 0; k < 4; ++k) {
                out.m[col * 4 + row] +=
                    a.m[k * 4 + row] * b.m[col * 4 + k];
            }
        }
    }
    return out;
}

static Mat4 mat4_translate(Vec3 v) {
    Mat4 out = mat4_identity();
    out.m[12] = v.x;
    out.m[13] = v.y;
    out.m[14] = v.z;
    return out;
}

static Mat4 mat4_scale_uniform(float s) {
    Mat4 out = mat4_identity();
    out.m[0] = s;
    out.m[5] = s;
    out.m[10] = s;
    return out;
}

static Mat4 mat4_scale_vec3(Vec3 v) {
    Mat4 out = mat4_identity();
    out.m[0] = v.x;
    out.m[5] = v.y;
    out.m[10] = v.z;
    return out;
}

static Mat4 mat4_perspective(float fovy_radians, float aspect,
                             float z_near, float z_far) {
    Mat4 out = {{0}};
    float f = 1.0f / tanf(fovy_radians / 2.0f);

    out.m[0] = f / aspect;
    out.m[5] = f;
    out.m[10] = (z_far + z_near) / (z_near - z_far);
    out.m[11] = -1.0f;
    out.m[14] = (2.0f * z_far * z_near) / (z_near - z_far);
    return out;
}

static Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return vec3_make(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

static float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 vec3_normalize(Vec3 v) {
    float len = vec3_length(v);
    if (len <= 1e-8f) {
        return vec3_zero();
    }
    return vec3_scale(v, 1.0f / len);
}

static Mat4 mat4_look_at(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = vec3_normalize((Vec3){
        center.x - eye.x,
        center.y - eye.y,
        center.z - eye.z
    });
    Vec3 s = vec3_normalize(vec3_cross(f, up));
    Vec3 u = vec3_cross(s, f);

    Mat4 out = mat4_identity();

    out.m[0] = s.x;
    out.m[1] = s.y;
    out.m[2] = s.z;

    out.m[4] = u.x;
    out.m[5] = u.y;
    out.m[6] = u.z;

    out.m[8] = -f.x;
    out.m[9] = -f.y;
    out.m[10] = -f.z;

    out.m[12] = -vec3_dot(s, eye);
    out.m[13] = -vec3_dot(u, eye);
    out.m[14] = vec3_dot(f, eye);

    return out;
}

/* ================= Constants ================= */

static const float a0 = 1.0f;
static float electron_r = 1.5f;
static const double hbar = 1.0;
static const double m_e = 1.0;
static const double zmSpeed = 10.0;

/* --- Global quantum numbers --- */
static int n = 2;
static int l = 1;
static int m = 0;
static int N = 100000;

/* ================= Random sampling ================= */

static double uniform01(void) {
    return (double)rand() / (double)RAND_MAX;
}

/* ================= Particle ================= */

typedef struct {
    Vec3 pos;
    Vec3 vel;
    Vec4 color;
} Particle;

typedef struct {
    Particle *data;
    size_t size;
    size_t capacity;
} ParticleArray;

static void particle_array_init(ParticleArray *arr) {
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
}

static void particle_array_clear(ParticleArray *arr) {
    arr->size = 0;
}

static int particle_array_reserve(ParticleArray *arr, size_t capacity) {
    Particle *new_data;

    if (capacity <= arr->capacity) {
        return 1;
    }

    new_data = (Particle *)realloc(arr->data, capacity * sizeof(Particle));
    if (new_data == NULL) {
        return 0;
    }

    arr->data = new_data;
    arr->capacity = capacity;
    return 1;
}

static int particle_array_push(ParticleArray *arr, Particle p) {
    size_t new_capacity;

    if (arr->size == arr->capacity) {
        new_capacity = (arr->capacity == 0) ? 1024 : arr->capacity * 2;
        if (!particle_array_reserve(arr, new_capacity)) {
            return 0;
        }
    }

    arr->data[arr->size++] = p;
    return 1;
}

static void particle_array_free(ParticleArray *arr) {
    free(arr->data);
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
}

static ParticleArray particles;

/* ================= Physics Sampling ================= */

/* --- Associated Laguerre polynomial L_k^alpha(rho) --- */
static double associated_laguerre(int k, int alpha, double rho) {
    double L = 1.0;
    double Lm1 = 1.0 + alpha - rho;

    if (k == 1) {
        return Lm1;
    } else if (k > 1) {
        double Lm2 = 1.0;
        int j;

        for (j = 2; j <= k; ++j) {
            L = ((2.0 * j - 1.0 + alpha - rho) * Lm1 -
                 (j - 1.0 + alpha) * Lm2) / (double)j;
            Lm2 = Lm1;
            Lm1 = L;
        }
    }

    return L;
}

/* --- sample R: CDF sampling --- */
static double sampleR(int quantum_n, int quantum_l) {
    const int sample_count = 4096;
    double rMax = 10.0 * quantum_n * quantum_n * a0;
    static double *cdf = NULL;
    static int built = 0;

    if (!built) {
        double dr = rMax / (double)(sample_count - 1);
        double sum = 0.0;
        int i;

        cdf = (double *)malloc((size_t)sample_count * sizeof(double));
        if (cdf == NULL) {
            fprintf(stderr, "Failed to allocate radial CDF.\n");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < sample_count; ++i) {
            double r = (double)i * dr;
            double rho = 2.0 * r / (quantum_n * a0);
            int k = quantum_n - quantum_l - 1;
            int alpha = 2 * quantum_l + 1;

            double L = associated_laguerre(k, alpha, rho);
            double norm =
                pow(2.0 / (quantum_n * a0), 3.0) *
                tgamma(quantum_n - quantum_l) /
                (2.0 * quantum_n * tgamma(quantum_n + quantum_l + 1));

            double R =
                sqrt(norm) *
                exp(-rho / 2.0) *
                pow(rho, quantum_l) *
                L;

            double pdf = r * r * R * R;
            sum += pdf;
            cdf[i] = sum;
        }

        if (sum > 0.0) {
            for (i = 0; i < sample_count; ++i) {
                cdf[i] /= sum;
            }
        }

        built = 1;
    }

    {
        double u = uniform01();
        int low = 0;
        int high = sample_count - 1;
        int idx;

        while (low < high) {
            int mid = low + (high - low) / 2;
            if (cdf[mid] < u) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }

        idx = low;
        return idx * (rMax / (double)(sample_count - 1));
    }
}

/* --- Associated Legendre polynomial P_l^m(x) --- */
static double associated_legendre(int quantum_l, int quantum_m, double x) {
    double Pmm = 1.0;

    if (quantum_m > 0) {
        double somx2 = sqrt((1.0 - x) * (1.0 + x));
        double fact = 1.0;
        int j;

        for (j = 1; j <= quantum_m; ++j) {
            Pmm *= -fact * somx2;
            fact += 2.0;
        }
    }

    if (quantum_l == quantum_m) {
        return Pmm;
    } else {
        double Pm1m = x * (2.0 * quantum_m + 1.0) * Pmm;

        if (quantum_l == quantum_m + 1) {
            return Pm1m;
        } else {
            int ll;
            double Pll = 0.0;

            for (ll = quantum_m + 2; ll <= quantum_l; ++ll) {
                Pll = ((2.0 * ll - 1.0) * x * Pm1m -
                       (ll + quantum_m - 1.0) * Pmm) /
                      (double)(ll - quantum_m);
                Pmm = Pm1m;
                Pm1m = Pll;
            }

            return Pm1m;
        }
    }
}

/* --- sample Theta: CDF sampling --- */
static double sampleTheta(int quantum_l, int quantum_m) {
    const int sample_count = 2048;
    static double *cdf = NULL;
    static int built = 0;

    if (!built) {
        double dtheta = M_PI / (double)(sample_count - 1);
        double sum = 0.0;
        int i;

        cdf = (double *)malloc((size_t)sample_count * sizeof(double));
        if (cdf == NULL) {
            fprintf(stderr, "Failed to allocate angular CDF.\n");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < sample_count; ++i) {
            double theta = (double)i * dtheta;
            double x = cos(theta);
            double Plm = associated_legendre(quantum_l, quantum_m, x);
            double pdf = sin(theta) * Plm * Plm;

            sum += pdf;
            cdf[i] = sum;
        }

        if (sum > 0.0) {
            for (i = 0; i < sample_count; ++i) {
                cdf[i] /= sum;
            }
        }

        built = 1;
    }

    {
        double u = uniform01();
        int low = 0;
        int high = sample_count - 1;
        int idx;

        while (low < high) {
            int mid = low + (high - low) / 2;
            if (cdf[mid] < u) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }

        idx = low;
        return idx * (M_PI / (double)(sample_count - 1));
    }
}

/* --- sample Phi (uniform) --- */
static float samplePhi(void) {
    return (float)(2.0 * M_PI * uniform01());
}

/* --- calculate probability current --- */
static Vec3 calculateProbabilityFlow(const Particle *p,
                                     int quantum_n,
                                     int quantum_l,
                                     int quantum_m) {
    (void)quantum_n;
    (void)quantum_l;

    double r = (double)vec3_length(p->pos);

    if (r < 1e-6) {
        return vec3_zero();
    }

    {
        double theta = acos((double)p->pos.y / r);
        double phi = atan2((double)p->pos.z, (double)p->pos.x);
        double sinTheta = sin(theta);

        if (fabs(sinTheta) < 1e-4) {
            sinTheta = 1e-4;
        }

        double v_mag = hbar * quantum_m / (m_e * r * sinTheta);
        double vx = -v_mag * sin(phi);
        double vy = 0.0;
        double vz = v_mag * cos(phi);

        return vec3_make((float)vx, (float)vy, (float)vz);
    }
}

/* ================= Color ================= */

static Vec4 heatmap_fire(float value) {
    const int num_stops = 6;
    static const Vec4 colors[6] = {
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.5f, 0.0f, 0.99f, 1.0f},
        {0.8f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.5f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f}
    };

    Vec4 result;
    float scaled_v;
    float local_t;
    int i;
    int next_i;

    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    scaled_v = value * (float)(num_stops - 1);
    i = (int)scaled_v;
    next_i = (i + 1 < num_stops) ? i + 1 : num_stops - 1;
    local_t = scaled_v - (float)i;

    result.r = colors[i].r + local_t * (colors[next_i].r - colors[i].r);
    result.g = colors[i].g + local_t * (colors[next_i].g - colors[i].g);
    result.b = colors[i].b + local_t * (colors[next_i].b - colors[i].b);
    result.a = 1.0f;

    return result;
}

static Vec4 inferno(double r, double theta, double phi,
                    int quantum_n, int quantum_l, int quantum_m) {
    double rho = 2.0 * r / (quantum_n * a0);
    int k = quantum_n - quantum_l - 1;
    int alpha = 2 * quantum_l + 1;

    double L = associated_laguerre(k, alpha, rho);
    double norm =
        pow(2.0 / (quantum_n * a0), 3.0) *
        tgamma(quantum_n - quantum_l) /
        (2.0 * quantum_n * tgamma(quantum_n + quantum_l + 1));

    double R = sqrt(norm) * exp(-rho / 2.0) * pow(rho, quantum_l) * L;
    double radial = R * R;

    double x = cos(theta);
    double Plm = associated_legendre(quantum_l, quantum_m, x);
    double angular = Plm * Plm;

    double intensity = radial * angular;

    (void)phi; /* phi does not affect the real-valued |Y_l^m|^2 here */

    return heatmap_fire((float)(intensity * 1.5 * pow(5.0, quantum_n)));
}

/* ================= Camera ================= */

typedef struct {
    Vec3 target;
    float radius;
    float azimuth;
    float elevation;
    float orbitSpeed;
    float panSpeed;
    double zoomSpeed;
    int dragging;
    int panning;
    double lastX;
    double lastY;
} Camera;

static Camera camera = {
    {0.0f, 0.0f, 0.0f},
    50.0f,
    0.0f,
    (float)(M_PI / 2.0),
    0.01f,
    0.01f,
    zmSpeed,
    0,
    0,
    0.0,
    0.0
};

static Vec3 camera_position(const Camera *cam) {
    float clampedElevation = cam->elevation;

    if (clampedElevation < 0.01f) {
        clampedElevation = 0.01f;
    }
    if (clampedElevation > (float)M_PI - 0.01f) {
        clampedElevation = (float)M_PI - 0.01f;
    }

    return vec3_make(
        cam->radius * sinf(clampedElevation) * cosf(cam->azimuth),
        cam->radius * cosf(clampedElevation),
        cam->radius * sinf(clampedElevation) * sinf(cam->azimuth)
    );
}

static void camera_update(Camera *cam) {
    cam->target = vec3_zero();
}

static void camera_process_mouse_move(Camera *cam, double x, double y) {
    float dx = (float)(x - cam->lastX);
    float dy = (float)(y - cam->lastY);

    if (cam->dragging) {
        cam->azimuth += dx * cam->orbitSpeed;
        cam->elevation -= dy * cam->orbitSpeed;

        if (cam->elevation < 0.01f) {
            cam->elevation = 0.01f;
        }
        if (cam->elevation > (float)M_PI - 0.01f) {
            cam->elevation = (float)M_PI - 0.01f;
        }
    }

    cam->lastX = x;
    cam->lastY = y;
    camera_update(cam);
}

static void camera_process_mouse_button(Camera *cam, int button, int action,
                                         int mods, GLFWwindow *win) {
    (void)mods;

    if (button == GLFW_MOUSE_BUTTON_LEFT ||
        button == GLFW_MOUSE_BUTTON_MIDDLE) {
        if (action == GLFW_PRESS) {
            cam->dragging = 1;
            glfwGetCursorPos(win, &cam->lastX, &cam->lastY);
        } else if (action == GLFW_RELEASE) {
            cam->dragging = 0;
        }
    }
}

static void camera_process_scroll(Camera *cam, double xoffset, double yoffset) {
    (void)xoffset;

    cam->radius -= (float)(yoffset * cam->zoomSpeed);
    if (cam->radius < 1.0f) {
        cam->radius = 1.0f;
    }
    camera_update(cam);
}

static Vec3 sphericalToCartesian(float r, float theta, float phi) {
    return vec3_make(
        r * sinf(theta) * cosf(phi),
        r * cosf(theta),
        r * sinf(theta) * sinf(phi)
    );
}

/* ================= Engine ================= */

typedef struct {
    GLFWwindow *window;
    int WIDTH;
    int HEIGHT;

    GLuint sphereVAO;
    GLuint sphereVBO;
    int sphereVertexCount;
    GLuint shaderProgram;
    GLint modelLoc;
    GLint viewLoc;
    GLint projLoc;
    GLint colorLoc;
} Engine;

static Engine engine;

/* --- shaders --- */

static const char *vertexShaderSource =
    "#version 330 core\n"
    "layout(location=0) in vec3 aPos;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "out float lightIntensity;\n"
    "void main() {\n"
    "    gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
    "    vec3 normal = normalize(aPos);\n"
    "    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));\n"
    "    lightIntensity = max(dot(normal, lightDir), 0.5);\n"
    "}\n";

static const char *fragmentShaderSource =
    "#version 330 core\n"
    "in float lightIntensity;\n"
    "out vec4 FragColor;\n"
    "uniform vec4 objectColor;\n"
    "void main() {\n"
    "    float glow = pow(lightIntensity, 2.0);\n"
    "    (void)glow;\n"
    "    FragColor = vec4(objectColor.rgb, objectColor.a);\n"
    "}\n";

static void check_shader(GLuint shader, const char *stage_name) {
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char infoLog[2048];
        glGetShaderInfoLog(shader, (GLsizei)sizeof(infoLog), NULL, infoLog);
        fprintf(stderr, "%s shader compilation failed:\n%s\n",
                stage_name, infoLog);
        exit(EXIT_FAILURE);
    }
}

static void check_program(GLuint program) {
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        char infoLog[2048];
        glGetProgramInfoLog(program, (GLsizei)sizeof(infoLog), NULL, infoLog);
        fprintf(stderr, "Shader program linking failed:\n%s\n", infoLog);
        exit(EXIT_FAILURE);
    }
}

static int engine_create_vbo_vao(GLuint *VAO, GLuint *VBO,
                                 const float *vertices, size_t floatCount) {
    glGenVertexArrays(1, VAO);
    glGenBuffers(1, VBO);

    glBindVertexArray(*VAO);
    glBindBuffer(GL_ARRAY_BUFFER, *VBO);

    glBufferData(GL_ARRAY_BUFFER,
                 floatCount * sizeof(float),
                 vertices,
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          3 * (GLsizei)sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return 1;
}

static void engine_init(Engine *eng) {
    float r = 0.05f;
    int stacks = 10;
    int sectors = 10;
    size_t vertex_capacity =
        (size_t)(stacks + 1) * (size_t)sectors * 18u;
    size_t vertex_count = 0;
    float *vertices;
    int i, j;

    eng->WIDTH = 800;
    eng->HEIGHT = 600;

    if (!glfwInit()) {
        fprintf(stderr, "glfwInit() failed.\n");
        exit(EXIT_FAILURE);
    }

    eng->window = glfwCreateWindow(
        eng->WIDTH, eng->HEIGHT, "Atom Prob-Flow", NULL, NULL);

    if (eng->window == NULL) {
        fprintf(stderr, "glfwCreateWindow() failed.\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(eng->window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "glewInit() failed.\n");
        glfwDestroyWindow(eng->window);
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glEnable(GL_DEPTH_TEST);

    vertices = (float *)malloc(vertex_capacity * sizeof(float));
    if (vertices == NULL) {
        fprintf(stderr, "Failed to allocate sphere vertices.\n");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i <= stacks; ++i) {
        float t1 = (float)i / (float)stacks * (float)M_PI;
        float t2 = (float)(i + 1) / (float)stacks * (float)M_PI;

        for (j = 0; j < sectors; ++j) {
            float p1 = (float)j / (float)sectors * (float)(2.0 * M_PI);
            float p2 = (float)(j + 1) / (float)sectors *
                       (float)(2.0 * M_PI);

            Vec3 v1 = vec3_make(r * sinf(t1) * cosf(p1),
                                r * cosf(t1),
                                r * sinf(t1) * sinf(p1));
            Vec3 v2 = vec3_make(r * sinf(t1) * cosf(p2),
                                r * cosf(t1),
                                r * sinf(t1) * sinf(p2));
            Vec3 v3 = vec3_make(r * sinf(t2) * cosf(p1),
                                r * cosf(t2),
                                r * sinf(t2) * sinf(p1));
            Vec3 v4 = vec3_make(r * sinf(t2) * cosf(p2),
                                r * cosf(t2),
                                r * sinf(t2) * sinf(p2));

            Vec3 tri[6] = {v1, v2, v3, v2, v4, v3};
            int k;

            for (k = 0; k < 6; ++k) {
                vertices[vertex_count++] = tri[k].x;
                vertices[vertex_count++] = tri[k].y;
                vertices[vertex_count++] = tri[k].z;
            }
        }
    }

    eng->sphereVertexCount = (int)(vertex_count / 3u);

    engine_create_vbo_vao(
        &eng->sphereVAO,
        &eng->sphereVBO,
        vertices,
        vertex_count
    );

    free(vertices);

    {
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        check_shader(vertexShader, "Vertex");

        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        check_shader(fragmentShader, "Fragment");

        eng->shaderProgram = glCreateProgram();
        glAttachShader(eng->shaderProgram, vertexShader);
        glAttachShader(eng->shaderProgram, fragmentShader);
        glLinkProgram(eng->shaderProgram);
        check_program(eng->shaderProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    eng->modelLoc = glGetUniformLocation(eng->shaderProgram, "model");
    eng->viewLoc = glGetUniformLocation(eng->shaderProgram, "view");
    eng->projLoc = glGetUniformLocation(eng->shaderProgram, "projection");
    eng->colorLoc = glGetUniformLocation(eng->shaderProgram, "objectColor");
}

static void engine_draw_spheres(Engine *eng, ParticleArray *arr) {
    Mat4 projection;
    Mat4 view;
    Vec3 camPos;
    size_t i;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->shaderProgram);

    projection = mat4_perspective(
        (float)(45.0 * M_PI / 180.0),
        800.0f / 600.0f,
        0.1f,
        2000.0f
    );

    camPos = camera_position(&camera);
    view = mat4_look_at(camPos, camera.target, vec3_make(0.0f, 1.0f, 0.0f));

    glUniformMatrix4fv(eng->viewLoc, 1, GL_FALSE, view.m);
    glUniformMatrix4fv(eng->projLoc, 1, GL_FALSE, projection.m);

    glBindVertexArray(eng->sphereVAO);

    for (i = 0; i < arr->size; ++i) {
        Particle *p = &arr->data[i];
        Mat4 model;

        if (p->pos.x < 0.0f && p->pos.y > 0.0f) {
            continue;
        }

        /*
         * The original GLM code did:
         *   model = translate(identity, p.pos);
         *   model = scale(model, vec3(electron_r));
         * which corresponds to T * S.
         */
        model = mat4_multiply(
            mat4_translate(p->pos),
            mat4_scale_uniform(electron_r)
        );

        glUniformMatrix4fv(eng->modelLoc, 1, GL_FALSE, model.m);
        glUniform4f(eng->colorLoc,
                    p->color.r, p->color.g,
                    p->color.b, p->color.a);

        glDrawArrays(GL_TRIANGLES, 0, eng->sphereVertexCount);
    }

    glBindVertexArray(0);
}

/* ================= Callbacks ================= */

static void mouse_button_callback(GLFWwindow *win, int button,
                                  int action, int mods) {
    (void)mods;
    camera_process_mouse_button(&camera, button, action, mods, win);
}

static void cursor_pos_callback(GLFWwindow *win, double x, double y) {
    (void)win;
    camera_process_mouse_move(&camera, x, y);
}

static void scroll_callback(GLFWwindow *win, double xoffset, double yoffset) {
    (void)win;
    camera_process_scroll(&camera, xoffset, yoffset);
}

static void generateParticles(int count);

static void key_callback(GLFWwindow *win, int key, int scancode,
                         int action, int mods) {
    (void)win;
    (void)scancode;
    (void)mods;

    if (!(action == GLFW_PRESS || action == GLFW_REPEAT)) {
        return;
    }

    if (key == GLFW_KEY_W) {
        n += 1;
        generateParticles(N);
    } else if (key == GLFW_KEY_S) {
        n -= 1;
        if (n < 1) n = 1;
        generateParticles(N);
    } else if (key == GLFW_KEY_E) {
        l += 1;
        generateParticles(N);
    } else if (key == GLFW_KEY_D) {
        l -= 1;
        if (l < 0) l = 0;
        generateParticles(N);
    } else if (key == GLFW_KEY_R) {
        m += 1;
        generateParticles(N);
    } else if (key == GLFW_KEY_F) {
        m -= 1;
        generateParticles(N);
    } else if (key == GLFW_KEY_T) {
        N += 100000;
        generateParticles(N);
    } else if (key == GLFW_KEY_G) {
        N -= 100000;
        if (N < 0) N = 0;
        generateParticles(N);
    }

    /* Clamp to valid ranges. */
    if (l > n - 1) l = n - 1;
    if (l < 0) l = 0;
    if (m > l) m = l;
    if (m < -l) m = -l;

    electron_r = (float)n / 3.0f;

    printf("Quantum numbers updated: n=%d l=%d m=%d N=%d\n",
           n, l, m, N);
}

static void engine_setup_camera_callbacks(Engine *eng) {
    (void)eng;
    glfwSetMouseButtonCallback(engine.window, mouse_button_callback);
    glfwSetCursorPosCallback(engine.window, cursor_pos_callback);
    glfwSetScrollCallback(engine.window, scroll_callback);
    glfwSetKeyCallback(engine.window, key_callback);
}

/* ================= Particle generation ================= */

static void generateParticles(int count) {
    int i;

    particle_array_clear(&particles);

    if (count > 0) {
        if (!particle_array_reserve(&particles, (size_t)count)) {
            fprintf(stderr, "Failed to reserve particle memory.\n");
            exit(EXIT_FAILURE);
        }
    }

    for (i = 0; i < count; ++i) {
        Vec3 pos;
        Particle p;
        float r;
        double theta;
        double phi;
        Vec4 col;

        pos = sphericalToCartesian(
            (float)sampleR(n, l),
            (float)sampleTheta(l, m),
            samplePhi()
        );

        r = vec3_length(pos);

        if (r > 1e-8f) {
            theta = acos((double)pos.y / (double)r);
            phi = atan2((double)pos.z, (double)pos.x);
        } else {
            theta = 0.0;
            phi = 0.0;
        }

        col = inferno(r, theta, phi, n, l, m);

        p.pos = pos;
        p.vel = vec3_zero();
        p.color = col;

        if (!particle_array_push(&particles, p)) {
            fprintf(stderr, "Failed to append particle.\n");
            exit(EXIT_FAILURE);
        }
    }
}

/* ================= Grid ================= */

typedef struct {
    GLuint gridVAO;
    GLuint gridVBO;
    float *vertices;
    size_t vertexFloatCount;
} Grid;

static float *create_grid_vertices(float size, int divisions,
                                    size_t *out_float_count) {
    size_t capacity;
    size_t count = 0;
    float *vertices;
    float step = size / (float)divisions;
    float halfSize = size / 2.0f;
    float extra = step * 3.0f;
    int midZ = divisions / 2;
    int xStep, zStep;

    /*
     * Each line segment adds 6 floats.
     * x-axis: (divisions+1) * divisions segments
     * z-axis: (divisions+1) * divisions segments
     */
    capacity = (size_t)(2 * (divisions + 1) * divisions * 6);
    vertices = (float *)malloc(capacity * sizeof(float));

    if (vertices == NULL) {
        fprintf(stderr, "Failed to allocate grid vertices.\n");
        exit(EXIT_FAILURE);
    }

    /* x axis */
    for (zStep = 0; zStep <= divisions; ++zStep) {
        float y = 0.0f;
        float z = -halfSize + zStep * step;

        for (xStep = 0; xStep < divisions; ++xStep) {
            float xStart = -halfSize + xStep * step;
            float xEnd = xStart + step;

            if (zStep == midZ) {
                if (xStep == 0) {
                    xStart -= extra;
                }
                if (xStep == divisions - 1) {
                    xEnd += extra;
                }
            }

            vertices[count++] = xStart;
            vertices[count++] = y;
            vertices[count++] = z;

            vertices[count++] = xEnd;
            vertices[count++] = y;
            vertices[count++] = z;
        }
    }

    /* z axis */
    for (xStep = 0; xStep <= divisions; ++xStep) {
        float x = -halfSize + xStep * step;

        for (zStep = 0; zStep < divisions; ++zStep) {
            float y = 0.0f;
            float zStart = -halfSize + zStep * step;
            float zEnd = zStart + step;

            vertices[count++] = x;
            vertices[count++] = y;
            vertices[count++] = zStart;

            vertices[count++] = x;
            vertices[count++] = y;
            vertices[count++] = zEnd;
        }
    }

    *out_float_count = count;
    return vertices;
}

static void grid_init(Grid *g) {
    g->vertices = create_grid_vertices(500.0f, 2, &g->vertexFloatCount);

    engine_create_vbo_vao(
        &g->gridVAO,
        &g->gridVBO,
        g->vertices,
        g->vertexFloatCount
    );
}

static void grid_draw(const Grid *g, GLint objectColorLoc) {
    Mat4 model = mat4_identity();

    glUseProgram(engine.shaderProgram);
    glUniform4f(objectColorLoc, 1.0f, 1.0f, 1.0f, 0.5f);

    /*
     * The original code refreshed the VBO each draw using GL_DYNAMIC_DRAW.
     * Keep that behavior here for a faithful conversion.
     */
    glBindBuffer(GL_ARRAY_BUFFER, g->gridVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 g->vertexFloatCount * sizeof(float),
                 g->vertices,
                 GL_DYNAMIC_DRAW);

    glUniformMatrix4fv(engine.modelLoc, 1, GL_FALSE, model.m);

    glBindVertexArray(g->gridVAO);
    glPointSize(2.0f);
    glDrawArrays(GL_LINES, 0, (GLsizei)(g->vertexFloatCount / 3u));
    glBindVertexArray(0);
}

/* ================= Main Loop ================= */

int main(void) {
    GLint objectColorLoc;
    Grid grid;
    float dt = 0.5f;

    srand((unsigned int)time(NULL));
    particle_array_init(&particles);

    engine_init(&engine);

    objectColorLoc =
        glGetUniformLocation(engine.shaderProgram, "objectColor");

    glUseProgram(engine.shaderProgram);
    engine_setup_camera_callbacks(&engine);

    electron_r = (float)n / 3.0f;

    /* Sample particles */
    generateParticles(250000);

    grid_init(&grid);

    printf("Starting simulation...\n");

    while (!glfwWindowShouldClose(engine.window)) {
        size_t i;

        grid_draw(&grid, objectColorLoc);

        /* ------ Update Probability current ------ */
        for (i = 0; i < particles.size; ++i) {
            Particle *p = &particles.data[i];
            double r = (double)vec3_length(p->pos);

            if (r > 1e-6) {
                double theta = acos((double)p->pos.y / r);

                p->vel = calculateProbabilityFlow(p, n, l, m);

                {
                    Vec3 temp_pos =
                        vec3_add(p->pos, vec3_scale(p->vel, dt));
                    double new_phi =
                        atan2((double)temp_pos.z, (double)temp_pos.x);

                    p->pos = sphericalToCartesian(
                        (float)r,
                        (float)theta,
                        (float)new_phi
                    );
                }
            }
        }

        /* ------ Draw Particles ------ */
        engine_draw_spheres(&engine, &particles);

        glfwSwapBuffers(engine.window);
        glfwPollEvents();
    }

    free(grid.vertices);

    glDeleteVertexArrays(1, &grid.gridVAO);
    glDeleteBuffers(1, &grid.gridVBO);

    glDeleteVertexArrays(1, &engine.sphereVAO);
    glDeleteBuffers(1, &engine.sphereVBO);
    glDeleteProgram(engine.shaderProgram);

    particle_array_free(&particles);

    glfwDestroyWindow(engine.window);
    glfwTerminate();

    return 0;
}
