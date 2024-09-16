#include <iostream>
#include <deque>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "wlp4data.h"

typedef std::string Type;
typedef std::string Symbol;
typedef std::string TokenKind;
typedef std::string TokenLexeme;
typedef std::string Production;
typedef std::string Register;
typedef std::string Identifier;

const Type INT = "int";
const Type INT_STAR = "int*";

std::unordered_set<Production> loadProductions() {
    std::stringstream ss(WLP4_CFG);
    std::unordered_set<Production> productions;
    std::string line;
    getline(ss, line);  // discard ".CFG"
    while (getline(ss, line)) {
        productions.insert(line);
    }
    return productions;
}

const std::unordered_set<Production> WLP4_PRODUCTIONS = loadProductions();

bool isProduction(std::string s) {
    if (WLP4_PRODUCTIONS.find(s) != WLP4_PRODUCTIONS.end()) return true;
    else return false;
}

std::vector<std::string> splitString(std::string s, std::string delim = " ") {
    std::vector<std::string> parsed;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delim)) != std::string::npos) {
        token = s.substr(0, pos);
        parsed.push_back(token);
        s.erase(0, pos + 1);
    }
    parsed.push_back(s);
    return parsed;
}

std::string joinVector(const std::vector<std::string>& v, std::string sep = " ") {
    std::string out = "";
    for (std::string s : v) {
        out += s;
        out += sep;
    }
    if (out.size() > 0) out.pop_back();
    return out;
}

struct Token {
    Token() = default;
    Token(TokenKind kind, TokenLexeme lexeme): kind(kind), lexeme(lexeme) {}
    TokenKind kind;
    TokenLexeme lexeme;
};

class TreeNode {
    public:
        std::vector<TreeNode*> children;

        TreeNode(Symbol symbol, Production production = "", Token token = Token())
        : symbol(symbol)
        , production(production)
        , token(token) {}

        TreeNode(const TreeNode& other)
        : symbol(other.symbol)
        , production(other.production)
        , token(other.token) {
            for (TreeNode* child : this->children) {
                delete child;
            }
            this->children.clear();

            for (TreeNode* child : other.children) {
                TreeNode* childCopy = new TreeNode(*child);
                this->children.push_back(childCopy);
            }
        }

        ~TreeNode() {
            for (TreeNode* child : this->children) {
                delete child;
            }
        }

        TreeNode& operator=(TreeNode other) {
            swap(*this, other);
            return *this;
        }

        friend void swap(TreeNode& a, TreeNode& b) {
            std::swap(a.symbol, b.symbol);
            std::swap(a.production, b.production);
            std::swap(a.token, b.token);
            std::swap(a.children, b.children);
        }

        friend TreeNode* loadParseTree(std::istream& stream);

        bool N() {
            if (this->production != "") return true;
            else return false;
        }

        bool T() {
            return !this->N();
        }

        Type getType() {
            return this->type;
        }

        Symbol getSymbol() {
            return this->symbol;
        }

        Production getProduction() {
            if (!this->N()) {
                std::cerr << "ERROR: called getProduction() on Terminal node" << std::endl;
                throw std::exception();
            }
            return this->production;
        }

        Token getToken() {
            if (!this->T()) {
                std::cerr << "ERROR: called getToken() on NonTerminal node" << std::endl;
                throw std::exception();
            }
            return this->token;
        }

        std::vector<Token> getLeaves() {
            std::vector<Token> leaves;
            for (TreeNode* child : this->children) {
                if (child->children.size() > 0) {
                    // Non-Leaf
                    std::vector<Token> childLeaves = child->getLeaves();
                    leaves.insert(leaves.end(), childLeaves.begin(), childLeaves.end());
                } else {
                    // Leaf
                    if (child->getSymbol() == ".EMPTY") continue;
                    leaves.push_back(child->getToken());
                }
            }
            return leaves;
        }

        std::vector<TreeNode*> getChildSymbolNodes(Symbol sym) {
            std::vector<TreeNode*> nodes;
            for (TreeNode* child : this->children) {
                if (child->symbol == sym) {
                    nodes.push_back(child);
                } else {
                    std::vector<TreeNode*> childNodes = child->getChildSymbolNodes(sym);
                    nodes.insert(nodes.end(), childNodes.begin(), childNodes.end());
                }
            }
            return nodes;
        }

