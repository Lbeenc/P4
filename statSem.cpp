// statSem.cpp
#include "statSem.h"
#include "token.h"
#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>

struct VarEntry {
    std::string name;   // id_x
    int defLine;        // line where defined
    bool used;          // ever used?
};

static std::vector<VarEntry> STV;   // global symbol table (global option)

// ------- helpers for reporting -------

[[noreturn]] static void errorP3(const std::string& msg) {
    std::cout << "ERROR in P3: " << msg << '\n';
    std::exit(EXIT_FAILURE);
}

static void warningP3(const std::string& msg) {
    std::cout << "WARNING in P3: " << msg << '\n';
}

// ------- STV API (insert / verify / checkVars) -------

static void stInsert(const Token& tk) {
    // tk must be an identifier token
    std::string name = tk.instance;
    int line = tk.line;

    // check redeclaration
    for (const auto& e : STV) {
        if (e.name == name) {
            errorP3("variable '" + name + "' redefined on line " +
                    std::to_string(line) +
                    " (first defined on line " +
                    std::to_string(e.defLine) + ")");
        }
    }

    VarEntry e;
    e.name = name;
    e.defLine = line;
    e.used = false;
    STV.push_back(e);
}

static void stUse(const Token& tk) {
    std::string name = tk.instance;
    int line = tk.line;

    for (auto& e : STV) {
        if (e.name == name) {
            e.used = true;
            return;
        }
    }

    // not found -> used before definition (global option)
    errorP3("variable '" + name + "' used before definition on line " +
            std::to_string(line));
}

static void checkVars() {
    for (const auto& e : STV) {
        if (!e.used) {
            warningP3("variable '" + e.name + "' defined on line " +
                      std::to_string(e.defLine) +
                      " but never used");
        }
    }
}

// ------- tree traversal -------

// Any identifier token on a VARS or VARLIST node is a *definition*.
// Any identifier token anywhere else is a *use*.

static void handleDefsInNode(Node* n) {
    if (!n) return;
    if (n->tk1.id == TokenID::IDENT_tk) stInsert(n->tk1);
    if (n->tk2.id == TokenID::IDENT_tk) stInsert(n->tk2);
    if (n->tk3.id == TokenID::IDENT_tk) stInsert(n->tk3);
}

static void handleUsesInNode(Node* n) {
    if (!n) return;
    if (n->tk1.id == TokenID::IDENT_tk) stUse(n->tk1);
    if (n->tk2.id == TokenID::IDENT_tk) stUse(n->tk2);
    if (n->tk3.id == TokenID::IDENT_tk) stUse(n->tk3);
}

static void walk(Node* n) {
    if (!n) return;

    // Definitions:
    if (n->label == NodeType::VARS || n->label == NodeType::VARLIST) {
        handleDefsInNode(n);
    } else {
        // Any other identifier is a use:
        handleUsesInNode(n);
    }

    // preorder traversal of children
    walk(n->child1);
    walk(n->child2);
    walk(n->child3);
    walk(n->child4);
}

void staticSemantics(Node* root) {
    STV.clear();
    walk(root);
    checkVars();
    // if no error was thrown, static semantics is OK (maybe warnings already printed)
}
