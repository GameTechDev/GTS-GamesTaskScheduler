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

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#pragma warning (disable: 4244)
#pragma warning (disable: 4305)
#endif

namespace ao_bench {

constexpr float pi = 3.1415926535f;
constexpr uint32_t numAoSamples = 8;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct DRand
{
    uint64_t drand48_x = 0x1234ABCD330E;

    //--------------------------------------------------------------------------
    inline void srand48(int x)
    {
        drand48_x = x ^ (x << 16);
    }

    //--------------------------------------------------------------------------
    inline double drand48()
    {
        drand48_x = drand48_x * 0x5DEECE66D + 0xB;
        return (drand48_x & 0xFFFFFFFFFFFF) * (1.0 / 281474976710656.0);
    }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct Vec3
{
    Vec3() { x=y=z=0.; }
    Vec3(float xx, float yy, float zz) { x = xx; y = yy; z = zz; }

    Vec3 operator*(float f) const
    { 
        return Vec3(x*f, y*f, z*f);
    }
    Vec3 operator+(const Vec3 &f2) const
    { 
        return Vec3(x+f2.x, y+f2.y, z+f2.z); 
    }
    Vec3 operator-(const Vec3 &f2) const
    { 
        return Vec3(x-f2.x, y-f2.y, z-f2.z); 
    }
    Vec3 operator*(const Vec3 &f2) const
    { 
        return Vec3(x*f2.x, y*f2.y, z*f2.z); 
    }

    float x, y, z;
};

//------------------------------------------------------------------------------
inline Vec3 operator*(float f, const Vec3 &v)
{
    return Vec3(f*v.x, f*v.y, f*v.z);
}

//------------------------------------------------------------------------------
inline float dot(const Vec3 &a, const Vec3 &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

//------------------------------------------------------------------------------
inline Vec3 vcross(const Vec3 &v0, const Vec3 &v1)
{
    Vec3 ret;
    ret.x = v0.y * v1.z - v0.z * v1.y;
    ret.y = v0.z * v1.x - v0.x * v1.z;
    ret.z = v0.x * v1.y - v0.y * v1.x;
    return ret;
}

//------------------------------------------------------------------------------
inline void vnormalize(Vec3 &v)
{
    float len2 = dot(v, v);
    float invlen = 1.f / sqrtf(len2);
    v = v * invlen;
}

//--------------------------------------------------------------------------
struct Isect
{
    Isect() {}
    Isect(Vec3 const& p, Vec3 const& n) : p(p), n(n) {}
    Vec3     p;
    float   t;
    Vec3     n;
    int     hit;
};

//------------------------------------------------------------------------------
struct Sphere
{
    Sphere() {}
    Sphere(Vec3 const& c, float r) : center(c), radius(r) {}
    Vec3     center;
    float   radius;
};

//------------------------------------------------------------------------------
struct Plane
{
    Plane() {}
    Plane(Vec3 const& p, Vec3 const& n) : p(p), n(n) {}
    Vec3     p;
    float   pad0;
    Vec3     n;
    float   pad1;
};

//------------------------------------------------------------------------------
struct Ray
{
    Ray() {}
    Ray(Vec3 const& o, Vec3 const& d) : org(o), dir(d) {}
    Vec3     org;
    float   pad0;
    Vec3     dir;
    float   pad1;
};

//------------------------------------------------------------------------------
inline void ray_plane_intersect(Isect &isect, Ray &ray, Plane &plane)
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
inline void ray_sphere_intersect(Isect &isect, Ray &ray, Sphere &sphere)
{
    Vec3 rs = ray.org;
    Vec3 rd = ray.dir;
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
inline void orthoBasis(Vec3* __restrict basis, const Vec3 &n)
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

//--------------------------------------------------------------------------
inline float ambient_occlusion(DRand& drand, Isect &isect, Plane &plane, Sphere* __restrict spheres)
{
    float eps = 0.0001f;
    Vec3 p, n;
    Vec3 basis[3];
    float occlusion = 0.0f;

    p = isect.p + eps * isect.n;

    orthoBasis(basis, isect.n);

    constexpr int ntheta = numAoSamples;
    constexpr int nphi   = numAoSamples;
    Ray rays[numAoSamples * numAoSamples];

    for (int j = 0; j < ntheta; j++)
    {
        for (int i = 0; i < nphi; i++)
        {
            float theta = sqrtf(drand.drand48());
            float phi = 2.0f * pi * drand.drand48();
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

//--------------------------------------------------------------------------
/**
 * Compute the image for the scanlines from [y0,y1), for an overall image
 * of width w and height h.
 */
inline void ao_scanlines(int x0, int x1, int y0, int y1, int w, int h, int nsubsamples,
    float* __restrict image, Plane &plane, Sphere* __restrict spheres)
{
    DRand drand;
    drand.srand48(y0);
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

                    ray.org = Vec3(0.f, 0.f, 0.f);

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
                        ret = ambient_occlusion(drand, isect, plane, spheres);

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

}; // namespace ao_bench
