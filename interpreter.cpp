#include <string> 
#include <vector>
#include <iostream> 
#include <string.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <sys/un.h>
#include <netinet/in.h> 
#include <netdb.h>
#include <sys/io.h>
#include "sock.hpp"

extern "C" 
{
    #include "table.h"
}

#include "tableint.hpp"
#include "interpreter.hpp"

namespace Interpreter { 

    std::string InterpretException::m_Message[] = {
        "unexepcted character",
        "unexpected lexeme",
        "data base request error" 
    };
      
    std::string lexer::LexerException::GetMessage() { 
        return "In expression: " + err_str + "\nPosition: " + std::to_string(err_pos) + 
            " --- " + m_Message[m_ErrCode] + " found\n";
    }

    void lexer::expr_init(std::string s) {
        expression = s;
        c = 0;
        cur_pos = 0;
        cur_lex_pos = 0;
        cur_lex_type = IDEN;
        where_flag = false;
        cur_lex_text = ""; 
    }
    
    void lexer::init() {
        c = expression[cur_pos];
    }
        
    void lexer::next() {
        cur_lex_text.clear();
        cur_lex_pos = cur_pos;
        
        // Availible state types
        enum state_t {H, PN, I, O, C, AL, MINN, NN, PL, MUL, DI, MO, COM, 
            QUOT, LE, LEEQ, GE, GEEQ, EQ, NEQ, NEQC, OK} state = H;
        while (state != OK) {
            switch (state) {
            case H:
                if (c == '\0') {
                    cur_lex_type = END;
                    state = OK;
                } else if (std::isspace(c)) {     
                    // Stay in H
                } else if (std::isdigit(c)) {
                    state = PN; // PosNum
                } else if (std::isalpha(c) || c == '_') {
                    state = I; // Ident 
                } else if (c == '(') {
                    state = O; // Open
                } else if (c == ')') {
                    state = C; // Close
                } else if (c == '*') {
                    if (where_flag) {
                        state = MUL; // Mult
                    } else {
                        state = AL; // All
                    }
                } else if (c == '-') {
                    state = MINN; // Sub or NegNum
                } else if (c == '+') {
                    state = PL; // Addition
                } else if (c == '/') {
                    state = DI; // Division
                } else if (c == '%') {
                    state = MO; // Division with remainder
                } else if (c == ',') {
                    state = COM; // Comma
                } else if (c == '\'') { 
                    state = QUOT; // Quote
                } else if (c == '<') {
                    state = LE; // Less or LessEqual
                } else if (c == '>') {
                    state = GE; // Greater or GreaterEqual
                } else if (c == '=') {
                    state = EQ; // Equal
                } else if (c == '!') {
                    state = NEQ; // NotEqual
                } else {
                    throw LexerException();
                }
                break;
            case PN: 
                if (std::isdigit(c)) {
                    // Stay in N
                } else if (std::isspace(c) || c == ',' || c == ')') {
                    //std::cout << cur_lex_text << std::endl;
                    cur_lex_type = POSNUM;
                    state = OK;
                } else {
                    throw LexerException();    
                }
                break;
            case I:
                //std::cout << cur_lex_text << std::endl;
                if (std::isdigit(c) || std::isalpha(c) || (c == '_')) {
                    // Stay in I
                } else if (std::isspace(c) || c == ',' || c == '(' || c == ')' || c == '\0') {
                    if (cur_lex_text == "SELECT") {
                        cur_lex_type = SELECT;
                        state = OK;
                    } else if (cur_lex_text == "INSERT") {
                        cur_lex_type = INSERT;
                        state = OK;
                    } else if (cur_lex_text == "UPDATE") {
                        cur_lex_type = UPDATE;
                        state = OK;
                    } else if (cur_lex_text == "DELETE") {
                        cur_lex_type = DELETE;
                        state = OK;
                    } else if (cur_lex_text == "CREATE") {
                        cur_lex_type = CREATE;
                        state = OK;
                    } else if (cur_lex_text == "DROP") {
                        cur_lex_type = DROP;
                        state = OK;
                    } else if (cur_lex_text == "WHERE") {
                        where_flag = true;
                        cur_lex_type = WHERE;
                        state = OK;
                    } else if (cur_lex_text == "FROM") {
                        cur_lex_type = FROM;
                        state = OK;
                    } else if (cur_lex_text == "INTO") {
                        cur_lex_type = INTO;
                        state = OK;
                    } else if (cur_lex_text == "SET") {
                        cur_lex_type = SET;
                        where_flag = true;
                        state = OK;
                    } else if (cur_lex_text == "TABLE") {
                        cur_lex_type = TABLE;
                        state = OK;        
                    } else if (cur_lex_text == "LIKE") {
                        cur_lex_type = LIKE;
                        state = OK;
                    } else if (cur_lex_text == "NOT") {
                        cur_lex_type = NOT;
                        state = OK;        
                    } else if (cur_lex_text == "IN") {
                        cur_lex_type = IN;
                        state = OK;        
                    } else if (cur_lex_text == "ALL") {
                        cur_lex_type = WALL;
                        state = OK;        
                    } else if (cur_lex_text == "OR") {
                        cur_lex_type = OR;
                        state = OK;        
                    } else if (cur_lex_text == "AND") {
                        cur_lex_type = AND;
                        state = OK;        
                    } else if (cur_lex_text == "TEXT") {
                        cur_lex_type = TEXT;
                        state = OK;        
                    } else if (cur_lex_text == "LONG") {
                        cur_lex_type = LONG;
                        state = OK;       
                    } else {
                        cur_lex_type = IDEN;
                        state = OK; 
                    }        
                } else {
                    throw LexerException();
                }
                break;
            case O:
                if (std::isspace(c) || std::isalpha(c) || std::isdigit(c) || c == '\'' || c =='(') {
                    //std::cout << cur_lex_text << std::endl;
                    cur_lex_type = OPEN;
                    state = OK;
                } else {
                    throw LexerException();  
                }
                break;
            case C:
                if (std::isspace(c) || (c == ')') || (c == ',') || (c == '\0')) {
                    //std::cout << cur_lex_text << std::endl;
                    cur_lex_type = CLOSE;
                    state = OK;
                } else {
                    throw LexerException();  
                }
                break;
            case AL:
                if (std::isspace(c)) {
                    //std::cout << cur_lex_text << std::endl;
                    cur_lex_type = ALL;
                    state = OK;
                } else {
                    throw LexerException();  
                }
                break;
            case MINN: 
                if (std::isspace(c)) {
                    cur_lex_type = MINUS;
                    state = OK;
                } else if (std::isdigit(c)) {
                    state = NN;
                } else {
                    throw LexerException(); 
                }
                break;
            case PL: 
                if (std::isspace(c)) {
                    cur_lex_type = PLUS;
                    state = OK;
                } else {
                    throw LexerException(); 
                }
                break;
            case MUL:
                if (std::isspace(c)) {
                    cur_lex_type = MULT;
                    state = OK;
                } else {
                    throw LexerException();  
                }
                break;
            case DI: 
                if (std::isspace(c)) {
                    cur_lex_type = DIV;
                    state = OK;
                } else {
                    throw LexerException(); 
                }
                break;
            case MO: 
                if (std::isspace(c)) {
                    cur_lex_type = MOD;
                    state = OK;
                } else {
                    throw LexerException(); 
                }
                break;
            case NN: 
                if (std::isdigit(c)) {
                    // Stay in NN
                } else if (std::isspace(c) || c == ',' || c == ')') {
                    //std::cout << cur_lex_text << std::endl;
                    cur_lex_type = NEGNUM;
                    state = OK;
                } else {
                    throw LexerException(); 
                }
                break;
            case COM:
                if (std::isspace(c)) {
                    //std::cout << cur_lex_text << std::endl;
                    cur_lex_type = COMMA;
                    state = OK;
                } else {
                    throw LexerException();  
                }
                break;
            case QUOT:
                while (c != '\'' && c != '\0') {
                    cur_lex_text.push_back(c);
                    c = expression[++cur_pos];
                }
                if (c == '\'') {
                    cur_lex_type = QUOTE;
                    c = expression[++cur_pos];
                    state = OK;
                } else if (c == '\0') {
                    cur_lex_type = END;
                    state = OK;
                } else {
                    throw LexerException();  
                }
                break;
            case LE: 
                if (c == '=') {
                    state = LEEQ; // LessEqual
                } else if (std::isspace(c)) {
                    cur_lex_type = LESS;
                    state = OK;
                } else {
                    throw LexerException();  
                }
                break;
            case LEEQ:
                if (std::isspace(c)) {
                    cur_lex_type = EQUALLE;
                    state = OK;
                } else {
                    throw LexerException();  
                }
                break;
            case GE:
                if (c == '=') {
                    state = GEEQ; // GreaterEqual
                } else if (std::isspace(c)) {
                    cur_lex_type = GREATER;
                    state = OK;
                } else {
                    throw LexerException();  
                }
                break;
            case GEEQ:
                if (std::isspace(c)) {
                    cur_lex_type = EQUALGE;
                    state = OK;
                } else {
                    throw LexerException();  
                }
                break;
            case EQ:
                if (std::isspace(c)) {
                    cur_lex_type = EQUAL;
                    state = OK;
                } else {
                    throw LexerException();  
                }
                break;
            case NEQ:
                if (c == '=') {
                    state = NEQC;
                } else {
                    throw LexerException();
                }
                break;
            case NEQC:
                if (std::isspace(c)) {
                    cur_lex_type = NOTEQUAL;
                    state = OK;
                } else {
                    throw LexerException();  
                }
                break;
            case OK:
                break;
            }
            if (state != OK) {
                if (!std::isspace(c) && cur_lex_type != END && c != '\'') {
                    ++cur_lex_pos;
                    cur_lex_text.push_back(c);
                }
                c = expression[++cur_pos];
            }
        }
    }

