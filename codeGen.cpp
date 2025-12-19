#include "codeGen.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "token.h"
#include "node.h"

// ---------- helpers ----------
static bool isId(const Token& t)    { return t.id == TokenID::IDENT_tk; }
static bool isNum(const Token& t)   { return t.id == TokenID::NUM_tk; }
static bool isOp(const Token& t)    { return t.id == TokenID::OP_tk; }
static bool isKw(const Token& t)    { return t.id == TokenID::KW_tk; }

static bool hasTok(const Token& t) {
    // Assuming default Token has ERR/empty instance when unused.
    // If your Token differs, adjust this check.
    return !t.instance.empty() || t.id == TokenID::NUM_tk || t.id == TokenID::IDENT_tk || t.id == TokenID::KW_tk || t.id == TokenID::OP_tk;
}

struct Emitter {
    std::ofstream out;
    int labelCounter = 0;
    int tempCounter = 0;

    std::unordered_set<std::string> vars;   // user vars
    std::vector<std::string> temps;         // compiler temps in order created

    explicit Emitter(const std::string& file) : out(file) {
        if (!out) throw std::runtime_error("cannot open output file '" + file + "'");
    }

    std::string newLabel(const std::string& base) {
        return base + std::to_string(labelCounter++);
    }

    std::string newTemp() {
        std::string t = "_t" + std::to_string(tempCounter++);
        temps.push_back(t);
        return t;
    }

    void emit(const std::string& line) {
        out << line << "\n";
    }

    void emitLabel(const std::string& lab) {
        out << lab << ":\n";
    }
};

// Collect declared variables from <vars>/<varList> nodes
static void collectVars(Node* n, std::unordered_set<std::string>& vars) {
    if (!n) return;

    // In your grammar, variable definitions appear under <vars> and <varList>
    if (n->label == NodeType::VARS || n->label == NodeType::VARLIST) {
        if (isId(n->tk1)) vars.insert(n->tk1.instance);
        if (isId(n->tk2)) vars.insert(n->tk2.instance);
        if (isId(n->tk3)) vars.insert(n->tk3.instance);
    }

    collectVars(n->child1, vars);
    collectVars(n->child2, vars);
    collectVars(n->child3, vars);
    collectVars(n->child4, vars);
}

// Forward decls for generation
static void genNode(Node* n, Emitter& e);
static std::string genExpr(Node* n, Emitter& e);

// ---------- expression generation ----------
// Returns a variable name (user var or temp) that holds the computed value.
static std::string genR(Node* n, Emitter& e) {
    // <R> -> { <exp> } | identifier | integer
    // Your parser likely stores the meaningful token in tk1
    if (!n) throw std::runtime_error("internal: null <R>");

    if (isId(n->tk1)) {
        return n->tk1.instance;
    }
    if (isNum(n->tk1)) {
        std::string t = e.newTemp();
        // Common VM style: LOADI value ; STORE name
        e.emit("LOADI " + n->tk1.instance);
        e.emit("STORE " + t);
        return t;
    }

    // if token is '{', child1 should be exp
    if (isOp(n->tk1) && n->tk1.instance == "{") {
        return genExpr(n->child1, e);
    }

    // fallback: search children
    if (n->child1) return genExpr(n->child1, e);
    throw std::runtime_error("internal: malformed <R>");
}

static std::string genN(Node* n, Emitter& e) {
    // <N> -> <R> % <N> | - <N> | <R>
    if (!n) throw std::runtime_error("internal: null <N>");

    // unary -
    if (isOp(n->tk1) && n->tk1.instance == "-") {
        std::string rhs = genN(n->child1, e);
        std::string t = e.newTemp();
        e.emit("LOADI 0");
        e.emit("SUB " + rhs);
        e.emit("STORE " + t);
        return t;
    }

    // modulo case typically stores '%' in tk1 or tk2 depending on your parser
    // We'll detect '%' in tk1/tk2/tk3.
    auto hasMod = (isOp(n->tk1) && n->tk1.instance == "%") ||
                  (isOp(n->tk2) && n->tk2.instance == "%") ||
                  (isOp(n->tk3) && n->tk3.instance == "%");

    if (hasMod && n->child1 && n->child2) {
        std::string left = genR(n->child1, e);
        std::string right = genN(n->child2, e);
        std::string t = e.newTemp();
        e.emit("LOAD " + left);
        e.emit("MOD " + right);
        e.emit("STORE " + t);
        return t;
    }

    // just <R>
    if (n->child1) return genR(n->child1, e);
    return genR(n, e);
}

