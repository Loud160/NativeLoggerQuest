// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: © 2026 Loud160 (AKA Whisp) and the Big Screen contributors
//
// Part of Big Screen.
// Distributed under GPL-3.0-only with additional terms under GPLv3
// section 7(b)/(c) and an interoperability permission under section 7;
// see LICENSE and LICENSE-ADDITIONAL-TERMS.md.

#include "NativeLoggerQuest/NativeLogger.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace {
    constexpr std::size_t MaximumBridgeFieldBytes = 64u * 1024u;

    std::string_view BoundedField(
        const std::uint8_t* data,
        std::size_t length) noexcept
    {
        if(!data || length == 0)
            return {};
        return {
            reinterpret_cast<const char*>(data),
            std::min(length, MaximumBridgeFieldBytes),
        };
    }

    NativeLoggerQuest::LogSeverity BridgeSeverity(int level) noexcept
    {
        // Paper2's public C ABI orders its levels as Info, Warn, Error,
        // Debug, Crit, Off. These numeric values are used only at the private
        // linker-wrap boundary; the consuming mod does not initialize or call
        // Paper through this private bridge.
        switch(level)
        {
            case 0:
                return NativeLoggerQuest::LogSeverity::Info;
            case 1:
                return NativeLoggerQuest::LogSeverity::Warning;
            case 2:
                return NativeLoggerQuest::LogSeverity::Error;
            case 3:
                return NativeLoggerQuest::LogSeverity::Debug;
            case 4:
                return NativeLoggerQuest::LogSeverity::Critical;
            default:
                return NativeLoggerQuest::LogSeverity::Debug;
        }
    }
}

// beatsaber-hook's inline SAFE_ABORT helpers currently route their diagnostics
// through two Paper2 C-ABI calls. Link-time --wrap redirects only references
// linked into the one consuming mod to these hidden functions. Their names are
// not exported and cannot satisfy, replace, or intercept Paper2 calls made by
// BSML, SongCore, or any other mod. This preserves hook failure details without
// giving the consumer a runtime dependency on Paper's ABI.
extern "C" __attribute__((visibility("hidden"))) bool
__wrap_paper2_queue_log_bytes_ffi(
    int level,
    const std::uint8_t* tagData,
    std::uintptr_t tagLength,
    const std::uint8_t* messageData,
    std::uintptr_t messageLength,
    const std::uint8_t* fileData,
    std::uintptr_t fileLength,
    int line,
    int,
    const std::uint8_t* functionData,
    std::uintptr_t functionLength) noexcept
{
    try
    {
        // Off is intentionally discarded, matching the source logger's
        // disabled level without allocating or waking the consumer's writer.
        if(level == 5)
            return true;

        const auto tag = BoundedField(tagData, tagLength);
        const auto message = BoundedField(messageData, messageLength);
        const auto file = BoundedField(fileData, fileLength);
        const auto function = BoundedField(functionData, functionLength);

        std::string record;
        record.reserve(
            tag.size() + message.size() + file.size() + function.size() + 48u);
        if(!tag.empty())
        {
            record.push_back('[');
            record.append(tag);
            record.append("] ");
        }
        record.append(message);
        if(!file.empty())
        {
            record.append(" (");
            record.append(file);
            if(line > 0)
            {
                record.push_back(':');
                record.append(std::to_string(line));
            }
            if(!function.empty())
            {
                record.append(" in ");
                record.append(function);
            }
            record.push_back(')');
        }

        NativeLoggerQuest::NativeLogger::Instance().Log(
            BridgeSeverity(level),
            std::move(record));
        return true;
    }
    catch(...)
    {
        return false;
    }
}

extern "C" __attribute__((visibility("hidden"))) bool
__wrap_paper2_wait_for_flush() noexcept
{
    return NativeLoggerQuest::NativeLogger::Instance().Flush(
        std::chrono::milliseconds(250));
}
