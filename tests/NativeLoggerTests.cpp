// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: © 2026 Loud160 (AKA Whisp) and the Big Screen contributors
//
// Part of Big Screen.
// Distributed under GPL-3.0-only with additional terms under GPLv3
// section 7(b)/(c) and an interoperability permission under section 7;
// see LICENSE and LICENSE-ADDITIONAL-TERMS.md.

#include "NativeLoggerQuest/NativeLogger.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    std::filesystem::path NewTestRoot(std::string_view name)
    {
        return std::filesystem::temp_directory_path() /
               ("native-logger-quest-" + std::string(name) + "-" +
                std::to_string(
                    std::chrono::steady_clock::now()
                        .time_since_epoch()
                        .count()));
    }

    std::string ReadAll(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return {
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
    }

    NativeLoggerQuest::NativeLoggerOptions OptionsFor(
        const std::filesystem::path& root)
    {
        NativeLoggerQuest::NativeLoggerOptions options;
        options.activePath = root / "native-logger.log";
        options.previousPath = root / "native-logger.previous.log";
        options.reopenInterval = 100ms;
        options.emitToLogcat = false;
        return options;
    }

    bool WaitForFileMessages(
        NativeLoggerQuest::NativeLogger& logger,
        std::uint64_t expected,
        std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while(std::chrono::steady_clock::now() < deadline)
        {
            if(logger.Statistics().fileMessages >= expected)
                return true;
            std::this_thread::sleep_for(2ms);
        }
        return logger.Statistics().fileMessages >= expected;
    }

    void TestBasicWriteAndLifecycle()
    {
        auto& logger = NativeLoggerQuest::NativeLogger::Instance();
        logger.Shutdown();
        const auto root = NewTestRoot("basic");
        auto options = OptionsFor(root);
        assert(options.maxFileBytes == 5u * 1024u * 1024u);
        assert(logger.Initialize(options, "test session started"));
        logger.Log(
            NativeLoggerQuest::LogSeverity::Info,
            "hello from the native logger",
            {"NativeLoggerTests.cpp", "TestBasicWriteAndLifecycle", 71});
        assert(logger.Flush(2s));
        logger.Shutdown();

        const std::string contents = ReadAll(options.activePath);
        assert(contents.find("test session started") != std::string::npos);
        assert(
            contents.find("hello from the native logger") !=
            std::string::npos);
        assert(contents.find("[INFO][NativeLoggerQuest][tid=") != std::string::npos);
        assert(contents.find("NativeLoggerTests.cpp:71") != std::string::npos);
        assert(
            contents.find("in TestBasicWriteAndLifecycle") !=
            std::string::npos);
        const auto statistics = logger.Statistics();
        assert(statistics.acceptedMessages == 2);
        assert(statistics.fileMessages == 2);
        assert(statistics.droppedMessages == 0);
        std::filesystem::remove_all(root);
    }

    void TestRotationKeepsOnePreviousFile()
    {
        auto& logger = NativeLoggerQuest::NativeLogger::Instance();
        logger.Shutdown();
        const auto root = NewTestRoot("rotation");
        auto options = OptionsFor(root);
        options.maxFileBytes = 1024;
        assert(logger.Initialize(options, "rotation session"));
        for(int index = 0; index < 80; ++index)
        {
            logger.Log(
                NativeLoggerQuest::LogSeverity::Info,
                "rotation record " + std::to_string(index) + " " +
                    std::string(96, 'x'));
        }
        assert(logger.Flush(2s));
        logger.Shutdown();

        assert(std::filesystem::is_regular_file(options.activePath));
        assert(std::filesystem::is_regular_file(options.previousPath));
        assert(std::filesystem::file_size(options.activePath) <= 1200u);
        assert(std::filesystem::file_size(options.previousPath) <= 1200u);
        assert(logger.Statistics().rotations > 0);
        std::filesystem::remove_all(root);
    }

    void TestCompletedBatchIsReadableBeforeShutdown()
    {
        auto& logger = NativeLoggerQuest::NativeLogger::Instance();
        logger.Shutdown();
        const auto root = NewTestRoot("crash-tail");
        auto options = OptionsFor(root);
        // No periodic flush exists. The record must become readable because
        // the completed writer batch itself is flushed, not because explicit
        // Flush or Shutdown happened to run.
        assert(logger.Initialize(options, "crash-tail session"));
        logger.Log(
            NativeLoggerQuest::LogSeverity::Info,
            "ordinary record visible before logger shutdown");
        assert(WaitForFileMessages(logger, 2, 2s));

        const std::string liveContents = ReadAll(options.activePath);
        assert(
            liveContents.find(
                "ordinary record visible before logger shutdown") !=
            std::string::npos);
        assert(logger.IsInitialized());

        logger.Shutdown();
        std::filesystem::remove_all(root);
    }

    void TestConcurrentProducers()
    {
        auto& logger = NativeLoggerQuest::NativeLogger::Instance();
        logger.Shutdown();
        const auto root = NewTestRoot("concurrent");
        auto options = OptionsFor(root);
        options.maxQueueEntries = 10000;
        options.maxQueueBytes = 8u * 1024u * 1024u;
        assert(logger.Initialize(options, "concurrent session"));

        constexpr int producerCount = 6;
        constexpr int recordsPerProducer = 200;
        std::vector<std::thread> producers;
        producers.reserve(producerCount);
        for(int producer = 0; producer < producerCount; ++producer)
        {
            producers.emplace_back([producer, &logger]() {
                for(int record = 0; record < recordsPerProducer; ++record)
                {
                    logger.Log(
                        NativeLoggerQuest::LogSeverity::Info,
                        "producer " + std::to_string(producer) +
                            " record " + std::to_string(record));
                }
            });
        }
        for(auto& producer : producers)
            producer.join();
        assert(logger.Flush(5s));
        logger.Shutdown();

        const auto statistics = logger.Statistics();
        assert(
            statistics.acceptedMessages ==
            1u + producerCount * recordsPerProducer);
        assert(statistics.fileMessages == statistics.acceptedMessages);
        assert(statistics.droppedMessages == 0);
        std::filesystem::remove_all(root);
    }

    void TestBoundedQueueDropsWithoutThrowing()
    {
        auto& logger = NativeLoggerQuest::NativeLogger::Instance();
        logger.Shutdown();
        const auto root = NewTestRoot("bounded");
        auto options = OptionsFor(root);
        options.maxQueueEntries = 0;
        options.maxQueueBytes = 0;
        options.urgentReserveEntries = 0;
        options.urgentReserveBytes = 0;
        assert(logger.Initialize(options, "this header is deliberately dropped"));
        logger.Log(NativeLoggerQuest::LogSeverity::Info, "also dropped");
        assert(logger.Flush(2s));
        logger.Shutdown();
        assert(logger.Statistics().acceptedMessages == 0);
        assert(logger.Statistics().droppedMessages >= 2);
        std::filesystem::remove_all(root);
    }

    void TestCallsOutsideInitializedLifetimeAreSafe()
    {
        auto& logger = NativeLoggerQuest::NativeLogger::Instance();
        logger.Shutdown();
        logger.Log(NativeLoggerQuest::LogSeverity::Error, "before initialization");
        assert(logger.Flush(1ms));
        logger.Shutdown();
        logger.Log(NativeLoggerQuest::LogSeverity::Error, "after shutdown");
    }

    void TestRepeatedInitializationOwnsOneWriterAtATime()
    {
        auto& logger = NativeLoggerQuest::NativeLogger::Instance();
        logger.Shutdown();
        const auto firstRoot = NewTestRoot("reinitialize-first");
        const auto secondRoot = NewTestRoot("reinitialize-second");
        const auto firstOptions = OptionsFor(firstRoot);
        const auto secondOptions = OptionsFor(secondRoot);

        assert(logger.Initialize(firstOptions, "first session"));
        logger.Log(NativeLoggerQuest::LogSeverity::Info, "first writer record");
        assert(logger.Initialize(secondOptions, "second session"));
        logger.Log(NativeLoggerQuest::LogSeverity::Info, "second writer record");
        assert(logger.Flush(2s));
        logger.Shutdown();

        assert(
            ReadAll(firstOptions.activePath).find("first writer record") !=
            std::string::npos);
        assert(
            ReadAll(secondOptions.activePath).find("second writer record") !=
            std::string::npos);
        std::filesystem::remove_all(firstRoot);
        std::filesystem::remove_all(secondRoot);
    }

    void TestUnavailableStorageFallsOpen()
    {
        auto& logger = NativeLoggerQuest::NativeLogger::Instance();
        logger.Shutdown();
        const auto root = NewTestRoot("unavailable");
        std::filesystem::create_directories(root);
        const auto blocker = root / "not-a-directory";
        {
            std::ofstream file(blocker, std::ios::binary);
            file << "block directory creation";
        }

        auto options = OptionsFor(root);
        options.activePath = blocker / "native-logger.log";
        options.previousPath = blocker / "native-logger.previous.log";
        assert(logger.Initialize(options, "unavailable storage session"));
        logger.Log(
            NativeLoggerQuest::LogSeverity::Error,
            "this remains safe when the file sink cannot open");
        assert(logger.Flush(2s));
        logger.Shutdown();
        assert(logger.Statistics().fileFailures > 0);
        assert(logger.Statistics().acceptedMessages == 2);
        assert(logger.Statistics().fileMessages == 0);
        std::filesystem::remove_all(root);
    }
}

int main()
{
    TestBasicWriteAndLifecycle();
    TestRotationKeepsOnePreviousFile();
    TestCompletedBatchIsReadableBeforeShutdown();
    TestConcurrentProducers();
    TestBoundedQueueDropsWithoutThrowing();
    TestCallsOutsideInitializedLifetimeAreSafe();
    TestRepeatedInitializationOwnsOneWriterAtATime();
    TestUnavailableStorageFallsOpen();
    return 0;
}