    std::string parser::ParserException::GetMessage() { 
        std::string lexems;
        lexems = lex_name[lex_expected[0]];
        for (int i = 1; i < lex_expected.size(); i++){
            lexems = lexems + " or " + lex_name[lex_expected[i]];
        } 
        return "In expression: " + err_str + "\nPosition: " + std::to_string(err_pos) + " --- " + m_Message[m_ErrCode] +
            " " + lex_name[lex_error] + " with text \"" + lex_text + "\" found." + " Expected lexeme: " + lexems + ".\n";
    }

    std::string parser::ParserException::lex_name[] = {
        "SELECT", "INSERT", "UPDATE", "DELETE", "CREATE", "DROP", "WHERE", 
        "FROM", "INTO", "SET", "TABLE", "LIKE", "NOT", "IN", "WALL", "OR", 
        "AND", "TEXT", "LONG", "POSNUM", "NEGNUM", "ALL", "IDEN",  "MINUS", 
        "PLUS", "MULT", "DIV", "MOD", "COMMA", "OPEN", "CLOSE", "QUOTE", 
        "LESS", "EQUALLE", "GREATER", "EQUALGE", "EQUAL", "NOTEQUAL", "END"
    };
    
    std::string parser::LogicException::GetMessage() { 
        return "Expected logic expression";
    }

