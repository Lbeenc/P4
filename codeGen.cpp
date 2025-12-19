#include "codeGen.h"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include <iostream>

// ==============================
//  IMPORTANT ADAPTER SECTION
// ==============================
// This file assumes your Node looks like:
//
//   struct Node {
//      NodeType label;
//      Token tokens[4];
//      int tokenCount;
//      Node* child1; child2; child3; child4;
//   };
//
// If your Node stores tokens differently (t1/t2/t3/t4 etc),
// ONLY change these helper functions to match your node.h.
// ------------------------------

static int tokenCount(const Node* n) {
    return n ? n->tokenCount : 0;
}

static const Token& tokenAt(const Node* n, int i) {
    return n->tokens[i];
}

// Find first token matching a given TokenID in this node (returns nullptr if none)
static const Token* findToken(const Node* n, TokenID id) {
    if (!n) return nullptr;
    for (int i = 0; i < tokenCount(n); ++i) {
        const Token& t = tokenAt(n, i);
        if (t.id == id) return &t;
    }
    return nullptr;
}

// Find first operator token (OP_tk) in this node (returns nullptr if none)
static const Token* findOpToken(const Node* n) {
    return findToken(n, TokenID::OP_tk);
}

// ==============================
//  CODEGEN IMPLEMENTATION
// ==============================

namespace {

struct ConstDef {
    std::string name;
    int value;
};

struct Gen {
    std::ofstream out;

    int tempCounter = 0;
    int labelCounter = 0;
    int constCounter = 0;

    std::unordered_set<std::string> userVars;     // program variables
    std::vector<std::string> temps;               // _t0, _t1, ...
    std::vector<ConstDef> consts;                 // _c0, _c1, ... with init values

    explicit Gen(const std::string& outFile) : out(outFile) {
        if (!out) throw std::runtime_error("Could not open output file: " + outFile);
    }

    void emit(const std::string& line) {
        out << line << "\n";
    }

    std::string newTemp() {
        std::ostringstream ss;
        ss << "_t" << tempCounter++;
        std::string name = ss.str();
        temps.push_back(name);
        return name;
    }

    std::string newLabel() {
        std::ostringstream ss;
        ss << "_L" << labelCounter++;
        return ss.str();
    }

    // Create a constant “storage” name initialized at end of file.
    std::string newConst(int value) {
        std::ostringstream ss;
        ss << "_c" << constCounter++;
        std::string name = ss.str();
        consts.push_back(ConstDef{name, value});
        return name;
    }

    // ------------------------------
    // Tree scans (collect vars)
    // ------------------------------
    void collectVars(Node* n) {
        if (!n) return;

        // In your grammar, <vars> / <varList> contain identifier definitions.
        // We add any IDENT token found under VARS/VARLIST nodes.
        if (n->label == NodeType::VARS || n->label == NodeType::VARLIST) {
            const Token* idTok = findToken(n, TokenID::IDENT_tk);
            if (idTok) {
                userVars.insert(idTok->instance);
            }
            // some of your <vars>/<varList> nodes can contain more than one ID
            // (depending on how you built the tree). We’ll also scan all tokens:
            for (int i = 0; i < tokenCount(n); ++i) {
                const Token& t = tokenAt(n, i);
                if (t.id == TokenID::IDENT_tk) userVars.insert(t.instance);
            }
        }

        collectVars(n->child1);
        collectVars(n->child2);
        collectVars(n->child3);
        collectVars(n->child4);
    }

    // ------------------------------
    // Expression generation
    // Leaves result in ACC
    // ------------------------------
    void genExp(Node* n);
    void genM(Node* n);
    void genN(Node* n);
    void genR(Node* n);

    void genExp(Node* n) {
        if (!n) return;

        // <exp> -> <M> + <exp> | <M> - <exp> | <M>
        // We assume operator (if any) is stored as OP token in this EXP node.
        const Token* op = findOpToken(n);

        if (!op) {
            // just <M>
            genM(n->child1);
            return;
        }

        // Right-first strategy:
        // eval right -> temp
        // eval left -> ACC
        // ADD/SUB temp
        std::string rightTmp = newTemp();
        genExp(n->child2);                 // right side
        emit("STORE " + rightTmp);

        genM(n->child1);                   // left side into ACC

        if (op->instance == "+") emit("ADD " + rightTmp);
        else if (op->instance == "-") emit("SUB " + rightTmp);
        else throw std::runtime_error("Unknown <exp> operator: " + op->instance);
    }

