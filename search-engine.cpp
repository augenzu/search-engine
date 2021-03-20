#include <cassert>
#include <cctype>
#include <functional>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <stack>
#include <string>
#include <variant>
#include <vector>


// TODO: inherit all the uncopyables from Uncopyable


class TermId {
public:
    TermId(const std::string &term)
    : _term(term),
    _term_id(_hash(term)) {}

    bool operator<(const TermId &rhs) const {
        bool lt = _term_id < rhs._term_id;
        return lt;
    }

    const std::string &term() const {
        return _term;
    }

private:
    std::string _term;
    int64_t _term_id;
    std::hash<std::string> _hash{};
};


// TODO: need DocIdFabric class
class DocId {
public:
    // TODO: need consts initialization
    static constexpr int64_t ALPHA_ID{ 0 };
    static constexpr int64_t OMEGA_ID{ 35 };
    static constexpr int64_t ANY_ID{ 100502 };
    static constexpr int64_t NO_ID{ -100502 };

    DocId(int64_t doc_id) : _doc_id(doc_id) {}
    DocId(const DocId &) = default;
    DocId(DocId &&) = default;
    ~DocId() = default;
    DocId &operator=(const DocId &) = default;
    DocId &operator=(DocId &&) = default;

    operator std::string() const {
        // std::string repr = "DocId " + std::to_string(_doc_id);
        std::string repr = std::to_string(_doc_id);
        return repr;
    }

    bool operator==(const DocId &rhs) const {
        return _doc_id == rhs._doc_id;
    }
    bool operator!=(const DocId &rhs) const {
        return !(*this == rhs);
    }
    bool operator<(const DocId &rhs) const {
        return _doc_id < rhs._doc_id;
    }
    bool operator<=(const DocId &rhs) const {
        return (*this < rhs || *this == rhs);
    }
    bool operator>(const DocId &rhs) const {
        return !(*this <= rhs);
    }
    bool operator>=(const DocId &rhs) const {
        return !(*this < rhs);
    }

    DocId &operator++() {
        ++_doc_id;
        return *this;
    }

    DocId operator~() const {
        int64_t inverted_id;
        switch (_doc_id) {
            case ANY_ID:
                inverted_id = NO_ID;
                break;
            case NO_ID:
                inverted_id = ANY_ID;
                break;
            case ALPHA_ID:
            case OMEGA_ID:
            default:
                inverted_id = -_doc_id;
        }
        return DocId(inverted_id);
    }

    DocId operator&(const DocId &rhs) const {
        int64_t and_id;
        int64_t rhs_doc_id = rhs._doc_id;
        if (rhs_doc_id == ANY_ID) {
            and_id = _doc_id;
        } else if (rhs_doc_id == NO_ID) {
            and_id = NO_ID;
        } else if (rhs == ~*this) {
            and_id = NO_ID;
        } else {
            and_id = std::max(_doc_id, rhs_doc_id);
        }
        return DocId(and_id);
    }

    DocId operator|(const DocId &rhs) const {
        int64_t and_id;
        int64_t rhs_doc_id = rhs._doc_id;
        if (rhs_doc_id == ANY_ID) {
            and_id = ANY_ID;
        } else if (rhs_doc_id == NO_ID) {
            and_id = _doc_id;
        } else if (rhs == ~*this) {
            and_id = ANY_ID;
        } else {
            and_id = std::min(_doc_id, rhs_doc_id);
        }
        return DocId(and_id);
    }

private:
    int64_t _doc_id;
};


std::ostream &operator<<(std::ostream &out, const DocId &doc_id) {
    out << static_cast<std::string>(doc_id);
    return out;
}


class PostingList {
public:
    PostingList(const std::vector<DocId> doc_ids)
        : _doc_ids(doc_ids),
        _size(doc_ids.size()) {}
    PostingList(const PostingList &) = default;
    PostingList(PostingList &&) = default;
    ~PostingList() = default;
    PostingList &operator=(const PostingList &) = delete;
    PostingList &operator=(PostingList &&) = delete;

    DocId getCurrent() const {
        if (_pos == _size) {
            return DocId::OMEGA_ID;
        } else {
            return _doc_ids[_pos];
        }
    }

    DocId goTo(const DocId &doc_id) const {
        while (_pos != _size && _doc_ids[_pos] < doc_id) {
            ++_pos;
        }
        return getCurrent();
    }

    void reset() const {
        _pos = 0;
    }

private:
    std::vector<DocId> _doc_ids;
    mutable int _size;
    mutable int _pos{ 0 };
};


