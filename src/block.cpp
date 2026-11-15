
#include "block.hpp"

#include <set>
#include <utility>

namespace {

std::string enc(const std::string& s) {
    return std::to_string(s.size()) + ":" + s;
}

std::string calculateTransactionsRoot(
    const std::vector<Transaction>& transactions
) {
    if (transactions.empty()) {
        return crypto::sha256("TX_EMPTY");
    }

    std::vector<std::string> hashes;
    hashes.reserve(transactions.size());

    for (const auto& tx : transactions) {
        hashes.push_back(tx.id());
    }

    while (hashes.size() > 1) {
        std::vector<std::string> nextLevel;

        for (std::size_t i = 0; i < hashes.size(); i += 2) {
            const std::string& left = hashes[i];

            const std::string& right =
                (i + 1 < hashes.size()) ? hashes[i + 1] : hashes[i];

            nextLevel.push_back(
                crypto::sha256("TX_NODE:" + enc(left) + enc(right))
            );
        }

        hashes = std::move(nextLevel);
    }

    return hashes.front();
}

} 

std::string Block::headerPayload() const {
    const std::string transactionsRoot =
        calculateTransactionsRoot(transactions);

    return std::to_string(height) + "|" +
           enc(parent_hash) +
           enc(state_root) +
           enc(proposer) +
           enc(transactionsRoot);
}

std::string Block::hash() const {
    return crypto::sha256(
        enc(headerPayload()) + enc(signature)
    );
}

void Block::sign(const crypto::KeyPair& key) {
    proposer = key.address();

    signature = crypto::sign(
        key,
        "HEADER:lab01",
        headerPayload()
    );
}

bool Block::verifyHeader() const {
    if (height == 0 ||
        parent_hash.empty() ||
        state_root.empty() ||
        proposer.empty() ||
        signature.empty()) {
        return false;
    }

    return crypto::verify(
        proposer,
        "HEADER:lab01",
        headerPayload(),
        signature
    );
}

bool Block::validateBlock(
    const State& previousState,
    const std::string& expectedParentHash,
    std::uint64_t expectedHeight,
    std::string* reason
) const {
    auto fail = [&](const std::string& why) {
        if (reason) {
            *reason = why;
        }

        return false;
    };

    if (height != expectedHeight) {
        return fail("invalid_block_height");
    }

    if (parent_hash != expectedParentHash) {
        return fail("invalid_parent_hash");
    }
    if (!verifyHeader()) {
        return fail("invalid_header_signature");
    }

    State candidateState = previousState;

    std::set<std::string> transactionIds;

    for (const auto& tx : transactions) {
        if (!transactionIds.insert(tx.id()).second) {
            return fail("duplicate_transaction");
        }

        std::string txReason;

        if (!candidateState.apply(tx, &txReason)) {
            return fail("invalid_transaction:" + txReason);
        }
    }

    if (candidateState.root() != state_root) {
        return fail("state_root_mismatch");
    }

    if (reason) {
        reason->clear();
    }

    return true;
}
