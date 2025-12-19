#include "codegen.h"
#include <fstream>
#include <unordered_set>
#include <vector>
#include <stdexcept>

// -------------------------
// Helpers for temps/labels
// -------------------------
static int tempCount = 0;
static int labelCount = 0;

static std::string newTemp() {
    return "_t" + std::to_string(++tempCount);
}

static std::string newLabel() {
    return "L" + std::to_string(++labelCount);
}

// -------------------------
// Token helpers (YOUR Node has tk1/tk2/tk3)
// -------------------------
static bool isEmptyTok(const Token& t) {
    return (t.id == TokenID::ERR_tk) || (t.id == TokenID::EOFTk) || t.instance.empty();
}

static bool isOp(const Token& t, const std::string& op) {
    return t.id == TokenID::OP_tk && t.instance == op;
}

static bool isKw(const Token& t, const std::string& kw) {
    return t.id == TokenID::KW_tk && t.instance == kw;
}

static bool isIdent(const Token& t) {
    return t.id == TokenID::IDENT_tk;
}

static bool isNum(const Token& t) {
    return t.id == TokenID::NUM_tk;
}

// Pick a “meaningful” token from a node (best-effort).
static const Token* pickTok(const Node* n) {
    if (!n) return nullptr;
    if (!isEmptyTok(n->tk1)) return &n->tk1;
    if (!isEmptyTok(n->tk2)) return &n->tk2;
    if (!isEmptyTok(n->tk3)) return &n->tk3;
    return nullptr;
}

// For nodes like READ/ASSIGN/VARS where an identifier is stored as tk2 often:
static const Token* pickIdentTok(const Node* n) {
    if (!n) return nullptr;
    if (isIdent(n->tk1)) return &n->tk1;
    if (isIdent(n->tk2)) return &n->tk2;
    if (isIdent(n->tk3)) return &n->tk3;
    return nullptr;
}

// -------------------------
// Storage collection
// -------------------------
static void collectVars(Node* n, std::unordered_set<std::string>& vars) {
    if (!n) return;

    // Your grammar defines variables under <vars> / <varList>.
    // In your tree printouts, vars nodes hold an ID token (often tk2).
    if (n->label == NodeType::VARS || n->label == NodeType::VARLIST) {
        const Token* idt = pickIdentTok(n);
        if (idt) vars.insert(idt->instance);
    }

    collectVars(n->child1, vars);
    collectVars(n->child2, vars);
    collectVars(n->child3, vars);
    collectVars(n->child4, vars);
}

// -------------------------
// Assembly emit helpers
// -------------------------
static void emit(std::ofstream& out, const std::string& s) {
    out << s << "\n";
}

static void emitLabel(std::ofstream& out, const std::string& lab) {
    out << lab << ": NOOP\n";
}

// -------------------------
// Expression codegen
// Convention: result ends in ACC.
// We store intermediate results in temps.
// -------------------------
static void genExpr(Node* n, std::ofstream& out, std::unordered_set<std::string>& temps);

static void genR(Node* n, std::ofstream& out) {
    // <R> -> ( <exp> ) | identifier | integer
    // Your tree output shows R holding either ID or INT in tk1.
    if (!n) return;

    if (n->child1) {
        // ( exp )
        genExpr(n->child1, out, *(new std::unordered_set<std::string>())); // should not happen in your tree, but safe
        return;
    }

    const Token* t = pickTok(n);
    if (!t) throw std::runtime_error("R node missing token");

    if (isIdent(*t)) {
        emit(out, "LOAD " + t->instance);
    } else if (isNum(*t)) {
        emit(out, "LOAD " + t->instance);
    } else {
        throw std::runtime_error("R node has unexpected token instance: " + t->instance);
    }
}

static void genN(Node* n, std::ofstream& out, std::unordered_set<std::string>& temps) {
    // <N> -> <R> % <N> | - <N> | <R>
    if (!n) return;

    // unary -
    if (isOp(n->tk1, "-") && n->child1) {
        genN(n->child1, out, temps);
        // ACC = -ACC
        // Typical VM: LOAD 0 ; SUB <temp> ... but we can do:
        std::string t = newTemp();
        temps.insert(t);
        emit(out, "STORE " + t);
        emit(out, "LOAD 0");
        emit(out, "SUB " + t);
        return;
    }

    // modulo: R % N (your print showed N OP:% then R then N)
    if (isOp(n->tk1, "%") && n->child1 && n->child2) {
        // Evaluate right, store, then left, then MOD (if VM lacks MOD, you must implement differently)
        std::string rt = newTemp();
        temps.insert(rt);
        genN(n->child2, out, temps);
        emit(out, "STORE " + rt);

        genR(n->child1, out);
        // If your VM does not support MOD, you must adjust.
        emit(out, "MOD " + rt);
        return;
    }

    // default: just R
    if (n->child1) genR(n->child1, out);
    else genR(n, out);
}

