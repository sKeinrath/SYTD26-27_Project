#include <CL/cl.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define PARTICLE_COUNT 24000
#define KERNEL_SOURCE \
"float hash(float n) { return fract(sin(n) * 43758.5453f); }\n" \
"__kernel void make_particles(__global float4 *particles, uint count, float time) {\n" \
"    uint id = get_global_id(0);\n" \
"    if (id >= count) return;\n" \
"    float seed = (float)id * 12.9898f;\n" \
"    float angle = 6.2831853f * hash(seed + 1.0f) + time * (0.15f + hash(seed + 2.0f) * 0.35f);\n" \
"    float radius = 30.0f + pow(hash(seed + 3.0f), 0.55f) * 310.0f;\n" \
"    float wave = 0.5f + 0.5f * sin(angle * 3.0f + radius * 0.075f - time * 1.8f);\n" \
"    radius *= 0.72f + wave * 0.42f;\n" \
"    float x = cos(angle) * radius;\n" \
"    float y = sin(angle) * radius * (0.62f + 0.2f * sin(time * 0.7f));\n" \
"    float brightness = 0.35f + 0.65f * wave;\n" \
"    particles[id] = (float4)(x, y, brightness, 1.0f);\n" \
"}\n"

// ================= Engine ================= //

typedef struct {
    GLFWwindow* window;
    int WIDTH;
    int HEIGHT;
} Engine;

Engine engine = { NULL, 800, 600 };

static cl_context cl_context_handle;
static cl_command_queue cl_queue;
static cl_kernel cl_particle_kernel;
static cl_mem cl_particles_buffer;
static float *particles = NULL;

static void check_opencl(cl_int error, const char *operation)
{
    if (error != CL_SUCCESS) {
        fprintf(stderr, "OpenCL error in %s: %d\n", operation, error);
        exit(EXIT_FAILURE);
    }
}

static void opencl_init(void)
{
    cl_platform_id platform = NULL;
    cl_device_id device = NULL;
    cl_int error;
    cl_uint platform_count = 0;

    check_opencl(clGetPlatformIDs(1, &platform, &platform_count), "clGetPlatformIDs");
    if (platform_count == 0) {
        fprintf(stderr, "No OpenCL platform found.\n");
        exit(EXIT_FAILURE);
    }

    error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (error != CL_SUCCESS)
        check_opencl(clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL), "clGetDeviceIDs");

    cl_context_handle = clCreateContext(NULL, 1, &device, NULL, NULL, &error);
    check_opencl(error, "clCreateContext");

    cl_queue = clCreateCommandQueue(cl_context_handle, device, 0, &error);
    check_opencl(error, "clCreateCommandQueue");

    const char *source = KERNEL_SOURCE;
    size_t source_length = sizeof(KERNEL_SOURCE) - 1;
    cl_program program = clCreateProgramWithSource(
        cl_context_handle, 1, &source, &source_length, &error);
    check_opencl(error, "clCreateProgramWithSource");

    error = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (error != CL_SUCCESS) {
        char build_log[4096] = { 0 };
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                              sizeof(build_log) - 1, build_log, NULL);
        fprintf(stderr, "OpenCL kernel build failed:\n%s\n", build_log);
        exit(EXIT_FAILURE);
    }

    cl_particle_kernel = clCreateKernel(program, "make_particles", &error);
    check_opencl(error, "clCreateKernel");
    cl_particles_buffer = clCreateBuffer(
        cl_context_handle, CL_MEM_WRITE_ONLY,
        PARTICLE_COUNT * 4 * sizeof(float), NULL, &error);
    check_opencl(error, "clCreateBuffer");
    particles = malloc(PARTICLE_COUNT * 4 * sizeof(float));
    if (!particles) {
        fprintf(stderr, "Could not allocate particle memory.\n");
        exit(EXIT_FAILURE);
    }
    clReleaseProgram(program);
}

static void opencl_update(float time)
{
    cl_int error;
    size_t global_size = PARTICLE_COUNT;
    error = clSetKernelArg(cl_particle_kernel, 0, sizeof(cl_mem), &cl_particles_buffer);
    check_opencl(error, "clSetKernelArg buffer");
    check_opencl(clSetKernelArg(cl_particle_kernel, 1, sizeof(cl_uint), &(cl_uint){ PARTICLE_COUNT }), "clSetKernelArg count");
    check_opencl(clSetKernelArg(cl_particle_kernel, 2, sizeof(float), &time), "clSetKernelArg time");
    check_opencl(clEnqueueNDRangeKernel(cl_queue, cl_particle_kernel, 1, NULL,
                                        &global_size, NULL, 0, NULL, NULL), "clEnqueueNDRangeKernel");
    check_opencl(clEnqueueReadBuffer(cl_queue, cl_particles_buffer, CL_TRUE, 0,
                                     PARTICLE_COUNT * 4 * sizeof(float), particles,
                                     0, NULL, NULL), "clEnqueueReadBuffer");
}

static void opencl_shutdown(void)
{
    free(particles);
    clReleaseMemObject(cl_particles_buffer);
    clReleaseKernel(cl_particle_kernel);
    clReleaseCommandQueue(cl_queue);
    clReleaseContext(cl_context_handle);
}

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

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "failed to init GLEW\n");
        exit(EXIT_FAILURE);
    }

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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        float brightness = particles[i * 4 + 2];
        glColor4f(0.15f + brightness * 0.75f,
                  0.35f + brightness * 0.55f,
                  1.0f,
                  0.18f + brightness * 0.65f);
        glVertex2f(particles[i * 4], particles[i * 4 + 1]);
    }
    glEnd();
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
    opencl_init();

    while (!glfwWindowShouldClose(engine.window)) {

        opencl_update((float)glfwGetTime());
        engine_run();

        glfwSwapBuffers(engine.window);
        glfwPollEvents();
    }

    opencl_shutdown();
    glfwTerminate();

    return 0;
}