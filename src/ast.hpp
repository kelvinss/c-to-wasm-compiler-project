#if !defined(AST_H)
#define AST_H

#include <algorithm>
#include <cassert>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

#include "symtable.hpp"
#include "types.hpp"

#define LABEL(TEXT)                                                            \
    virtual const char* const get_label() const { return (TEXT); };

namespace ast {

struct Node {
    LABEL("Node");
    virtual bool is_typed() const { return false; }
    virtual const std::vector<Node*> get_children() const
    {
        return std::vector<Node*>();
    }
    virtual void write_data_repr(std::ostream& stream) const {}
    virtual void write_repr(std::ostream& stream) const
    {
        stream << "(" << this->get_label();
        this->write_data_repr(stream);
        for (auto const& child : this->get_children()) {
            stream << " ";
            child->write_repr(stream);
        }
        stream << ")";
    }
};
}; // namespace ast

std::ostream& operator<<(std::ostream& stream, const ast::Node& node);

namespace ast {
using std::string;
using std::variant;

// Node templates

// Root node of type R with single child of type T
template <typename T, typename R = T>
struct SingleChildBase : R {
    SingleChildBase(T* child) : child(child) { assert(child); }
    virtual const std::vector<Node*> get_children() const
    {
        return std::vector<Node*>{this->child};
    }

  protected:
    T* child;
};

// Root node of type R with two children of types T1 and T2
template <typename T1, typename T2, typename R>
struct TwoChildrenBase : R {
    TwoChildrenBase(T1* left, T2* right) : left(left), right(right)
    {
        assert(left);
        assert(right);
    }
    virtual const std::vector<Node*> get_children() const
    {
        return std::vector<Node*>{this->left, this->right};
    }

  protected:
    T1* left;
    T2* right;
};

// Root node of type R with multiple children of type T
template <typename T, typename R = T>
struct MultiChildrenBase : R {
    virtual void add(T* child)
    {
        assert(child);
        this->children.push_back(child);
    };

    virtual const std::vector<Node*> get_children() const
    {
        // TODO descobrir se dá para usar um simples cast aqui?
        std::vector<Node*> result;
        std::copy(
            this->children.cbegin(),
            this->children.cend(),
            std::back_inserter(result));
        return result;
    }

  protected:
    std::vector<T*> children;
};

struct TypedNode : Node {
    virtual bool is_typed() const { return true; }
    types::Type* get_type() const { return this->type; };

    void set_type(types::Type* t) { this->type = t; };
    void set_type(types::PrimKind k) { this->type = new types::PrimType{k}; };

    virtual void write_data_repr(std::ostream& stream) const
    {
        stream << " \"[" << *(this->type) << "]\"";
    }

  protected:
    types::Type* type = new types::PrimType{types::PrimKind::VOID};
};

struct Expr : TypedNode {};

struct Exprs : MultiChildrenBase<Expr, Node> {
    LABEL("");
};

struct Value : Expr {};

template <typename T, types::PrimKind type_kind>
struct BaseValue : Expr {
    BaseValue(T value)
    {
        this->value = value;
        this->type = new types::PrimType{type_kind};
    };
    virtual void write_data_repr(std::ostream& stream) const
    {
        this->Expr::write_data_repr(stream);
        stream << " ";
        stream << this->value;
    };
    T get_value() const { return this->value; }

  protected:
    T value;
};

struct IntegerValue : BaseValue<long long, types::PrimKind::INTEGER> {
    LABEL("Integer");
    IntegerValue(long long value)
        : BaseValue<long long, types::PrimKind::INTEGER>(value)
    {}
};
struct FloatingValue : BaseValue<double, types::PrimKind::REAL> {
    LABEL("Real");
    FloatingValue(double value)
        : BaseValue<double, types::PrimKind::REAL>(value)
    {}
};
struct CharValue : BaseValue<char, types::PrimKind::CHAR> {
    LABEL("Char");
    CharValue(char value) : BaseValue<char, types::PrimKind::CHAR>(value) {}
};
struct StringValue : BaseValue<size_t, types::PrimKind::VOID> {
    LABEL("String");
    StringValue(size_t value) : BaseValue<size_t, types::PrimKind::VOID>(value)
    {}
};

struct Variable : Expr {
    LABEL("Var");
    Variable(sbtb::NameRef& ref) : ref(ref){};
    const std::string& get_name() const { return this->ref.get().name; };
    void write_data_repr(std::ostream& stream) const
    {
        stream << " \"" << this->get_name() << "\"";
        this->Expr::write_data_repr(stream);
    };

