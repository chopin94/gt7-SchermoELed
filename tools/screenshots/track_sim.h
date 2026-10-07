// A small driving simulation for the screenshots: a closed circuit built from
// control points (Catmull-Rom spline), a speed profile limited by grip, braking
// and engine, and a driver that laps it at 60 Hz like GT7 sends its packets.
#pragma once
#include <math.h>
#include <vector>

struct TrackSim
{
    struct Node { float x, z, hx, hz, curvature; };
    static constexpr float STEP = 2.0f; // metres between nodes
    std::vector<Node> nodes;
    std::vector<float> speed; // speed profile in m/s
    float length = 0;

    void build(const std::vector<std::pair<float, float>> &control)
    {
        // Dense Catmull-Rom polyline through the control points.
        std::vector<std::pair<float, float>> dense;
        const int n = static_cast<int>(control.size());
        for (int i = 0; i < n; ++i)
        {
            const auto &p0 = control[(i - 1 + n) % n], &p1 = control[i];
            const auto &p2 = control[(i + 1) % n], &p3 = control[(i + 2) % n];
            for (int k = 0; k < 80; ++k)
            {
                const float t = k / 80.0f, t2 = t * t, t3 = t2 * t;
                const auto blend = [&](float a, float b, float c, float d) {
                    return 0.5f * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
                };
                dense.push_back({blend(p0.first, p1.first, p2.first, p3.first),
                                 blend(p0.second, p1.second, p2.second, p3.second)});
            }
        }
        // Resample every STEP metres along the closed polyline.
        std::vector<float> cumulative(dense.size() + 1, 0.0f);
        for (size_t i = 0; i < dense.size(); ++i)
        {
            const auto &a = dense[i], &b = dense[(i + 1) % dense.size()];
            cumulative[i + 1] = cumulative[i] + hypotf(b.first - a.first, b.second - a.second);
        }
        length = cumulative.back();
        const int count = static_cast<int>(length / STEP);
        length = count * STEP;
        nodes.resize(count);
        size_t segment = 0;
        for (int k = 0; k < count; ++k)
        {
            const float target = k * STEP * cumulative.back() / length;
            while (cumulative[segment + 1] < target) segment++;
            const auto &a = dense[segment], &b = dense[(segment + 1) % dense.size()];
            const float f = (target - cumulative[segment]) / (cumulative[segment + 1] - cumulative[segment]);
            nodes[k].x = a.first + (b.first - a.first) * f;
            nodes[k].z = a.second + (b.second - a.second) * f;
        }
        for (int k = 0; k < count; ++k)
        {
            const Node &prev = nodes[(k - 1 + count) % count], &next = nodes[(k + 1) % count];
            const float dx = next.x - prev.x, dz = next.z - prev.z, d = hypotf(dx, dz);
            nodes[k].hx = dx / d;
            nodes[k].hz = dz / d;
        }
        for (int k = 0; k < count; ++k)
        {
            // Same scale as the path the car follows, so the speed profile
            // respects the grip in the tightest point of each corner.
            const Node &prev = nodes[(k - 2 + count) % count], &next = nodes[(k + 2) % count];
            float turn = atan2f(next.hz, next.hx) - atan2f(prev.hz, prev.hx);
            while (turn > 3.14159265f) turn -= 6.2831853f;
            while (turn < -3.14159265f) turn += 6.2831853f;
            nodes[k].curvature = turn / (4 * STEP);
        }
    }

    // Grip-limited corners, braking and engine-limited acceleration.
    void profile(float vmax, float lateral, float braking, float traction)
    {
        const int count = static_cast<int>(nodes.size());
        speed.assign(count, vmax);
        for (int k = 0; k < count; ++k)
        {
            const float c = fabsf(nodes[k].curvature);
            if (c > 1e-5f) speed[k] = fminf(vmax, sqrtf(lateral / c));
        }
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int k = 1; k <= count; ++k)
            {
                const float v = speed[(k - 1) % count];
                const float accel = traction * (1.0f - v / (vmax * 1.08f));
                const float limit = sqrtf(v * v + 2 * fmaxf(accel, 0.3f) * STEP);
                if (speed[k % count] > limit) speed[k % count] = limit;
            }
            for (int k = count - 1; k >= -1; --k)
            {
                const int i = (k + count) % count;
                const float v = speed[(i + 1) % count];
                const float limit = sqrtf(v * v + 2 * braking * STEP);
                if (speed[i] > limit) speed[i] = limit;
            }
        }
    }

    // Position and heading interpolated between the nodes, so the velocity
    // turns smoothly like the one sent by the game.
    Node at(float s) const
    {
        float u = fmodf(s, length) / STEP;
        if (u < 0) u += nodes.size();
        const int i = static_cast<int>(floorf(u)) % static_cast<int>(nodes.size());
        const Node &a = nodes[i], &b = nodes[(i + 1) % nodes.size()];
        const float f = u - floorf(u);
        Node out;
        out.x = a.x + (b.x - a.x) * f;
        out.z = a.z + (b.z - a.z) * f;
        const float hx = a.hx + (b.hx - a.hx) * f, hz = a.hz + (b.hz - a.hz) * f;
        const float h = hypotf(hx, hz);
        out.hx = hx / h;
        out.hz = hz / h;
        out.curvature = a.curvature + (b.curvature - a.curvature) * f;
        return out;
    }
    float speedAt(float s) const
    {
        const float u = fmodf(s, length) / STEP;
        const int i = static_cast<int>(floorf(u)) % static_cast<int>(speed.size());
        const float f = u - floorf(u);
        return speed[i] + (speed[(i + 1) % speed.size()] - speed[i]) * f;
    }
};
