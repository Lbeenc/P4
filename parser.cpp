#include <iostream>
#include <cstdlib>
#include <string>
#include "parser.h"
#include "scanner.h"

// ---------- helpers ----------

static Token tk;   // current lookahead token

static void getNextToken() {
    tk = scanner();
}

static void parseError(const std::string& msg) {
    std::cout << "ERROR: " << msg << " at line " << tk.line << std::endl;
    std::exit(1);
}

static bool isKw(const Token& t, const std::string& kw) {
    return t.id == TokenID::KW_tk && t.instance == kw;
}

static bool isOp(const Token& t, const std::string& op) {
    return t.id == TokenID::OP_tk && t.instance == op;
}

static bool isId(const Token& t) {
    return t.id == TokenID::IDENT_tk;
}

static bool isNum(const Token& t) {
    return t.id == TokenID::NUM_tk;
}

// forward declarations
static Node* program();
static Node* vars();
static Node* varList();
static Node* block();
static Node* stats();
static Node* mStat();
static Node* stat();
static Node* readStmt();
static Node* printStmt();
static Node* cond();
static Node* loopStmt();
static Node* assign();
static Node* relational();
static Node* exp();
static Node* M();
static Node* N();
static Node* R();

// ---------- public entry ----------

Node* parser() {
    getNextToken();
    Node* root = program();

    if (tk.id != TokenID::EOFTk) {
        parseError("unexpected extra tokens after program");
    }
    return root;
}

// ---------- nonterminals ----------

// <program>  -> start <vars> <block> trats
static Node* program() {
    Node* n = createNode(NodeType::PROGRAM);

    if (!isKw(tk, "start")) {
        parseError("expected 'start' at beginning of program");
    }
    n->tk1 = tk;    // 'start'
    getNextToken();

    n->child1 = vars();
    n->child2 = block();

    if (!isKw(tk, "trats")) {
        parseError("expected 'trats' at end of program");
    }
    n->tk2 = tk;    // 'trats'
    getNextToken();

    return n;
}

// <vars> -> empty | var identifier ~ integer <varList> :
static Node* vars() {
    Node* n = createNode(NodeType::VARS);

    if (isKw(tk, "var")) {
        n->tk1 = tk;    // 'var'
        getNextToken();

        if (!isId(tk)) {
            parseError("expected identifier after 'var'");
        }
        n->tk2 = tk;    // identifier
        getNextToken();

        if (!isOp(tk, "~")) {
            parseError("expected '~' in variable declaration");
        }
        getNextToken();

        if (!isNum(tk)) {
            parseError("expected integer in variable declaration");
        }
        n->tk3 = tk;    // integer
        getNextToken();

        n->child1 = varList();

        if (!isOp(tk, ":")) {
            parseError("expected ':' after variable declarations");
        }
        getNextToken();
    }
    // else epsilon

    return n;
}

// <varList> -> identifier ~ integer <varList> | empty
static Node* varList() {
    if (!isId(tk)) {
        return nullptr; // epsilon
    }

    Node* n = createNode(NodeType::VARLIST);

    n->tk1 = tk;    // identifier
    getNextToken();

    if (!isOp(tk, "~")) {
        parseError("expected '~' in varList");
    }
    getNextToken();

    if (!isNum(tk)) {
        parseError("expected integer in varList");
    }
    n->tk2 = tk;    // integer
    getNextToken();

    n->child1 = varList();
    return n;
}

// <block> -> { <vars> <stats> }
static Node* block() {
    Node* n = createNode(NodeType::BLOCK);

    if (!isOp(tk, "{")) {
        parseError("expected '{' to start block");
    }
    n->tk1 = tk;
    getNextToken();

    n->child1 = vars();
    n->child2 = stats();

    if (!isOp(tk, "}")) {
        parseError("expected '}' to end block");
    }
    n->tk2 = tk;
    getNextToken();

    return n;
}

// <stats> -> <stat> <mStat>
static Node* stats() {
    Node* n = createNode(NodeType::STATS);
    n->child1 = stat();
    n->child2 = mStat();
    return n;
}

// <mStat> -> empty | <stat> <mStat>
static Node* mStat() {
    // FIRST(stat) = { read, print, {, if, while, set }
    if (isKw(tk, "read") || isKw(tk, "print") ||
        isOp(tk, "{")     || isKw(tk, "if")    ||
        isKw(tk, "while") || isKw(tk, "set")) {

        Node* n = createNode(NodeType::MSTAT);
        n->child1 = stat();
        n->child2 = mStat();
        return n;
    }
    return nullptr; // epsilon
}

// <stat> -> <read> | <print> | <block> | <cond> | <loop> | <assign>
static Node* stat() {
    Node* n = createNode(NodeType::STAT);

    if (isKw(tk, "read")) {
        n->child1 = readStmt();
    } else if (isKw(tk, "print")) {
        n->child1 = printStmt();
    } else if (isOp(tk, "{")) {
        n->child1 = block();
    } else if (isKw(tk, "if")) {
        n->child1 = cond();
    } else if (isKw(tk, "while")) {
        n->child1 = loopStmt();
    } else if (isKw(tk, "set")) {
        n->child1 = assign();
    } else {
        parseError("expected a statement (read/print/{/if/while/set)");
    }

    return n;
}