enum class Token {
    LEFT_BKT,
    RIGHT_BKT,
    NOT,
    AND,
    OR
};


std::string to_string(const std::variant<Token, TermId> &var) {
    if (std::holds_alternative<Token>(var)) {
        Token token = std::get<Token>(var);
        switch (token) {
            case Token::LEFT_BKT:
                return "LEFT_BKT";
            case Token::RIGHT_BKT:
                return "RIGHT_BKT";
            case Token::NOT:
                return "NOT";
            case Token::AND:
                return "AND";
            case Token::OR:
                return "OR";
        }
    } else if (std::holds_alternative<TermId>(var)) {
        TermId term_id = std::get<TermId>(var);
        return "'" + term_id.term() + "'";
    }

    return "";  // never come here; return only to suppress warning
}


class QueryParser {
public:
    QueryParser() = default;
    QueryParser(const QueryParser &) = delete;
    QueryParser(QueryParser &&) = delete;
    ~QueryParser() = default;
    QueryParser &operator=(const QueryParser &) = delete;
    QueryParser &operator=(QueryParser &&) = delete;

    void parse(const std::string &query) {
        tokenize(query);

        _parsed.clear();
        State init_state{ State::OR_EXPR };
        int beg_idx{ 0 };

        int end_idx = parseRecursive(init_state, beg_idx);
    }

    auto parsed() const {
        return _parsed;
    }

    auto term_ids() const {
        return _term_ids;
    }

private:
    enum class State {
        OR_EXPR,
        AND_EXPR,
        UNIT_EXPR,
        NESTED_EXPR
    };

    std::regex _word{"(\\w)"};  // TODO: check this!
    std::vector<std::variant<Token, TermId>> _tokens;
    int _n_tokens;
    std::vector<std::variant<Token, TermId>> _parsed;
    std::set<TermId> _term_ids;

    void tokenize(const std::string query) {
        _tokens.clear();
        _term_ids.clear();

        int idx = 0;
        int end_idx = query.size();

        for (; idx != end_idx; ++idx) {
            while (std::isspace(query[idx])) {
                ++idx;
            }
            if (query[idx] == '(') {
                _tokens.push_back(Token::LEFT_BKT);
            } else if (query[idx] == ')') {
                _tokens.push_back(Token::RIGHT_BKT);
            } else if (query[idx] == '(') {
                _tokens.push_back(Token::LEFT_BKT);
            } else if (query[idx] == '!') {
                _tokens.push_back(Token::NOT);
            } else if (query[idx] == '&') {
                _tokens.push_back(Token::AND);
            } else if (query[idx] == '|') {
                _tokens.push_back(Token::OR);
            } else if (query[idx] == '(') {
                _tokens.push_back(Token::LEFT_BKT);
            } else {  // TODO: use regexp here
                std::string token = "";
                for (; idx != end_idx
                        && (std::isalnum(query[idx]) || query[idx] == '_'); ++idx) {
                    token += query[idx];
                }
                --idx;
                TermId term_id{ token };
                _tokens.push_back(term_id);
                if (!_term_ids.contains(term_id)) {
                    _term_ids.insert(term_id);
                }
            }
        }

        _n_tokens = _tokens.size();
    }

    int parseRecursive(State state, int beg_idx) {
        int idx = beg_idx;

        switch(state) {
            case State::OR_EXPR:
                idx = parseRecursive(State::AND_EXPR, idx);
                while (idx != _n_tokens
                        && std::holds_alternative<Token>(_tokens[idx])
                        && std::get<Token>(_tokens[idx]) == Token::OR) {
                    idx = parseRecursive(State::AND_EXPR, idx + 1);
                    _parsed.push_back(Token::OR);
                }
                return idx;
            case State::AND_EXPR:
                idx = parseRecursive(State::UNIT_EXPR, idx);
                while (idx != _n_tokens
                        && std::holds_alternative<Token>(_tokens[idx])
                        && std::get<Token>(_tokens[idx]) == Token::AND) {
                    idx = parseRecursive(State::UNIT_EXPR, idx + 1);
                    _parsed.push_back(Token::AND);
                }
                return idx;
            case State::UNIT_EXPR: {
                int n_nots = 0;
                for (; std::holds_alternative<Token>(_tokens[idx])
                        && std::get<Token>(_tokens[idx]) == Token::NOT;
                        ++idx, ++n_nots) {}
                if (std::holds_alternative<Token>(_tokens[idx])
                        && std::get<Token>(_tokens[idx]) == Token::LEFT_BKT) {
                    idx = parseRecursive(State::NESTED_EXPR, idx);
                } else {
                    _parsed.push_back(_tokens[idx]);
                    ++idx;
                }
                for (int i = 0; i < n_nots; ++i) {
                    _parsed.push_back(Token::NOT);
                }
                return idx;
            }
            case State::NESTED_EXPR:
                idx = parseRecursive(State::OR_EXPR, idx + 1);
                assert(std::holds_alternative<Token>(_tokens[idx])
                        && std::get<Token>(_tokens[idx]) == Token::RIGHT_BKT);
                ++idx;
                return idx;
        }

        return idx;  // never come here; returs only to suppress warning
    }
};


