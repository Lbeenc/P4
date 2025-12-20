#include "codeGen.h"
#include "node.h"
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

/* ---------- expressions (MATCHES YOUR PARSER) ---------- */

static std::string genExpr(Node* n);
static std::string genM(Node* n);
static std::string genN(Node* n);
static std::string genR(Node* n);

static std::string genR(Node* n) {
    if (!n) return "";

    // R -> IDENT | NUM | ( exp )
    if (isId(n->tk1)) return n->tk1.instance;

    if (isNum(n->tk1)) {
        std::string t = newTemp();
        emit("LOAD " + n->tk1.instance);
        emit("STORE " + t);
        return t;
    }

    // ( exp )
    if (n->child1) return genExpr(n->child1);
    return "";
}

// N -> - N | R % N | R
// In your parser, '%' is stored in tk2 and RHS in child2, with the base R in child1.
static std::string genN(Node* n) {
    if (!n) return "";

    // unary -
    if (n->tk1.instance == "-") {
        std::string rhs = genN(n->child1);
        std::string t = newTemp();
        emit("LOAD 0");
        emit("SUB " + rhs);
        emit("STORE " + t);
        return t;
    }

    // base R is in child1
    std::string left = genR(n->child1);

    // percent
    if (n->tk2.instance == "%" && n->child2) {
        std::string right = genN(n->child2);
        std::string t = newTemp();
        emit("LOAD " + left);
        emit("MOD " + right);
        emit("STORE " + t);
        return t;
    }

    return left;
}

// M -> N * M | N
// In your parser, '*' is stored in tk1, left N in child1, right M in child2.
static std::string genM(Node* n) {
    if (!n) return "";

    std::string left = genN(n->child1);

    if (n->tk1.instance == "*" && n->child2) {
        std::string right = genM(n->child2);
        std::string t = newTemp();
        emit("LOAD " + left);
        emit("MULT " + right);
        emit("STORE " + t);
        return t;
    }

    return left;
}

// EXP -> M + EXP | M - EXP | M
// In your parser, +|- in tk1, left M in child1, right EXP in child2.
static std::string genExpr(Node* n) {
    if (!n) return "";

    std::string left = genM(n->child1);

    if ((n->tk1.instance == "+" || n->tk1.instance == "-") && n->child2) {
        std::string right = genExpr(n->child2);
        std::string t = newTemp();
        emit("LOAD " + left);
        emit((n->tk1.instance == "+") ? ("ADD " + right) : ("SUB " + right));
        emit("STORE " + t);
        return t;
    }

    return left;
}

/* ---------- conditionals (MATCHES YOUR PARSER) ---------- */

// Branch to lab when (leftVar op rightExp) is FALSE
static void genRelFalseFromParent(const std::string& op,
                                  const std::string& leftVar,
                                  Node* rightExp,
                                  const std::string& lab) {
    std::string right = genExpr(rightExp);

    // ACC = left - right
    emit("LOAD " + leftVar);
    emit("SUB " + right);

    // Branch when condition is FALSE
    if (op == ">") {
        // false if <= 0
        emit("BRNEG " + lab);
        emit("BRZERO " + lab);
    } else if (op == "<") {
        // false if >= 0
        emit("BRPOS " + lab);
        emit("BRZERO " + lab);
    } else if (op == ">=") {
        // false if < 0
        emit("BRNEG " + lab);
    } else if (op == "<=") {
        // false if > 0
        emit("BRPOS " + lab);
    } else if (op == "eq") {
        // false if != 0
        emit("BRNEG " + lab);
        emit("BRPOS " + lab);
    } else if (op == "neq") {
        // false if == 0
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
    // In many of your nodes, tk2 is the IDENT after "set"
    if (isId(n->tk2)) return n->tk2.instance;
    if (n->child1 && isId(n->child1->tk1)) return n->child1->tk1.instance;
    throw std::runtime_error("ASSIGN missing target");
}

static std::string getReadTarget(Node* n) {
    // In your READ node, tk2 is the IDENT after "read"
    if (isId(n->tk2)) return n->tk2.instance;
    if (n->child1 && isId(n->child1->tk1)) return n->child1->tk1.instance;
    throw std::runtime_error("READ missing identifier");
}

static void genStat(Node* n) {
    if (!n) return;

    switch (n->label) {
        case NodeType::READ: {
            emit("READ " + getReadTarget(n));
            break;
        }

        case NodeType::PRINT: {
            emit("WRITE " + genExpr(n->child1));
            break;
        }

        case NodeType::ASSIGN: {
            std::string id = getAssignTarget(n);

            // In your ASSIGN node, the RHS expression is typically child2.
            // Fallback to child1 if needed.
            Node* rhs = (n->child2 ? n->child2 : n->child1);
            std::string v = genExpr(rhs);

            emit("LOAD " + v);
            emit("STORE " + id);
            break;
        }

        case NodeType::COND: {
            // Parser layout (from your parser.cpp):
            // COND:
            //   tk2 = left IDENT
            //   child1 = REL (op in tk1)
            //   child2 = EXP (right)
            //   child3 = STAT (body)
            std::string end = newLabel("ENDIF");

            std::string left = n->tk2.instance;
            std::string op = (n->child1 ? n->child1->tk1.instance : "");

            genRelFalseFromParent(op, left, n->child2, end);
            genStat(n->child3);

            emit(end + " NOOP");
            break;
        }

        case NodeType::LOOP: {
            // LOOP:
            //   tk2 = left IDENT
            //   child1 = REL (op)
            //   child2 = EXP (right)
            //   child3 = STAT (body)
            std::string top = newLabel("WHILE");
            std::string end = newLabel("ENDWHILE");

            emit(top + " NOOP");

            std::string left = n->tk2.instance;
            std::string op = (n->child1 ? n->child1->tk1.instance : "");

            genRelFalseFromParent(op, left, n->child2, end);
            genStat(n->child3);

            emit("BR " + top);
            emit(end + " NOOP");
            break;
        }

        case NodeType::BLOCK: {
            // BLOCK: child2 = stats
            genStats(n->child2);
            break;
        }

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

    // program -> vars block (your root->child2 is block)
    if (root && root->child2)
        genStat(root->child2);

    // STORAGE FIRST
    for (const auto& v : vars) out << v << " 0\n";
    for (const auto& t : temps) out << t << " 0\n";

    // CODE
    for (const auto& c : code) out << c << "\n";

    out << "STOP\n";
}

