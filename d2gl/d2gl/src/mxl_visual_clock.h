#pragma once
#include <stdint.h>

/* Visual epoch only. The client owns update_ms and updates; never write them.
 * Keep a single QPC anchor across ordinary 40 ms client steps instead of
 * independently quantizing a fresh wall-clock conversion on every update. */
enum MxlEpochReason { MXL_EPOCH_OFF, MXL_EPOCH_ANCHOR, MXL_EPOCH_STEP,
    MXL_EPOCH_AREA, MXL_EPOCH_GAP, MXL_EPOCH_DRIFT };
typedef struct {
    uint64_t epoch, raw, render, frequency;
    uint32_t update_ms, updates, level, active, samples, resets, reason;
} MxlVisualClock;

static uint64_t mxl_visual_epoch(MxlVisualClock *s, uint64_t raw, uint64_t render,
    uint64_t frequency, uint32_t update_ms, uint32_t updates, uint32_t level, uint32_t type)
{
    uint32_t reason=MXL_EPOCH_ANCHOR;
    ++s->samples;
    s->raw=raw;
    /* Bound arithmetic and pass every non-realm game through unchanged. */
    if(type!=3 || !level || frequency<1000 || frequency>10000000000ULL || frequency%1000) {
        s->active=0;s->reason=MXL_EPOCH_OFF;return raw;
    }
    if(s->active) {
        uint32_t dt=update_ms-s->update_ms, steps=updates-s->updates;
        if(level!=s->level) reason=MXL_EPOCH_AREA;
        else if(frequency!=s->frequency || !steps || steps>5 || dt!=steps*40
            || render<s->render || render-s->render>frequency/4)
            reason=MXL_EPOCH_GAP;
        else {
            /* dt is a multiple of 40. Accumulate game intervals exactly for
             * the verified clock; reject overflow or a >20 ms disagreement. */
            uint64_t advance=(frequency/25)*steps;
            uint64_t next=s->epoch+advance;
            uint64_t error=next>raw?next-raw:raw-next;
            if(next<s->epoch || error>frequency/50) reason=MXL_EPOCH_DRIFT;
            else {s->epoch=next;reason=MXL_EPOCH_STEP;}
        }
    }
    if(reason!=MXL_EPOCH_STEP) {s->epoch=raw;++s->resets;}
    s->active=1;s->reason=reason;s->frequency=frequency;s->render=render;
    s->update_ms=update_ms;s->updates=updates;s->level=level;
    return s->epoch;
}
