using Raylib_cs;
using System;
using System.Collections.Generic;
using System.IO;
using System.Numerics;
using System.Text.Json;
using static Raylib_cs.Raylib;

// ============================================================
// Constants
// ============================================================

const float C = 299792458.0f / 100000000.0f;
const float EU = 2.71828182845904523536f;
const float K = 8.9875517923e9f;
const float A0 = 52.9f;
const float ELECTRON_R = 5.0f;
const float FIELD_RES = 25.0f;

// ============================================================
// Camera
// ============================================================

CameraState camera = new CameraState();


List<Particle> LoadWavefunction(string src)
{
    List<Particle> particles = new List<Particle>();

    if (!File.Exists(src))
    {
        Console.WriteLine($"Datei nicht gefunden: {src}");
        return particles;
    }

    try
    {
        string json = File.ReadAllText(src);

        using JsonDocument document =
            JsonDocument.Parse(json);

        JsonElement root =
            document.RootElement;

        // mesh.json verwendet "vertices"
        if (!root.TryGetProperty(
                "vertices",
                out JsonElement vertices))
        {
            Console.WriteLine(
                "JSON enthält kein 'vertices'-Feld."
            );

            return particles;
        }

        foreach (JsonElement vertex in
                 vertices.EnumerateArray())
        {
            // Wir brauchen mindestens X, Y und Z
            if (vertex.GetArrayLength() < 3)
                continue;

            float x = vertex[0].GetSingle();
            float y = vertex[1].GetSingle();
            float z = vertex[2].GetSingle();

            Particle particle = new Particle(
                1.0f,
                new Color(255, 0, 255, 255),
                new Vector3(x, y, z)
            );

            particles.Add(particle);
        }

        Console.WriteLine(
            $"Loaded {particles.Count} particles from {src}"
        );
    }
    catch (Exception ex)
    {
        Console.WriteLine(
            $"Fehler beim Laden von {src}: {ex.Message}"
        );
    }

    return particles;
}


const int WIDTH = 800;
const int HEIGHT = 600;

InitWindow(
    WIDTH,
    HEIGHT,
    "Quantum Simulation by kavan G"
);

SetTargetFPS(60);

SetExitKey(KeyboardKey.Null);

// ------------------------------------------------------------
// 3D camera
// ------------------------------------------------------------

Camera3D rayCamera = new Camera3D
{
    Position = camera.Position(),
    Target = camera.Target,
    Up = new Vector3(0, 1, 0),
    FovY = 45.0f,
    Projection = CameraProjection.Perspective
};

// ============================================================
// Load objects
// ============================================================

Grid grid = new Grid();

List<Particle> particles =
    LoadWavefunction(
        "../../../src/mesh.json"
    );


Console.WriteLine(
    $"Loaded {particles.Count} particles."
);

// ============================================================
// Main loop
// ============================================================

while (!WindowShouldClose())
{
    // --------------------------------------------------------
    // Camera input
    // --------------------------------------------------------

    camera.ProcessInput();

    rayCamera.Position =
        camera.Position();

    rayCamera.Target =
        camera.Target;

    // --------------------------------------------------------
    // Rendering
    // --------------------------------------------------------

    BeginDrawing();

    ClearBackground(Color.Black);

    BeginMode3D(rayCamera);

    // --------------------------------------------------------
    // Grid
    // --------------------------------------------------------

    grid.Draw();

    // --------------------------------------------------------
    // Particles
    // --------------------------------------------------------

    int count = Math.Min(particles.Count, 1000);

    for (int i = 0; i < count; i++)
    {
        particles[i].Draw();
    }

    EndMode3D();

    // --------------------------------------------------------
    // Optional information
    // --------------------------------------------------------

    DrawText(
        $"Particles: {particles.Count}",
        10,
        10,
        20,
        Color.White
    );

    DrawText(
        "Left Mouse = Orbit | Wheel = Zoom",
        10,
        35,
        18,
        new Color(180, 180, 180, 255)
    );

    EndDrawing();
}

// ============================================================
// Cleanup
// ============================================================

CloseWindow();

class CameraState
{
    public Vector3 Target = new Vector3(0, 0, 0);

    public float Radius = 500.0f;

    public float Azimuth = 0.0f;
    public float Elevation = MathF.PI / 2.0f;

    public float OrbitSpeed = 0.01f;
    public float ZoomSpeed = 125.0f;

    public bool Dragging = false;
    public bool Moving = false;

    public float LastX = 0.0f;
    public float LastY = 0.0f;

    public Vector3 Position()
    {
        float clampedElevation =
            Math.Clamp(Elevation, 0.01f, MathF.PI - 0.01f);

        return new Vector3(
            Radius * MathF.Sin(clampedElevation) * MathF.Cos(Azimuth),
            Radius * MathF.Cos(clampedElevation),
            Radius * MathF.Sin(clampedElevation) * MathF.Sin(Azimuth)
        );
    }