    private:
        void setType(Type type) {
            this->type = type;
        }

        void addChild(TreeNode* child) {
            this->children.push_back(child);
        }

        Type type = "";
        Symbol symbol;
        Production production;
        Token token;
};

TreeNode* loadParseTree(std::istream& stream) {
    std::string line;
    if (!getline(stream, line)) {
        std::cerr << "ERROR: malformed parse tree" << std::endl;
        throw std::exception();
    }

    TreeNode* root;
    std::vector<std::string> parsedLine = splitString(line);

    // Get type
    Type type = "";
    if (*(parsedLine.end() - 2) == ":") {
        type = *(parsedLine.end() - 1);
        parsedLine = std::vector<std::string>(parsedLine.begin(), parsedLine.end() - 2);
        line = joinVector(parsedLine);
    }

    if (isProduction(line)) {
        // Non-Terminal Node
        root = new TreeNode(parsedLine[0], line);
        for (size_t i = 1; i < parsedLine.size(); ++i) {
            if (parsedLine[i] == ".EMPTY") {
                root->addChild(new TreeNode(".EMPTY"));
                continue;
            }
            root->addChild(loadParseTree(stream));
        }
    } else {
        // Terminal Node
        root = new TreeNode(parsedLine[0], "", Token(parsedLine[0], parsedLine[1]));
    }

    root->setType(type);
    return root;
}

class SymbolTable {
    public:
        SymbolTable() = default;

        size_t nLocals() {
            return this->localCtr / 4;
        }

        void insertLocalVariable(Identifier id, Type type) {
            this->varTable.insert({id, {type, this->localCtr}});
            this->localCtr -= 4;
        }

        void insertParameterVariable(Identifier id, Type type) {
            this->varTable.insert({id, {type, this->paramCtr}});
            this->paramCtr += 4;
        }

        std::pair<Type, int> getVariable(Identifier id) {
            if (this->varTable.find(id) == this->varTable.end()) {
                std::cerr << "ERROR: Cannot get unknown variable " << id << std::endl;
                throw std::exception();
            }
            return this->varTable[id];
        }

        Type getType(Identifier id) {
            return this->getVariable(id).first;
        }

        std::string getOffset(Identifier id) {
            return std::to_string(this->getVariable(id).second);
        }

        void invertParamOffsets() {
            int nParams = 0;
            for (auto& var : this->varTable) {
                int offset = var.second.second;
                if (offset > 0) nParams++;
            }

            int maxOffset = nParams*4;
            for (auto& var : this->varTable) {
                int offset = var.second.second;
                if (offset > 0) {
                    var.second.second = 4 + maxOffset - offset;
                }
            }
        }

    private:
        std::unordered_map<Identifier, std::pair<Type, int>> varTable;
        int localCtr = 0;
        int paramCtr = 4;
};

class SymbolTableStack {
    public:
        SymbolTableStack() = default;

        SymbolTable& current() {
            if (this->s.size() == 0) {
                std::cerr << "ERROR: Cannot peek empty SymbolTableStack." << std::endl;
                throw std::exception();
            }
            return this->s.back();
        }

        size_t nLocals() {
            return this->s.back().nLocals();
        }

        void push() {
            this->s.push_back(SymbolTable());
        }

        void pop() {
            if (this->s.size() == 0) {
                std::cerr << "ERROR: Cannot pop empty SymbolTableStack." << std::endl;
                throw std::exception();
            }
            this->s.pop_back();
        }

        void insertLocalVariable(Identifier id, Type type) {
            this->current().insertLocalVariable(id, type);
        }

        void insertParameterVariable(Identifier id, Type type) {
            this->current().insertParameterVariable(id, type);
        }

        std::pair<Type, int> getVariable(Identifier id) {
            return this->current().getVariable(id);
        }

        Type getType(Identifier id) {
            return this->current().getType(id);
        }

        std::string getOffset(Identifier id) {
            return this->current().getOffset(id);
        }

