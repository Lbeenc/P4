#ifndef TOKEN_H
#define TOKEN_H
#include <string>


// Token groups (can be printed via tokenName[])
enum class TokenID {
IDENT_tk, // identifier (must start with id_)
NUM_tk, // integer (<= 8 digits)
KW_tk, // keyword
OP_tk, // operators and delimiters (instance holds exact lexeme)
EOFTk, // end of file
ERR_tk // internal use
};


static const char* tokenName[] = {
"Identifier",
"Number",
"Keyword",
"OperatorOrDelimiter",
"EOF",
"Error"
};


struct Token {
TokenID id;
std::string instance;
int line;
};


#endif // TOKEN_H
