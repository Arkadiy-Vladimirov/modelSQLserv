#ifndef __Interpreter_HPP__ 
#define __Interpreter_HPP__

#include "tableint.hpp"

#include <vector>
#include <string>

extern "C" 
{
    #include "table.h"
}

namespace Interpreter { 

    using Sockets::Exception;
    
    using namespace lexemes;
    
    // SQL Interpreter Exception class hierarchy
    class InterpretException : public Exception {
    protected:
        static std::string m_Message[];
        int err_pos;
        std::string err_str;
    public:
        enum InterpretExceptionCode {
            EIE_LEX,
            EIE_PARS,
            EIE_LOGIC,
            EIE_DATABASE
        };
        InterpretException(InterpretExceptionCode errcode, int cur_pos, std::string cur_str) :
            Exception(errcode), err_pos(cur_pos), err_str(cur_str) {}
        virtual std::string GetMessage() = 0; 
    };

    // Namespace for lexical analysis
    namespace lexer { 

        // String for analyzed command must be got in server_get_string 
        static std::string expression = "";
       
        // Current symbol position in the analyzing command
        static int cur_pos = 0;
        
        // Current symbol
        static int c;
        
        // Current lexeme position in the analyzing command
        static int cur_lex_pos;

        // Current lexeme type
        static enum lex_type_t cur_lex_type;

        // Current lexeme
        static std::string cur_lex_text;
        
        // Lexer exception class
        class LexerException : public InterpretException {
        public:
            LexerException(InterpretExceptionCode errcode = EIE_LEX, int pos = cur_pos, std::string cur_expr = expression) : 
                InterpretException(errcode, pos, cur_expr) {}
            std::string GetMessage();
        };
      
        // Initialyzing expression string
        void expr_init(std::string);

        // Initialyzing lexer 
        void init();

        // Reading next lexeme 
        void next();

        // Flag for using WHERE-CLAUSE specialities
        static bool where_flag = false;

    }

    namespace parser {

        class ParserException : public InterpretException {
            static std::string lex_name[];
            std::string lex_text;
            lex_type_t lex_error;
            std::vector<lex_type_t> lex_expected;
        public: 
            ParserException(std::vector<lex_type_t> lex_exp, InterpretExceptionCode errcode = EIE_PARS, int cur_lex_pos = lexer::cur_lex_pos,
                std::string cur_expr = lexer::expression, std::string lex_tex = lexer::cur_lex_text, 
                lex_type_t lex_err = lexer::cur_lex_type) : InterpretException(errcode, cur_lex_pos, cur_expr), 
                lex_text(lex_tex), lex_error(lex_err), lex_expected(lex_exp) {}
            std::string GetMessage();
        };

        class LogicException : public InterpretException {
        public:
            LogicException(InterpretExceptionCode errcode = EIE_LOGIC, int pos = lexer::cur_pos, std::string cur_expr = lexer::expression) : 
                InterpretException(errcode, pos, cur_expr) {}
            std::string GetMessage();
        };
        
        enum type_t {EText, EStr, ELogic, ELong};

        // String-answer to client-request
        static std::string answer = "OKAY!\n";
       
        static std::string table_name;
        
        static bool all_flag;

        static TableInterface::FieldCont cur_field_cont;
        static std::vector<struct TableInterface::FieldCont> line_params;

        static FieldDef cur_field_def; 
        static std::vector<struct FieldDef> table_params;
        
        static std::string field_name;
        static std::vector<std::string> field_name_list;

        static std::vector<std::string> poliz_texts;
        static std::vector<lex_type_t> poliz_types;
        static std::vector<lex_type_t> lex_exp;
        static std::vector<int> filter_list;
        static std::vector<std::string> const_list;
  	static bool not_flag;

        // Get answer
        std::string get_answer();

        //prototypes
        void init_parse();
        void select_p();
        void insert_p();
        void update_p();
        void delete_p(); 
        void create_p(); 
        void drop_p();
        void flds_name_list_p();
        void fld_name_p();
        void flds_cont_list_p();
        void fld_cont_p();
        void flds_def_list_p();
        void fld_def_p();
        void txt_type();
        void Push(std::string &, lex_type_t);
        void where_p();
        type_t where_expr();
        type_t where_expr_term();
        type_t where_expr_mult();
        type_t where_open(type_t);
        void where_like(bool&);
        void where_in(bool&);
    }
}

#endif 