    std::string parser::get_answer() {
        return answer;
    }

    void parser::init_parse() {
       
        poliz_texts.clear();
        poliz_types.clear(); 
        table_params.clear();
        line_params.clear();
        field_name_list.clear();
        field_name.clear();
        filter_list.clear();
        all_flag = false;

        answer = "\n";

        lexer::init();
        lexer::next();

        switch (lexer::cur_lex_type) {
            case SELECT :
                lexer::next();
                select_p(); 
                break;
            case INSERT :
                lexer::next(); 
                insert_p();
                break;
            case UPDATE :
                lexer::next(); 
                update_p();
                break;
            case DELETE :
                lexer::next(); 
                delete_p();
                break;
            case CREATE :
                lexer::next(); 
                create_p();
                break;
            case DROP :
                lexer::next(); 
                drop_p();
                break;
            default:
                lex_exp.push_back(SELECT);
                lex_exp.push_back(INSERT);
                lex_exp.push_back(UPDATE);
                lex_exp.push_back(DELETE);
                lex_exp.push_back(CREATE);
                lex_exp.push_back(DROP);
                throw ParserException(lex_exp);
                throw ParserException(lex_exp);
        }
    }

    void parser::select_p() {

        flds_name_list_p();
       
        if (lexer::cur_lex_type == FROM) {
            lexer::next();
        } else {
            lex_exp.push_back(FROM);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == IDEN) {
            table_name = lexer::cur_lex_text;                           //action: memorize table name, 
            lexer::next();
        } else {
            lex_exp.push_back(IDEN);
            throw ParserException(lex_exp);
        }
        where_p();                                                       
        answer = TableInterface::select(table_name, field_name_list, filter_list, all_flag); //select
        return; 
    }

    void parser::flds_name_list_p() {
        if (lexer::cur_lex_type == ALL) {
            all_flag = true;                                               //memorize select all flag
            lexer::next();
            return;
        } else {
            fld_name_p(); 
            while (lexer::cur_lex_type == COMMA) {
                lexer::next();
                fld_name_p();
            }
            return;
        }
    }

    void parser::fld_name_p() {
        if (lexer::cur_lex_type == IDEN) {
            field_name_list.push_back(lexer::cur_lex_text);             //add field name to list
            lexer::next();
        } else {
            lex_exp.push_back(IDEN);
            throw ParserException(lex_exp);
        }
    }
           
