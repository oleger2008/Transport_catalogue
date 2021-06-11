#include "svg.h"

namespace svg {

using namespace std::literals;

// ----------- Color ------------------
void OstreamColorPrinter::operator()(std::monostate) {
	using namespace std::literals;
	out << "none"sv;
}

void OstreamColorPrinter::operator()(const std::string& s) {
	out << s;
}

void OstreamColorPrinter::operator()(Rgb rgb) {
	using namespace std::literals;
	out << "rgb("sv
			<< static_cast<int>(rgb.red) << ","sv
			<< static_cast<int>(rgb.green) << ","sv
			<< static_cast<int>(rgb.blue) << ")"sv;
}

void OstreamColorPrinter::operator()(Rgba rgba) {
	using namespace std::literals;
	out << "rgba("sv
			<< static_cast<int>(rgba.red) << ","sv
			<< static_cast<int>(rgba.green) << ","sv
			<< static_cast<int>(rgba.blue) << ","sv
			<< rgba.opacity << ")"sv;
}

// ----------- StrokeLineCap ----------
std::ostream& operator<<(std::ostream& out, StrokeLineCap line_cap) {
	using namespace std::literals;
	switch (line_cap) {
		case (StrokeLineCap::BUTT):
			out << "butt"sv;
			break;
		case (StrokeLineCap::ROUND):
			out << "round"sv;
			break;
		case (StrokeLineCap::SQUARE):
			out << "square"sv;
			break;
	}
	return out;
}

// ----------- StrokeLineJoin ---------
std::ostream& operator<<(std::ostream& out, StrokeLineJoin line_cap) {
	using namespace std::literals;
	switch(line_cap) {
		case (StrokeLineJoin::ARCS):
			out << "arcs"sv;
			break;
		case (StrokeLineJoin::BEVEL):
			out << "bevel"sv;
			break;
		case (StrokeLineJoin::MITER):
			out << "miter"sv;
			break;
		case (StrokeLineJoin::MITER_CLIP):
			out << "miter-clip"sv;
			break;
		case (StrokeLineJoin::ROUND):
			out << "round"sv;
			break;
	}
	return out;
}

// ----------- Point ------------------
Point operator+(const Point& lhs, const Point& rhs) {
	return Point{lhs.x + rhs.x, lhs.y + rhs.y};
}

// ----------- Object -----------------
void Object::Render(const RenderContext& context) const {
	context.RenderIndent();

	// Делегируем вывод тега своим подклассам
	RenderObject(context);

	context.out << std::endl;
}

// ---------- Circle ------------------
Circle& Circle::SetCenter(Point center)  {
	center_ = center;
	return *this;
}

Circle& Circle::SetRadius(double radius)  {
	radius_ = radius;
	return *this;
}

void Circle::RenderObject(const RenderContext& context) const {
	auto& out = context.out;
	out << "<circle cx=\""sv << center_.x << "\" cy=\""sv << center_.y << "\" "sv;
	out << "r=\""sv << radius_ << "\""sv;

	// Выводим атрибуты, унаследованные от PathProps
	RenderAttrs(context.out);

	out << "/>"sv;
}

// ---------- Polyline ----------------
Polyline& Polyline::AddPoint(Point point) {
	points_.push_back(point);
	return *this;
}

void Polyline::RenderObject(const RenderContext& context) const {
	// <polyline points="0,100 50,25 50,75 100,0" />
	auto& out = context.out;
	out << "<polyline points=\""sv;
	bool is_first = true;
	for (const auto [x, y] : points_) {
		if (is_first) {
			out << x << ","sv << y;
			is_first = false;
			continue;
		}
		out << " "sv << x << ","sv << y;
	}
	out << "\""sv;

	// Выводим атрибуты, унаследованные от PathProps
	RenderAttrs(context.out);

	out << "/>"sv;
}

// ---------- Text --------------------

// Задаёт координаты опорной точки (атрибуты x и y)
Text& Text::SetPosition(Point pos) {
	pos_ = pos;
	return *this;
}

// Задаёт смещение относительно опорной точки (атрибуты dx, dy)
Text& Text::SetOffset(Point offset) {
	offset_ = offset;
	return *this;
}

// Задаёт размеры шрифта (атрибут font-size)
Text& Text::SetFontSize(uint32_t size) {
	font_size_ = size;
	return *this;
}

// Задаёт название шрифта (атрибут font-family)
Text& Text::SetFontFamily(std::string font_family) {
	font_family_ = std::move(font_family);
	return *this;
}

// Задаёт толщину шрифта (атрибут font-weight)
Text& Text::SetFontWeight(std::string font_weight) {
	font_weight_ = std::move(font_weight);
	return *this;
}

// Задаёт текстовое содержимое объекта (отображается внутри тега text)
Text& Text::SetData(std::string data) {
	data_ = std::move(data);
	return *this;
}

void Text::RenderObject(const RenderContext& context) const {
// <text x="35" y="20" dx="0" dy="6" font-size="12" font-family="Verdana" font-weight="bold">Hello C++</text>
	auto& out = context.out;
	out << "<text"sv;

	// Выводим атрибуты, унаследованные от PathProps
	RenderAttrs(context.out);

	out << " x=\""sv << pos_.x << "\""sv << " y=\""sv << pos_.y << "\""sv
		<< " dx=\""sv << offset_.x << "\""sv << " dy=\""sv << offset_.y << "\""sv
		<< " font-size=\""sv << font_size_ << "\""sv;

	if (font_family_) {
		out << " font-family=\""sv << *font_family_ << "\""sv ;
	}
	if (font_weight_) {
		out << " font-weight=\""sv << *font_weight_ << "\""sv ;
	}
	out << ">"sv;

	for (const char c : data_) {
		if (c == '\"') {
			out << "&quot;"sv;
		} else if (c == '\'') {
			out << "&apos;"sv;
		} else if (c == '<') {
			out << "&lt;"sv;
		} else if (c == '>') {
			out << "&gt;"sv;
		} else if (c == '&') {
			out << "&amp;"sv;
		} else {
			out << c;
		}
	}

	out << "</text>"sv;
}

// ---------- Document ----------------

// Добавляет в svg-документ объект-наследник svg::Object
void Document::AddPtr(std::shared_ptr<Object>&& obj) {
	objects_.emplace_back(std::move(obj));
}

// Выводит в ostream svg-представление документа
void Document::Render(std::ostream& out) const {
	out << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"sv << std::endl;
	out << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">"sv << std::endl;
	for (const auto& obj : objects_) {
		obj->Render(RenderContext{out, 2, 2});
	}
	out << "</svg>"sv;
}

}  // namespace svg
