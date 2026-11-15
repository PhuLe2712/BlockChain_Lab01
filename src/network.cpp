#include "network.hpp"

#include <stdexcept>
#include <utility>

Network::Network(const NetworkConfig& config, Logger* logger)
    : config_(config),
      logger_(logger),
      rng_(config.seed) {

    if (config_.min_delay_ticks > config_.max_delay_ticks) {
        throw std::invalid_argument("Invalid network delay range");
    }

    if (config_.duplicate_probability < 0.0 ||
        config_.duplicate_probability > 1.0 ||
        config_.drop_probability < 0.0 ||
        config_.drop_probability > 1.0) {
        throw std::invalid_argument("Network probability must be in [0, 1]");
    }
}

std::uint64_t Network::randomDelay() {
    if (scriptedDelayIndex_ < config_.scripted_delays.size()) {
        return config_.scripted_delays[scriptedDelayIndex_++];
    }

    std::uniform_int_distribution<std::uint64_t> distribution(
        config_.min_delay_ticks,
        config_.max_delay_ticks
    );

    return distribution(rng_);
}

bool Network::randomEvent(double probability) {
    if (probability <= 0.0) {
        return false;
    }

    if (probability >= 1.0) {
        return true;
    }

    std::bernoulli_distribution distribution(probability);
    return distribution(rng_);
}

void Network::logEvent(
    std::uint64_t tick,
    const std::string& node,
    const std::string& type,
    const std::string& detail
) {
    if (logger_) {
        logger_->event(tick, node, type, detail);
    }
}

void Network::send(
    std::uint64_t currentTick,
    const std::string& from,
    const std::string& to,
    const std::string& type,
    std::uint64_t height,
    const std::string& payload
) {
    if (from.empty() || to.empty() || type.empty()) {
        throw std::invalid_argument("Message fields cannot be empty");
    }

    const auto peer = std::make_pair(from, to);
    const std::string detail = from + "->" + to + "|" + type +
                               "|height=" + std::to_string(height);

    if (blockedPeers_[peer]) {
        logEvent(currentTick, from, "MESSAGE_BLOCKED", detail);
        return;
    }

    if (randomEvent(config_.drop_probability)) {
        logEvent(currentTick, from, "MESSAGE_DROPPED", detail);
        return;
    }

    Message message;
    message.id = nextMessageId_++;
    message.from = from;
    message.to = to;
    message.type = type;
    message.height = height;
    message.payload = payload;
    message.delivery_tick = currentTick + randomDelay();

    queue_.push({message, nextSequence_++});

    logEvent(
        currentTick,
        from,
        "MESSAGE_SENT",
        detail + "|id=" + std::to_string(message.id)
    );

    logEvent(
        currentTick,
        from,
        "MESSAGE_DELAYED",
        detail + "|delivery_tick=" +
            std::to_string(message.delivery_tick)
    );

    if (randomEvent(config_.duplicate_probability)) {
        Message duplicate = message;
        duplicate.id = nextMessageId_++;
        duplicate.delivery_tick += randomDelay();

        queue_.push({duplicate, nextSequence_++});

        logEvent(
            currentTick,
            from,
            "MESSAGE_DUPLICATED",
            detail + "|original_id=" +
                std::to_string(message.id) +
                "|duplicate_id=" +
                std::to_string(duplicate.id)
        );
    }
}

std::vector<Message> Network::receive(
    std::uint64_t currentTick,
    const std::string& node
) {
    std::vector<Message> delivered;
    std::vector<PendingMessage> remaining;

    while (!queue_.empty() &&
           queue_.top().message.delivery_tick <= currentTick) {

        PendingMessage pending = queue_.top();
        queue_.pop();

        if (pending.message.to == node) {
            logEvent(
                currentTick,
                node,
                "MESSAGE_DELIVERED",
                "from=" + pending.message.from +
                    "|type=" + pending.message.type +
                    "|id=" + std::to_string(pending.message.id)
            );

            delivered.push_back(std::move(pending.message));
        } else {
            remaining.push_back(std::move(pending));
        }
    }

    for (auto& pending : remaining) {
        queue_.push(std::move(pending));
    }

    return delivered;
}

void Network::setPeerBlocked(
    const std::string& from,
    const std::string& to,
    bool blocked
) {
    blockedPeers_[{from, to}] = blocked;

    logEvent(
        0,
        from,
        blocked ? "PEER_BLOCKED" : "PEER_UNBLOCKED",
        "peer=" + to
    );
}

std::size_t Network::pendingMessages() const {
    return queue_.size();
}