    void parser::insert_p() {
        if (lexer::cur_lex_type == INTO) {
            lexer::next();
        } else {
            lex_exp.push_back(INTO);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == IDEN) {
            table_name = lexer::cur_lex_text;                           //action: memorize table name, 
            lexer::next();
        } else {
            lex_exp.push_back(IDEN);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == OPEN) {
            lexer::next();
            flds_cont_list_p();
        } else {
            lex_exp.push_back(OPEN);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == CLOSE) {
            lexer::next();  
        } else { 
            lex_exp.push_back(CLOSE);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == END) {
            TableInterface::insert(table_name, line_params);            //insert line.
            answer = TableInterface::print_table(table_name);           //print  up to date table.
        } else { 
            lex_exp.push_back(END);
            throw ParserException(lex_exp);
        }
        return;    
    }  

    void parser::flds_cont_list_p() {
        fld_cont_p();
        while (lexer::cur_lex_type == COMMA) {
            lexer::next();
            fld_cont_p();    
        }
        return;
    }

    void parser::fld_cont_p() {
        if (lexer::cur_lex_type == QUOTE) {
            cur_field_cont.type = Text;                                 //memorize field type,
            cur_field_cont.cont = lexer::cur_lex_text;                  //memorize field content,
            lexer::next();
        } else if (lexer::cur_lex_type == NEGNUM || lexer::cur_lex_type == POSNUM) {
            cur_field_cont.type = Long;                                 //memorize field type,
            cur_field_cont.cont = lexer::cur_lex_text;                  //memorize field content,
            lexer::next();
        } else {
            lex_exp.push_back(QUOTE);
            throw ParserException(lex_exp);
        }
        line_params.push_back(cur_field_cont);                          //add to line params list.
        return;  
    }

    void parser::update_p() {
        std::string field_name_upd;
        if (lexer::cur_lex_type == IDEN) {
            table_name = lexer::cur_lex_text;                           //action: memorize table name, 
            lexer::next();
        } else {
            lex_exp.push_back(IDEN);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == SET) {
            lexer::next();
        } else {
            lex_exp.push_back(SET);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == IDEN) {
            field_name_upd = lexer::cur_lex_text;                       //memorize field name
            lexer::next();
        } else {
            lex_exp.push_back(IDEN);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == EQUAL) {
            lexer::next();
            where_expr();
            std::vector<std::string> poliz_upd_texts = poliz_texts;     //save upd_poliz
            std::vector<lex_type_t> poliz_upd_types = poliz_types;      //save upd_poliz
            poliz_texts.clear();                                        //poliz clear
            poliz_types.clear();                                        //poliz clear
            where_p();
            TableInterface::update(table_name, field_name_upd, poliz_upd_texts, poliz_upd_types, filter_list);   //action: update
            answer = TableInterface::print_table(table_name);
            return;
        } else {
            lex_exp.push_back(EQUAL);
            throw ParserException(lex_exp);
        } 
    }

    void parser::delete_p() {
        if (lexer::cur_lex_type == FROM) {
            lexer::next();
        } else {
            lex_exp.push_back(FROM);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == IDEN) {
            table_name = lexer::cur_lex_text;                           //memorize table name          
            lexer::next();
            where_p();
            TableInterface::delete_fields(table_name, filter_list);     //action: delete   
            answer = TableInterface::print_table(table_name);                    //action: print table  
            return;
        } else {
            lex_exp.push_back(IDEN);
            throw ParserException(lex_exp);
        }
    }

    void parser::create_p() {
        if (lexer::cur_lex_type == TABLE) {
            lexer::next();
        } else {
            lex_exp.push_back(TABLE);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == IDEN) {
            table_name = lexer::cur_lex_text;                           //memorize table name
            lexer::next();
        } else {
            lex_exp.push_back(IDEN);
            throw ParserException(lex_exp);
        } 
        if (lexer::cur_lex_type == OPEN) {
            lexer::next();
            flds_def_list_p();
        } else {
            lex_exp.push_back(OPEN);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == CLOSE) {
            lexer::next();
        } else { 
            lex_exp.push_back(CLOSE);
            throw ParserException(lex_exp);
        } 
        if (lexer::cur_lex_type == END) {
            
            TableInterface::create(table_name, table_params);           //create table    
            answer = TableInterface::print_table(table_name);           //print up to date table.
        } else { 
            lex_exp.push_back(END);
            throw ParserException(lex_exp);
        }
        return;
    }  
        
