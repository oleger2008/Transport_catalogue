#pragma once

#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace svg {

// ----------- Rgb --------------------
struct Rgb {
	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;

	Rgb() = default;
	Rgb(uint8_t r, uint8_t g, uint8_t b)
	: red(r)
	, green(g)
	, blue(b)
	{}
};

// ----------- Rgba -------------------
struct Rgba {
	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;
	double opacity = 1.;

	Rgba() = default;
	Rgba(uint8_t r, uint8_t g, uint8_t b, double o)
	: red(r)
	, green(g)
	, blue(b)
	, opacity(o)
	{}
};

// ----------- Color ------------------
using Color = std::variant<std::monostate, std::string, Rgb, Rgba>;

// Объявив в заголовочном файле константу со спецификатором inline,
// мы сделаем так, чтобы она была одной на все единицы трансляции,
// которые подключают этот заголовок.
// В противном случае каждая единица трансляции будет использовать свою копию этой константы
inline const Color NoneColor{};

struct OstreamColorPrinter {
	std::ostream& out;

	void operator()(std::monostate);
	void operator()(const std::string& s);
	void operator()(Rgb rgb);
	void operator()(Rgba rgba);
};

// ----------- StrokeLineCap ----------
enum class StrokeLineCap {
	BUTT,
	ROUND,
	SQUARE,
};

std::ostream& operator<<(std::ostream& out, StrokeLineCap line_cap);

// ----------- StrokeLineJoin ---------
enum class StrokeLineJoin {
	ARCS,
	BEVEL,
	MITER,
	MITER_CLIP,
	ROUND,
};

std::ostream& operator<<(std::ostream& out, StrokeLineJoin line_cap);


// ----------- PathProps --------------
template <typename Owner>
class PathProps {
public:
	Owner& SetFillColor(Color color) {
		fill_color_ = std::move(color);
		return AsOwner();
	}
	Owner& SetStrokeColor(Color color) {
		stroke_color_ = std::move(color);
		return AsOwner();
	}
	Owner& SetStrokeWidth(double width) {
		stroke_width_ = width;
		return AsOwner();
	}
	Owner& SetStrokeLineCap(StrokeLineCap line_cap) {
		stroke_line_cap_ = line_cap;
		return AsOwner();
	}
	Owner& SetStrokeLineJoin(StrokeLineJoin line_join) {
		stroke_line_join_ = line_join;
		return AsOwner();
	}

protected:
	~PathProps() = default;

	void RenderAttrs(std::ostream& out) const {
		using namespace std::literals;

		if (fill_color_) {
			out << " fill=\""sv;
			std::visit(OstreamColorPrinter{out}, *fill_color_);
			out << "\""sv;
		}
		if (stroke_color_) {
			out << " stroke=\""sv;
			std::visit(OstreamColorPrinter{out}, *stroke_color_);
			out << "\""sv;
		}
		if (stroke_width_) {
			out << " stroke-width=\""sv;
			out << *stroke_width_;
			out << "\""sv;
		}
		if (stroke_line_cap_) {
			out << " stroke-linecap=\""sv;
			out << *stroke_line_cap_;
			out << "\""sv;
		}
		if (stroke_line_join_) {
			out << " stroke-linejoin=\""sv;
			out << *stroke_line_join_;
			out << "\""sv;
		}
	}

private:
	Owner& AsOwner() {
		// static_cast безопасно преобразует *this к Owner&,
		// если класс Owner - наследник PathProps
		return static_cast<Owner&>(*this);
	}

	std::optional<Color> fill_color_;
	std::optional<Color> stroke_color_;
	std::optional<double> stroke_width_;
	std::optional<StrokeLineCap> stroke_line_cap_;
	std::optional<StrokeLineJoin> stroke_line_join_;
};

// ----------- Point ------------------
struct Point {
	Point() = default;
	Point(double x, double y)
		: x(x)
		, y(y) {
	}
	double x = 0.;
	double y = 0.;
};

Point operator+(const Point& lhs, const Point& rhs);

