#include "Vectors.h"

#include <vector>
#include <algorithm>

float VectorHelper::dist2DPointToSegmentSq(const Vec2 &p, const Vec2 &a, const Vec2 &b) {
  Vec2 ab = {b.x - a.x, b.y - a.y};
  Vec2 ap = {p.x - a.x, p.y - a.y};

  // Dot product to project p onto segment ab
  float ab2 = ab.x * ab.x + ab.y * ab.y;
  if (ab2 == 0.0f) {
    float dx = p.x - a.x, dy = p.y - a.y;
    return dx * dx + dy * dy;
  }

  // Clamp projection factor t between 0 and 1
  float t = std::max(0.0f, std::min(1.0f, (ap.x * ab.x + ap.y * ab.y) / ab2));

  Vec2 proj = {a.x + t * ab.x, a.y + t * ab.y};
  float dx = p.x - proj.x;
  float dy = p.y - proj.y;
  return dx * dx + dy * dy;
}

bool VectorHelper::isPointOnPath(const std::vector<Vec3> &points, Vec3 testPoint, float tolerance, bool circular) {
  if (points.size() < 2) return false;

  Vec2 p = testPoint.toVec2();
  float maxDistSq = tolerance * tolerance;
  size_t numSegments = circular ? points.size() : points.size() - 1;

  for (size_t i = 0; i < numSegments; ++i) {
    Vec3 index = points[i];
    Vec3 nextIndex = points[(i + 1) % points.size()];
    Vec2 a = index.toVec2();
    Vec2 b = nextIndex.toVec2();

    if (dist2DPointToSegmentSq(p, a, b) <= maxDistSq) {
      return true;
    }
  }

  return false;
}