   void parser::fld_def_p() {
        if (lexer::cur_lex_type == IDEN) { 
            strcpy(cur_field_def.name, lexer::cur_lex_text.data());     //memorize: field name,
            lexer::next();
        } else {
            lex_exp.push_back(IDEN);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == LONG) {    
            cur_field_def.type = Long;                                  //field type,
            cur_field_def.len = sizeof(long);                           //field size.
            table_params.push_back(cur_field_def);                      //add to table params list.
            lexer::next();
        } else {
            txt_type(); 
        }
        return;
    } 

    void parser::txt_type() {
        if (lexer::cur_lex_type == TEXT) {
            cur_field_def.type = Text;                                  //memorize: field type,
            lexer::next();
        } else {
            lex_exp.push_back(TEXT);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == OPEN) {
            lexer::next();
        } else {
            lex_exp.push_back(OPEN);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == POSNUM) {
            cur_field_def.len = stoi(lexer::cur_lex_text);              //field size. 
            lexer::next();
        } else {
            lex_exp.push_back(POSNUM);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == CLOSE) {
            table_params.push_back(cur_field_def);                      //add to table params list.
            lexer::next();
        } else {
            lex_exp.push_back(CLOSE);
            throw ParserException(lex_exp);
        }
        return;
    }

    void parser::flds_def_list_p() {
        fld_def_p();
        while (lexer::cur_lex_type == COMMA) {
            lexer::next();
            fld_def_p();
        } 
        return;
    }
   
    void parser::drop_p() {
        if (lexer::cur_lex_type == TABLE) {
            lexer::next();
        } else {
             lex_exp.push_back(TABLE);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == IDEN) {
            table_name = lexer::cur_lex_text;                           //memorize table name,
            lexer::next();
        } else {
             lex_exp.push_back(IDEN);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == END) {
            TableInterface::drop(table_name);                           //drop table.
            answer = "The table is dropped succesfully\n";
        } else {
            lex_exp.push_back(END);
            throw ParserException(lex_exp);
        }
        return;
    }

    void parser::Push(std::string &lex_text, lex_type_t type) {
        poliz_texts.push_back(lex_text);
        poliz_types.push_back(type);
    }
/////////////WHERE CLAUSE//////////////
    void parser::where_p() {
        not_flag = false;
        parser::type_t type;
        if (lexer::cur_lex_type == WHERE) {
            lexer::next();
        } else {
            lex_exp.push_back(WHERE); throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type == WALL){
            lexer::next();
            if (lexer::cur_lex_type == END) {
                filter_list = TableInterface::filter_all(table_name);
            return;
            } else {
                lex_exp.push_back(END);
                throw ParserException(lex_exp);
            }
        } else if (lexer::cur_lex_type == IDEN ||
                    lexer::cur_lex_type == POSNUM ||
                    lexer::cur_lex_type == NEGNUM ||
                    lexer::cur_lex_type == QUOTE ||
                    lexer::cur_lex_type == OPEN ||
                    lexer::cur_lex_type == NOT){
            type = where_expr();
        } else {
            lex_exp.push_back(IDEN);
            lex_exp.push_back(POSNUM);
            lex_exp.push_back(NEGNUM);
            lex_exp.push_back(QUOTE);
            lex_exp.push_back(OPEN);
            lex_exp.push_back(NOT);
            throw ParserException(lex_exp);
        }
        switch(type){
            case EText:
                if (lexer::cur_lex_type == NOT) {
                    lexer::next();
                    if (lexer::cur_lex_type == LIKE) {
                        lexer::next();
                        not_flag = true;
                        where_like(not_flag);
                    } else if (lexer::cur_lex_type == IN) {
                        lexer::next();
                        not_flag = true;
                        where_in(not_flag);
                    } else {
                        lex_exp.push_back(LIKE);
                        lex_exp.push_back(IN);
                        throw ParserException(lex_exp);
                    }
                } else if (lexer::cur_lex_type == LIKE) {
                    lexer::next();
                    where_like(not_flag);
                } else if (lexer::cur_lex_type == IN) {
                    lexer::next();
                    where_in(not_flag);
                } else {
                    lex_exp.push_back(LIKE);
                    lex_exp.push_back(IN);
                    throw ParserException(lex_exp);
                }
                break;
            case EStr:
                if (lexer::cur_lex_type == NOT) {
                    lexer::next();
                    if (lexer::cur_lex_type == IN) {
                        lexer::next();
                        not_flag = true;
                        where_in(not_flag);
                    } else {
                        lex_exp.push_back(IN);
                        throw ParserException(lex_exp);
                    }
                } else if (lexer::cur_lex_type == IN) {
                    lexer::next();
                    where_in(not_flag);
                } else {
                    lex_exp.push_back(IN);
                    throw ParserException(lex_exp);               
                }
                break;
            case ELong:
                if (lexer::cur_lex_type == NOT) {
                    lexer::next();
                    if (lexer::cur_lex_type == IN) {
                        lexer::next();
                        not_flag = true;
                        where_in(not_flag);
                    } else {
                        lex_exp.push_back(IN);
                        throw ParserException(lex_exp);
                    }
                } else if (lexer::cur_lex_type == IN) {
                    lexer::next();
                    where_in(not_flag);
                } else {
                    lex_exp.push_back(IN);
                    throw ParserException(lex_exp);
                }
                break;
            case ELogic:
                if (lexer::cur_lex_type == END) {
                    filter_list = TableInterface::filter_logic(table_name, poliz_texts, poliz_types);
                } else {
                    lex_exp.push_back(END);
                    throw ParserException(lex_exp);
                }
                break;
        }
    }

    
    parser::type_t parser::where_expr() {
        parser::type_t type_right, type_left;
        lex_type_t operation_type;
        std::string poliz_str;

        type_left = where_expr_term();
        switch(type_left){
            case ELong:
                while ((operation_type = lexer::cur_lex_type) == PLUS || 
                        (operation_type = lexer::cur_lex_type) == MINUS) {
                    lexer::next();
                    type_right = where_expr_term();
                    if (type_right != ELong) {
                        lex_exp.push_back(IDEN);
                        lex_exp.push_back(POSNUM);
                        lex_exp.push_back(NEGNUM);
                        lex_exp.push_back(OPEN);
                        throw ParserException(lex_exp);
                    }
                    switch(operation_type){
                        case PLUS:
                            poliz_str = "+";
                            Push(poliz_str, PLUS);
                            break;
                        case MINUS:
                            poliz_str = "-";
                            Push(poliz_str, MINUS);
                            break;
                    }
                }
                return ELong;
                break;
            case ELogic:
                while (lexer::cur_lex_type == OR) {
                    lexer::next();
                    type_right = where_expr_term();
                    if (type_right != ELogic) {
                        throw LogicException();
                    }
                    poliz_str = "OR";
                    Push(poliz_str, OR);
                }
                return ELogic;
                break;
            case EText:
                return EText;
                break;
            default:
                return EStr;
        }
    }