static void genM(Node* n, std::ofstream& out, std::unordered_set<std::string>& temps) {
    // <M> -> <N> * <M> | <N>
    if (!n) return;

    if (isOp(n->tk1, "*") && n->child1 && n->child2) {
        std::string rt = newTemp();
        temps.insert(rt);

        genM(n->child2, out, temps);
        emit(out, "STORE " + rt);

        genN(n->child1, out, temps);
        emit(out, "MULT " + rt);
        return;
    }

    if (n->child1) genN(n->child1, out, temps);
    else genN(n, out, temps);
}

static void genExpr(Node* n, std::ofstream& out, std::unordered_set<std::string>& temps) {
    // <exp> -> <M> + <exp> | <M> - <exp> | <M>
    if (!n) return;

    if ((isOp(n->tk1, "+") || isOp(n->tk1, "-")) && n->child1 && n->child2) {
        std::string rt = newTemp();
        temps.insert(rt);

        genExpr(n->child2, out, temps);
        emit(out, "STORE " + rt);

        genM(n->child1, out, temps);

        if (isOp(n->tk1, "+")) emit(out, "ADD " + rt);
        else emit(out, "SUB " + rt);

        return;
    }

    if (n->child1) genM(n->child1, out, temps);
    else genM(n, out, temps);
}

// -------------------------
// Relational helpers
// We'll generate: if !(left REL right) BR <falseLabel>
// Strategy:
//   compute left-right into ACC, then branch depending on rel
// -------------------------
static void genRelJumpFalse(Node* relNode, const std::string& falseLabel,
                           std::ofstream& out, std::unordered_set<std::string>& temps) {
    // relNode should store operator in tk1
    const Token* opTok = pickTok(relNode);
    if (!opTok) throw std::runtime_error("relational node missing operator");

    const std::string& op = opTok->instance;

    // ACC currently holds (left - right)
    // Branch to false depending on operator
    // NOTE: opcode names may vary by your VM.
    if (op == ">") {
        // true if ACC > 0, false if ACC <= 0
        emit(out, "BRZNEG " + falseLabel);
    } else if (op == ">=") {
        // true if ACC >= 0, false if ACC < 0
        emit(out, "BRNEG " + falseLabel);
    } else if (op == "<") {
        // true if ACC < 0, false if ACC >= 0
        emit(out, "BRZPOS " + falseLabel); // if your VM lacks BRZPOS, tell me the correct one
    } else if (op == "<=") {
        // true if ACC <= 0, false if ACC > 0
        emit(out, "BRPOS " + falseLabel);
    } else if (op == "eq") {
        // true if ACC == 0, false if ACC != 0
        emit(out, "BRPOS " + falseLabel);
        emit(out, "BRNEG " + falseLabel);
    } else if (op == "neq") {
        // true if ACC != 0, false if ACC == 0
        emit(out, "BRZERO " + falseLabel);
    } else {
        throw std::runtime_error("Unknown relational operator: " + op);
    }
}

// -------------------------
// Statement codegen
// -------------------------
static void genStat(Node* n, std::ofstream& out, std::unordered_set<std::string>& temps);

static void genStats(Node* n, std::ofstream& out, std::unordered_set<std::string>& temps) {
    if (!n) return;
    // <stats> -> <stat> <mStat>
    genStat(n->child1, out, temps);
    genStat(n->child2, out, temps);
}

static void genMStat(Node* n, std::ofstream& out, std::unordered_set<std::string>& temps) {
    if (!n) return;
    // <mStat> -> empty | <stat> <mStat>
    genStat(n->child1, out, temps);
    genMStat(n->child2, out, temps);
}

static void genBlock(Node* n, std::ofstream& out, std::unordered_set<std::string>& temps) {
    if (!n) return;
    // { <vars> <stats> }
    // vars don't generate code in global option; stats do
    genStats(n->child2, out, temps);
}

