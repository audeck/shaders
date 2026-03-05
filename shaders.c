#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <pthread.h>
#include "glsl_math.h"

static vec3 shader_march(float t, vec2 r, vec2 FC) {
    vec2 rot = rotate2D_diag(t);
    vec3 p = v3(t / 0.1f, rot.x, rot.y);

    ivec2 iFC = iv2((int)(FC.x * 0.5f + 0.5f), (int)(FC.y * 0.5f + 0.5f));
    vec3 rxxy = v3(r.x, r.x, r.y);
    vec3 rhs = v3(0.f, (float)(iFC.x * 4), (float)(iFC.y * 4));
    vec3 d = v3norm(v3sub(rxxy, rhs));

    float iter = 0.f;
    vec3 a = v3s(0.f);
    vec3 b = v3s(0.f);

    while (iter++ < 900.f) {
        vec3 sp = v3div(v3ceil(v3add(p, v3mul(d, v3step(a, b)))), v3s(28.f));
        a = sp;

        float noise = snoise3D(a);
        float len_azy = v2len(zy(a)); // length(a.zy)

        if (noise >= 1.f - len_azy) {
            break;
        }

        a = v3sign(d);
        b = v3fract(v3mul(v3neg(p), a)); // fract(-p * sign(d))
        b = v3add(b, v3step(b, v3neg(b))); // b += step(b, -b)
        a = v3mul(a, v3div(b, d)); // a *= b / d  (may produce NaN for d==0, safe via gmin)
        b = v3min(a, v3min(yzx(a), zxy(a)));
        p = v3add(p, v3mul(d, b));
    }

    return p;
}

// Adapted from https://x.com/XorDev/status/1441621505704669185
// Full fwidth implementation
static vec3 shader(float t, vec2 r, vec2 FC) {
    vec3 p = shader_march(t, r, FC); // current pixel
    vec3 px = shader_march(t, r, v2(FC.x + 1.f, FC.y)); // neighbour in x
    vec3 py = shader_march(t, r, v2(FC.x, FC.y + 1.f)); // neighbour in y

    vec3 pp = v3add(p,  p);
    vec3 pppx = v3add(px, px);
    vec3 pppy = v3add(py, py);

    // fwidth = |dFdx| + |dFdy|
    vec3 color = v3add(v3abs(v3sub(pppx, pp)), v3abs(v3sub(pppy, pp)));

    color.x = gmax(0.f, gmin(1.f, color.x));
    color.y = gmax(0.f, gmin(1.f, color.y));
    color.z = gmax(0.f, gmin(1.f, color.z));
    return color;
}

typedef struct {
    int frame, width, height;
    float t;
} FrameJob;

static void* render_frame(void *arg) {
    FrameJob *job = (FrameJob *)arg;

    int width = job->width;
    int height = job->height;
    float t = job->t;
    vec2 r = v2((float)width, (float)height);

    // Each thread allocates its own pixel buffer (ew)
    unsigned char *pixels = malloc((size_t)width * height * 3);
    if (!pixels) {
        perror("malloc");
        return NULL;
    }

    for (int y = 0; y < height; ++y) {
        float fc_y = (float)(height - 1 - y); // flip Y
        for (int x = 0; x < width; ++x) {
            vec3 color = shader(t, r, v2((float)x, fc_y));
            unsigned char *p = &pixels[(y * width + x) * 3];

            p[0] = (unsigned char)(gmax(0.f, gmin(1.f, color.x)) * 255.f);
            p[1] = (unsigned char)(gmax(0.f, gmin(1.f, color.y)) * 255.f);
            p[2] = (unsigned char)(gmax(0.f, gmin(1.f, color.z)) * 255.f);
        }
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "output/output-%03d.ppm", job->frame);
    FILE *file = fopen(buf, "wb");
    if (file) {
        fprintf(file, "P6\n"); // Format specifier(?)
        fprintf(file, "%d %d\n", width, height); // w/h
        fprintf(file, "255\n"); // Bit depth
        fwrite(pixels, 1, (size_t)width * height * 3, file);
        fclose(file);
        printf(" - generated %s\n", buf);
    }

    free(pixels);
    return NULL;
}

int main(void) {
    const int WIDTH = 16 * 60;
    const int HEIGHT = 9 * 60;
    const float FPS = 60.f;
    const int LENGTH_IN_SECONDS = 6;
    const int FRAMES = FPS * LENGTH_IN_SECONDS;

    pthread_t threads[FRAMES];
    FrameJob jobs[FRAMES];

    for (int frame = 0; frame < FRAMES; ++frame) {
        jobs[frame] = (FrameJob) {
            .frame  = frame,
            .width  = WIDTH,
            .height = HEIGHT,
            .t      = (float)frame / FPS,
        };
        pthread_create(&threads[frame], NULL, render_frame, &jobs[frame]);
    }

    for (int frame = 0; frame < FRAMES; ++frame) {
        pthread_join(threads[frame], NULL);
    }

    return 0;
}