  protected:
    const sbtb::NameRef ref;
};

// Unary operator
struct UnOp : SingleChildBase<Expr> {
    UnOp(Expr* child) : SingleChildBase<Expr>(child)
    {
        this->type = child->get_type();
    }
};

// Binary operator
struct BinOp : MultiChildrenBase<Expr> {
    BinOp(Expr* left, Expr* right)
    {
        assert(left != NULL);
        assert(right != NULL);
        this->children = std::vector<Expr*>{left, right};
    }
};

struct Not : UnOp {
    LABEL("!");
    using UnOp::UnOp;
};

struct BitNot : UnOp {
    LABEL("~");
    using UnOp::UnOp;
};
struct InvertSignal : UnOp {
    LABEL("-");
    using UnOp::UnOp;
};
struct PosfixPlusPlus : UnOp {
    LABEL("++");
    using UnOp::UnOp;
};
struct PosfixMinusMinus : UnOp {
    LABEL("--");
    using UnOp::UnOp;
};
struct PrefixPlusPlus : UnOp {
    LABEL("p++");
    using UnOp::UnOp;
};
struct PrefixMinusMinus : UnOp {
    LABEL("p--");
    using UnOp::UnOp;
};

struct Plus : BinOp {
    LABEL("+");
    using BinOp::BinOp;
};
struct Minus : BinOp {
    LABEL("-");
    using BinOp::BinOp;
};
struct Times : BinOp {
    LABEL("*");
    using BinOp::BinOp;
};
struct Over : BinOp {
    LABEL("/");
    using BinOp::BinOp;
};

struct AddressOf : UnOp {
    LABEL("&x");
    using UnOp::UnOp;
};

struct IndexAccess : BinOp {
    LABEL("v[x]");
    using BinOp::BinOp;
};

struct Call : TwoChildrenBase<Expr, Exprs, Expr> {
    LABEL("\"f(x)\"");
    using TwoChildrenBase<Expr, Exprs, Expr>::TwoChildrenBase;
};

struct Statement : Node {
    LABEL("Statement");
};

struct IfStmt : Statement {
    LABEL("IfStmt");
    IfStmt(Expr* expr, Statement* stmt)
    {
        assert(expr);
        assert(stmt);
        this->expr = expr;
        this->stmt = stmt;
    }
    virtual const std::vector<Node*> get_children() const
    {
        return std::vector<Node*>{this->expr, this->stmt};
    }

  protected:
    Expr* expr;
    Statement* stmt;
};

struct WhileStmt : Statement {
    LABEL("WhileStmt");
    WhileStmt(Expr* expr, Statement* stmt)
    {
        assert(expr);
        assert(stmt);
        this->expr = expr;
        this->stmt = stmt;
    }
    virtual const std::vector<Node*> get_children() const
    {
        return std::vector<Node*>{this->expr, this->stmt};
    }

  protected:
    Expr* expr;
    Statement* stmt;
};

struct DoWhileStmt : Statement {
    LABEL("DoWhileStmt");
    DoWhileStmt(Expr* expr, Statement* stmt)
    {
        assert(expr);
        assert(stmt);
        this->expr = expr;
        this->stmt = stmt;
    }
    virtual const std::vector<Node*> get_children() const
    {
        return std::vector<Node*>{this->expr, this->stmt};
    }

  protected:
    Expr* expr;
    Statement* stmt;
};

struct Block : MultiChildrenBase<Statement> {
    LABEL("Block");
    std::optional<ScopeId> scope_id;

    virtual void add(Statement* stmt)
    {
        // TODO
        // obs.: esse método inteiro pode ter removido em favor do método da
        // super classe que tem o assert
        // assert(stmt);
        if (stmt)
            this->children.push_back(stmt);
    };
    void set_scope(const ScopeId scope_id) { this->scope_id = scope_id; }

    virtual void write_data_repr(std::ostream& stream) const
    {
        if (this->scope_id)
            stream << " " << this->scope_id.value();
    }
};

struct ExpressionStmt : SingleChildBase<Expr, Statement> {
    LABEL("ExpressionStmt");
    using SingleChildBase<Expr, Statement>::SingleChildBase;
};

struct Declaration : Node {
    LABEL("Declaration");
    string name;
};

struct FunctionDefinition : Declaration {
    LABEL("FunctionDefinition");
    FunctionDefinition(Block* body)
    {
        assert(body);
        this->body = body;
    }
    virtual const std::vector<Node*> get_children() const
    {
        return std::vector<Node*>{this->body};
    }

  protected:
    Block* body;
};

struct Program : MultiChildrenBase<Declaration, Node> {
    LABEL("Program");
    void add(Declaration* decl)
    {
        assert(decl);
        this->children.push_back(decl);
    };
};

}; // namespace ast

#endif /* AST_H */
