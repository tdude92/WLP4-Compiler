#include<string>
const std::string WLP4_CFG = R"END(.CFG
start BOF procedures EOF
procedures procedure procedures
procedures main
procedure INT ID LPAREN params RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE
main INT WAIN LPAREN dcl COMMA dcl RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE
params .EMPTY
params paramlist
paramlist dcl
paramlist dcl COMMA paramlist
type INT
type INT STAR
dcls .EMPTY
dcls dcls dcl BECOMES NUM SEMI
dcls dcls dcl BECOMES NULL SEMI
dcl type ID
statements .EMPTY
statements statements statement
statement lvalue BECOMES expr SEMI
statement IF LPAREN test RPAREN LBRACE statements RBRACE ELSE LBRACE statements RBRACE
statement WHILE LPAREN test RPAREN LBRACE statements RBRACE
statement PRINTLN LPAREN expr RPAREN SEMI
statement DELETE LBRACK RBRACK expr SEMI
test expr EQ expr
test expr NE expr
test expr LT expr
test expr LE expr
test expr GE expr
test expr GT expr
expr term
expr expr PLUS term
expr expr MINUS term
term factor
term term STAR factor
term term SLASH factor
term term PCT factor
factor ID
factor NUM
factor NULL
factor LPAREN expr RPAREN
factor AMP lvalue
factor STAR factor
factor NEW INT LBRACK expr RBRACK
factor ID LPAREN RPAREN
factor ID LPAREN arglist RPAREN
arglist expr
arglist expr COMMA arglist
lvalue ID
lvalue STAR factor
lvalue LPAREN lvalue RPAREN
)END";