class SearchEngine {
public:
    SearchEngine(const std::map<TermId, const PostingList> &inverted_index)
        : _inverted_index(inverted_index) {}
    // TODO: uncopyable

    auto search(const std::string &query_str) {
        _query_parser.parse(query_str);
        auto query = _query_parser.parsed();
        auto term_ids = _query_parser.term_ids();
        for (const auto &term_id : term_ids) {
            _inverted_index.at(term_id).reset();
        }

        std::vector<DocId> search_result;
        DocId search_doc_id = _alpha_doc_id;
        ++search_doc_id;

        while (search_doc_id != _omega_doc_id) {
            for (const auto &token : query) {
                if (std::holds_alternative<Token>(token)
                        && std::get<Token>(token) == Token::NOT) {
                    DocId lhs = _stack.top();
                    _stack.pop();
                    _stack.push(~lhs);
                } else if (std::holds_alternative<Token>(token)
                        && std::get<Token>(token) == Token::AND) {
                    DocId lhs = _stack.top();
                    _stack.pop();
                    DocId rhs = _stack.top();
                    _stack.pop();
                    _stack.push(lhs & rhs);
                } else if (std::holds_alternative<Token>(token)
                        && std::get<Token>(token) == Token::OR) {
                    DocId lhs = _stack.top();
                    _stack.pop();
                    DocId rhs = _stack.top();
                    _stack.pop();
                    _stack.push(lhs | rhs);
                } else if (std::holds_alternative<TermId>(token)) {
                    TermId term_id = std::get<TermId>(token);
                    DocId doc_id = _inverted_index.at(term_id).goTo(search_doc_id);
                    _stack.push(doc_id);
                }
            }

            DocId found_doc_id = _stack.top();
            _stack.pop();

            if (found_doc_id == _no_doc_id) {
                ++search_doc_id;
            } else if (found_doc_id == _any_doc_id) {
                search_result.push_back(search_doc_id);
                ++search_doc_id;
            } else if (found_doc_id < _alpha_doc_id) {
                if (found_doc_id != ~search_doc_id) {
                    search_result.push_back(search_doc_id);
                }
                ++search_doc_id;
            } else if (found_doc_id >= _alpha_doc_id) {
                if (found_doc_id == search_doc_id) {
                    search_result.push_back(search_doc_id);
                    ++search_doc_id;
                } else {
                    search_doc_id = found_doc_id;
                }
            }
        }

        return search_result;
    }

private:
    std::map<TermId, const PostingList> _inverted_index;
    QueryParser _query_parser;
    std::stack<DocId> _stack;

    DocId _alpha_doc_id{ DocId::ALPHA_ID };
    DocId _omega_doc_id{ DocId::OMEGA_ID };
    DocId _any_doc_id{ DocId::ANY_ID };
    DocId _no_doc_id{ DocId::NO_ID };
};


int main() {
    std::map<TermId, const PostingList> inverted_index{
        { TermId("cat"), PostingList(std::vector<DocId>{ 1, 4, 7 }) },
        { TermId("door"), PostingList(std::vector<DocId>{ 1, 2, 5, 34 }) },
        { TermId("occasion"), PostingList(std::vector<DocId>{ 4, 6, 8, 9, 10, 13, 19 }) },
        { TermId("actual"), PostingList(std::vector<DocId>{ 13, 17, 19 }) },
        { TermId("batman"), PostingList(std::vector<DocId>{ 1, 5, 6, 9, 10 }) },
        { TermId("main"), PostingList(std::vector<DocId>{ 6, 11 }) }
    };

    SearchEngine search_engine{ inverted_index };

    std::string query;
    std::getline(std::cin, query);
    std::cout << "query: [" << query << "]" << std::endl;

    auto found = search_engine.search(query);

    std::cout << "found: ";
    for (const auto &doc_id : found) {
        std::cout << doc_id << " ";
    }
    std::cout << std::endl;

    return 0;
}
