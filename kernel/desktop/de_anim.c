/* =============================================================================
 * Quark-OS kernel/desktop/de_anim.c
 * Tiny tween system: linear interpolation with easing curves.
 * ============================================================================= */

#include <quark/de.h>

float de_ease_linear(float t) {
    return t;
}

float de_ease_out_cubic(float t) {
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float de_ease_in_out_cubic(float t) {
    if (t < 0.5f) return 4.0f * t * t * t;
    float u = 1.0f - t;
    return 1.0f - 4.0f * u * u * u;
}

float de_ease_out_back(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

void de_anim_start(de_anim_t* a, float from, float to, float dur) {
    if (!a) return;
    a->from     = from;
    a->to       = to;
    a->elapsed  = 0.0f;
    a->duration = dur > 0.0001f ? dur : 0.0001f;
    a->active   = 1;
}

void de_anim_to(de_anim_t* a, float to, float dur) {
    if (!a) return;
    de_anim_start(a, de_anim_value(a), to, dur);
}

void de_anim_tick(de_anim_t* a, float dt) {
    if (!a || !a->active) return;
    a->elapsed += dt;
    if (a->elapsed >= a->duration) {
        a->elapsed = a->duration;
        a->active  = 0;
    }
}

float de_anim_value(const de_anim_t* a) {
    if (!a) return 0.0f;
    if (!a->active && a->elapsed <= 0.0f) return a->from;
    float t = a->elapsed / a->duration;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float e = de_ease_out_cubic(t);   /* default curve for all animations */
    return a->from + (a->to - a->from) * e;
}

int de_anim_done(const de_anim_t* a) {
    if (!a) return 1;
    return !a->active;
}