#pragma once

// Central switch for development-only conveniences. Two layers, deliberately
// distinct:
//
//  - `Enabled` (compile-time): do debug capabilities EXIST in this binary?
//    Follows the build type (ON for Debug builds, OFF when NDEBUG). Gates the
//    pure OBSERVABILITY channels — console logging (rng seed, turn dumps,
//    refused-value warnings) — and whether the runtime toggle below is shown
//    at all. `constexpr`, so every `if (debug::Enabled)` branch compiles out
//    entirely in release.
//
//  - `active()` (runtime, meaningful in debug builds only): are the
//    GAMEPLAY-AFFECTING debug features currently on? Free shop prices, the
//    fully-stocked debug shop, the X / Delete / B / V keys. Starts true; the
//    play screen's DEBUG button (PlayUI) flips it, so the REAL game and shop
//    economy can be play-tested without rebuilding — and flips it back to
//    return to the admin features. In release builds it is constant false
//    and folds away exactly like Enabled.
//
// Rule of thumb for new code: prints and developer tooling branch on
// `Enabled`; anything a player would notice branches on `active()`.
namespace debug {
#ifdef NDEBUG
inline constexpr bool Enabled = false;
constexpr bool active() { return false; }
inline void setActive(bool) {}
#else
inline constexpr bool Enabled = true;
namespace detail {
inline bool runtimeActive = true;
}
inline bool active() { return detail::runtimeActive; }
inline void setActive(bool value) { detail::runtimeActive = value; }
#endif
} // namespace debug
