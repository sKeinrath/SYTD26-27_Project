#include <CL/cl.h> #include <GL/glew.h> #include <GLFW/glfw3.h> #include
<math.h> #include <stdio.h> #include <stdlib.h> #include <string.h>
#include <time.h>

#define WIDTH 800 #define HEIGHT 600 #define MAX_SPHERES 100000 #define
M_PI_F 3.14159265358979323846f

typedef struct { float x,y,z; } Vec3; typedef struct { float x,y,z,w; }
Vec4;

typedef struct { Vec4 center_radius; Vec4 color; } Sphere;

typedef struct { Vec3 pos; Vec3 vel; Vec4 color; } Particle;

static Particle *particles = NULL; static int N = 100000; static int n =
3; static int l = 1; static int m = 1;

static const float a0 = 1.0f; static const float electron_r = 0.25f;
static const float hbar = 1.0f; static const float m_e = 1.0f; static
const float LightingScaler = 700.0f;

static float random_float(void) { return (float)rand() /
(float)RAND_MAX; }

static Vec3 make_vec3(float x,float y,float z) { Vec3 v = {x,y,z};
return v; }

static Vec4 make_vec4(float x,float y,float z,float w) { Vec4 v =
{x,y,z,w}; return v; }

static float length3(Vec3 p) { return sqrtf(p.xp.x+p.yp.y+p.z*p.z); }

static Vec3 sphericalToCartesian(float r,float theta,float phi) { return
make_vec3( rsinf(theta)cosf(phi), rcosf(theta), rsinf(theta)*sinf(phi)
); }

/* Associated Laguerre polynomial */ static double laguerre(int k,int
alpha,double rho) { if(k==0) return 1.0;

    double L0 = 1.0;
    double L1 = 1.0 + alpha - rho;

    if(k==1) return L1;

    for(int j=2;j<=k;j++)
    {
        double L =
            ((2.0*j-1.0+alpha-rho)*L1
            -(j-1.0+alpha)*L0)/j;

        L0=L1;
        L1=L;
    }

    return L1;

}

/* Associated Legendre polynomial */ static double legendre(int l,int
m,double x) { double Pmm=1.0;

    if(m>0)
    {
        double somx2=sqrt(fmax(0.0,(1.0-x)*(1.0+x)));
        double fact=1.0;

        for(int j=1;j<=m;j++)
        {
            Pmm *= -fact*somx2;
            fact += 2.0;
        }
    }

    if(l==m)
        return Pmm;

    double Pm1m=x*(2.0*m+1.0)*Pmm;

    if(l==m+1)
        return Pm1m;

    for(int ll=m+2;ll<=l;ll++)
    {
        double Pll=
            ((2.0*ll-1.0)*x*Pm1m
            -(ll+m-1.0)*Pmm)/(ll-m);

        Pmm=Pm1m;
        Pm1m=Pll;
    }

    return Pm1m;

}

static double radial_pdf(double r,int n,int l) { double rho=2.0r/(na0);

    int k=n-l-1;
    int alpha=2*l+1;

    double L=laguerre(k,alpha,rho);

    double norm=
        pow(2.0/(n*a0),3.0)
        *tgamma(n-l)
        /(2.0*n*tgamma(n+l+1));

    double R=
        sqrt(norm)
        *exp(-rho/2.0)
        *pow(rho,l)
        *L;

    return r*r*R*R;

}

static double angular_pdf(double theta,int l,int m) { double
P=legendre(l,m,cos(theta)); return sin(theta)PP; }