// <read> -> read identifier :
static Node* readStmt() {
    Node* n = createNode(NodeType::READ);

    n->tk1 = tk; // 'read'
    getNextToken();

    if (!isId(tk)) {
        parseError("expected identifier after 'read'");
    }
    n->tk2 = tk; // identifier
    getNextToken();

    if (!isOp(tk, ":")) {
        parseError("expected ':' after read statement");
    }
    getNextToken();

    return n;
}

// <print> -> print <exp> :
static Node* printStmt() {
    Node* n = createNode(NodeType::PRINT);

    n->tk1 = tk; // 'print'
    getNextToken();

    n->child1 = exp();

    if (!isOp(tk, ":")) {
        parseError("expected ':' after print statement");
    }
    getNextToken();

    return n;
}

// <cond> -> if [ identifier <relational> <exp> ] <stat>
static Node* cond() {
    Node* n = createNode(NodeType::COND);

    n->tk1 = tk; // 'if'
    getNextToken();

    if (!isOp(tk, "[")) {
        parseError("expected '[' after 'if'");
    }
    getNextToken();

    if (!isId(tk)) {
        parseError("expected identifier in condition");
    }
    n->tk2 = tk; // identifier
    getNextToken();

    n->child1 = relational();
    n->child2 = exp();

    if (!isOp(tk, "]")) {
        parseError("expected ']' at end of condition");
    }
    getNextToken();

    n->child3 = stat();
    return n;
}

// <loop> -> while [ identifier <relational> <exp> ] <stat>
static Node* loopStmt() {
    Node* n = createNode(NodeType::LOOP);

    n->tk1 = tk; // 'while'
    getNextToken();

    if (!isOp(tk, "[")) {
        parseError("expected '[' after 'while'");
    }
    getNextToken();

    if (!isId(tk)) {
        parseError("expected identifier in while condition");
    }
    n->tk2 = tk;
    getNextToken();

    n->child1 = relational();
    n->child2 = exp();

    if (!isOp(tk, "]")) {
        parseError("expected ']' at end of while condition");
    }
    getNextToken();

    n->child3 = stat();
    return n;
}

// <assign> -> set identifier ~ <exp> :
static Node* assign() {
    Node* n = createNode(NodeType::ASSIGN);

    n->tk1 = tk; // 'set'
    getNextToken();

    if (!isId(tk)) {
        parseError("expected identifier in assignment");
    }
    n->tk2 = tk; // identifier
    getNextToken();

    if (!isOp(tk, "~")) {
        parseError("expected '~' in assignment");
    }
    getNextToken();

    n->child1 = exp();

    if (!isOp(tk, ":")) {
        parseError("expected ':' after assignment");
    }
    getNextToken();

    return n;
}

// <relational> -> >  | >= | < | <= | eq | neq
static Node* relational() {
    Node* n = createNode(NodeType::REL);

    if (isOp(tk, ">") || isOp(tk, ">=") ||
        isOp(tk, "<") || isOp(tk, "<=") ||
        isOp(tk, "eq") || isOp(tk, "neq")) {

        n->tk1 = tk;
        getNextToken();
    } else {
        parseError("expected relational operator (>,>=,<,<=,eq,neq)");
    }
    return n;
}

// <exp> -> <M> + <exp> | <M> - <exp> | <M>
static Node* exp() {
    Node* n = createNode(NodeType::EXP);

    n->child1 = M();

    if (isOp(tk, "+") || isOp(tk, "-")) {
        n->tk1 = tk; // store + or -
        getNextToken();
        n->child2 = exp();
    }

    return n;
}

// <M> -> <N> * <M> | <N>
static Node* M() {
    Node* n = createNode(NodeType::M);

    n->child1 = N();

    if (isOp(tk, "*")) {
        n->tk1 = tk; // '*'
        getNextToken();
        n->child2 = M();
    }

    return n;
}

// <N> -> <R> % <N> | - <N> | <R>
static Node* N() {
    Node* n = createNode(NodeType::N);

    if (isOp(tk, "-")) {
        n->tk1 = tk; // unary -
        getNextToken();
        n->child1 = N();
    } else {
        n->child1 = R();
        if (isOp(tk, "%")) {
            n->tk2 = tk; // '%'
            getNextToken();
            n->child2 = N();
        }
    }

    return n;
}

// <R> -> ( <exp> ) | identifier | integer
static Node* R() {
    Node* n = createNode(NodeType::R);

    if (isOp(tk, "(")) {
        n->tk1 = tk; // '('
        getNextToken();
        n->child1 = exp();
        if (!isOp(tk, ")")) {
            parseError("expected ')' after expression");
        }
        n->tk2 = tk; // ')'
        getNextToken();
    } else if (isId(tk)) {
        n->tk1 = tk; // identifier
        getNextToken();
    } else if (isNum(tk)) {
        n->tk1 = tk; // integer
        getNextToken();
    } else {
        parseError("expected '(', identifier, or integer in <R>");
    }

    return n;
}