    void genM(Node* n) {
        if (!n) return;

        // <M> -> <N> * <M> | <N>
        const Token* op = findOpToken(n);
        if (!op) {
            genN(n->child1);
            return;
        }

        std::string rightTmp = newTemp();
        genM(n->child2);
        emit("STORE " + rightTmp);

        genN(n->child1);

        if (op->instance == "*") emit("MULT " + rightTmp);
        else throw std::runtime_error("Unknown <M> operator: " + op->instance);
    }

    void genN(Node* n) {
        if (!n) return;

        // <N> -> <R> % <N> | - <N> | <R>
        const Token* op = findOpToken(n);

        if (!op) {
            genR(n->child1);
            return;
        }

        if (op->instance == "%") {
            // In your P4 slide: % is integer division
            std::string rightTmp = newTemp();
            genN(n->child2);
            emit("STORE " + rightTmp);

            genR(n->child1);
            emit("DIV " + rightTmp);
            return;
        }

        if (op->instance == "-") {
            // unary negation: -(child)
            genN(n->child1); // child result in ACC
            // multiply by -1 using const storage
            std::string cneg1 = newConst(-1);
            emit("MULT " + cneg1);
            return;
        }

        throw std::runtime_error("Unknown <N> operator: " + op->instance);
    }

    void genR(Node* n) {
        if (!n) return;

        // <R> -> ( <exp> ) | identifier | integer
        // We detect by token presence.

        const Token* idTok = findToken(n, TokenID::IDENT_tk);
        if (idTok) {
            emit("LOAD " + idTok->instance);
            return;
        }

        const Token* numTok = findToken(n, TokenID::NUM_tk);
        if (numTok) {
            int val = std::stoi(numTok->instance);
            std::string c = newConst(val);
            emit("LOAD " + c);
            return;
        }

        // parentheses case: child1 is exp
        if (n->child1) {
            genExp(n->child1);
            return;
        }

        throw std::runtime_error("Bad <R> node: missing ID/INT/(exp)");
    }

    // ------------------------------
    // Relational / control flow
    // ------------------------------
    // We assume <relational> node stores the operator as either:
    // - OP token with instance one of: >, <, >=, <=
    // - or KW/OP token instance: eq, neq
    std::string getRelOp(Node* relNode) {
        if (!relNode) return "";

        // try OP token first
        const Token* op = findOpToken(relNode);
        if (op) return op->instance;

        // some scanners treat eq/neq as OP too; but in case they’re KW:
        for (int i = 0; i < tokenCount(relNode); ++i) {
            const Token& t = tokenAt(relNode, i);
            if (t.instance == "eq" || t.instance == "neq") return t.instance;
        }

        // last resort: check any token instance
        const Token* any = findToken(relNode, TokenID::KW_tk);
        if (any) return any->instance;

        return "";
    }

    // ACC contains (left - right). Emit branch(es) to labelFalse when condition is FALSE.
    void branchOnFalse(const std::string& rel, const std::string& labelFalse) {
        if (rel == ">") {
            // true when ACC > 0; false when <= 0
            emit("BRNEG " + labelFalse);
            emit("BRZERO " + labelFalse);
        } else if (rel == "<") {
            // true when ACC < 0; false when >= 0
            emit("BRPOS " + labelFalse);
            emit("BRZERO " + labelFalse);
        } else if (rel == ">=") {
            // true when ACC >= 0; false when < 0
            emit("BRNEG " + labelFalse);
        } else if (rel == "<=") {
            // true when ACC <= 0; false when > 0
            emit("BRPOS " + labelFalse);
        } else if (rel == "eq") {
            // true when ACC == 0; false when != 0
            emit("BRNEG " + labelFalse);
            emit("BRPOS " + labelFalse);
        } else if (rel == "neq") {
            // true when ACC != 0; false when == 0
            emit("BRZERO " + labelFalse);
        } else {
            throw std::runtime_error("Unknown relational operator: " + rel);
        }
    }

    // ------------------------------
    // Statement generation
    // ------------------------------
    void genStat(Node* n);
    void genStats(Node* n);
    void genBlock(Node* n);
    void genCond(Node* n);
    void genLoop(Node* n);
    void genRead(Node* n);
    void genPrint(Node* n);
    void genAssign(Node* n);

    void genProgram(Node* n) {
        if (!n) return;

        // program -> start <vars> <block> trats
        // child1 vars, child2 block (based on your tree)
        if (n->child1) { /* vars ignored runtime */ }
        if (n->child2) genBlock(n->child2);
    }