static double sampleR(int n,int l) { const int SIZE=4096; const double
rMax=10.0nn*a0;

    double *cdf=malloc(SIZE*sizeof(double));
    double sum=0.0;

    for(int i=0;i<SIZE;i++)
    {
        double r=(double)i*rMax/(SIZE-1);

        sum+=radial_pdf(r,n,l);
        cdf[i]=sum;
    }

    for(int i=0;i<SIZE;i++)
        cdf[i]/=sum;

    double u=random_float();

    int low=0;
    int high=SIZE-1;

    while(low<high)
    {
        int mid=(low+high)/2;

        if(cdf[mid]<u)
            low=mid+1;
        else
            high=mid;
    }

    double r=(double)low*rMax/(SIZE-1);

    free(cdf);

    return r;

}

static double sampleTheta(int l,int m) { const int SIZE=2048;

    double *cdf=malloc(SIZE*sizeof(double));
    double sum=0.0;

    for(int i=0;i<SIZE;i++)
    {
        double theta=(double)i*M_PI/(SIZE-1);

        sum+=angular_pdf(theta,l,m);
        cdf[i]=sum;
    }

    for(int i=0;i<SIZE;i++)
        cdf[i]/=sum;

    double u=random_float();

    int low=0;
    int high=SIZE-1;

    while(low<high)
    {
        int mid=(low+high)/2;

        if(cdf[mid]<u)
            low=mid+1;
        else
            high=mid;
    }

    double theta=(double)low*M_PI/(SIZE-1);

    free(cdf);

    return theta;

}

static float samplePhi(void) { return 2.0fM_PI_Frandom_float(); }

static Vec3 calculateProbabilityFlow(Vec3 p,int m) { double
r=length3(p);

    if(r<1e-6)
        return make_vec3(0,0,0);

    double theta=acos(fmax(-1.0,fmin(1.0,p.y/r)));
    double phi=atan2(p.z,p.x);

    double sinTheta=sin(theta);

    if(fabs(sinTheta)<1e-4)
        sinTheta=1e-4;

    double v_mag=hbar*m/(m_e*r*sinTheta);

    return make_vec3(
        (float)(-v_mag*sin(phi)),
        0.0f,
        (float)(v_mag*cos(phi))
    );

}

static Vec4 heatmap_fire(float value) { if(value<0) value=0; if(value>1)
value=1;

    Vec4 colors[6]={
        {0,0,0,1},
        {0.3f,0,0.6f,1},
        {0.8f,0,0,1},
        {1,0.5f,0,1},
        {1,1,0,1},
        {1,1,1,1}
    };

    float scaled=value*5.0f;
    int i=(int)scaled;

    if(i>4) i=4;

    float t=scaled-i;

    return make_vec4(
        colors[i].x+t*(colors[i+1].x-colors[i].x),
        colors[i].y+t*(colors[i+1].y-colors[i].y),
        colors[i].z+t*(colors[i+1].z-colors[i].z),
        1
    );

}

static Vec4 inferno(double r,double theta,int n,int l,int m) { double
rho=2.0r/(na0);

    int k=n-l-1;
    int alpha=2*l+1;

    double L=laguerre(k,alpha,rho);

    double norm=
        pow(2.0/(n*a0),3.0)
        *tgamma(n-l)
        /(2.0*n*tgamma(n+l+1));

    double R=
        sqrt(norm)
        *exp(-rho/2.0)
        *pow(rho,l)
        *L;

    double radial=R*R;

    double P=legendre(l,m,cos(theta));

    double angular=P*P;

    double intensity=radial*angular;

    return heatmap_fire((float)(intensity*LightingScaler));

}

static void generateParticles(void) { free(particles);

    particles=malloc((size_t)N*sizeof(Particle));

    for(int i=0;i<N;i++)
    {
        Vec3 pos=sphericalToCartesian(
            (float)sampleR(n,l),
            (float)sampleTheta(l,m),
            samplePhi()
        );

        float r=length3(pos);

        double theta=acos(pos.y/r);

        particles[i].pos=pos;
        particles[i].vel=make_vec3(0,0,0);
        particles[i].color=
            inferno(r,theta,n,l,m);
    }

}

/ OpenCL-Initialisierung */

