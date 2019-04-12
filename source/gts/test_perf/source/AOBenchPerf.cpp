/*******************************************************************************
 * Copyright (c) 2010-2019, Intel Corporation
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of Intel Corporation nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/
/*
  Based on Syoyo Fujita's aobench: http://code.google.com/p/aobench
*/

#include <cmath>
#include <chrono>

#include "gts_perf/PerfTests.h"

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>
#include <gts/micro_scheduler/patterns/ParallelFor.h>
#include <gts/micro_scheduler/patterns/BlockedRange1d.h>
#include <gts/micro_scheduler/patterns/BlockedRange2d.h>


#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#pragma warning (disable: 4244)
#pragma warning (disable: 4305)
#endif

static long long drand48_x = 0x1234ABCD330E;

//------------------------------------------------------------------------------
static inline void srand48(int x)
{
    drand48_x = x ^ (x << 16);
}

//------------------------------------------------------------------------------
static inline double drand48()
{
    drand48_x = drand48_x * 0x5DEECE66D + 0xB;
    return (drand48_x & 0xFFFFFFFFFFFF) * (1.0 / 281474976710656.0);
}

//------------------------------------------------------------------------------
struct vec
{
    vec() { x=y=z=0.; }
    vec(float xx, float yy, float zz) { x = xx; y = yy; z = zz; }

    vec operator*(float f) const
    { 
        return vec(x*f, y*f, z*f);
    }
    vec operator+(const vec &f2) const
    { 
        return vec(x+f2.x, y+f2.y, z+f2.z); 
    }
    vec operator-(const vec &f2) const
    { 
        return vec(x-f2.x, y-f2.y, z-f2.z); 
    }
    vec operator*(const vec &f2) const
    { 
        return vec(x*f2.x, y*f2.y, z*f2.z); 
    }

    float x, y, z;
};


//------------------------------------------------------------------------------
inline vec operator*(float f, const vec &v)
{
    return vec(f*v.x, f*v.y, f*v.z);
}

