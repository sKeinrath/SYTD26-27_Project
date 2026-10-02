__kernel void update_probability_flow(
    __global float4* positions,
    const int quantumM,
    const float dt,
    const int particleCount)
{
    int id = get_global_id(0);

    if (id >= particleCount)
        return;

    float3 p = positions[id].xyz;

    float r = length(p);

    if (r < 0.000001f)
        return;

    float theta = acos(clamp(p.y / r, -1.0f, 1.0f));
    float phi = atan2(p.z, p.x);

    float sinTheta = sin(theta);

    if (fabs(sinTheta) < 0.0001f)
        sinTheta = 0.0001f;

    float vMag =
        ((float)quantumM) /
        (r * sinTheta);

    float vx = -vMag * sin(phi);
    float vz =  vMag * cos(phi);

    float tempX = p.x + vx * dt;
    float tempZ = p.z + vz * dt;

    float newPhi = atan2(tempZ, tempX);

    float sinTheta2 = sin(theta);
    float cosTheta2 = cos(theta);

    float x =
        r * sinTheta2 * cos(newPhi);

    float y =
        r * cosTheta2;

    float z =
        r * sinTheta2 * sin(newPhi);

    positions[id] =
        (float4)(x, y, z, 1.0f);
}