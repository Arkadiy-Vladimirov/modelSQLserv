#ifndef __TABLE_INT__
#define __TABLE_INT__

#include <vector>
#include <string>
#include <stack>
#include <regex>

extern "C" {
    #include "table.h"
}

namespace lexemes {
// Availible lexeme types
    enum lex_type_t {
        SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, WHERE, FROM, INTO, SET, TABLE, LIKE, NOT, 
        IN, WALL/*ALL FOR WHERECLAUSE*/, OR, AND, TEXT, LONG, POSNUM, NEGNUM, ALL, IDEN, MINUS, 
        PLUS, MULT, DIV, MOD, COMMA, OPEN, CLOSE, QUOTE, LESS, EQUALLE, GREATER, EQUALGE, EQUAL, 
        NOTEQUAL, END,
    };
}

namespace TableInterface {

    using namespace lexemes;

    class DataBaseException : public Sockets::Exception {
    public:
        DataBaseException(Errors errcode) : Exception(errcode) {}
        std::string GetMessage();
    };
   
    struct FieldCont {
        std::string cont;
        FieldType type;
    };
    std::vector<int> filter_all(std::string &table_name);

    std::vector<int> filter_in(std::string &table_name, std::vector<std::string> &poliz_texts, std::vector<lex_type_t> &poliz_types,
        std::vector<FieldCont> &const_list, bool &not_flag);

    std::vector<int> filter_like (std::string &table_name, std::string &field_name, std::string &model, bool &not_flag);

    std::vector<int> filter_logic(std::string &table_name,  std::vector<std::string> &poliz_texts, std::vector<lex_type_t> &poliz_types);

    std::string print_table(std::string &table_name);

    void get_type (std::string &table_name, std::string &field_name, FieldType *ptype);

    bool in_list(std::string &str, std::vector<std::string> &list);

    std::string select(std::string &table_name, std::vector<std::string> &field_list, std::vector<int> &fields_filtered, bool &all_flag);

    void insert(std::string &table_name, const std::vector<FieldCont> &field_vect);
     
    void update(std::string &table_name, std::string &field_name, std::vector<std::string> &poliz_texts, std::vector<lex_type_t> &poliz_types, std::vector<int> &fields_filtered);
    
    void delete_fields(std::string &table_name, std::vector<int> &fields_filtered);

    void drop(std::string &table_name);

    void create(std::string &table_name, std::vector<struct FieldDef> &field_vect);

    FieldCont poliz_count(THandle td, std::vector<std::string> &poliz_texts, std::vector<lex_type_t> &poliz_types);
}

#endif
