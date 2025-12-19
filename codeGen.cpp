#include "codeGen.h"
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <vector>
#include <stdexcept>

static int tempCount = 0;
static int labelCount = 0;

static std::vector<std::string> temps;
static std::unordered_set<std::string> vars;

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

static void collectVars(Node* n) {
    if (!n) return;

    // Any ID token that appears under <vars>/<varList> should already exist in those nodes.
    // But to be safe, collect every IDENT token we see.
    if (isIdTok(n->tk1)) vars.insert(n->tk1.instance);
    if (isIdTok(n->tk2)) vars.insert(n->tk2.instance);
    if (isIdTok(n->tk3)) vars.insert(n->tk3.instance);

    collectVars(n->child1);
    collectVars(n->child2);
    collectVars(n->child3);
    collectVars(n->child4);
}

/*
  Expression codegen strategy (stack-ish using temps):
  - genExpr returns the name of a temp or a variable holding the expression value.
  - For NUM: loads constant into temp via LOAD <num>, STORE <temp>
  - For ID: just returns ID name (or loads into temp if you want; this works if VM supports LOAD var)
*/
static std::string genExpr(Node* n, std::ostream& out);

// NodeType mapping depends on your parser building:
// EXP handles +/-, M handles *, N handles % and unary -, R handles () / id / int.
static std::string genR(Node* n, std::ostream& out) {
    if (!n) return "";

    // If your parser stores a single token for R in tk1:
    if (isIdTok(n->tk1)) return n->tk1.instance;

    if (isNumTok(n->tk1)) {
        std::string t = newTemp();
        out << "LOAD " << n->tk1.instance << "\n";
        out << "STORE " << t << "\n";
        return t;
    }

    // Parenthesized: usually child1 is <exp>
    if (n->child1) return genExpr(n->child1, out);

    return "";
}

static std::string genN(Node* n, std::ostream& out) {
    if (!n) return "";

    // unary '-' is often stored as tk1 instance == "-" OR child pattern
    if (n->tk1.id == TokenID::OP_tk && n->tk1.instance == "-") {
        // assume child1 holds the <N> or <R>
        std::string rhs = n->child1 ? genN(n->child1, out) : genR(n->child1, out);
        std::string t = newTemp();
        out << "LOAD 0\n";
        out << "SUB " << rhs << "\n";
        out << "STORE " << t << "\n";
        return t;
    }

    // % chain: child1 <R>, tk1 "%"?, child2 <N> etc — depends on your tree.
    // Common build: N -> R % N OR R
    if (n->tk1.id == TokenID::OP_tk && n->tk1.instance == "%") {
        // if you built as: child1=R, child2=N
        std::string left = genR(n->child1, out);
        std::string right = genN(n->child2, out);
        std::string t = newTemp();
        out << "LOAD " << left << "\n";
        out << "MOD " << right << "\n";
        out << "STORE " << t << "\n";
        return t;
    }

    // fallback: if has child1 it’s usually R
    if (n->child1 && n->label == NodeType::N) {
        // could be R or nested
        return genExpr(n->child1, out);
    }

    return genR(n, out);
}

static std::string genM(Node* n, std::ostream& out) {
    if (!n) return "";

    // M -> N * M | N
    if (n->tk1.id == TokenID::OP_tk && n->tk1.instance == "*") {
        // child1=N, child2=M
        std::string left = genExpr(n->child1, out);
        std::string right = genM(n->child2, out);
        std::string t = newTemp();
        out << "LOAD " << left << "\n";
        out << "MULT " << right << "\n";
        out << "STORE " << t << "\n";
        return t;
    }

    // otherwise just N
    if (n->child1) return genExpr(n->child1, out);
    return genN(n, out);
}

static std::string genExpr(Node* n, std::ostream& out) {
    if (!n) return "";

    // If this node is literally R/N/M, dispatch
    if (n->label == NodeType::R) return genR(n, out);
    if (n->label == NodeType::N) return genN(n, out);
    if (n->label == NodeType::M) return genM(n, out);

    // EXP -> M + EXP | M - EXP | M
    if (n->tk1.id == TokenID::OP_tk && (n->tk1.instance == "+" || n->tk1.instance == "-")) {
        std::string left = genM(n->child1, out);
        std::string right = genExpr(n->child2, out);
        std::string t = newTemp();
        out << "LOAD " << left << "\n";
        if (n->tk1.instance == "+") out << "ADD " << right << "\n";
        else out << "SUB " << right << "\n";
        out << "STORE " << t << "\n";
        return t;
    }

    // otherwise child1 likely M
    if (n->child1) return genExpr(n->child1, out);

    return "";
}

