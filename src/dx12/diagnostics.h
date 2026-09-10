#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace mxl::diag {
enum class Metric : uint32_t { Render, InputWait, FenceWait, Present, LatencyWait, Submit,
    Pipeline, Bindings, IndexScan, Upload, Allocate, ProducerBuild,
    LootEffects, LootLabels, LootNames, LootPickupSampled, LootCaptureSampled,
    GameWorld, GameUI, GameMap, Probe, PresentProbe, Count };
enum class Count : uint32_t { Draws, Indices, TextureBytes, BufferBytes, SpillBytes, Pipelines,
    BindingMisses, Barriers, Minimap, Width, Height, GameScreen, Textures,
    LootEnabled, LootPickupEnabled, LootTargets, LootLabels, LootSprites, LootNameFormats,
    LootSelectionCalls, LootSelectionSamples, LootCaptureCalls, LootCaptureSamples,
    LootInventoryQueries, LootUnitLookups, LootCacheReclaims, LootCacheUploads,
    LootCacheHits, LootCacheSkipped,
    MotionValid, MotionSamples, MotionElapsedTicks, MotionIntervalTicks, MotionClampedTicks,
    MotionClientUpdates, MotionUpdateMs, MotionClockMs, MotionGameType,
    MotionPlayerValid, MotionPlayerId, MotionPlayerX, MotionPlayerY, MotionCameraX, MotionCameraY, MotionPanels,
    ContextValid, Level, PlayerMode, WorldUnits, WorldPlayers, WorldMonsters, WorldMissiles, WorldItems,
    MotionRenderTicks, MotionUpdateTicks, MotionProbeTicks,
    BuildCyclesValid, BuildCycles, WaitCyclesValid, WaitCycles, RenderCyclesValid, RenderCycles,
    PresentStart, PresentEnd, PresentResult, PresentVsync, PresentProbed, PresentStatsResult,
    PresentStatsCount, PresentRefresh, SyncRefresh, SyncQpc, PresentIdValid, PresentId, LatencyResult,
    ProcessValid, ProcessUser, ProcessKernel, ProcessRead, ProcessWrite, ProcessFaults, WorkingSet, PrivateBytes,
    LoggerCpuValid, LoggerCpu, QueuePeak, QueueSize,
    MotionEpochRawTicks, MotionEpochSamples, MotionEpochResets, MotionEpochReason, MotionEpochActive,
    MotionRenderRawTicks, MotionRenderNowTicks, MotionRenderResets, MotionRenderActive, Count };
enum class Audio : uint32_t { Factory, CreateBuffer, DuplicateBuffer, Play, Stop, Lock, Unlock,
    Volume, Pan, Frequency, Cursor, Restore, Parameters3D, Position3D, Commit3D, Status, GetCursor, Release, Query, Count };
uint64_t ticks() noexcept;
double milliseconds(uint64_t elapsed) noexcept;
#if MXL_ENABLE_DIAGNOSTICS
bool start(HWND window, const std::wstring& test_directory = {});
bool enabled() noexcept;
bool audio_enabled() noexcept;
bool detail_logs_enabled() noexcept;
bool comprehensive_enabled() noexcept;
uint64_t current_frame() noexcept;
void begin_frame(uint64_t id, double input_wait_ms, uint32_t width, uint32_t height, bool minimap, uint32_t screen=1) noexcept;
void end_frame() noexcept;
void add(Metric metric, uint64_t elapsed) noexcept;
void count(Count metric, uint64_t amount=1) noexcept;
void producer(uint64_t id, uint64_t ready, uint64_t returned, double interval_ms, uint32_t vertices, double build_ms=0) noexcept;
// Game-thread totals are attached to its next producer row, not render-thread
// frames. Each producer submission resets them; no per-hook queue or disk I/O.
void producer_add(Metric metric, uint64_t elapsed) noexcept;
void producer_count(Count metric, uint64_t amount=1) noexcept;
void producer_set(Count metric, uint64_t amount) noexcept;
void producer_begin() noexcept;
void producer_stage(unsigned stage) noexcept;
void producer_ready() noexcept;
void input_event(const char* category, uint64_t began) noexcept;
bool producer_sample(Count calls, Count samples) noexcept;
void gpu_batch(uint64_t frame, double duration_ms) noexcept;
void audio_call(Audio operation, uint64_t start, uint64_t end, HRESULT result) noexcept;
void note(const char* message, int64_t value=0) noexcept;
void stop(bool wait=false) noexcept;
std::wstring session_directory();
uint64_t dropped_records() noexcept;
#else
inline bool start(HWND, const std::wstring& = {}) { return false; }
inline constexpr bool enabled() noexcept { return false; }
inline constexpr bool audio_enabled() noexcept { return false; }
inline constexpr bool detail_logs_enabled() noexcept { return false; }
inline constexpr bool comprehensive_enabled() noexcept { return false; }
inline constexpr uint64_t current_frame() noexcept { return 0; }
inline void begin_frame(uint64_t, double, uint32_t, uint32_t, bool, uint32_t=1) noexcept {}
inline void end_frame() noexcept {}
inline void add(Metric, uint64_t) noexcept {}
inline void count(Count, uint64_t=1) noexcept {}
inline void producer(uint64_t, uint64_t, uint64_t, double, uint32_t, double=0) noexcept {}
inline void producer_add(Metric, uint64_t) noexcept {}
inline void producer_count(Count, uint64_t=1) noexcept {}
inline void producer_set(Count, uint64_t) noexcept {}
inline void producer_begin() noexcept {}
inline void producer_stage(unsigned) noexcept {}
inline void producer_ready() noexcept {}
inline void input_event(const char*, uint64_t) noexcept {}
inline constexpr bool producer_sample(Count, Count) noexcept { return false; }
inline void gpu_batch(uint64_t, double) noexcept {}
inline void audio_call(Audio, uint64_t, uint64_t, HRESULT) noexcept {}
inline void note(const char*, int64_t=0) noexcept {}
inline void stop(bool=false) noexcept {}
inline std::wstring session_directory() { return {}; }
inline constexpr uint64_t dropped_records() noexcept { return 0; }
#endif
struct Scope {
    Metric metric; uint64_t began;
    explicit Scope(Metric m) noexcept : metric(m),began(enabled()?ticks():0) {}
    ~Scope() { if(began) add(metric,ticks()-began); }
};
struct ProducerScope {
    Metric metric; uint64_t began;
    explicit ProducerScope(Metric m) noexcept : metric(m),began(enabled()?ticks():0) {}
    ~ProducerScope() { if(began) producer_add(metric,ticks()-began); }
};
struct ProducerSampleScope {
    Metric metric; uint64_t began;
    ProducerSampleScope(Metric m, Count calls, Count samples) noexcept
        : metric(m),began(producer_sample(calls,samples)?ticks():0) {}
    ~ProducerSampleScope() { if(began) producer_add(metric,ticks()-began); }
};
}