        void invertParamOffsets() {
            this->current().invertParamOffsets();
        }
    private:
        std::deque<SymbolTable> s;
} g_tables;

std::string pop(Register reg) {
    std::string code = "";
    code += "add $30, $30, $4\n";
    code += std::string("lw ") + reg + ", -4($30)\n";
    return code;
}

std::string push(Register reg) {
    std::string code = "";
    code += std::string("sw ") + reg + ", -4($30)\n";
    code += "sub $30, $30, $4\n";
    return code;
}

unsigned long long labelCtr = 0;

std::string code(TreeNode* root);

std::string codeT(TreeNode* root) {
    Symbol sym = root->getSymbol();
    TokenLexeme lexeme = root->getToken().lexeme;
    if (sym == "NUM") {
        std::string num = lexeme;
        return std::string("lis $3\n") +
               std::string(".word ") + num + "\n";
    } else if (sym == "NULL") {
        return std::string("lis $3\n") +
               std::string(".word 69\n");
    } else if (sym == "ID") {
        Identifier id = lexeme;
        return std::string("lw $3, ") + g_tables.getOffset(id) + "($29)\n";
    }
    return "";
}

std::string codeN(TreeNode* root) {
    Production production = root->getProduction();
    //std::cerr << production << std::endl;
    if (production == "start BOF procedures EOF") {
        TreeNode* procedures = root->children[1];
        return code(procedures);
    } else if (production == "procedures main") {
        TreeNode* main = root->children[0];
        return code(main);
    } else if (production == "main INT WAIN LPAREN dcl COMMA dcl RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE") {
        g_tables.push();

        TreeNode* paramDcl1 = root->children[3];
        TreeNode* paramDcl2 = root->children[5];
        TreeNode* varDcls = root->children[8];
        TreeNode* statements = root->children[9];
        TreeNode* returnExpr = root->children[11];

        // Initialize alloc library
        std::string out;
        out += std::string("Fwain:\n");
        out += std::string("sub $29, $30, $4\n");
        if (paramDcl1->children[1]->getType() == INT_STAR) {
            // array input
            out += push("$29");
            out += push("$31");
            out += std::string("lis $5\n");
            out += std::string(".word init\n");
            out += std::string("jalr $5\n");
            out += pop("$31");
            out += pop("$29");
        } else if (paramDcl1->children[1]->getType() == INT) {
            // twoints input
            out += push("$29");
            out += push("$31");
            out += push("$2");
            out += std::string("lis $2\n");
            out += std::string(".word 0\n");
            out += std::string("lis $5\n");
            out += std::string(".word init\n");
            out += std::string("jalr $5\n");
            out += pop("$2");
            out += pop("$31");
            out += pop("$29");
        }
        out += push("$1");
        out += code(paramDcl1);
        out += push("$2");
        out += code(paramDcl2);
        out += code(varDcls);
        out += code(statements);
        out += code(returnExpr);
        out += std::string("jr $31\n");
        return out;
    } else if (production == "type INT") {
        return "";
    } else if (production == "dcl type ID") {
        TreeNode* idNode = root->children[1];
        Identifier id = idNode->getToken().lexeme;
        Type type = idNode->getType();
        
        g_tables.insertLocalVariable(id, type);
        return "";
    } else if (production == "dcls .EMPTY") {
        return "";
    } else if (production == "statements .EMPTY") {
        return "";
    } else if (production == "expr term") {
        TreeNode* term = root->children[0];
        return code(term);
    } else if (production == "term factor") {
        TreeNode* factor = root->children[0];
        return code(factor);
    } else if (production == "factor NUM") {
        TreeNode* num = root->children[0];
        return code(num);
    } else if (production == "factor ID") {
        TreeNode* id = root->children[0];
        return code(id);
    } else if (production == "factor LPAREN expr RPAREN") {
        TreeNode* expr = root->children[1];
        return code(expr);
    } else if (production == "dcls dcls dcl BECOMES NUM SEMI") {
        TreeNode* dcls = root->children[0];
        TreeNode* dcl = root->children[1];
        TreeNode* num = root->children[3];

        std::string out = "";
        out += code(dcls);
        out += code(dcl);
        out += code(num);
        out += push("$3");
        return out;
    } else if (production == "statements statements statement") {
        TreeNode* statements = root->children[0];
        TreeNode* statement = root->children[1];

        std::string out = "";
        out += code(statements);
        out += code(statement);
        return out;
    } else if (production == "statement lvalue BECOMES expr SEMI") {
        TreeNode* lvalue = root->children[0];
        TreeNode* expr = root->children[2];

        std::string out = "";
        out += code(lvalue);
        out += push("$3");
        out += code(expr);
        out += pop("$5");
        out += std::string("sw $3, 0($5)\n");
        return out;
    } else if (production == "lvalue ID") {
        // LVALUES RETURN EXACT ADDRESS;
        Identifier id = root->children[0]->getToken().lexeme;

        std::string out = "";
        out += std::string("lis $5\n");
        out += std::string(".word ") + g_tables.getOffset(id) + "\n";
        out += std::string("add $3, $29, $5\n");
        return out;
    } else if (production == "lvalue LPAREN lvalue RPAREN") {
        TreeNode* lvalue = root->children[1];
        return code(lvalue);
    } else if (production == "expr expr PLUS term") {
        TreeNode* expr = root->children[0];
        TreeNode* term = root->children[2];

        std::string out = "";

        Type t1 = expr->getType();
        Type t2 = term->getType();
        if (t1 == INT && t2 == INT) {
            out += code(expr);
            out += push("$3");
            out += code(term);
            out += pop("$5");
            out += std::string("add $3, $5, $3\n");
        } else if (t1 == INT_STAR && t2 == INT) {
            out += code(expr);
            out += push("$3");
            out += code(term);
            out += std::string("mult $3, $4\n");
            out += std::string("mflo $3\n");
            out += pop("$5");
            out += std::string("add $3, $5, $3\n");
        } else if (t1 == INT && t2 == INT_STAR) {
            out += code(expr);
            out += std::string("mult $3, $4\n");
            out += std::string("mflo $3\n");
            out += push("$3");
            out += code(term);
            out += pop("$5");
            out += std::string("add $3, $5, $3\n");
        }
        return out;
    } else if (production == "expr expr MINUS term") {
        TreeNode* expr = root->children[0];
        TreeNode* term = root->children[2];

        std::string out = "";

        Type t1 = expr->getType();
        Type t2 = term->getType();
        if (t1 == INT && t2 == INT) {
            out += code(expr);
            out += push("$3");
            out += code(term);
            out += pop("$5");
            out += std::string("sub $3, $5, $3\n");
        } else if (t1 == INT_STAR && t2 == INT) {
            out += code(expr);
            out += push("$3");
            out += code(term);
            out += std::string("mult $3, $4\n");
            out += std::string("mflo $3\n");
            out += pop("$5");
            out += std::string("sub $3, $5, $3\n");
        } else if (t1 == INT_STAR && t2 == INT_STAR) {
            out += code(expr);
            out += push("$3");
            out += code(term);
            out += pop("$5");
            out += std::string("sub $3, $5, $3\n");
            out += std::string("div $3, $4\n");
            out += std::string("mflo $3\n");
        }
        return out;
    } else if (production == "term term STAR factor") {
        TreeNode* term = root->children[0];
        TreeNode* factor = root->children[2];

        std::string out = "";
        out += code(term);
        out += push("$3");
        out += code(factor);
        out += pop("$5");
        out += std::string("mult $5, $3\n");
        out += std::string("mflo $3\n");
        return out;
    } else if (production == "term term SLASH factor") {
        TreeNode* term = root->children[0];
        TreeNode* factor = root->children[2];

        std::string out = "";
        out += code(term);
        out += push("$3");
        out += code(factor);
        out += pop("$5");
        out += std::string("div $5, $3\n");
        out += std::string("mflo $3\n");
        return out;
    } else if (production == "term term PCT factor") {
        TreeNode* term = root->children[0];
        TreeNode* factor = root->children[2];

        std::string out = "";
        out += code(term);
        out += push("$3");
        out += code(factor);
        out += pop("$5");
        out += std::string("div $5, $3\n");
        out += std::string("mfhi $3\n");
        return out;
    } else if (production == "statement IF LPAREN test RPAREN LBRACE statements RBRACE ELSE LBRACE statements RBRACE") {
        TreeNode* test = root->children[2];
        TreeNode* ifStatements = root->children[5];
        TreeNode* elseStatements = root->children[9];
        std::string else_label = std::string("Felse") + std::to_string(labelCtr++) ;
        std::string endif_label = std::string("Fendif") + std::to_string(labelCtr++);

        std::string out = "";
        out += code(test);
        out += std::string("beq $3, $0, ") + else_label + "\n";
        out += code(ifStatements);
        out += std::string("beq $0, $0, ") + endif_label + "\n";
        out += else_label + ":\n";
        out += code(elseStatements);
        out += endif_label + ":\n";
        return out;
    } else if (production == "statement WHILE LPAREN test RPAREN LBRACE statements RBRACE") {
        TreeNode* test = root->children[2];
        TreeNode* statements = root->children[5];
        std::string loop_label = std::string("Floop") + std::to_string(labelCtr++);
        std::string endwhile_label = std::string("Fendwhile") + std::to_string(labelCtr++);

        std::string out = "";
        out += loop_label + ":\n";
        out += code(test);
        out += std::string("beq $3, $0, ") + endwhile_label + "\n";
        out += code(statements);
        out += std::string("beq $0, $0, ") + loop_label + "\n";
        out += endwhile_label + ":\n";
        return out;
    } else if (production == "test expr EQ expr") {
        TreeNode* e1 = root->children[0];
        TreeNode* e2 = root->children[2];

        Type type = e1->getType();
        std::string comparisonOp = "slt";
        if (type == INT_STAR) comparisonOp = "sltu";

        std::string out = "";
        out += code(e1);
        out += push("$3");
        out += code(e2);
        out += pop("$5");
        out += comparisonOp + std::string(" $6, $3, $5\n");
        out += comparisonOp + std::string(" $7, $5, $3\n");
        out += std::string("add $3, $6, $7\n");
        out += std::string("sub $3, $11, $3\n");
        return out;
    } else if (production == "test expr NE expr") {
        TreeNode* e1 = root->children[0];
        TreeNode* e2 = root->children[2];

        Type type = e1->getType();
        std::string comparisonOp = "slt";
        if (type == INT_STAR) comparisonOp = "sltu";

        std::string out = "";
        out += code(e1);
        out += push("$3");
        out += code(e2);
        out += pop("$5");
        out += comparisonOp + std::string(" $6, $3, $5\n");
        out += comparisonOp + std::string(" $7, $5, $3\n");
        out += std::string("add $3, $6, $7\n");
        return out;
    } else if (production == "test expr LT expr") {
        TreeNode* e1 = root->children[0];
        TreeNode* e2 = root->children[2];

        Type type = e1->getType();
        std::string comparisonOp = "slt";
        if (type == INT_STAR) comparisonOp = "sltu";

        std::string out = "";
        out += code(e1);
        out += push("$3");
        out += code(e2);
        out += pop("$5");
        out += comparisonOp + std::string(" $3, $5, $3\n");
        return out;
    } else if (production == "test expr LE expr") {
        TreeNode* e1 = root->children[0];
        TreeNode* e2 = root->children[2];

        Type type = e1->getType();
        std::string comparisonOp = "slt";
        if (type == INT_STAR) comparisonOp = "sltu";

        std::string out = "";

        // LT
        out += code(e1);
        out += push("$3");
        out += code(e2);
        out += pop("$5");
        out += comparisonOp + std::string(" $3, $5, $3\n");

        // Push LT output
        out += push("$3");

        // EQ
        out += code(e1);
        out += push("$3");
        out += code(e2);
        out += pop("$5");
        out += comparisonOp + std::string(" $6, $3, $5\n");
        out += comparisonOp + std::string(" $7, $5, $3\n");
        out += std::string("add $3, $6, $7\n");
        out += std::string("sub $3, $11, $3\n");

        // LT or EQ
        out += pop("$5");
        out += std::string("add $3, $5, $3\n");

        return out;
    } else if (production == "test expr GE expr") {
        TreeNode* e1 = root->children[0];
        TreeNode* e2 = root->children[2];

        Type type = e1->getType();
        std::string comparisonOp = "slt";
        if (type == INT_STAR) comparisonOp = "sltu";

        std::string out = "";

        // GT
        out += code(e1);
        out += push("$3");
        out += code(e2);
        out += pop("$5");
        out += comparisonOp + std::string(" $3, $3, $5\n");

        // Push GT output
        out += push("$3");

        // EQ
        out += code(e1);
        out += push("$3");
        out += code(e2);
        out += pop("$5");
        out += comparisonOp + std::string(" $6, $3, $5\n");
        out += comparisonOp + std::string(" $7, $5, $3\n");
        out += std::string("add $3, $6, $7\n");
        out += std::string("sub $3, $11, $3\n");

        // LT or EQ
        out += pop("$5");
        out += std::string("add $3, $5, $3\n");
        
        return out;
    } else if (production == "test expr GT expr") {
        TreeNode* e1 = root->children[0];
        TreeNode* e2 = root->children[2];

        Type type = e1->getType();
        std::string comparisonOp = "slt";
        if (type == INT_STAR) comparisonOp = "sltu";

        std::string out = "";
        out += code(e1);
        out += push("$3");
        out += code(e2);
        out += pop("$5");
        out += comparisonOp + std::string(" $3, $3, $5\n");
        return out;
    } else if (production == "statement PRINTLN LPAREN expr RPAREN SEMI") {
        TreeNode* expr = root->children[2];

        std::string out = "";
        out += code(expr);
        out += push("$3");
        out += pop("$1");
        out += push("$31");
        out += push("$29");
        out += std::string("jalr $10\n");
        out += pop("$29");
        out += pop("$31");
        return out;
    } else if (production == "procedures procedure procedures") {
        TreeNode* procedure = root->children[0];
        TreeNode* procedures = root->children[1];

        std::string out = "";
        out += code(procedure);
        out += code(procedures);
        return out;
    } else if (production == "procedure INT ID LPAREN params RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE") {
        g_tables.push();

        TreeNode* params = root->children[3];
        TreeNode* dcls = root->children[6];
        TreeNode* statements = root->children[7];
        TreeNode* returnExpr = root->children[9];

        std::string label = std::string("F") + root->children[1]->getToken().lexeme;

        std::string out = "";
        out += label + ":\n";
        out += std::string("sub $29, $30, $4\n");
        out += code(params);  // g_tables elements for arguments will be inserted here
        out += code(dcls);
        out += push("$1") + push("$2") + push("$5") + push("$6") + push("$7");  // Save registers after dcls to keep local vars and params contiguous
        out += code(statements);
        out += code(returnExpr);
        out += pop("$1") + pop("$2") + pop("$5") + pop("$6") + pop("$7");
        out += std::string("jr $31\n");

        g_tables.pop();
        return out;
    } else if (production == "params .EMPTY") {
        return "";
    } else if (production == "params paramlist") {
        TreeNode* paramlist = root->children[0];
        std::string out = "";
        out += code(paramlist);
        g_tables.invertParamOffsets();
        return out;
    } else if (production == "paramlist dcl") {
        TreeNode* dcl = root->children[0];

        TreeNode* idNode = dcl->children[1];
        Identifier id = idNode->getToken().lexeme;
        Type type = idNode->getType();

        g_tables.insertParameterVariable(id, type);
        return "";
    } else if (production == "paramlist dcl COMMA paramlist") {
        TreeNode* dcl = root->children[0];
        TreeNode* paramlist = root->children[2];

        TreeNode* idNode = dcl->children[1];
        Identifier id = idNode->getToken().lexeme;
        Type type = idNode->getType();

        g_tables.insertParameterVariable(id, type);
        return code(paramlist);
    } else if (production == "factor ID LPAREN RPAREN") {
        Identifier id = root->children[0]->getToken().lexeme;
        std::string label = std::string("F") + id;

        std::string out = "";
        out += push("$29");
        out += push("$31");
        out += std::string("lis $5\n");
        out += std::string(".word ") + label + "\n";
        out += std::string("jalr $5\n");
        out += pop("$31");
        out += pop("$29");
        return out;
    } else if (production == "factor ID LPAREN arglist RPAREN") {
        TreeNode* arglist = root->children[2];

        std::vector<TreeNode*> args = arglist->getChildSymbolNodes("expr");
        Identifier id = root->children[0]->getToken().lexeme;
        std::string label = std::string("F") + id;

        std::string out = "";
        out += push("$29");
        out += push("$31");

        for (TreeNode* expr : args) {
            out += code(expr);
            out += push("$3");
        }

        out += std::string("lis $5\n");
        out += std::string(".word ") + label + "\n";
        out += std::string("jalr $5\n");

        for (size_t i = 0; i < args.size(); ++i) {
            out += pop("$5");
        }

        out += pop("$31");
        out += pop("$29");
        return out;
    } else if (production == "arglist expr") {
        return "";
    } else if (production == "arglist expr COMMA arglist") {
        return "";
    } else if (production == "type INT STAR") {
        return "";
    } else if (production == "dcls dcls dcl BECOMES NULL SEMI") {
        TreeNode* dcls = root->children[0];
        TreeNode* dcl = root->children[1];
        TreeNode* null = root->children[3];

        std::string out = "";
        out += code(dcls);
        out += code(dcl);
        out += code(null);
        out += push("$3");
        return out;
    } else if (production == "factor NULL") {
        TreeNode* null = root->children[0];
        return code(null);
    } else if (production == "factor AMP lvalue") {
        TreeNode* lvalue = root->children[1];
        return code(lvalue);
    } else if (production == "factor STAR factor") {
        TreeNode* factor = root->children[1];
        
        std::string out = "";
        out += code(factor);
        out += std::string("lw $3, 0($3)\n");
        return out;
    } else if (production == "lvalue STAR factor") {
        TreeNode* factor = root->children[1];
        
        std::string out = "";
        out += code(factor);
        return out;
    } else if (production == "factor NEW INT LBRACK expr RBRACK") {
        TreeNode* expr = root->children[3];

        std::string out = "";
        out += code(expr);
        out += push("$3");
        out += pop("$1");
        out += push("$31");
        out += push("$29");
        out += std::string("lis $5\n");
        out += std::string(".word new\n");
        out += std::string("jalr $5\n");
        out += pop("$29");
        out += pop("$31");
        out += std::string("bne $3, $0, 2\n");
        out += std::string("lis $3\n");
        out += std::string(".word 69\n");
        return out;
    } else if (production == "statement DELETE LBRACK RBRACK expr SEMI") {
        TreeNode* expr = root->children[3];
        std::string skipDelete_label = std::string("FskipDelete") + std::to_string(labelCtr++);

        std::string out = "";
        out += code(expr);
        out += std::string("lis $5\n");
        out += std::string(".word 69\n");
        out += std::string("beq $3, $5, ") + skipDelete_label + "\n";
        out += push("$3");
        out += pop("$1");
        out += push("$31");
        out += push("$29");
        out += std::string("lis $5\n");
        out += std::string(".word delete\n");
        out += std::string("jalr $5\n");
        out += pop("$29");
        out += pop("$31");
        out += skipDelete_label + ":\n";
        return out;
    }
    return "";
}

std::string code(TreeNode* root) {
    if (root->N()) {
        return codeN(root);
    } else {
        return codeT(root);
    }
}

int main() {
    TreeNode* root = loadParseTree(std::cin);
    std::string asmCode;
    asmCode += std::string(".import print\n");
    asmCode += std::string(".import init\n");
    asmCode += std::string(".import new\n");
    asmCode += std::string(".import delete\n");
    asmCode += std::string("lis $4\n");
    asmCode += std::string(".word 4\n");
    asmCode += std::string("lis $10\n");
    asmCode += std::string(".word print\n");
    asmCode += std::string("lis $11\n");
    asmCode += std::string(".word 1\n");
    asmCode += "beq $0, $0, Fwain\n";
    asmCode += code(root);
    std::cout << asmCode;
    delete root;
    return 0;
}
