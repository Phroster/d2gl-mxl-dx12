#pragma once
#include <stdint.h>

/* Only the visual timestamp is filtered. The original limiter still decides
 * whether to draw and owns its deadlines/counts. A fixed phase offset retains
 * the initial interpolation latency, while a gentle correction follows actual
 * cadence without snapping movement to whole nominal refresh slots. */
typedef struct {
    uint64_t raw, visual, now, frequency, period_q16, offset, fraction;
    uint32_t numerator, denominator, level, active, resets;
} MxlRenderClock;

static uint64_t mxl_render_time(MxlRenderClock *s, uint64_t raw, uint64_t now,
    uint64_t frequency, uint32_t denominator, uint32_t numerator,
    uint32_t level, uint32_t type)
{
    uint64_t period=0, q16=0;
    int changed=frequency!=s->frequency || denominator!=s->denominator || numerator!=s->numerator;
    if(type!=3 || !level || frequency<1000 || frequency>10000000000ULL ||
        !denominator || denominator>1000000 || !numerator || numerator>1000000000) {
        s->active=0;return raw;
    }
    if(changed || !s->period_q16) {
        uint64_t product=frequency*denominator;
        period=product/numerator;
        if(period<frequency/1000 || period>frequency/10) {s->active=0;return raw;}
        q16=period*65536+((product%numerator)*65536)/numerator;
    } else {q16=s->period_q16;period=q16/65536;}
    if(s->active && !changed && level==s->level && now>s->now &&
        now-s->now>=period/4 && now-s->now<=period*7/4 && raw>=s->raw && raw<=now) {
        uint64_t predicted=s->visual+q16/65536;
        uint64_t target=now>=s->offset?now-s->offset:0;
        int64_t error=target>=predicted?(int64_t)(target-predicted):-(int64_t)(predicted-target);
        if(predicted>=s->visual && error>-(int64_t)period && error<(int64_t)period) {
            /* Q16 residue avoids integer truncation at small clock frequencies.
             * One frame of phase disagreement can change a step by <0.8%.
             * Genuine missed frames and pauses re-anchor instead of slowing time. */
            int64_t advance=(int64_t)q16+error*512+(int64_t)s->fraction;
            s->visual+=(uint64_t)advance/65536;s->fraction=(uint64_t)advance%65536;
            s->raw=raw;s->now=now;return s->visual;
        }
    }
    s->raw=raw;s->visual=raw;s->now=now;s->frequency=frequency;
    s->period_q16=q16;s->numerator=numerator;s->denominator=denominator;
    s->offset=now>=raw && now-raw<=period?now-raw:0;
    s->fraction=0;s->level=level;s->active=1;++s->resets;return raw;
}
