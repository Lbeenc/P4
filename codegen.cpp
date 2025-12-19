#include "codegen.h"
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

static std::ofstream out;
static int tempCount = 0;

// generate new temporary variable
static std::string newTemp() {
    return "t" + std::to_string(tempCount++);
}

/* Forward declarations */
static void gen(Node* n);
static std::string genExpr(Node* n);

/* Entry */
void generateCode(Node* root, const std::string& outName) {
    out.open(outName);
    if (!out) {
        std::cerr << "ERROR: could not open output file\n";
        exit(1);
    }

    out << "START\n";
    gen(root);
    out << "STOP\n";

    // Allocate temp variables
    for (int i = 0; i < tempCount; i++) {
        out << "t" << i << " 0\n";
    }

    out.close();
}

/* Recursive traversal */
static void gen(Node* n) {
    if (!n) return;

    switch (n->label) {
        case NodeType::PROGRAM:
            gen(n->child1);
            gen(n->child2);
            break;

        case NodeType::VARS:
            // allocate variable
            out << n->token.instance << " 0\n";
            gen(n->child1);
            break;

        case NodeType::BLOCK:
            gen(n->child1);
            gen(n->child2);
            break;

        case NodeType::STATS:
        case NodeType::MSTAT:
            gen(n->child1);
            gen(n->child2);
            break;

        case NodeType::READ:
            out << "READ " << n->token.instance << "\n";
            break;

        case NodeType::PRINT: {
            std::string t = genExpr(n->child1);
            out << "WRITE " << t << "\n";
            break;
        }

        case NodeType::ASSIGN: {
            std::string t = genExpr(n->child2);
            out << "LOAD " << t << "\n";
            out << "STORE " << n->child1->token.instance << "\n";
            break;
        }

        default:
            gen(n->child1);
            gen(n->child2);
            gen(n->child3);
            gen(n->child4);
            break;
    }
}

/* Expression code */
static std::string genExpr(Node* n) {
    if (!n) return "";

    if (n->label == NodeType::R) {
        if (n->token.id == TokenID::IDENT_tk ||
            n->token.id == TokenID::NUM_tk) {
            return n->token.instance;
        }
    }

    if (n->label == NodeType::EXP || n->label == NodeType::M || n->label == NodeType::N) {
        std::string left = genExpr(n->child1);
        std::string right = genExpr(n->child2);
        std::string temp = newTemp();

        out << "LOAD " << left << "\n";

        if (n->token.instance == "+")
            out << "ADD " << right << "\n";
        else if (n->token.instance == "-")
            out << "SUB " << right << "\n";
        else if (n->token.instance == "*")
            out << "MULT " << right << "\n";
        else if (n->token.instance == "%")
            out << "DIV " << right << "\n";

        out << "STORE " << temp << "\n";
        return temp;
    }

    return "";
}
