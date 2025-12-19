#include <iostream>
#include "printTree.h"
#include "token.h"

static const char* nodeLabel(NodeType t) {
    switch (t) {
        case NodeType::PROGRAM: return "program";
        case NodeType::VARS:    return "vars";
        case NodeType::VARLIST: return "varList";
        case NodeType::BLOCK:   return "block";
        case NodeType::STATS:   return "stats";
        case NodeType::MSTAT:   return "mStat";
        case NodeType::STAT:    return "stat";
        case NodeType::READ:    return "read";
        case NodeType::PRINT:   return "print";
        case NodeType::COND:    return "cond";
        case NodeType::LOOP:    return "loop";
        case NodeType::ASSIGN:  return "assign";
        case NodeType::REL:     return "relational";
        case NodeType::EXP:     return "exp";
        case NodeType::M:       return "M";
        case NodeType::N:       return "N";
        case NodeType::R:       return "R";
    }
    return "unknown";
}

// Convert TokenID to professor-required short label
static const char* shortGroup(TokenID id) {
    switch (id) {
        case TokenID::IDENT_tk: return "ID";
        case TokenID::NUM_tk:   return "INT";
        case TokenID::KW_tk:    return "KW";
        case TokenID::OP_tk:    return "OP";
        case TokenID::EOFTk:    return "EOF";
        default:                return nullptr;
    }
}

// Prints token like: ID:id_1:4
static void emit(const Token& tk) {
    const char* g = shortGroup(tk.id);
    if (!g) return;
    std::cout << " " << g << ":" << tk.instance << ":" << tk.line;
}

void printTree(Node* n, int depth) {
    if (!n) return;

    for (int i = 0; i < depth; ++i)
        std::cout << "  ";

    std::cout << nodeLabel(n->label);

    emit(n->tk1);
    emit(n->tk2);
    emit(n->tk3);

    std::cout << "\n";

    printTree(n->child1, depth + 1);
    printTree(n->child2, depth + 1);
    printTree(n->child3, depth + 1);
    printTree(n->child4, depth + 1);
}