// relational: >, <, >=, <=, eq, neq
static void genRelJumpFalse(Node* relNode, const std::string& falseLabel, std::ostream& out) {
    // Expect: relNode stores operator in tk1.instance and children hold LHS/RHS expressions
    std::string op = relNode->tk1.instance;
    std::string left = genExpr(relNode->child1, out);
    std::string right = genExpr(relNode->child2, out);

    // Convention:
    // LOAD left; SUB right; then branch based on result.
    out << "LOAD " << left << "\n";
    out << "SUB " << right << "\n";

    if (op == "eq") {
        out << "BRNEG " << falseLabel << "\n";
        out << "BRPOS " << falseLabel << "\n";
    } else if (op == "neq") {
        // if result == 0 => false
        std::string ok = newLabel("L");
        out << "BRNEG " << ok << "\n";
        out << "BRPOS " << ok << "\n";
        out << "BR " << falseLabel << "\n";
        out << ok << ": NOOP\n";
    } else if (op == "<") {
        // left-right < 0 is true; false if >=0
        out << "BRPOS " << falseLabel << "\n";
        out << "BRZPOS " << falseLabel << "\n"; // if your VM has BRZPOS; if not, ignore/remove
    } else if (op == ">") {
        // true if >0; false if <=0
        out << "BRNEG " << falseLabel << "\n";
        out << "BRZERO " << falseLabel << "\n";
    } else if (op == "<=") {
        // false if >0
        out << "BRPOS " << falseLabel << "\n";
    } else if (op == ">=") {
        // false if <0
        out << "BRNEG " << falseLabel << "\n";
    } else {
        // If your VM uses different branches, adjust here.
        throw std::runtime_error("Unknown relational operator: " + op);
    }
}

static void genStat(Node* n, std::ostream& out);

static void genStats(Node* n, std::ostream& out) {
    if (!n) return;
    // stats -> stat mStat
    if (n->child1) genStat(n->child1, out);
    if (n->child2) genStats(n->child2, out);
}

static void genStat(Node* n, std::ostream& out) {
    if (!n) return;

    switch (n->label) {
        case NodeType::READ: {
            // read identifier :
            std::string id = n->tk1.instance; // usually ID stored in tk1
            out << "READ " << id << "\n";
            break;
        }
        case NodeType::PRINT: {
            // print <exp> :
            std::string v = genExpr(n->child1, out);
            out << "WRITE " << v << "\n";
            break;
        }
        case NodeType::ASSIGN: {
            // set identifier ~ <exp> :
            std::string id = n->tk1.instance;
            std::string v = genExpr(n->child1, out);
            out << "LOAD " << v << "\n";
            out << "STORE " << id << "\n";
            break;
        }
        case NodeType::BLOCK: {
            // { <vars> <stats> }
            if (n->child2) genStats(n->child2, out);
            break;
        }
        case NodeType::COND: {
            // if [ identifier <rel> <exp> ] <stat>
            std::string endLabel = newLabel("ENDIF");
            // your tree likely stores relational node as child2 and exp as child3
            Node* rel = n->child2;
            genRelJumpFalse(rel, endLabel, out);
            if (n->child4) genStat(n->child4, out);
            out << endLabel << ": NOOP\n";
            break;
        }
        case NodeType::LOOP: {
            // while [ identifier <rel> <exp> ] <stat>
            std::string topLabel = newLabel("WHILE");
            std::string endLabel = newLabel("ENDWHILE");
            out << topLabel << ": NOOP\n";
            Node* rel = n->child2;
            genRelJumpFalse(rel, endLabel, out);
            if (n->child4) genStat(n->child4, out);
            out << "BR " << topLabel << "\n";
            out << endLabel << ": NOOP\n";
            break;
        }
        default: {
            // wrappers: STAT nodes may wrap child1
            if (n->child1) genStat(n->child1, out);
            break;
        }
    }
}

void generateTarget(Node* root, std::ostream& out) {
    temps.clear();
    vars.clear();
    tempCount = 0;
    labelCount = 0;

    // gather vars so we can emit storage at the end
    collectVars(root);

    // program -> start vars block trats
    // typically root PROGRAM: child1=vars, child2=block
    if (root && root->child2) {
        genStat(root->child2, out);
    }

    out << "STOP\n";

    // storage (user vars + temps)
    for (const auto& v : vars) out << v << " 0\n";
    for (const auto& t : temps) out << t << " 0\n";
}
