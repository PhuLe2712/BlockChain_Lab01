#include "block.hpp"
#include "logger.hpp"
#include <iostream>
#include <filesystem>
int main(){
    try {
        std::filesystem::create_directories("logs");
        std::vector<unsigned char> seed(32,0x11);
        auto alice=crypto::keyPairFromSeed(seed);
        Transaction tx; tx.key=alice.address()+"/message"; tx.value="hello blockchain"; tx.nonce=0; tx.sign(alice);
        State state; std::string reason;
        if(!state.apply(tx,&reason)) throw std::runtime_error("Transaction rejected: "+reason);
        Block genesis; genesis.height=1; genesis.parent_hash="GENESIS"; genesis.state_root=state.root(); genesis.transactions.push_back(tx); genesis.sign(alice);
        if(!genesis.verifyHeader()) throw std::runtime_error("Block header signature invalid");
        Logger log("logs/run.log"); log.event(0,"node-0","TX_ACCEPTED",tx.id()); log.event(1,"node-0","BLOCK_CREATED",genesis.hash());
        std::cout<<"Lab01 core demo completed\n"<<"Sender: "<<alice.address()<<"\n"<<"State root: "<<state.root()<<"\n"<<"Block hash: "<<genesis.hash()<<"\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
