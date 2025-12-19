#include "codeGen.h"
#include <iostream>
#include <sstream>
#include <vector>

static std::ofstream out;
static int tempCount = 0;

// create temp variable names
std::string newTemp() {
    return "_t" + std::to_string(tempCount++);
}

// forward declarations
std::string genExpr(Node* n);
void gen(Node* n);

// -----------------------------------
void generateTarget(Node* root, const std::string& outFile) {
    out.open(outFile);
    if (!out) {
        std::cerr << "ERROR: could not open output file\n";
        exit(1);
    }

    // header
    out << "START\n";
    gen(root);
    out << "STOP\n";

    // allocate temps
    for (int i = 0; i < tempCount; i++) {
        out << "_t" << i << " 0\n";
    }

    out.close();
}

// -----------------------------------
void gen(Node* n) {
    if (!n) return;

    switch (n->label) {

    case NodeType::PROGRAM:
        gen(n->child1);
        gen(n->child2);
        break;

    case NodeType::VARS:
        out << n->tk1.instance << " 0\n";
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
        out << "READ " << n->tk1.instance << "\n";
        break;

    case NodeType::PRINT: {
        std::string r = genExpr(n->child1);
        out << "WRITE " << r << "\n";
        break;
    }

    case NodeType::ASSIGN: {
        std::string r = genExpr(n->child1);
        out << "LOAD " << r << "\n";
        out << "STORE " << n->tk1.instance << "\n";
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

// -----------------------------------
std::string genExpr(Node* n) {
    if (!n) return "";

    // identifier
    if (n->label == NodeType::R &&
        n->tk1.id == TokenID::IDENT_tk) {
        return n->tk1.instance;
    }

    // integer
    if (n->label == NodeType::R &&
        n->tk1.id == TokenID::NUM_tk) {
        std::string t = newTemp();
        out << "LOAD " << n->tk1.instance << "\n";
        out << "STORE " << t << "\n";
        return t;
    }

    // binary expression
    std::string left = genExpr(n->child1);
    std::string right = genExpr(n->child2);
    std::string t = newTemp();

    out << "LOAD " << left << "\n";

    if (n->tk1.instance == "+") out << "ADD ";
    else if (n->tk1.instance == "-") out << "SUB ";
    else if (n->tk1.instance == "*") out << "MULT ";
    else if (n->tk1.instance == "%") out << "DIV ";

    out << right << "\n";
    out << "STORE " << t << "\n";

    return t;
}
