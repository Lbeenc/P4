#include "codeGen.h"
#include <unordered_set>
#include <vector>
#include <string>
#include <stdexcept>

static int tempCount = 0;
static int labelCount = 0;

static std::unordered_set<std::string> vars;
static std::vector<std::string> temps;
static std::vector<std::string> code;

/* ---------- helpers ---------- */

static void emit(const std::string& s) {
    code.push_back(s);
}

static std::string newTemp() {
    std::string t = "_t" + std::to_string(tempCount++);
    temps.push_back(t);
    return t;
}

static std::string newLabel(const std::string& base) {
    return base + std::to_string(labelCount++);
}

static bool isId(const Token& t) {
    return t.id == TokenID::IDENT_tk;
}

static bool isNum(const Token& t) {
    return t.id == TokenID::NUM_tk;
}

/* ---------- variable collection ---------- */

static void collectVars(Node* n) {
    if (!n) return;

    if (isId(n->tk1)) vars.insert(n->tk1.instance);
    if (isId(n->tk2)) vars.insert(n->tk2.instance);
    if (isId(n->tk3)) vars.insert(n->tk3.instance);

    collectVars(n->child1);
    collectVars(n->child2);
    collectVars(n->child3);
    collectVars(n->child4);
}

/* ---------- expressions ---------- */

static std::string genExpr(Node* n);

static std::string genR(Node* n) {
    if (!n) return "";

    if (isId(n->tk1)) return n->tk1.instance;

    if (isNum(n->tk1)) {
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

    if (n->tk1.instance == "-") {
        std::string v = genN(n->child1);
        std::string t = newTemp();
        emit("LOAD 0");
        emit("SUB " + v);
        emit("STORE " + t);
        return t;
    }

    return genR(n);
}

static std::string genM(Node* n) {
    if (!n) return "";

    if (n->tk1.instance == "*") {
        std::string l = genExpr(n->child1);
        std::string r = genM(n->child2);
        std::string t = newTemp();
        emit("LOAD " + l);
        emit("MULT " + r);
        emit("STORE " + t);
        return t;
    }

    if (n->tk1.instance == "%") {
        std::string l = genExpr(n->child1);
        std::string r = genM(n->child2);
        std::string t = newTemp();
        emit("LOAD " + l);
        emit("MOD " + r);
        emit("STORE " + t);
        return t;
    }

    return genN(n);
}

static std::string genExpr(Node* n) {
    if (!n) return "";

    if (n->label == NodeType::R) return genR(n);
    if (n->label == NodeType::N) return genN(n);
    if (n->label == NodeType::M) return genM(n);

    if (n->tk1.instance == "+" || n->tk1.instance == "-") {
        std::string l = genExpr(n->child1);
        std::string r = genExpr(n->child2);
        std::string t = newTemp();
        emit("LOAD " + l);
        emit((n->tk1.instance == "+") ? "ADD " + r : "SUB " + r);
        emit("STORE " + t);
        return t;
    }

    return genExpr(n->child1);
}

/* ---------- conditionals ---------- */

static std::string ensureValue(const Token& t) {
    // IDENT -> return variable name
    if (isId(t)) return t.instance;

    // NUM -> load into temp so we always have a named operand
    if (isNum(t)) {
        std::string tmp = newTemp();
        emit("LOAD " + t.instance);
        emit("STORE " + tmp);
        return tmp;
    }

    return "";
}

static void genRelFalse(Node* n, const std::string& lab) {
    if (!n) throw std::runtime_error("REL node is null");

    std::string op = n->tk1.instance;

    std::string l, r;

    // Prefer children (for complex expressions like p4_9)
    if (n->child1) l = genExpr(n->child1);
    if (n->child2) r = genExpr(n->child2);

    // Fallback to tokens (for simple comparisons like id_1 > 0)
    if (l.empty()) l = ensureValue(n->tk2);
    if (r.empty()) r = ensureValue(n->tk3);

    if (l.empty() || r.empty()) {
        throw std::runtime_error("REL missing operand(s)");
    }

    emit("LOAD " + l);
    emit("SUB " + r);

    // Branch when condition is FALSE
    if (op == ">") {
        emit("BRNEG " + lab);
        emit("BRZERO " + lab);
    } else if (op == "<") {
        emit("BRPOS " + lab);
        emit("BRZERO " + lab);
    } else if (op == ">=") {
        emit("BRNEG " + lab);
    } else if (op == "<=") {
        emit("BRPOS " + lab);
    } else if (op == "eq") {
        emit("BRNEG " + lab);
        emit("BRPOS " + lab);
    } else if (op == "neq") {
        emit("BRZERO " + lab);
    } else {
        throw std::runtime_error("Unknown relational operator: " + op);
    }
}


/* ---------- statements ---------- */

static void genStat(Node* n);

static void genStats(Node* n) {
    if (!n) return;
    genStat(n->child1);
    genStats(n->child2);
}

static std::string getAssignTarget(Node* n) {
    if (isId(n->tk2)) return n->tk2.instance;
    if (n->child1 && isId(n->child1->tk1)) return n->child1->tk1.instance;
    throw std::runtime_error("ASSIGN missing target");
}

static std::string getReadTarget(Node* n) {
    if (isId(n->tk2)) return n->tk2.instance;
    if (n->child1 && isId(n->child1->tk1)) return n->child1->tk1.instance;
    throw std::runtime_error("READ missing identifier");
}

static void genStat(Node* n) {
    if (!n) return;

    switch (n->label) {
        case NodeType::READ:
            emit("READ " + getReadTarget(n));
            break;

        case NodeType::PRINT:
            emit("WRITE " + genExpr(n->child1));
            break;

        case NodeType::ASSIGN: {
            std::string id = getAssignTarget(n);
            std::string v = genExpr(n->child2 ? n->child2 : n->child1);
            emit("LOAD " + v);
            emit("STORE " + id);
            break;
        }

        case NodeType::COND: {
            std::string end = newLabel("ENDIF");
            genRelFalse(n->child2, end);
            genStat(n->child4);
            emit(end + " NOOP");     // ✅ FIXED
            break;
        }

        case NodeType::LOOP: {
            std::string top = newLabel("WHILE");
            std::string end = newLabel("ENDWHILE");
            emit(top + " NOOP");     // ✅ FIXED
            genRelFalse(n->child2, end);
            genStat(n->child4);
            emit("BR " + top);
            emit(end + " NOOP");     // ✅ FIXED
            break;
        }

        case NodeType::BLOCK:
            genStats(n->child2);
            break;

        default:
            genStat(n->child1);
    }
}

/* ---------- entry ---------- */

void generateTarget(Node* root, std::ostream& out) {
    vars.clear();
    temps.clear();
    code.clear();
    tempCount = 0;
    labelCount = 0;

    collectVars(root);

    if (root && root->child2)
        genStat(root->child2);

    for (const auto& v : vars) out << v << " 0\n";
    for (const auto& t : temps) out << t << " 0\n";
    for (const auto& c : code) out << c << "\n";

    out << "STOP\n";
}

