#pragma once

#include <memory>
#include <string>
#include <optional>
#include <vector>

#include "json.h"

namespace json {

class DictItemContext;
class ArrayItemContext;
class KeyValueContext;

class Builder {
private:
	struct NodeGetter {
		Node operator() (std::nullptr_t) const {
			return Node();
		}
		Node operator() (std::string&& value) const {
			return Node(std::move(value));
		}
		Node operator() (bool&& value) const {
			return Node(value);
		}
		Node operator() (int&& value) const {
			return Node(value);
		}
		Node operator() (double&& value) const {
			return Node(value);
		}
		Node operator() (Array&& value) const {
			return Node(std::move(value));
		}
		Node operator() (Dict&& value) const {
			return Node(std::move(value));
		}
	};

public:
	Builder() = default;

	KeyValueContext Key(std::string key);
	Builder& Value(Node::Value value);
	DictItemContext StartDict();
	ArrayItemContext StartArray();
	Builder& EndDict();
	Builder& EndArray();
	Node Build();

private:
	Node root_;
	std::vector<std::unique_ptr<Node>> nodes_stack_;
	bool is_root_valued_by_null_ = false;

	bool UnableToStartNode();
};


class DictItemContext {
public:
	DictItemContext(Builder& builder);
	KeyValueContext Key(std::string key);
	Builder& EndDict();
private:
	Builder& builder_;
};

class ArrayItemContext {
public:
	ArrayItemContext(Builder& builder);
	ArrayItemContext Value(Node::Value value);
	DictItemContext StartDict();
	ArrayItemContext StartArray();
	Builder& EndArray();
private:
	Builder& builder_;
};

class KeyValueContext {
public:
	KeyValueContext(Builder& builder);
	DictItemContext Value(Node::Value value);
	DictItemContext StartDict();
	ArrayItemContext StartArray();
private:
	Builder& builder_;
};

}  // namespace json