    public void Update()
    {
        Target = new Vector3(0, 0, 0);
        Moving = Dragging;
    }

    public void ProcessInput()
    {
        // ----------------------------------------------------
        // Left mouse = orbit camera
        // ----------------------------------------------------

        if (IsMouseButtonPressed(MouseButton.Left))
        {
            Dragging = true;

            Vector2 mouse = GetMousePosition();

            LastX = mouse.X;
            LastY = mouse.Y;
        }

        if (IsMouseButtonReleased(MouseButton.Left))
        {
            Dragging = false;
        }

        // ----------------------------------------------------
        // Camera rotation
        // ----------------------------------------------------

        if (Dragging)
        {
            Vector2 mouse = GetMousePosition();

            float dx = mouse.X - LastX;
            float dy = mouse.Y - LastY;

            Azimuth += dx * OrbitSpeed;
            Elevation -= dy * OrbitSpeed;

            Elevation = Math.Clamp(
                Elevation,
                0.01f,
                MathF.PI - 0.01f
            );

            LastX = mouse.X;
            LastY = mouse.Y;
        }

        // ----------------------------------------------------
        // Mouse wheel = zoom
        // ----------------------------------------------------

        float wheel = GetMouseWheelMove();

        if (MathF.Abs(wheel) > 0.001f)
        {
            Radius -= wheel * ZoomSpeed;

            // Prevent camera from going inside the center
            Radius = Math.Clamp(Radius, 20.0f, 5000.0f);
        }

        Update();
    }
}

// ============================================================
// Particle
// ============================================================

class Particle
{
    public float Radius;
    public Color Color;
    public Vector3 Position;

    public Particle(float radius, Color color, Vector3 position)
    {
        Radius = radius;
        Color = color;
        Position = position;
    }

    public void Draw()
    {
        DrawSphere(
            Position,
            Radius,
            Color
        );
    }
}

// ============================================================
// JSON loading
// ============================================================

class WavefunctionData
{
    public List<List<float>>? points { get; set; }
}

// ============================================================
// Grid
// ============================================================

class Grid
{
    public List<(Vector3 start, Vector3 end)> Lines =
        new List<(Vector3, Vector3)>();

    public Grid()
    {
        Lines = CreateGridVertices(500.0f, 2);
    }

    private List<(Vector3 start, Vector3 end)> CreateGridVertices(
        float size,
        int divisions)
    {
        List<(Vector3, Vector3)> lines =
            new List<(Vector3, Vector3)>();

        float step = size / divisions;
        float halfSize = size / 2.0f;

        float extra = step * 3.0f;

        int midZ = divisions / 2;

        // ----------------------------------------------------
        // X axis lines
        // ----------------------------------------------------

        for (int yStep = 3; yStep <= 3; yStep++)
        {
            float y = 0;

            for (int zStep = 0; zStep <= divisions; zStep++)
            {
                float z =
                    -halfSize + zStep * step;

                for (int xStep = 0;
                     xStep < divisions;
                     xStep++)
                {
                    float xStart =
                        -halfSize + xStep * step;

                    float xEnd =
                        xStart + step;

                    // Extend central X axis
                    if (zStep == midZ)
                    {
                        if (xStep == 0)
                            xStart -= extra;

                        if (xStep == divisions - 1)
                            xEnd += extra;
                    }

                    lines.Add(
                        (
                            new Vector3(xStart, y, z),
                            new Vector3(xEnd, y, z)
                        )
                    );
                }
            }
        }

        // ----------------------------------------------------
        // Z axis lines
        // ----------------------------------------------------

        for (int xStep = 0;
             xStep <= divisions;
             xStep++)
        {
            float x =
                -halfSize + xStep * step;

            for (int yStep = 3;
                 yStep <= 3;
                 yStep++)
            {
                float y = 0;

                for (int zStep = 0;
                     zStep < divisions;
                     zStep++)
                {
                    float zStart =
                        -halfSize + zStep * step;

                    float zEnd =
                        zStart + step;

                    lines.Add(
                        (
                            new Vector3(x, y, zStart),
                            new Vector3(x, y, zEnd)
                        )
                    );
                }
            }
        }

        return lines;
    }

    public void Draw()
    {
        // Original:
        // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
        //
        // Raylib uses its own 3D drawing pipeline.
        // We simulate the very transparent grid with alpha.

        Color gridColor = new Color(
            255,
            255,
            255,
            20
        );

        foreach (var line in Lines)
        {
            DrawLine3D(
                line.start,
                line.end,
                gridColor
            );
        }
    }
}

// ============================================================
// Initialize window
// ============================================================