    parser::type_t parser::where_expr_term() {
        parser::type_t type_right, type_left;
        lex_type_t operation_type;
        std::string poliz_str;

        type_left = where_expr_mult();
        switch(type_left){
            case ELong:
                while ((operation_type = lexer::cur_lex_type) == MULT ||
                (operation_type = lexer::cur_lex_type) == DIV  ||
                (operation_type = lexer::cur_lex_type) == MOD) {
                    lexer::next();
                    type_right = where_expr_mult();
                    if (type_right != ELong) {
                        lex_exp.push_back(IDEN);
                        lex_exp.push_back(POSNUM);
                        lex_exp.push_back(NEGNUM);
                        lex_exp.push_back(OPEN);
                        throw ParserException(lex_exp); 
                    }
                    switch(operation_type){
                        case MULT:
                            poliz_str = "*";
                            Push(poliz_str, MULT);
                            break;
                        case DIV:
                            poliz_str = "/";
                            Push(poliz_str, DIV);
                            break;
                        case MOD:
                            poliz_str = "%";
                            Push(poliz_str, MOD);
                    }
                }
                return ELong;
                break;
            case ELogic:
                while (lexer::cur_lex_type == AND) {
                    lexer::next();
                    type_right = where_expr_mult();
                    if (type_right != ELogic) {
                        throw LogicException();
                    }
                    poliz_str = "AND";
                    Push(poliz_str, AND);
                }
                return ELogic;
                break;
            case EText:
                return EText;
                break;
            default:
                return EStr;
        }
    }

