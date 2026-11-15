#pragma once

#include "logger.hpp"

#include <cstdint>
#include <map>
#include <queue>
#include <random>
#include <string>
#include <vector>

struct NetworkConfig {
    std::uint64_t min_delay_ticks{1};
    std::uint64_t max_delay_ticks{5};
    double duplicate_probability{0.0};
    double drop_probability{0.0};
    std::uint64_t seed{42};

    // Optional deterministic delay sequence for reproducible network tests.
    // When exhausted, the simulator falls back to seeded random delays.
    std::vector<std::uint64_t> scripted_delays;
};

struct Message {
    std::uint64_t id{};
    std::string from;
    std::string to;
    std::string type;
    std::uint64_t height{};
    std::string payload;
    std::uint64_t delivery_tick{};
};

class Network {
public:
    explicit Network(
        const NetworkConfig& config,
        Logger* logger = nullptr
    );

    void send(
        std::uint64_t currentTick,
        const std::string& from,
        const std::string& to,
        const std::string& type,
        std::uint64_t height,
        const std::string& payload
    );

    std::vector<Message> receive(
        std::uint64_t currentTick,
        const std::string& node
    );

    void setPeerBlocked(
        const std::string& from,
        const std::string& to,
        bool blocked
    );

    std::size_t pendingMessages() const;

private:
    struct PendingMessage {
        Message message;
        std::uint64_t sequence{};

        bool operator>(const PendingMessage& other) const {
            if (message.delivery_tick != other.message.delivery_tick) {
                return message.delivery_tick > other.message.delivery_tick;
            }
            return sequence > other.sequence;
        }
    };

    NetworkConfig config_;
    Logger* logger_;
    std::mt19937_64 rng_;

    std::priority_queue<
        PendingMessage,
        std::vector<PendingMessage>,
        std::greater<PendingMessage>
    > queue_;

    std::map<std::pair<std::string, std::string>, bool> blockedPeers_;

    std::uint64_t nextMessageId_{1};
    std::uint64_t nextSequence_{0};
    std::size_t scriptedDelayIndex_{0};

    std::uint64_t randomDelay();
    bool randomEvent(double probability);

    void logEvent(
        std::uint64_t tick,
        const std::string& node,
        const std::string& type,
        const std::string& detail
    );
};