static std::string genM(Node* n, Emitter& e) {
    // <M> -> <N> * <M> | <N>
    if (!n) throw std::runtime_error("internal: null <M>");

    bool hasMul = (isOp(n->tk1) && n->tk1.instance == "*") ||
                  (isOp(n->tk2) && n->tk2.instance == "*") ||
                  (isOp(n->tk3) && n->tk3.instance == "*");

    if (hasMul && n->child1 && n->child2) {
        std::string left = genN(n->child1, e);
        std::string right = genM(n->child2, e);
        std::string t = e.newTemp();
        e.emit("LOAD " + left);
        e.emit("MULT " + right);
        e.emit("STORE " + t);
        return t;
    }

    if (n->child1) return genN(n->child1, e);
    return genN(n, e);
}

static std::string genExpr(Node* n, Emitter& e) {
    // <exp> -> <M> + <exp> | <M> - <exp> | <M>
    if (!n) throw std::runtime_error("internal: null <exp>");

    bool hasPlus = (isOp(n->tk1) && n->tk1.instance == "+") ||
                   (isOp(n->tk2) && n->tk2.instance == "+") ||
                   (isOp(n->tk3) && n->tk3.instance == "+");

    bool hasMinus = (isOp(n->tk1) && n->tk1.instance == "-") ||
                    (isOp(n->tk2) && n->tk2.instance == "-") ||
                    (isOp(n->tk3) && n->tk3.instance == "-");

    if ((hasPlus || hasMinus) && n->child1 && n->child2) {
        std::string left = genM(n->child1, e);
        std::string right = genExpr(n->child2, e);
        std::string t = e.newTemp();

        e.emit("LOAD " + left);
        if (hasPlus) e.emit("ADD " + right);
        else         e.emit("SUB " + right);
        e.emit("STORE " + t);
        return t;
    }

    if (n->child1) return genM(n->child1, e);
    return genM(n, e);
}

// ---------- relational / control ----------
static void genRelJumpFalse(Node* relNode, const std::string& leftVar, const std::string& rightVar, const std::string& falseLabel, Emitter& e) {
    // <relational> -> > | >= | < | <= | eq | neq
    // Strategy: compute (left - right) then branch depending on operator.
    // We'll store difference in a temp and test it.

    std::string diff = e.newTemp();
    e.emit("LOAD " + leftVar);
    e.emit("SUB " + rightVar);
    e.emit("STORE " + diff);

    std::string op;
    if (relNode) {
        if (isOp(relNode->tk1)) op = relNode->tk1.instance;
        else if (isOp(relNode->tk2)) op = relNode->tk2.instance;
        else if (isOp(relNode->tk3)) op = relNode->tk3.instance;
        else if (isKw(relNode->tk1)) op = relNode->tk1.instance; // eq/neq sometimes tokenized as word
        else if (isKw(relNode->tk2)) op = relNode->tk2.instance;
        else if (isKw(relNode->tk3)) op = relNode->tk3.instance;
    }

    // Branch-on-condition instructions vary by VM.
    // These names are common. If your VM differs, swap here.
    if (op == ">") {
        // true when diff > 0; false when diff <= 0
        e.emit("BRZNEG " + falseLabel);
    } else if (op == ">=") {
        // true when diff >= 0; false when diff < 0
        e.emit("BRNEG " + falseLabel);
    } else if (op == "<") {
        // true when diff < 0; false when diff >= 0
        e.emit("BRZPOS " + falseLabel);
    } else if (op == "<=") {
        // true when diff <= 0; false when diff > 0
        e.emit("BRPOS " + falseLabel);
    } else if (op == "eq") {
        // true when diff == 0; false otherwise
        e.emit("BRNZ " + falseLabel); // if your VM doesn't have BRNZ, tell me.
    } else if (op == "neq") {
        // true when diff != 0; false when diff == 0
        e.emit("BRZERO " + falseLabel);
    } else {
        throw std::runtime_error("unknown relational operator in codegen: '" + op + "'");
    }
}

