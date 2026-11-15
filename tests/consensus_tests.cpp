
#include "consensus.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    ValidatorSet validators("lab01", 4, 1);

    const std::string blockA = "block-A";
    const std::string blockB = "block-B";

    std::string validatorId;

    // ==========================================
    // TEST 1: Normal PREVOTE quorum
    // ==========================================

    ConsensusEngine engine("lab01", validators, 1);

    for (int i = 0; i < 3; ++i) {
        validatorId = "validator-" + std::to_string(i);

        Vote vote = engine.makeVote(
            validatorId,
            VotePhase::PREVOTE,
            blockA
        );

        assert(engine.submitVote(vote) == VoteResult::ACCEPTED);
    }

    assert(engine.hasPrevoteQuorum(0, blockA));

    // ==========================================
    // TEST 2: Duplicate vote
    // ==========================================

    Vote duplicate = engine.makeVote(
        "validator-3",
        VotePhase::PREVOTE,
        blockA
    );

    assert(engine.submitVote(duplicate) == VoteResult::ACCEPTED);
    assert(engine.submitVote(duplicate) == VoteResult::DUPLICATE);

    // ==========================================
    // TEST 3: PRECOMMIT requires PREVOTE quorum
    // ==========================================

    for (int i = 0; i < 3; ++i) {
        validatorId = "validator-" + std::to_string(i);

        Vote vote = engine.makeVote(
            validatorId,
            VotePhase::PRECOMMIT,
            blockA
        );

        assert(engine.submitVote(vote) == VoteResult::ACCEPTED);
    }

    assert(engine.hasPrecommitQuorum(0, blockA));
    assert(engine.isFinalized());
    assert(engine.finalizedBlock().value() == blockA);

    // ==========================================
    // TEST 4: No double voting by one validator
    // ==========================================

    ConsensusEngine doubleVoteEngine(
        "lab01",
        validators,
        1
    );

    Vote firstVote = doubleVoteEngine.makeVote(
        "validator-0",
        VotePhase::PREVOTE,
        blockA
    );

    assert(
        doubleVoteEngine.submitVote(firstVote) ==
        VoteResult::ACCEPTED
    );

    bool doubleVoteRejected = false;

    try {
        doubleVoteEngine.makeVote(
            "validator-0",
            VotePhase::PREVOTE,
            blockB
        );
    } catch (const std::logic_error&) {
        doubleVoteRejected = true;
    }

    assert(doubleVoteRejected);

    // ==========================================
    // TEST 5: Detect equivocation
    // ==========================================

    ConsensusEngine equivocationEngine(
        "lab01",
        validators,
        1
    );

    Vote originalVote = equivocationEngine.makeVote(
        "validator-0",
        VotePhase::PREVOTE,
        blockA
    );

    assert(
        equivocationEngine.submitVote(originalVote) ==
        VoteResult::ACCEPTED
    );

    // Construct a conflicting, correctly signed vote.
    Vote conflictingVote;

    conflictingVote.chain_id = "lab01";
    conflictingVote.height = 1;
    conflictingVote.round = 0;
    conflictingVote.phase = VotePhase::PREVOTE;
    conflictingVote.block_hash = blockB;
    conflictingVote.validator_id = "validator-0";

    const Validator* equivocatingValidator =
        validators.find("validator-0");

    assert(equivocatingValidator != nullptr);

    conflictingVote.sign(equivocatingValidator->key);

    assert(
        equivocationEngine.submitVote(conflictingVote) ==
        VoteResult::EQUIVOCATION
    );

    assert(
        equivocationEngine.isEquivocator(
            "validator-0",
            VotePhase::PREVOTE,
            0
        )
    );

    // Equivocator's previous vote must no longer count.
    assert(
        !equivocationEngine.hasPrevoteQuorum(0, blockA)
    );

    // ==========================================
    // TEST 6: Equivocator cannot vote again
    // ==========================================

    Vote thirdVote;

    thirdVote.chain_id = "lab01";
    thirdVote.height = 1;
    thirdVote.round = 0;
    thirdVote.phase = VotePhase::PREVOTE;
    thirdVote.block_hash = blockA;
    thirdVote.validator_id = "validator-0";

    thirdVote.sign(equivocatingValidator->key);

    assert(
        equivocationEngine.submitVote(thirdVote) ==
        VoteResult::EQUIVOCATION
    );

    // ==========================================
    // TEST 7: Validator lock
    // ==========================================

    ConsensusEngine lockEngine(
        "lab01",
        validators,
        1
    );

    // Validator 0 submits the PREVOTE that completes the quorum.
    // In this single-engine harness, it is treated as the observer that
    // has now seen the quorum; validators 1 and 2 have not independently
    // observed it in their local views.
    const int lockVoteOrder[] = {1, 2, 0};

    for (int i : lockVoteOrder) {
        validatorId = "validator-" + std::to_string(i);

        Vote vote = lockEngine.makeVote(
            validatorId,
            VotePhase::PREVOTE,
            blockA
        );

        assert(lockEngine.submitVote(vote) == VoteResult::ACCEPTED);
    }

    // A validator locks immediately after observing a non-NIL PREVOTE quorum,
    // before it sends PRECOMMIT.
    assert(lockEngine.lockedBlock("validator-0").has_value());
    assert(lockEngine.lockedBlock("validator-0").value() == blockA);
    assert(lockEngine.lockedRound("validator-0") == 0);

    Vote lockVote = lockEngine.makeVote(
        "validator-0",
        VotePhase::PRECOMMIT,
        blockA
    );

    assert(lockEngine.submitVote(lockVote) == VoteResult::ACCEPTED);

    assert(lockEngine.lockedBlock("validator-0").has_value());

    assert(
        lockEngine.lockedBlock("validator-0").value() == blockA
    );

    assert(lockEngine.lockedRound("validator-0") == 0);

    // ==========================================
    // TEST 8: Locked validator cannot switch
    //         without a valid-round proof
    // ==========================================

    assert(lockEngine.advanceRound());
    assert(lockEngine.round() == 1);

    // Validator 0 is still locked on A.
    bool lockedVoteRejected = false;

    try {
        lockEngine.makeVote(
            "validator-0",
            VotePhase::PREVOTE,
            blockB
        );
    } catch (const std::logic_error&) {
        lockedVoteRejected = true;
    }

    assert(lockedVoteRejected);

    // ==========================================
    // TEST 9: A later-round PREVOTE quorum
    //         can provide an unlock proof
    // ==========================================

    // In this single-engine harness, only validator 0 was treated as
    // observing the quorum that locked A. Validators 1, 2 and 3 can
    // therefore prevote B in round 1.

    for (int i = 1; i < 4; ++i) {
        validatorId = "validator-" + std::to_string(i);

        Vote vote = lockEngine.makeVote(
            validatorId,
            VotePhase::PREVOTE,
            blockB
        );

        assert(lockEngine.submitVote(vote) == VoteResult::ACCEPTED);
    }

    assert(lockEngine.hasPrevoteQuorum(1, blockB));

    // Move to round 2.
    assert(lockEngine.advanceRound());

    // Validator 0 can now prevote B because round 1
    // has a valid PREVOTE quorum for B.
    Vote unlockVote = lockEngine.makeVote(
        "validator-0",
        VotePhase::PREVOTE,
        blockB,
        1
    );

    assert(unlockVote.valid_round.has_value());
    assert(unlockVote.valid_round.value() == 1);

    assert(lockEngine.submitVote(unlockVote) == VoteResult::ACCEPTED);

    // ==========================================
    // TEST 10: Invalid valid_round
    // ==========================================

    Vote invalidValidRound;

    invalidValidRound.chain_id = "lab01";
    invalidValidRound.height = 1;
    invalidValidRound.round = 2;
    invalidValidRound.phase = VotePhase::PREVOTE;
    invalidValidRound.block_hash = blockB;
    invalidValidRound.validator_id = "validator-3";

    // First create a valid signature.
    invalidValidRound.sign(
        validators.find("validator-3")->key
    );

    // Then tamper with valid_round.
    invalidValidRound.valid_round = 2;

    // Signature verification must fail because
    // valid_round is part of the signed payload
    // and must be lower than round.
    assert(
        !invalidValidRound.verify(
            validators.find("validator-3")->key.address()
        )
    );

    // ==========================================
    // TEST 11: NIL PREVOTE and PRECOMMIT
    // ==========================================

    ConsensusEngine nilEngine(
        "lab01",
        validators,
        1
    );

    Vote nilPrevote = nilEngine.makeVote(
        "validator-0",
        VotePhase::PREVOTE,
        ""
    );

    assert(nilEngine.submitVote(nilPrevote) == VoteResult::ACCEPTED);

    Vote nilPrecommit = nilEngine.makeVote(
        "validator-0",
        VotePhase::PRECOMMIT,
        ""
    );

    assert(
        nilEngine.submitVote(nilPrecommit) ==
        VoteResult::ACCEPTED
    );

    assert(!nilEngine.isFinalized());

    // ==========================================
    // TEST 12: Wrong chain ID
    // ==========================================

    Vote wrongChainVote;

    wrongChainVote.chain_id = "another-chain";
    wrongChainVote.height = 1;
    wrongChainVote.round = 0;
    wrongChainVote.phase = VotePhase::PREVOTE;
    wrongChainVote.block_hash = blockA;
    wrongChainVote.validator_id = "validator-0";

    wrongChainVote.sign(
        validators.find("validator-0")->key
    );

    assert(
        nilEngine.submitVote(wrongChainVote) ==
        VoteResult::INVALID_SIGNATURE
    );

    
    // ==========================================
    // TEST 13: Finalized height rejects more votes
    // ==========================================

    bool finalizedVoteRejected = false;

    try {
        engine.makeVote(
            "validator-3",
            VotePhase::PREVOTE,
            blockA
        );
    } catch (const std::logic_error&) {
        finalizedVoteRejected = true;
    }

    assert(finalizedVoteRejected);

    std::cout << "All consensus safety tests passed\n";

    return 0;
}
