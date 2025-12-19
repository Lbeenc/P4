#ifndef NODE_H
#define NODE_H

#include "token.h"

enum class NodeType {
    PROGRAM,
    VARS,
    VARLIST,
    BLOCK,
    STATS,
    MSTAT,
    STAT,
    READ,
    PRINT,
    COND,
    LOOP,
    ASSIGN,
    REL,
    EXP,
    M,
    N,
    R
};

struct Node {
    NodeType label;

    // store up to 3 tokens per node (ID, keyword, op, int, etc.)
    Token tk1;
    Token tk2;
    Token tk3;

    Node* child1;
    Node* child2;
    Node* child3;
    Node* child4;
};

Node* createNode(NodeType t);

#endif // NODE_H