// ---------- statements ----------
static void genAssign(Node* n, Emitter& e) {
    // <assign> -> set identifier ~ <exp> :
    // id usually stored in tk2/tk1 depending on parser. We’ll search.
    std::string id;
    if (isId(n->tk1)) id = n->tk1.instance;
    else if (isId(n->tk2)) id = n->tk2.instance;
    else if (isId(n->tk3)) id = n->tk3.instance;
    else if (n->child1 && isId(n->child1->tk1)) id = n->child1->tk1.instance;

    if (id.empty()) throw std::runtime_error("internal: assign missing identifier");

    // exp likely child1 or child2; search a child labeled EXP
    Node* expNode = nullptr;
    if (n->child1 && n->child1->label == NodeType::EXP) expNode = n->child1;
    else if (n->child2 && n->child2->label == NodeType::EXP) expNode = n->child2;
    else if (n->child3 && n->child3->label == NodeType::EXP) expNode = n->child3;
    else expNode = n->child1 ? n->child1 : n->child2;

    std::string rhs = genExpr(expNode, e);
    e.emit("LOAD " + rhs);
    e.emit("STORE " + id);
}

static void genRead(Node* n, Emitter& e) {
    std::string id;
    if (isId(n->tk1)) id = n->tk1.instance;
    else if (isId(n->tk2)) id = n->tk2.instance;
    else if (isId(n->tk3)) id = n->tk3.instance;
    else if (n->child1 && isId(n->child1->tk1)) id = n->child1->tk1.instance;

    if (id.empty()) throw std::runtime_error("internal: read missing identifier");
    e.emit("READ " + id);
}

static void genPrint(Node* n, Emitter& e) {
    // print <exp> :
    Node* expNode = nullptr;
    if (n->child1 && n->child1->label == NodeType::EXP) expNode = n->child1;
    else if (n->child2 && n->child2->label == NodeType::EXP) expNode = n->child2;
    else if (n->child3 && n->child3->label == NodeType::EXP) expNode = n->child3;
    else expNode = n->child1 ? n->child1 : n->child2;

    std::string v = genExpr(expNode, e);
    e.emit("WRITE " + v);
}

static void genCond(Node* n, Emitter& e) {
    // <cond> -> if [ identifier <relational> <exp> ] <stat>
    // From grammar: left is identifier; right is <exp>
    // We'll find id token in children/toks, rel node child?, exp node child?, stat child?
    std::string id;
    if (isId(n->tk1)) id = n->tk1.instance;
    else if (isId(n->tk2)) id = n->tk2.instance;
    else if (isId(n->tk3)) id = n->tk3.instance;

    Node* relNode = nullptr;
    Node* expNode = nullptr;
    Node* statNode = nullptr;

    // Search children by label
    Node* kids[4] = {n->child1, n->child2, n->child3, n->child4};
    for (Node* k : kids) {
        if (!k) continue;
        if (!relNode && k->label == NodeType::REL) relNode = k;
        if (!expNode && k->label == NodeType::EXP) expNode = k;
        if (!statNode && k->label == NodeType::STAT) statNode = k;
    }

    // Identifier sometimes stored under a child <R> or direct token
    if (id.empty()) {
        for (Node* k : kids) {
            if (k && isId(k->tk1)) { id = k->tk1.instance; break; }
        }
    }
    if (id.empty()) throw std::runtime_error("internal: if missing identifier");
    if (!expNode) throw std::runtime_error("internal: if missing expression");
    if (!statNode) throw std::runtime_error("internal: if missing stat");

    std::string rhs = genExpr(expNode, e);

    std::string Lfalse = e.newLabel("L_if_false_");
    genRelJumpFalse(relNode, id, rhs, Lfalse, e);
    genNode(statNode, e);
    e.emitLabel(Lfalse);
}

