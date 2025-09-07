// scanline_filler.h
#pragma once
#include <algorithm>
#include <climits>
#include <cmath>
#include <functional>
#include <list>
#include <stdexcept>
#include <vector>
#include <limits>

namespace scanline {

struct Point {
    double x, y;
};

using Contour = std::vector<Point>;

struct FillResult {
    bool success = false;
    std::vector<int> xSpans;        // x1, x2, x1, x2, ...
    std::vector<int> spanCounts;    // количество x-координат на каждой строке
    double minx = 0.0, miny = 0.0, maxx = 0.0, maxy = 0.0;
    int pixelWidth = 0, pixelHeight = 0;
};

struct Edge {
    double x1, y1, x2, y2;
    double ymin, ymax;
    double delta_y;
    mutable double current_x;
    bool includeBottom;

    Edge(Point p1, Point p2)
        : x1(p1.x),
          y1(p1.y),
          x2(p2.x),
          y2(p2.y),
          ymin(std::min(p1.y, p2.y)),
          ymax(std::max(p1.y, p2.y)),
          delta_y(p2.y - p1.y),
          current_x(p1.x),
          includeBottom(false) {}

    // inline и оптимизированная интерполяция
    double x(double scan_y) const {
        if (std::abs(delta_y) < 1e-10) {
            return x1;
        }
        // Избегаем лишнего деления: (x2 - x1) / delta_y — предвычисляемо, но здесь не критично
        return x1 + (scan_y - y1) * (x2 - x1) / delta_y;
    }
};

// Универсальное округление до ближайшего целого
static inline int roundDot(double x) {
    return static_cast<int>(std::floor(x + 0.5));
}

class ScanlineFiller {
public:
    using SpanCallback = std::function<void(int x1, int x2, int y)>;

	FillResult fillWithResult(const std::vector<Contour>& contours) {
		FillResult result;
	
		if (contours.empty()) return result;
	
		// --- 1. Вычисляем bounding box ---
		result.minx = std::numeric_limits<double>::max();
		result.miny = std::numeric_limits<double>::max();
		result.maxx = std::numeric_limits<double>::lowest();
		result.maxy = std::numeric_limits<double>::lowest();
	
		size_t totalEdgesHint = 0;
		for (const auto& contour : contours) {
			if (contour.size() < 3) continue;
			for (const auto& p : contour) {
				result.minx = std::min(result.minx, p.x);
				result.miny = std::min(result.miny, p.y);
				result.maxx = std::max(result.maxx, p.x);
				result.maxy = std::max(result.maxy, p.y);
			}
			totalEdgesHint += contour.size();
		}
	
		if (totalEdgesHint == 0) return result;
	
		double width = result.maxx - result.minx;
		double height = result.maxy - result.miny;
		result.pixelWidth = std::max(1, roundDot(width));
		result.pixelHeight = std::max(1, roundDot(height));
	
		if (result.pixelWidth <= 1 && result.pixelHeight <= 1) {
			return result;
		}
	
		// --- 2. Построение рёбер ---
		std::vector<Edge> edges;
		edges.reserve(totalEdgesHint);
	
		for (const auto& contour : contours) {
			if (contour.size() < 3) continue;
			Point p1 = contour.back();
			for (const auto& p2 : contour) {
				if (std::abs(p1.y - p2.y) > 1e-6) {
					edges.emplace_back(p1, p2);
				}
				p1 = p2;
			}
		}
	
		if (edges.empty()) return result;
	
		// --- 3. Отметка нижних вершин ---
		auto prevIt = edges.end();
		if (!edges.empty()) {
			--prevIt;
			for (auto it = edges.begin(); it != edges.end(); ++it) {
				const auto& prev = *prevIt;
				const auto& curr = *it;
				if (prev.delta_y > 1e-6 && curr.delta_y < -1e-6) {
					const_cast<Edge&>(prev).includeBottom = true;
					const_cast<Edge&>(curr).includeBottom = true;
				}
				prevIt = it;
			}
		}
	
		std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
			return a.ymin < b.ymin;
		});
	
		// --- 4. Scanline ---
		auto currentEdge = edges.begin();
		std::list<Edge*> active;
	
		int start_y = roundDot(result.miny);
		int end_y = start_y + result.pixelHeight - 1;
	
		// --- Инициализируем данные ---
		result.xSpans.clear();
		result.spanCounts.clear();
		result.spanCounts.reserve(end_y - start_y + 1);  // reserve по высоте
	
		int currentY = INT_MIN;
		int intersectionCount = 0;
	
		for (int y_int = start_y; y_int <= end_y; ++y_int) {
			double scan_y = y_int + 0.5;
	
			// Добавляем активные рёбра
			while (currentEdge != edges.end() && scan_y >= currentEdge->ymin - 1e-6) {
				active.push_back(&(*currentEdge));
				++currentEdge;
			}
	
			// Удаляем завершённые
			active.remove_if([scan_y](Edge* e) {
				return scan_y > e->ymax + 1e-6 ||
					   (std::abs(scan_y - e->ymax) < 1e-6 && !e->includeBottom);
			});
	
			// Пересечения
			std::vector<double> intersections;
			for (Edge* e : active) {
				intersections.push_back(e->x(scan_y));
			}
	
			// --- Имитируем поведение original fill: ---
			// Если y изменился — закрываем предыдущую строку
			if (y_int != currentY) {
				if (currentY != INT_MIN) {
					result.spanCounts.push_back(intersectionCount);
				}
				currentY = y_int;
				intersectionCount = 0;
			}
	
			if (intersections.empty()) {
				// Пустая строка: intersectionCount остаётся 0
				// span будет добавлен в spanCounts при смене Y или в конце
				continue;
			}
	
			if (intersections.size() % 2 != 0) continue;
	
			std::sort(intersections.begin(), intersections.end());

            // Обработка пар пересечений	
			for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
				double x1 = intersections[i];
				double x2 = intersections[i + 1];

				
                // Пропускаем вырожденные интервалы	
				if (std::abs(x2 - x1) < 0.5) continue;
	
				int begin_span = roundDot(x1);
				int end_span = begin_span + std::max(1, roundDot(x2 - x1)) - 1;
	
				if (begin_span <= end_span) {
					result.xSpans.push_back(begin_span);
					result.xSpans.push_back(end_span);
					intersectionCount += 2;
					result.success = true;
				}
			}
		}
	
		// Закрываем последнюю строку
		if (currentY != INT_MIN) {
			result.spanCounts.push_back(intersectionCount);
		}
	
		return result;
	}
};

}  // namespace scanline