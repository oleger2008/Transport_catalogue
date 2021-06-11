#include "json_builder.h"

namespace json {

KeyValueContext Builder::Key(std::string key) {
	// если узлов нет, или если посл элемент не словарь
	// (это включает в себя случай, когда последний элемент является строкой ключом)
	if (nodes_stack_.empty() || !nodes_stack_.back()->IsDict()) {
		throw std::logic_error("Unable to make Key to non-Dictionary Node");
	}
	nodes_stack_.emplace_back(std::make_unique<Node>(std::move(key)));
	return KeyValueContext{*this};
}

Builder& Builder::Value(Node::Value value) {
	if (UnableToStartNode()) {
		throw std::logic_error("Unable to make Value");
	}
	if (nodes_stack_.empty()) { // случай для корневой ноды
		if (std::holds_alternative<std::nullptr_t>(value) && !nodes_stack_.capacity()) {
			is_root_valued_by_null_ = true;
		}

		nodes_stack_.emplace_back(std::make_unique<Node>(std::move(std::visit(NodeGetter{}, std::move(value)))));
		root_ = std::move(*(nodes_stack_.back()));
		nodes_stack_.pop_back();

	} else if (nodes_stack_.back()->IsArray()) { // для массива
		Array array = std::move(nodes_stack_.back()->AsArray());
		nodes_stack_.pop_back();
		array.emplace_back(std::move(std::visit(NodeGetter{}, std::move(value))));
		nodes_stack_.emplace_back(std::make_unique<Node>(std::move(array)));

	} else { // для словаря, для которого уже подготовлен ключ
		std::string key = std::move(nodes_stack_.back()->AsString());
		nodes_stack_.pop_back();
		if (!nodes_stack_.back()->IsDict()) {
			throw std::logic_error("Expected Dictionary");
		}
		Dict dict = std::move(nodes_stack_.back()->AsDict());
		nodes_stack_.pop_back();
		dict[std::move(key)] = std::move(std::visit(NodeGetter{}, std::move(value)));
		nodes_stack_.emplace_back(std::make_unique<Node>(std::move(dict)));

	}
	return *this;
}

DictItemContext Builder::StartDict() {
	if (UnableToStartNode()) {
		throw std::logic_error("Unable to start Dictionary");
	}
	nodes_stack_.emplace_back(std::make_unique<Node>(Dict()));
	return DictItemContext{*this};
}

ArrayItemContext Builder::StartArray() {
	if (UnableToStartNode()) {
		throw std::logic_error("Unable to start Array");
	}
	nodes_stack_.emplace_back(std::make_unique<Node>(Array()));
	return ArrayItemContext{*this};
}

Builder& Builder::EndDict() {
	if (nodes_stack_.empty() || !nodes_stack_.back()->IsDict()) {
		throw std::logic_error("Expected Dictionary to end");
	}
	auto temp = std::move(nodes_stack_.back());
	nodes_stack_.pop_back();
	return Value(std::move(temp->AsDict()));
}

Builder& Builder::EndArray() {
	if (nodes_stack_.empty() || !nodes_stack_.back()->IsArray()) {
		throw std::logic_error("Expected Dictionary to end");
	}

	auto temp = std::move(nodes_stack_.back());
	nodes_stack_.pop_back();
	return Value(std::move(temp->AsArray()));
}

Node Builder::Build() {
	if (!nodes_stack_.capacity()) {
		throw std::logic_error("Empty object");
	}
	if (!nodes_stack_.empty()) {
		throw std::logic_error("Unfinished object");
	}
	return root_;
}

bool Builder::UnableToStartNode() {
	return  is_root_valued_by_null_
			|| (!root_.IsNull() && nodes_stack_.capacity())
			|| !(nodes_stack_.empty()
				|| nodes_stack_.back()->IsArray()
				|| nodes_stack_.back()->IsString() // ключ-строка
			);
}

// ------ DictItemContext -------------
DictItemContext::DictItemContext(Builder& builder) : builder_(builder) {}
KeyValueContext DictItemContext::Key(std::string key) {
	return builder_.Key(std::move(key));
}
Builder& DictItemContext::EndDict() {
	return builder_.EndDict();
}

// ------ ArrayItemContext ------------
ArrayItemContext::ArrayItemContext(Builder& builder) : builder_(builder) {}
ArrayItemContext ArrayItemContext::Value(Node::Value value) {
	builder_.Value(std::move(value));
	return ArrayItemContext{builder_};
}
DictItemContext ArrayItemContext::StartDict() {
	return builder_.StartDict();
}
ArrayItemContext ArrayItemContext::StartArray() {
	return builder_.StartArray();
}
Builder& ArrayItemContext::EndArray() {
	return builder_.EndArray();
}

// ------ KeyValueContext -------------
KeyValueContext::KeyValueContext(Builder& builder) : builder_(builder) {}
DictItemContext KeyValueContext::Value(Node::Value value) {
	builder_.Value(std::move(value));
	return DictItemContext{builder_};
}
DictItemContext KeyValueContext::StartDict() {
	return builder_.StartDict();
}
ArrayItemContext KeyValueContext::StartArray() {
	return builder_.StartArray();
}

}  // namespace json
