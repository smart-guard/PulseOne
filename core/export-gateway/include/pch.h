/**
 * @file pch.h
 * @brief Precompiled Header for export-gateway
 *
 * 모든 TU에서 반복 파싱되던 무거운 헤더들을 한 번만 컴파일.
 * - Linux native / Docker: g++ → build/pch.h.gch
 * - Windows cross-compile: x86_64-w64-mingw32-g++ → build/pch.h.gch
 *
 * 사용법: Makefile이 -include include/pch.h 플래그를 모든 TU에 주입.
 *         g++/mingw가 build/pch.h.gch 를 자동 검출해 재사용.
 */
#pragma once

// ── C++ Standard Library ─────────────────────────────────────────────────────
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// ── 무거운 헤더 (이것이 PCH 핵심 대상) ──────────────────────────────────────
#include <nlohmann/json.hpp>

// ── 플랫폼 공통 시스템 헤더 ──────────────────────────────────────────────────
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#else
#include <unistd.h>
#endif