static void checkCL(cl_int result,const char *text) {
if(result!=CL_SUCCESS) { fprintf(stderr,“OpenCL Fehler: %s
(%d)”,text,result); exit(EXIT_FAILURE); } }

static cl_context createOpenCLContext( cl_device_id device,
cl_command_queue queue) { cl_int err;

    cl_uint platformCount=0;

    checkCL(
        clGetPlatformIDs(0,NULL,&platformCount),
        "clGetPlatformIDs"
    );

    cl_platform_id *platforms=
        malloc(platformCount*sizeof(cl_platform_id));

    checkCL(
        clGetPlatformIDs(
            platformCount,
            platforms,
            NULL),
        "clGetPlatformIDs"
    );

    *device=NULL;

    for(cl_uint i=0;i<platformCount;i++)
    {
        err=clGetDeviceIDs(
            platforms[i],
            CL_DEVICE_TYPE_GPU,
            1,
            device,
            NULL);

        if(err==CL_SUCCESS)
            break;
    }

    if(*device==NULL)
    {
        for(cl_uint i=0;i<platformCount;i++)
        {
            err=clGetDeviceIDs(
                platforms[i],
                CL_DEVICE_TYPE_CPU,
                1,
                device,
                NULL);

            if(err==CL_SUCCESS)
                break;
        }
    }

    free(platforms);

    if(*device==NULL)
    {
        fprintf(stderr,"Kein OpenCL Device gefunden\n");
        exit(EXIT_FAILURE);
    }

    cl_context context=
        clCreateContext(
            NULL,
            1,
            device,
            NULL,
            NULL,
            &err);

    checkCL(err,"clCreateContext");

    *queue=
        clCreateCommandQueue(
            context,
            *device,
            0,
            &err);

    checkCL(err,"clCreateCommandQueue");

    return context;

}

/ Vereinfachtes GLFW/OpenGL-Hauptprogramm. * OpenCL übernimmt das
eigentliche Raytracing. */