    void genBlock(Node* n) {
        if (!n) return;
        // <block> -> { <vars> <stats> }
        // child1 vars, child2 stats (based on your tree)
        if (n->child2) genStats(n->child2);
    }

    void genStats(Node* n) {
        if (!n) return;
        // <stats> -> <stat> <mStat>
        genStat(n->child1);
        genStat(n->child2); // child2 is mStat wrapper; genStat handles recursion
    }

    void genStat(Node* n) {
        if (!n) return;

        // STAT node is usually just a wrapper with one child
        if (n->label == NodeType::STAT) {
            genStat(n->child1);
            return;
        }

        // MSTAT: empty | <stat> <mStat>
        if (n->label == NodeType::MSTAT) {
            genStat(n->child1);
            genStat(n->child2);
            return;
        }

        switch (n->label) {
            case NodeType::READ:   genRead(n);   break;
            case NodeType::PRINT:  genPrint(n);  break;
            case NodeType::ASSIGN: genAssign(n); break;
            case NodeType::COND:   genCond(n);   break;
            case NodeType::LOOP:   genLoop(n);   break;
            case NodeType::BLOCK:  genBlock(n);  break;
            default:
                // ignore nodes that are non-runtime wrappers
                break;
        }
    }

    void genRead(Node* n) {
        // <read> -> read identifier :
        const Token* idTok = findToken(n, TokenID::IDENT_tk);
        if (!idTok) throw std::runtime_error("read missing identifier");
        emit("READ " + idTok->instance);
    }

    void genPrint(Node* n) {
        // <print> -> print <exp> :
        std::string tmp = newTemp();
        genExp(n->child1);
        emit("STORE " + tmp);
        emit("WRITE " + tmp);
    }

    void genAssign(Node* n) {
        // <assign> -> set identifier ~ <exp> :
        const Token* idTok = findToken(n, TokenID::IDENT_tk);
        if (!idTok) throw std::runtime_error("assign missing identifier");
        genExp(n->child1);
        emit("STORE " + idTok->instance);
    }

    void genCond(Node* n) {
        // <cond> -> if [ identifier <relational> <exp> ] <stat>
        // expected: ID token in this node, child1=REL, child2=EXP, child3=STAT
        const Token* idTok = findToken(n, TokenID::IDENT_tk);
        if (!idTok) throw std::runtime_error("cond missing identifier");

        std::string rel = getRelOp(n->child1);
        if (rel.empty()) throw std::runtime_error("cond missing relational operator");

        std::string tmp = newTemp();
        genExp(n->child2);
        emit("STORE " + tmp);

        emit("LOAD " + idTok->instance);
        emit("SUB " + tmp);

        std::string Lfalse = newLabel();
        branchOnFalse(rel, Lfalse);

        genStat(n->child3);

        emit(Lfalse + ": NOOP");
    }

    void genLoop(Node* n) {
        // <loop> -> while [ identifier <relational> <exp> ] <stat>
        const Token* idTok = findToken(n, TokenID::IDENT_tk);
        if (!idTok) throw std::runtime_error("loop missing identifier");

        std::string rel = getRelOp(n->child1);
        if (rel.empty()) throw std::runtime_error("loop missing relational operator");

        std::string Ltop = newLabel();
        std::string Lend = newLabel();

        emit(Ltop + ": NOOP");

        std::string tmp = newTemp();
        genExp(n->child2);
        emit("STORE " + tmp);

        emit("LOAD " + idTok->instance);
        emit("SUB " + tmp);

        branchOnFalse(rel, Lend);

        genStat(n->child3);

        emit("BR " + Ltop);
        emit(Lend + ": NOOP");
    }

    // ------------------------------
    // Final assembly layout
    // ------------------------------
    void finishAndWriteStorage() {
        emit("STOP");

        // program vars
        for (const auto& v : userVars) {
            emit(v + " 0");
        }

        // temps
        for (const auto& t : temps) {
            emit(t + " 0");
        }

        // constants
        for (const auto& c : consts) {
            emit(c.name + " " + std::to_string(c.value));
        }
    }
};

} // namespace

void generateTarget(Node* root, const std::string& outAsmFile) {
    Gen g(outAsmFile);

    // collect all user vars from <vars>/<varList> nodes (global storage option)
    g.collectVars(root);

    // generate code from tree
    if (root && root->label == NodeType::PROGRAM) g.genProgram(root);
    else g.genProgram(root); // still try

    // append storage at end
    g.finishAndWriteStorage();
}
