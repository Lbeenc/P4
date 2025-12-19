#include "codeGen.h"
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>

static int tempCount = 0;
static int labelCount = 0;

static std::vector<std::string> temps;
static std::unordered_set<std::string> vars;
static std::vector<std::string> code;   // <-- BUFFERED CODE

// helpers
static std::string newTemp() {
    std::string t = "_t" + std::to_string(tempCount++);
    temps.push_back(t);
    return t;
}

static std::string newLabel(const std::string& base) {
    return base + std::to_string(labelCount++);
}

static bool isIdTok(const Token& t) { return t.id == TokenID::IDENT_tk; }
static bool isNumTok(const Token& t) { return t.id == TokenID::NUM_tk; }

static void emit(const std::string& s) {
    code.push_back(s);
}

static void collectVars(Node* n) {
    if (!n) return;

    if (isIdTok(n->tk1)) vars.insert(n->tk1.instance);
    if (isIdTok(n->tk2)) vars.insert(n->tk2.instance);
    if (isIdTok(n->tk3)) vars.insert(n->tk3.instance);

    collectVars(n->child1);
    collectVars(n->child2);
    collectVars(n->child3);
    collectVars(n->child4);
}

// ====================== EXPRESSIONS ======================

static std::string genExpr(Node* n);

static std::string genR(Node* n) {
    if (!n) return "";

    if (isIdTok(n->tk1)) return n->tk1.instance;

    if (isNumTok(n->tk1)) {
        std::string t = newTemp();
        emit("LOAD " + n->tk1.instance);
        emit("STORE " + t);
        return t;
    }

    if (n->child1) return genExpr(n->child1);
    return "";
}

static std::string genN(Node* n) {
    if (!n) return "";

    if (n->tk1.id == TokenID::OP_tk && n->tk1.instance == "-") {
        std::string rhs = genN(n->child1);
        std::string t = newTemp();
        emit("LOAD 0");
        emit("SUB " + rhs);
        emit("STORE " + t);
        return t;
    }

    if (n->child1) return genExpr(n->child1);
    return genR(n);
}

static std::string genM(Node* n) {
    if (!n) return "";

    if (n->tk1.id == TokenID::OP_tk && n->tk1.instance == "*") {
        std::string left = genExpr(n->child1);
        std::string right = genM(n->child2);
        std::string t = newTemp();
        emit("LOAD " + left);
        emit("MULT " + right);
        emit("STORE " + t);
        return t;
    }

    if (n->child1) return genExpr(n->child1);
    return genN(n);
}

static std::string genExpr(Node* n) {
    if (!n) return "";

    if (n->label == NodeType::R) return genR(n);
    if (n->label == NodeType::N) return genN(n);
    if (n->label == NodeType::M) return genM(n);

    if (n->tk1.id == TokenID::OP_tk &&
        (n->tk1.instance == "+" || n->tk1.instance == "-")) {

        std::string left = genM(n->child1);
        std::string right = genExpr(n->child2);
        std::string t = newTemp();

        emit("LOAD " + left);
        if (n->tk1.instance == "+") emit("ADD " + right);
        else emit("SUB " + right);
        emit("STORE " + t);
        return t;
    }

    if (n->child1) return genExpr(n->child1);
    return "";
}

// ====================== RELATIONAL ======================

static void genRelJumpFalse(Node* relNode, const std::string& falseLabel) {
    std::string op = relNode->tk1.instance;
    std::string left = genExpr(relNode->child1);
    std::string right = genExpr(relNode->child2);

    emit("LOAD " + left);
    emit("SUB " + right);

    if (op == "eq") {
        emit("BRNEG " + falseLabel);
        emit("BRPOS " + falseLabel);
    } else if (op == "neq") {
        std::string ok = newLabel("L");
        emit("BRNEG " + ok);
        emit("BRPOS " + ok);
        emit("BR " + falseLabel);
        emit(ok + ": NOOP");
    } else if (op == "<") {
        emit("BRPOS " + falseLabel);
        emit("BRZERO " + falseLabel);
    } else if (op == ">") {
        emit("BRNEG " + falseLabel);
        emit("BRZERO " + falseLabel);
    } else if (op == "<=") {
        emit("BRPOS " + falseLabel);
    } else if (op == ">=") {
        emit("BRNEG " + falseLabel);
    } else {
        throw std::runtime_error("Unknown relational operator");
    }
}

// ====================== STATEMENTS ======================

static void genStat(Node* n);

static void genStats(Node* n) {
    if (!n) return;
    if (n->child1) genStat(n->child1);
    if (n->child2) genStats(n->child2);
}

static void genStat(Node* n) {
    if (!n) return;

    switch (n->label) {
        case NodeType::READ: {
            emit("READ " + n->tk1.instance);
            break;
        }
        case NodeType::PRINT: {
            std::string v = genExpr(n->child1);
            emit("WRITE " + v);
            break;
        }
        case NodeType::ASSIGN: {
            std::string id = n->tk1.instance;
            std::string v = genExpr(n->child1);
            emit("LOAD " + v);
            emit("STORE " + id);
            break;
        }
        case NodeType::BLOCK: {
            if (n->child2) genStats(n->child2);
            break;
        }
        case NodeType::COND: {
            std::string endLabel = newLabel("ENDIF");
            genRelJumpFalse(n->child2, endLabel);
            if (n->child4) genStat(n->child4);
            emit(endLabel + ": NOOP");
            break;
        }
        case NodeType::LOOP: {
            std::string top = newLabel("WHILE");
            std::string end = newLabel("ENDWHILE");
            emit(top + ": NOOP");
            genRelJumpFalse(n->child2, end);
            if (n->child4) genStat(n->child4);
            emit("BR " + top);
            emit(end + ": NOOP");
            break;
        }
        default: {
            if (n->child1) genStat(n->child1);
        }
    }
}

// ====================== ENTRY POINT ======================

void generateTarget(Node* root, std::ostream& out) {
    temps.clear();
    vars.clear();
    code.clear();
    tempCount = 0;
    labelCount = 0;

    collectVars(root);

    if (root && root->child2)
        genStat(root->child2);

    // -------- STORAGE FIRST --------
    for (const auto& v : vars)
        out << v << " 0\n";
    for (const auto& t : temps)
        out << t << " 0\n";

    // -------- EXECUTABLE CODE --------
    for (const auto& c : code)
        out << c << "\n";

    out << "STOP\n";
}