//------------------------------------------------------------------------------
static inline float dot(const vec &a, const vec &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

//------------------------------------------------------------------------------
static inline vec vcross(const vec &v0, const vec &v1)
{
    vec ret;
    ret.x = v0.y * v1.z - v0.z * v1.y;
    ret.y = v0.z * v1.x - v0.x * v1.z;
    ret.z = v0.x * v1.y - v0.y * v1.x;
    return ret;
}

//------------------------------------------------------------------------------
static inline void vnormalize(vec &v)
{
    float len2 = dot(v, v);
    float invlen = 1.f / sqrtf(len2);
    v = v * invlen;
}


#define NAO_SAMPLES 8

#ifdef M_PI
#undef M_PI
#endif
#define M_PI 3.1415926535f

//------------------------------------------------------------------------------
struct Isect
{
    Isect() {}
    Isect(vec const& p, vec const& n) : p(p), n(n) {}
    vec     p;
    float   t;
    vec     n;
    int     hit;
};

//------------------------------------------------------------------------------
struct Sphere
{
    Sphere() {}
    Sphere(vec const& c, float r) : center(c), radius(r) {}
    vec     center;
    float   radius;
};

//------------------------------------------------------------------------------
struct Plane
{
    Plane() {}
    Plane(vec const& p, vec const& n) : p(p), n(n) {}
    vec     p;
    float   pad0;
    vec     n;
    float   pad1;
};

//------------------------------------------------------------------------------
struct Ray
{
    Ray() {}
    Ray(vec const& o, vec const& d) : org(o), dir(d) {}
    vec     org;
    float   pad0;
    vec     dir;
    float   pad1;
};

//------------------------------------------------------------------------------
static inline void ray_plane_intersect(Isect &isect, Ray &ray, Plane &plane)
{
    float d = -dot(plane.p, plane.n);
    float v = dot(ray.dir, plane.n);

    if (fabsf(v) < 1.0e-17f)
    {
        return;
    }
    else
    {
        float t = -(dot(ray.org, plane.n) + d) / v;

        if ((t > 0.0) && (t < isect.t))
        {
            isect.p = ray.org + ray.dir * t;
            isect.t = t;
            isect.n = plane.n;
            isect.hit = 1;
        }
    }
}

//------------------------------------------------------------------------------
static inline void ray_sphere_intersect(Isect &isect, Ray &ray, Sphere &sphere)
{
    vec rs = ray.org;
    vec rd = ray.dir;
    rs = rs - sphere.center;

    float B = dot(rs, rd);
    float SR = sphere.radius * sphere.radius;
    float C = dot(rs, rs) - SR;
    float D = B * B - C;

    if (D > 0.f) 
    {
        float t = -B - sqrtf(D);

        if ((t > 0.f) && (t < isect.t))
        {
            isect.t = t;
            isect.hit = 1;
            isect.p = ray.org + t * ray.dir;
            isect.n = isect.p - sphere.center;
            vnormalize(isect.n);
        }
    }
}

//------------------------------------------------------------------------------
static inline void orthoBasis(vec basis[3], const vec &n)
{
    basis[2] = n;
    basis[1].x = 0.0f; basis[1].y = 0.0f; basis[1].z = 0.0f;

    if ((n.x < 0.6f) && (n.x > -0.6f))
    {
        basis[1].x = 1.0;
    }
    else if ((n.y < 0.6f) && (n.y > -0.6f))
    {
        basis[1].y = 1.0f;
    }
    else if ((n.z < 0.6f) && (n.z > -0.6f))
    {
        basis[1].z = 1.0f;
    }
    else
    {
        basis[1].x = 1.0f;
    }

    basis[0] = vcross(basis[1], basis[2]);
    vnormalize(basis[0]);

    basis[1] = vcross(basis[2], basis[0]);
    vnormalize(basis[1]);
}

//------------------------------------------------------------------------------
static float ambient_occlusion(Isect &isect, Plane &plane, Sphere* spheres)
{
    float eps = 0.0001f;
    vec p, n;
    vec basis[3];
    float occlusion = 0.0f;

    p = isect.p + eps * isect.n;

    orthoBasis(basis, isect.n);

    constexpr int ntheta = NAO_SAMPLES;
    constexpr int nphi   = NAO_SAMPLES;
    Ray rays[NAO_SAMPLES * NAO_SAMPLES];

    for (int j = 0; j < ntheta; j++)
    {
        for (int i = 0; i < nphi; i++)
        {
            float theta = sqrtf(drand48());
            float phi = 2.0f * M_PI * drand48();
            float x = cosf(phi) * theta;
            float y = sinf(phi) * theta;
            float z = sqrtf(1.0f - theta * theta);

            // local . global
            float rx = x * basis[0].x + y * basis[1].x + z * basis[2].x;
            float ry = x * basis[0].y + y * basis[1].y + z * basis[2].y;
            float rz = x * basis[0].z + y * basis[1].z + z * basis[2].z;

            Ray ray;
            ray.org = p;
            ray.dir.x = rx;
            ray.dir.y = ry;
            ray.dir.z = rz;

            Isect occIsect;

            occIsect.t = 1.0e+17f;
            occIsect.hit = 0;

            for (int snum = 0; snum < 3; ++snum)
            {
                ray_sphere_intersect(occIsect, ray, spheres[snum]);
            }
            ray_plane_intersect(occIsect, ray, plane);

            if (occIsect.hit) occlusion += 1.f;
        }
    }

    occlusion = (ntheta * nphi - occlusion) / (float)(ntheta * nphi);
    return occlusion;
}

//------------------------------------------------------------------------------
/**
 * Compute the image for the scanlines from [y0,y1), for an overall image
 * of width w and height h.
 */
static void ao_scanlines(int x0, int x1, int y0, int y1, int w, int h, int nsubsamples, float image[], Plane &plane, Sphere* spheres)
{
    srand48(y0);
    float invSamples = 1.f / nsubsamples;
    
    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            int offset = 3 * (y * w + x);
            for (int u = 0; u < nsubsamples; ++u)
            {
                for (int v = 0; v < nsubsamples; ++v)
                {
                    // Figure out x,y pixel in NDC
                    float px = (x + (u / (float)nsubsamples) - (w / 2.0f)) / (w / 2.0f);
                    float py = -(y + (v / (float)nsubsamples) - (h / 2.0f)) / (h / 2.0f);

                    // Scale NDC based on width/height ratio, supporting non-square image output
                    px *= (float)w / (float)h;

                    float ret = 0.f;
                    Ray ray;
                    Isect isect;

                    ray.org = vec(0.f, 0.f, 0.f);

                    // Poor man's perspective projection
                    ray.dir.x = px;
                    ray.dir.y = py;
                    ray.dir.z = -1.0f;
                    vnormalize(ray.dir);

                    isect.t   = 1.0e+17f;
                    isect.hit = 0;

                    for (int snum = 0; snum < 3; ++snum)
                    {
                        ray_sphere_intersect(isect, ray, spheres[snum]);
                    }
                    ray_plane_intersect(isect, ray, plane);

                    if (isect.hit)
                    {
                        ret = ambient_occlusion(isect, plane, spheres);

                        // Normalize by number of samples taken.
                        ret *= invSamples * invSamples;

                        // Update image for AO for this ray
                        image[offset + 0] += ret;
                        image[offset + 1] += ret;
                        image[offset + 2] += ret;
                    }
                }
            }
        }
    }
}