/* ----------- RenderContext ----------
 * Вспомогательная структура, хранящая контекст для вывода SVG-документа с отступами.
 * Хранит ссылку на поток вывода, текущее значение и шаг отступа при выводе элемента
 */
struct RenderContext {
	RenderContext(std::ostream& out)
		: out(out) {
	}

	RenderContext(std::ostream& out, int indent_step, int indent = 0)
		: out(out)
		, indent_step(indent_step)
		, indent(indent) {
	}

	RenderContext Indented() const {
		return {out, indent_step, indent + indent_step};
	}

	void RenderIndent() const {
		for (int i = 0; i < indent; ++i) {
			out.put(' ');
		}
	}

	std::ostream& out;
	int indent_step = 0;
	int indent = 0;
};

/* ----------- Object -----------------
 * Абстрактный базовый класс Object служит для унифицированного хранения
 * конкретных тегов SVG-документа
 * Реализует паттерн "Шаблонный метод" для вывода содержимого тега
 */
class Object {
public:
	void Render(const RenderContext& context) const;

	virtual ~Object() = default;

private:
	virtual void RenderObject(const RenderContext& context) const = 0;
};

/* ----------- Circle -----------------
 * Класс Circle моделирует элемент <circle> для отображения круга
 * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/circle
 */
class Circle final : public Object, public PathProps<Circle> {
public:
	Circle& SetCenter(Point center);
	Circle& SetRadius(double radius);

private:
	void RenderObject(const RenderContext& context) const override;

	Point center_;
	double radius_ = 1.0;
};

/* ----------- Polyline ---------------
 * Класс Polyline моделирует элемент <polyline> для отображения ломаных линий
 * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/polyline
 */
class Polyline final : public Object, public PathProps<Polyline> {
public:
	// Добавляет очередную вершину к ломаной линии
	Polyline& AddPoint(Point point);

private:
	void RenderObject(const RenderContext& context) const override;

	std::deque<Point> points_;
};

/* ----------- Text -------------------
 * Класс Text моделирует элемент <text> для отображения текста
 * https://developer.mozilla.org/en-US/docs/Web/SVG/Element/text
 */
class Text final : public Object, public PathProps<Text> {
public:
	// Задаёт координаты опорной точки (атрибуты x и y)
	Text& SetPosition(Point pos);

	// Задаёт смещение относительно опорной точки (атрибуты dx, dy)
	Text& SetOffset(Point offset);

	// Задаёт размеры шрифта (атрибут font-size)
	Text& SetFontSize(uint32_t size);

	// Задаёт название шрифта (атрибут font-family)
	Text& SetFontFamily(std::string font_family);

	// Задаёт толщину шрифта (атрибут font-weight)
	Text& SetFontWeight(std::string font_weight);

	// Задаёт текстовое содержимое объекта (отображается внутри тега text)
	Text& SetData(std::string data);

private:
	void RenderObject(const RenderContext& context) const override;

	Point pos_;
	Point offset_;
	uint32_t font_size_ = 1;
	std::optional<std::string> font_family_; // по-умолчанию не выводится
	std::optional<std::string> font_weight_; // по-умолчанию не выводится
	std::string data_; // по-умолчанию текст пуст
};

// ----------- ObjectContainer --------
class ObjectContainer {
public:
	virtual ~ObjectContainer() = default;

	// Метод Add добавляет в svg-документ любой объект-наследник svg::Object.
	template <typename Obj>
	void Add(Obj obj) {
		AddPtr(std::move(std::make_shared<Obj>(std::move(obj))));
	}

	virtual void AddPtr(std::shared_ptr<Object>&& obj) = 0;
};

// ----------- Drawable ---------------
class Drawable {
public:
	virtual ~Drawable() = default;
	virtual void Draw(ObjectContainer& container) const = 0;
protected:
	Drawable() = default;
};

// ----------- Document ---------------
class Document final : public ObjectContainer {
public:
	// Добавляет в svg-документ объект-наследник svg::Object
	void AddPtr(std::shared_ptr<Object>&& obj) override;

	// Выводит в ostream svg-представление документа
	void Render(std::ostream& out) const;

private:
	std::deque<std::shared_ptr<Object>> objects_;
};

}  // namespace svg
