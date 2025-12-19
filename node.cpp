#include "node.h"

Node* createNode(NodeType t) {
    Node* n = new Node{
        t,
        Token{TokenID::ERR_tk, "", 0},
        Token{TokenID::ERR_tk, "", 0},
        Token{TokenID::ERR_tk, "", 0},
        nullptr, nullptr, nullptr, nullptr
    };
    return n;
}