static void genLoop(Node* n, Emitter& e) {
    // <loop> -> while [ identifier <relational> <exp> ] <stat>
    std::string id;
    if (isId(n->tk1)) id = n->tk1.instance;
    else if (isId(n->tk2)) id = n->tk2.instance;
    else if (isId(n->tk3)) id = n->tk3.instance;

    Node* relNode = nullptr;
    Node* expNode = nullptr;
    Node* statNode = nullptr;

    Node* kids[4] = {n->child1, n->child2, n->child3, n->child4};
    for (Node* k : kids) {
        if (!k) continue;
        if (!relNode && k->label == NodeType::REL) relNode = k;
        if (!expNode && k->label == NodeType::EXP) expNode = k;
        if (!statNode && k->label == NodeType::STAT) statNode = k;
    }
    if (id.empty()) {
        for (Node* k : kids) {
            if (k && isId(k->tk1)) { id = k->tk1.instance; break; }
        }
    }

    if (id.empty()) throw std::runtime_error("internal: while missing identifier");
    if (!expNode) throw std::runtime_error("internal: while missing expression");
    if (!statNode) throw std::runtime_error("internal: while missing stat");

    std::string Ltop = e.newLabel("L_while_top_");
    std::string Lend = e.newLabel("L_while_end_");

    e.emitLabel(Ltop);
    std::string rhs = genExpr(expNode, e);
    genRelJumpFalse(relNode, id, rhs, Lend, e);
    genNode(statNode, e);
    e.emit("BR " + Ltop);
    e.emitLabel(Lend);
}

static void genStat(Node* n, Emitter& e) {
    // <stat> -> <read> | <print> | <block> | <cond> | <loop> | <assign>
    if (!n) return;

    Node* k = n->child1 ? n->child1 : n; // sometimes stat node wraps
    switch (k->label) {
        case NodeType::READ:   genRead(k, e); break;
        case NodeType::PRINT:  genPrint(k, e); break;
        case NodeType::BLOCK:  genNode(k, e); break;
        case NodeType::COND:   genCond(k, e); break;
        case NodeType::LOOP:   genLoop(k, e); break;
        case NodeType::ASSIGN: genAssign(k, e); break;
        default:
            // If wrapper, search children
            genNode(k, e);
            break;
    }
}

// generic traversal
static void genNode(Node* n, Emitter& e) {
    if (!n) return;

    switch (n->label) {
        case NodeType::PROGRAM:
            genNode(n->child1, e); // vars
            genNode(n->child2, e); // block
            e.emit("STOP");
            return;

        case NodeType::BLOCK:
            // { <vars> <stats> }
            genNode(n->child1, e);
            genNode(n->child2, e);
            return;

        case NodeType::STATS:
            genNode(n->child1, e);
            genNode(n->child2, e);
            return;

        case NodeType::MSTAT:
            genNode(n->child1, e);
            genNode(n->child2, e);
            return;

        case NodeType::STAT:
            genStat(n, e);
            return;

        case NodeType::READ:   genRead(n, e); return;
        case NodeType::PRINT:  genPrint(n, e); return;
        case NodeType::ASSIGN: genAssign(n, e); return;
        case NodeType::COND:   genCond(n, e); return;
        case NodeType::LOOP:   genLoop(n, e); return;

        default:
            // default: just traverse children left-to-right
            genNode(n->child1, e);
            genNode(n->child2, e);
            genNode(n->child3, e);
            genNode(n->child4, e);
            return;
    }
}

void generateTarget(Node* root, const std::string& outFile) {
    Emitter e(outFile);

    // Gather declared variables for storage
    collectVars(root, e.vars);

    // Generate code
    genNode(root, e);

    // Emit storage (globals + temps)
    e.emit("");
    e.emit("// storage");
    for (const auto& v : e.vars) {
        e.emit(v + " 0");
    }
    for (const auto& t : e.temps) {
        e.emit(t + " 0");
    }
}