int main(void) { srand((unsigned)time(NULL));

    if(!glfwInit())
        return 1;

    GLFWwindow *window=
        glfwCreateWindow(
            WIDTH,
            HEIGHT,
            "Quantum Simulation - C/OpenCL",
            NULL,
            NULL);

    if(!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    glewExperimental=GL_TRUE;

    if(glewInit()!=GLEW_OK)
    {
        glfwTerminate();
        return 1;
    }

    generateParticles();

    /*
     * OpenCL Kernel aus quantum_raytracer.cl laden.
     * Die Sphere-Daten werden an OpenCL übergeben.
     */

    while(!glfwWindowShouldClose(window))
    {
        for(int i=0;i<N;i++)
        {
            double r=length3(particles[i].pos);

            if(r>1e-6)
            {
                double theta=
                    acos(particles[i].pos.y/r);

                particles[i].vel=
                    calculateProbabilityFlow(
                        particles[i].pos,
                        m);

                Vec3 temp=
                    make_vec3(
                        particles[i].pos.x+
                        particles[i].vel.x*0.5f,
                        particles[i].pos.y+
                        particles[i].vel.y*0.5f,
                        particles[i].pos.z+
                        particles[i].vel.z*0.5f);

                double newPhi=
                    atan2(temp.z,temp.x);

                particles[i].pos=
                    sphericalToCartesian(
                        (float)r,
                        (float)theta,
                        (float)newPhi);
            }
        }

        /*
         * Hier werden die Sphere-Daten an den OpenCL-Kernel
         * geschickt und das gerenderte Bild zurückgegeben.
         */

        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    free(particles);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;

}

============================================================ DATEI 2:
quantum_raytracer.cl
============================================================

typedef struct { float4 center_radius; float4 color; } Sphere;

float intersect_sphere( float3 ray_origin, float3 ray_dir, float3
sphere_center, float sphere_radius) { float3 oc=
ray_origin-sphere_center;

    float a=dot(ray_dir,ray_dir);

    float b=
        2.0f*dot(oc,ray_dir);

    float c=
        dot(oc,oc)-
        sphere_radius*sphere_radius;

    float discriminant=
        b*b-4.0f*a*c;

    if(discriminant<0.0f)
        return -1.0f;

    float t=
        (-b-sqrt(discriminant))
        /(2.0f*a);

    if(t<=0.0f)
    {
        t=
            (-b+sqrt(discriminant))
            /(2.0f*a);
    }

    return t;

}

int any_hit( __global const Sphere *spheres, int sphere_count, float3
ray_origin, float3 ray_dir, float max_distance) { for(int
i=0;i<sphere_count;i++) { float t= intersect_sphere( ray_origin,
ray_dir, spheres[i].center_radius.xyz, spheres[i].center_radius.w);

        if(t>0.0f && t<max_distance)
            return 1;
    }

    return 0;

}

__kernel void raytrace( __global const Sphere *spheres, int
sphere_count, __global uchar4 *image,

    float3 camera_pos,
    float3 target,

    float aspect,
    float tan_half_fovy,

    float3 light_pos,
    float3 ambient_light,
    float light_intensity)

{ int x=get_global_id(0); int y=get_global_id(1);

    int width=get_global_size(0);
    int height=get_global_size(1);

    float3 forward=
        normalize(target-camera_pos);

    float3 world_up=
        (float3)(0.0f,1.0f,0.0f);

    float3 right=
        normalize(cross(forward,world_up));

    float3 up=
        normalize(cross(right,forward));

    float u=
        ((float)x+0.5f)/(float)width;

    float v=
        ((float)y+0.5f)/(float)height;

    float sx=
        (2.0f*u-1.0f)
        *aspect
        *tan_half_fovy;

    float sy=
        (1.0f-2.0f*v)
        *tan_half_fovy;

    float3 ray_dir=
        normalize(
            forward+
            sx*right+
            sy*up);

    float closest=3.402823466e+38F;

    int sphere_index=-1;

    for(int i=0;i<sphere_count;i++)
    {
        float t=
            intersect_sphere(
                camera_pos,
                ray_dir,
                spheres[i].center_radius.xyz,
                spheres[i].center_radius.w);

        if(t>0.0f && t<closest)
        {
            closest=t;
            sphere_index=i;
        }
    }

    uchar4 color=
        (uchar4)(0,0,0,0);

    if(sphere_index>=0)
    {
        float3 center=
            spheres[sphere_index]
            .center_radius.xyz;

        float3 hit=
            camera_pos+
            closest*ray_dir;

        float3 normal=
            normalize(hit-center);

        float3 sphere_color=
            spheres[sphere_index]
            .color.xyz;

        float3 to_light=
            light_pos-hit;

        float light_distance=
            length(to_light);

        float3 light_dir=
            to_light/light_distance;

        float shadow=1.0f;

        if(any_hit(
            spheres,
            sphere_count,
            hit+normal*0.001f,
            light_dir,
            light_distance))
        {
            shadow=0.0f;
        }

        float diffuse_factor=
            fmax(
                dot(normal,light_dir),
                0.0f);

        float3 diffuse=
            diffuse_factor*
            sphere_color*
            shadow*
            light_intensity;

        float3 ambient=
            ambient_light*
            sphere_color*
            light_intensity;

        float3 final_color=
            clamp(
                ambient+diffuse,
                0.0f,
                1.0f);

        color=
            (uchar4)(
                convert_uchar_sat_rte(
                    final_color.x*255.0f),

                convert_uchar_sat_rte(
                    final_color.y*255.0f),

                convert_uchar_sat_rte(
                    final_color.z*255.0f),

                convert_uchar_sat_rte(
                    spheres[sphere_index]
                    .color.w*255.0f)
            );
    }

    image[y*width+x]=color;

}