    parser::type_t parser::where_expr_mult() {
        enum FieldType ftype;
        std::string poliz_str;
        if (lexer::cur_lex_type == NOT) {
            lexer::next();
            type_t type_not = where_expr_mult();
            if (type_not != ELogic){
                throw LogicException();
            }
            poliz_str = "NOT";
            Push(poliz_str, NOT);
            return ELogic;
        } else if (lexer::cur_lex_type == IDEN) {
            TableInterface::get_type(table_name, lexer::cur_lex_text, &ftype);
            if (ftype == Long) {
                Push(lexer::cur_lex_text, IDEN);
                lexer::next();
                return ELong;
            } else if (ftype == Text) {
                Push(lexer::cur_lex_text, IDEN);
                lexer::next();
                return EText;
            }
        } else if (lexer::cur_lex_type == QUOTE) {
            Push(lexer::cur_lex_text, QUOTE);
            lexer::next();
            return EStr;
        } else if (lexer::cur_lex_type == POSNUM ||
                    lexer::cur_lex_type == NEGNUM){
            if (lexer::cur_lex_type == POSNUM){
                Push(lexer::cur_lex_text, POSNUM);
            } else {
                Push(lexer::cur_lex_text, NEGNUM);
            }
            lexer::next();
            return ELong;
        } else if (lexer::cur_lex_type == OPEN) {
            lexer::next();
            parser::type_t type_left = where_expr();
            return where_open(type_left);
        }

        lex_exp.push_back(IDEN);
        lex_exp.push_back(NOT);
        lex_exp.push_back(QUOTE);
        lex_exp.push_back(POSNUM);
        lex_exp.push_back(NEGNUM);
        lex_exp.push_back(OPEN);
        throw ParserException(lex_exp);
    }

    parser::type_t parser::where_open(parser::type_t type_left) {
        parser::type_t type_right;
        lex_type_t operation_type;
        std::string poliz_str;
        if (type_left == ELong){
            if ((operation_type = lexer::cur_lex_type) == LESS ||
                (operation_type = lexer::cur_lex_type) == EQUALLE ||
                (operation_type = lexer::cur_lex_type) == GREATER ||
                (operation_type = lexer::cur_lex_type) == EQUALGE ||
                (operation_type = lexer::cur_lex_type) == EQUAL ||
                (operation_type = lexer::cur_lex_type) == NOTEQUAL) {
                    lexer::next();
                    type_right = where_expr();
                    if (type_right != ELong) {
                        lex_exp.push_back(IDEN);
                        lex_exp.push_back(POSNUM);
                        lex_exp.push_back(NEGNUM);
                        lex_exp.push_back(OPEN);
                        throw ParserException(lex_exp);
                    } else {
                        switch(operation_type){
                            case LESS:
                                poliz_str = "<";
                                Push(poliz_str, LESS);
                                break;
                            case EQUALLE:
                                poliz_str = "<=";
                                Push(poliz_str, EQUALLE);
                                break;
                            case GREATER:
                                poliz_str = ">";
                                Push(poliz_str, GREATER);
                                break;
                            case EQUALGE:
                                poliz_str = ">=";
                                Push(poliz_str, EQUALGE);
                                break;
                            case EQUAL:
                                poliz_str = "=";
                                Push(poliz_str, EQUAL);
                                break;
                            case NOTEQUAL:
                                poliz_str = "!=";
                                Push(poliz_str, NOTEQUAL);
                                break;
                        }
                    }
                    if (lexer::cur_lex_type == CLOSE) {
                        lexer::next();
                        return ELogic;
                    } else {
                        lex_exp.push_back(CLOSE);
                        throw ParserException(lex_exp);
                    }
            } else if (lexer::cur_lex_type == CLOSE) {
                lexer::next();
                return ELong;
            } else {
                lex_exp.push_back(CLOSE);
                lex_exp.push_back(EQUAL);
                lex_exp.push_back(NOTEQUAL);
                lex_exp.push_back(LESS);
                lex_exp.push_back(GREATER);
                lex_exp.push_back(EQUALLE);
                lex_exp.push_back(EQUALGE);
                throw ParserException(lex_exp);
            }
        } else if (type_left == EText || type_left == EStr){
            if ((operation_type = lexer::cur_lex_type) == LESS ||
                (operation_type = lexer::cur_lex_type) == EQUALLE ||
                (operation_type = lexer::cur_lex_type) == GREATER ||
                (operation_type = lexer::cur_lex_type) == EQUALGE ||
                (operation_type = lexer::cur_lex_type) == EQUAL ||
                (operation_type = lexer::cur_lex_type) == NOTEQUAL) {
                    lexer::next();
                    type_right = where_expr();
                    if (type_right != EText && type_right != EStr) {
                        lex_exp.push_back(IDEN);
                        lex_exp.push_back(QUOTE);
                        throw ParserException(lex_exp);
                    } else {
                        switch(operation_type){
                            case LESS:
                                poliz_str = "<";
                                Push(poliz_str, LESS);
                                break;
                            case EQUALLE:
                                poliz_str = "<=";
                                Push(poliz_str, EQUALLE);
                                break;
                            case GREATER:
                                poliz_str = ">";
                                Push(poliz_str, GREATER);
                                break;
                            case EQUALGE:
                                poliz_str = ">=";
                                Push(poliz_str, EQUALGE);
                                break;
                            case EQUAL:
                                poliz_str = "=";
                                Push(poliz_str, EQUAL);
                                break;
                            case NOTEQUAL:
                                poliz_str = "!=";
                                Push(poliz_str, NOTEQUAL);
                        }
                        if (lexer::cur_lex_type == CLOSE) {
                            lexer::next();
                            return ELogic;
                        } else {
                            lex_exp.push_back(CLOSE);
                            throw ParserException(lex_exp);
                        }
                    }
            } else {
                lex_exp.push_back(EQUAL);
                lex_exp.push_back(NOTEQUAL);
                lex_exp.push_back(LESS);
                lex_exp.push_back(GREATER);
                lex_exp.push_back(EQUALLE);
                lex_exp.push_back(EQUALGE);
                throw ParserException(lex_exp);
            }
        } else if (type_left == ELogic){
            if (lexer::cur_lex_type == CLOSE) {
                lexer::next();
                return ELogic;
            } else {
                lex_exp.push_back(CLOSE);
                throw ParserException(lex_exp);
            }
        }
        lex_exp.push_back(ALL);
        lex_exp.push_back(IN);
        throw ParserException(lex_exp);
    }