//------------------------------------------------------------------------------
struct AoUniformData
{
    Plane plane;
    Sphere* spheres;
    uint32_t w;
    uint32_t h;
    uint32_t nsubsamples;
    float* image;
};

//------------------------------------------------------------------------------
Stats aoBenchPerfSerial(uint32_t w, uint32_t h, uint32_t nsubsamples, uint32_t iterations)
{
    Stats stats(iterations);

    float* image = new float[w * h * 3];

    Plane plane = { vec(0.0f, -0.5f, 0.0f), vec(0.f, 1.f, 0.f) };
    Sphere spheres[3] = {
        { vec(-2.0f, 0.0f, -3.5f), 0.5f },
        { vec(-0.5f, 0.0f, -3.0f), 0.5f },
        { vec(1.0f, 0.0f, -2.2f), 0.5f } };

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        auto start = std::chrono::high_resolution_clock::now();

        ao_scanlines(0, w, 0, h, w, h, nsubsamples, image, plane, spheres);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    delete[] image;

    return stats;
}

//------------------------------------------------------------------------------
Stats aoBenchPerfParallel(uint32_t w, uint32_t h, uint32_t nsubsamples, uint32_t iterations, uint32_t threadCount)
{
    Stats stats(iterations);

    gts::WorkerPool workerPool;
    workerPool.initialize(threadCount);

    gts::MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    gts::ParallelFor parallelFor(taskScheduler);

    float* image = new float[w * h * 3];

    Plane plane = { vec(0.0f, -0.5f, 0.0f), vec(0.f, 1.f, 0.f) };
    Sphere spheres[3] = {
        { vec(-2.0f, 0.0f, -3.5f), 0.5f },
        { vec(-0.5f, 0.0f, -3.0f), 0.5f },
        { vec(1.0f, 0.0f, -2.2f), 0.5f } };


    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        auto start = std::chrono::high_resolution_clock::now();

        AoUniformData taskData;
        taskData.w = w;
        taskData.h = h;
        taskData.nsubsamples = nsubsamples;
        taskData.image = image;
        taskData.plane = plane;
        taskData.spheres = spheres;

        parallelFor(
            gts::BlockedRange2d<int>(0, w, 1, 0, h, 16),
            [](gts::BlockedRange2d<int>& range, void* param, gts::TaskContext const&)
            {
                AoUniformData& data = *(AoUniformData*)param;
                
                ao_scanlines(
                    range.range0().begin(),
                    range.range0().end(),
                    range.range1().begin(),
                    range.range1().end(),
                    data.w,
                    data.h,
                    data.nsubsamples,
                    data.image,
                    data.plane,
                    data.spheres);
            },
            gts::AdaptivePartitioner(),
            &taskData);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    delete[] image;

    return stats;
}