static void genRead(Node* n, std::ofstream& out) {
    const Token* idt = pickIdentTok(n);
    if (!idt) throw std::runtime_error("read missing identifier token");
    emit(out, "READ " + idt->instance);
}

static void genPrint(Node* n, std::ofstream& out, std::unordered_set<std::string>& temps) {
    // print <exp> :
    genExpr(n->child1, out, temps);
    emit(out, "WRITE"); // if your VM wants WRITE <var>, tell me and we’ll adjust
}

static void genAssign(Node* n, std::ofstream& out, std::unordered_set<std::string>& temps) {
    // set identifier ~ <exp> :
    const Token* idt = pickIdentTok(n);
    if (!idt) throw std::runtime_error("assign missing identifier token");
    genExpr(n->child1, out, temps);
    emit(out, "STORE " + idt->instance);
}

static void genCond(Node* n, std::ofstream& out, std::unordered_set<std::string>& temps) {
    // if [ identifier <relational> <exp> ] <stat>
    // We'll compute (id - exp) in ACC then branch false
    const Token* idt = pickIdentTok(n);
    if (!idt) throw std::runtime_error("cond missing identifier token");

    std::string falseLab = newLabel();
    std::string endLab = newLabel();

    // right
    std::string rt = newTemp();
    temps.insert(rt);
    genExpr(n->child2, out, temps);
    emit(out, "STORE " + rt);

    // left
    emit(out, "LOAD " + idt->instance);
    emit(out, "SUB " + rt);

    genRelJumpFalse(n->child1, falseLab, out, temps);

    // true-stat
    genStat(n->child3, out, temps);
    emit(out, "BR " + endLab);

    emitLabel(out, falseLab);
    emitLabel(out, endLab);
}

static void genLoop(Node* n, std::ofstream& out, std::unordered_set<std::string>& temps) {
    // while [ identifier <relational> <exp> ] <stat>
    const Token* idt = pickIdentTok(n);
    if (!idt) throw std::runtime_error("loop missing identifier token");

    std::string topLab = newLabel();
    std::string exitLab = newLabel();

    emitLabel(out, topLab);

    std::string rt = newTemp();
    temps.insert(rt);
    genExpr(n->child2, out, temps);
    emit(out, "STORE " + rt);

    emit(out, "LOAD " + idt->instance);
    emit(out, "SUB " + rt);

    genRelJumpFalse(n->child1, exitLab, out, temps);

    genStat(n->child3, out, temps);

    emit(out, "BR " + topLab);
    emitLabel(out, exitLab);
}

static void genStat(Node* n, std::ofstream& out, std::unordered_set<std::string>& temps) {
    if (!n) return;

    switch (n->label) {
        case NodeType::STAT:
            // wrapper node: first non-null child contains the real statement node
            genStat(n->child1, out, temps);
            genStat(n->child2, out, temps);
            genStat(n->child3, out, temps);
            genStat(n->child4, out, temps);
            break;

        case NodeType::READ:   genRead(n, out); break;
        case NodeType::PRINT:  genPrint(n, out, temps); break;
        case NodeType::ASSIGN: genAssign(n, out, temps); break;
        case NodeType::COND:   genCond(n, out, temps); break;
        case NodeType::LOOP:   genLoop(n, out, temps); break;
        case NodeType::BLOCK:  genBlock(n, out, temps); break;

        case NodeType::STATS:  genStats(n, out, temps); break;
        case NodeType::MSTAT:  genMStat(n, out, temps); break;

        default:
            // nodes that don't directly emit statements
            break;
    }
}

// -------------------------
// Entry
// -------------------------
void generateTarget(Node* root, const std::string& outFile) {
    if (!root) throw std::runtime_error("generateTarget: null root");

    std::ofstream out(outFile);
    if (!out) throw std::runtime_error("Could not open output file: " + outFile);

    std::unordered_set<std::string> vars;
    collectVars(root, vars);

    std::unordered_set<std::string> temps;

    // program structure: start <vars> <block> trats
    // Your PROGRAM node usually has the BLOCK as child2 or child3 depending on your parser.
    // We'll just traverse everything statement-like from the tree:
    genStat(root, out, temps);

    emit(out, "STOP");

    // Storage section
    emit(out, "");
    emit(out, ""); // spacing
    for (const auto& v : vars) {
        emit(out, v + " 0");
    }
    for (const auto& t : temps) {
        emit(out, t + " 0");
    }
}