    void parser::where_in(bool &n_flag) {
        std::vector<TableInterface::FieldCont> const_list;
        if (lexer::cur_lex_type == OPEN) {
            lexer::next();
        } else {
            lex_exp.push_back(OPEN);
            throw ParserException(lex_exp);
        }
        if (lexer::cur_lex_type ==  QUOTE) {
            TableInterface::FieldCont str;
            str.cont = lexer::cur_lex_text;
            str.type = Text;
            const_list.push_back(str);
            lexer::next();
            while (lexer::cur_lex_type == COMMA) {
                lexer::next();
                if (lexer::cur_lex_type == QUOTE) {
                    TableInterface::FieldCont str;
                    str.cont = lexer::cur_lex_text;
                    str.type = Text;
                    const_list.push_back(str);
                    lexer::next();
                } else {
                    lex_exp.push_back(QUOTE);
                    throw ParserException(lex_exp);
                }
            }

            if (lexer::cur_lex_type == CLOSE) {
                lexer::next();
            } else {
                lex_exp.push_back(CLOSE);
                throw ParserException(lex_exp);
            }
            if (lexer::cur_lex_type == END){
                filter_list = TableInterface::filter_in(table_name, poliz_texts, poliz_types, const_list, not_flag);
                return;
            } else {
                lex_exp.push_back(END);
                throw ParserException(lex_exp);
            }
        } else if (lexer::cur_lex_type == POSNUM ||
                    lexer::cur_lex_type == NEGNUM) {
            TableInterface::FieldCont str;
            str.cont = lexer::cur_lex_text;
            str.type = Long; 
            const_list.push_back(str);
            lexer::next();
            while (lexer::cur_lex_type == COMMA) {
                lexer::next();
                if (lexer::cur_lex_type == POSNUM ||
                        lexer::cur_lex_type == NEGNUM) {
                    TableInterface::FieldCont str;
                    str.cont = lexer::cur_lex_text;
                    str.type = Long;
                    const_list.push_back(str);
                    lexer::next();
                } else {
                    lex_exp.push_back(POSNUM);
                    lex_exp.push_back(NEGNUM);
                    throw ParserException(lex_exp);
                }
            }
            if (lexer::cur_lex_type == CLOSE) {
                lexer::next();
            } else {
                lex_exp.push_back(CLOSE);
                throw ParserException(lex_exp);
            }
            if (lexer::cur_lex_type == END){
                    filter_list = TableInterface::filter_in(table_name, poliz_texts, poliz_types, const_list, not_flag);
                    return;
            } else {
                lex_exp.push_back(END);
                throw ParserException(lex_exp);
            }
        } else {
            lex_exp.push_back(QUOTE);
            lex_exp.push_back(POSNUM);
            lex_exp.push_back(NEGNUM);
            throw ParserException(lex_exp);
        }
    }

    void parser::where_like(bool &n_flag) {
        if (lexer::cur_lex_type == QUOTE) {
            std::string model = lexer::cur_lex_text;
            field_name = poliz_texts[0];
            filter_list = TableInterface::filter_like(table_name, field_name, model, not_flag);
            lexer::next();
            if (lexer::cur_lex_type == END){
                return;
            } else {
                lex_exp.push_back(END);
                throw ParserException(lex_exp);
            }
        } else {
            lex_exp.push_back(QUOTE);
            throw ParserException (lex_exp);
        }
    }
}
