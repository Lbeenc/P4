// scanner.cpp
#include "scanner.h"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <unordered_set>

// Everything private lives in an anonymous namespace
namespace {
    FILE* SRC = stdin;
    int LINE = 1;

    // Keywords per spec
    const std::unordered_set<std::string> KEYWORDS = {
        "start","trats","while","var","exit","read","print",
        "if","then","set","func","program"
    };

    // Single-char operators/delimiters
    const std::unordered_set<char> SINGLE = {
        '<','>','~',':',';','+','-','*','%','(',')','{','}','[',']'
    };

    int peekc() {
        int c = fgetc(SRC);
        if (c != EOF) ungetc(c, SRC);
        return c;
    }
    int getc_and_track() {
        int c = fgetc(SRC);
        if (c == '\n') LINE++;
        return c;
    }

    [[noreturn]] void lexError(const std::string& msg) {
        std::cerr << "LEXICAL ERROR: " << msg << " at line " << LINE << '\n';
        std::exit(EXIT_FAILURE);
    }

    bool isLetter(int c){ return std::isalpha(c); }
    bool isDigit(int c){ return std::isdigit(c); }

    void skipWhitespace() {
        for (;;) {
            int c = peekc();
            if (c==' ' || c=='\t' || c=='\r' || c=='\n') { getc_and_track(); continue; }
            // #...# comment (same line per assignment simplification)
            if (c == '#') {
                getc_and_track(); // consume '#'
                bool closed = false;
                int d;
                while ((d = getc_and_track()) != EOF) {
                    if (d == '#') { closed = true; break; }
                    if (d == '\n') break; // violates “same line” simplification
                }
                if (!closed) lexError("unterminated comment '#...#' on same line");
                continue;
            }
            break;
        }
    }

    // Try to read multi-char ops: eq, neq, <=, >=
    bool tryMultiOp(Token& tk) {
        int c = peekc();
        if (c == EOF) return false;

        // alphabetic ops: eq, neq
        if (std::isalpha(c)) {
            std::string buf;
            while (std::isalpha(peekc())) {
                buf.push_back(static_cast<char>(getc_and_track()));
                if (buf.size() > 3) break;
            }
            if (buf == "eq" || buf == "neq") {
                tk.id = TokenID::OP_tk; tk.instance = buf; tk.line = LINE;
                return true;
            }
            // push back if not an op
            for (int i = (int)buf.size()-1; i >= 0; --i) ungetc(buf[i], SRC);
            return false;
        }
        // <= or >=
        if (c == '<' || c == '>') {
            int first = getc_and_track();
            if (peekc() == '=') {
                getc_and_track();
                tk.id = TokenID::OP_tk;
                tk.instance = std::string(1, char(first)) + "=";
                tk.line = LINE;
                return true;
            }
            ungetc(first, SRC);
            return false;
        }
        return false;
    }

    Token lexIdentifier() {
        // must start with id_
        // we've not consumed anything yet
        int c1 = getc_and_track(); // should be 'i'
        int c2 = peekc(); if (c2=='d') getc_and_track(); else lexError("identifier must start with 'id_'");
        int c3 = peekc(); if (c3=='_') getc_and_track(); else lexError("identifier must start with 'id_'");

        std::string buf = "id_";
        while (isLetter(peekc()) || isDigit(peekc())) {
            buf.push_back(static_cast<char>(getc_and_track()));
            if ((int)buf.size() > 8) lexError("identifier length exceeds 8 characters");
        }
        return Token{TokenID::IDENT_tk, buf, LINE};
    }

    Token lexNumber() {
        std::string buf;
        while (isDigit(peekc())) {
            buf.push_back(static_cast<char>(getc_and_track()));
            if ((int)buf.size() > 8) lexError("integer length exceeds 8 digits");
        }
        return Token{TokenID::NUM_tk, buf, LINE};
    }

    Token lexWordOrError() {
        if (!std::isalpha(peekc())) lexError("unexpected non-letter where a word token was expected");

        // identifier prefix check
        if (peekc() == 'i') {
            int c1 = fgetc(SRC), c2 = fgetc(SRC), c3 = fgetc(SRC);
            if (c3 != EOF) ungetc(c3, SRC);
            if (c2 != EOF) ungetc(c2, SRC);
            if (c1 != EOF) ungetc(c1, SRC);
            if (c1=='i' && c2=='d' && c3=='_') return lexIdentifier();
        }

        std::string w;
        while (std::isalpha(peekc())) {
            w.push_back(static_cast<char>(getc_and_track()));
            if (w.size() > 32) break;
        }
        if (KEYWORDS.count(w)) return Token{TokenID::KW_tk, w, LINE};
        if (w == "eq" || w == "neq") return Token{TokenID::OP_tk, w, LINE};
        lexError(std::string("invalid word token '") + w + "'");
    }
} // end anonymous namespace

void initScanner(FILE* in) {
    SRC = in ? in : stdin;
    LINE = 1;
}

Token scanner() {
    skipWhitespace();

    int c = peekc();
    if (c == EOF) return Token{TokenID::EOFTk, "", LINE};

    Token tk{TokenID::ERR_tk, "", LINE};

    // multi-char ops first
    if (tryMultiOp(tk)) return tk;

    c = peekc();
    if (c == EOF) return Token{TokenID::EOFTk, "", LINE};

    if (std::isdigit(c)) return lexNumber();
    if (std::isalpha(c)) return lexWordOrError();

    if (SINGLE.count(static_cast<char>(c))) {
        getc_and_track();
        return Token{TokenID::OP_tk, std::string(1, static_cast<char>(c)), LINE};
    }

    // unknown character
    std::string bad(1, static_cast<char>(getc_and_track()));
    lexError(std::string("unrecognized character '") + bad + "'");
}
