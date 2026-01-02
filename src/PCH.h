#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// D3D11 headers for graphics system
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <SimpleIni.h>

#define DLLEXPORT __declspec(dllexport)

// Macro for version-specific offsets (SE, AE, VR)
#define RELOCATION_OFFSET(SE, AE) REL::VariantOffset(SE, AE, 0).offset()

using namespace std::literals;

namespace logger = SKSE::log;
namespace stl = SKSE::stl;

