#pragma once

// Central switch for development-only conveniences (e.g. the shop offering every
// item for free so they can be tested at once). Kept in one place so debug
// behaviour is easy to find and grep — features simply branch on `debug::Enabled`.
//
// By default it follows the build type: ON for Debug builds, OFF for Release
// (NDEBUG is defined). To force a value regardless of build type, replace the
// body below with a plain `true` / `false`.
//
// It is `constexpr`, so any `if (debug::Enabled)` branch is compiled out entirely
// when disabled — zero runtime cost in release.
namespace debug {
#ifdef NDEBUG
inline constexpr bool Enabled = false;
#else
inline constexpr bool Enabled = true;
#endif
} // namespace debug
