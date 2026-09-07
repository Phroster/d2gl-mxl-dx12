# Beta 1 capture: crowded-area FPS drops

Capture: 7 September 2026, 02:59:08 local start, stopped at 03:02:04 after the user's reported run. 9,307 focused gameplay frames, 450 frames at least 10 ms, one dropped diagnostic record. The median frame interval was 6.951 ms.

## The reported 75-85 FPS band

117 captured frames fell between 11.765 and 13.333 ms. Their medians were:

| Measurement | Result |
| --- | ---: |
| Frame interval | 12.591 ms |
| Render-thread wall time | 12.031 ms |
| Geometry/texture upload wall time | 10.828 ms |
| Resource-allocation wall time, nested inside uploads | 4.769 ms |
| Buffer bytes uploaded | 97,584,368 |
| Upload spill bytes | 31,506,496 |
| Own DX12 GPU execution | 2.347 ms |
| Waiting for the game to submit | 0.0005 ms |
| GPU fence wait | 0 ms |
| New pipeline creation | 0 |

All 117 were dominated by renderer work. In the broader 10-25 ms band, 429 of 440 frames were renderer-dominated. This isolates the sustained drop from the separate large game-side pauses. GPU measurements exclude ReShade's separate submissions, but the captured CPU upload work itself accounts for most of these frame intervals.

The upload cache copied only the prefix required by the current draw. Later draws with increasing base vertices requested larger prefixes, causing another full prefix copy. Dense scenes exceeded the 64 MiB upload arena and repeatedly allocated spill resources. Beta 2 prefetches the current updated data range once per buffer version/frame, while still honoring later requests for preserved data outside that range.

The native regression workload uses 150 increasing-prefix draws with 2,457,600 useful bytes. Reproducing beta 1's zero-prefetch behavior copied 185,548,800 bytes in 20.242 ms. The fixed path copied 2,457,600 bytes in 0.252 ms. GPU readback matched byte-for-byte, including partial updates and preserved tails. These are controlled test timings, not an in-game FPS claim.

## Audio and map reveal

DirectSound hooks were active, recording 1,834 Play, 1,889 Stop, 2,171 Lock and thousands of pan/volume calls. There were occasional roughly 2 ms Stop calls. The largest Play call (25.217 ms) occurred during startup, before the crowded segment. Sound API calls did not account for the sustained 75-85 FPS band. This does not rule out other audio work outside the measured API calls.

A separate 1,223.6 ms frame spent 1,221.2 ms waiting for the game to submit, while renderer work took 1.88 ms. Other game-side pauses reached 60-90 ms. Beta 1 did not record T key events, so none can yet be definitively assigned to map reveal. Beta 2 timestamps T down/up, measures their handler durations, and records frontend frame-building time to localize the next reveal. It does not change the reveal action itself.
