#include "geometry.h"

vector<float32, 2> quadratic_bezier_point(
	vector<float32, 2> p1,
	vector<float32, 2> p2,
	vector<float32, 2> p3,
	float32 t)
{
	return vector<float32, 2>(
		(1.0f - t) * ((1.0f - t) * p1.x + (2.0f * t) * p2.x) + (t * t) * p3.x,
		(1.0f - t) * ((1.0f - t) * p1.y + (2.0f * t) * p2.y) + (t * t) * p3.y);
}

vector<float32, 2> elliptic_arc_point(
	vector<float32, 2> center,
	float32 rx,
	float32 ry,
	float32 angle)
{
	vector<float32, 2> n = vector<float32, 2>(cos(angle), sin(angle));
	return center + vector<float32, 2>(rx * n.x, ry * n.y);
}

vector<float32, 2> closest_line_point(
	vector<float32, 2> point,
	vector<float32, 2> line_point1,
	vector<float32, 2> line_point2)
{
	vector<float32, 2> dir = line_point2 - line_point1;
	float32 len = vector_length(dir),
		dist = vector_dot(point - line_point1, dir /= len);
	if(dist < float32(0)) return line_point1;
	else if(dist > len) return line_point2;
	return line_point1 + dir * dist;
}

vector<float32, 2> closest_infinite_line_point(
	vector<float32, 2> point,
	vector<float32, 2> linePoint1,
	vector<float32, 2> linePoint2)
{
	vector<float32, 2> dir = vector_normal(linePoint2 - linePoint1);
	return linePoint1 + dir * vector_dot(point - linePoint1, dir);
}

bool intersect_lines(
	vector<float32, 2> line1_point1,
	vector<float32, 2> line1_point2,
	vector<float32, 2> line2_point1,
	vector<float32, 2> line2_point2,
	vector<float32, 2> *intersect_point)
{
	vector<float32, 2> v1 = line1_point2 - line1_point1,
		v2 = line2_point2 - line2_point1;
	if(abs(v1.x * v2.y - v1.y * v2.x) < 0.0001f) return false;
	float32 c = v2.x * (line1_point1.y - line2_point1.y) - v2.y * (line1_point1.x - line2_point1.x);
	intersect_point->x = line1_point1.x + c * v1.x / (v1.x * v2.y - v1.y * v2.x);
	intersect_point->y = line1_point1.y + c * v1.y / (v1.x * v2.y - v1.y * v2.x);
	return true;
}

bool ccw_test(vector<float32, 2> a, vector<float32, 2> b, vector<float32, 2> c)
{
	return (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y) >= 0.0f;
}

matrix<float32, 3, 3> scaling_matrix(float32 x, float32 y, vector<float32, 2> origin)
{
	matrix<float32, 3, 3> mat;
	mat.m[0][0] = x;
	mat.m[0][1] = 0.0f;
	mat.m[0][2] = 0.0f;
	mat.m[1][0] = 0.0f;
	mat.m[1][1] = y;
	mat.m[1][2] = 0.0f;
	mat.m[2][0] = (1.0f - x)*origin.x;
	mat.m[2][1] = (1.0f - y)*origin.y;
	mat.m[2][2] = 1.0f;
	return mat;
}

matrix<float32, 3, 3> rotating_matrix(float32 angle, vector<float32, 2> origin)
{
	matrix<float32, 3, 3> mat;
	vector<float32, 2> n = vector<float32, 2>(cos(angle), sin(angle));
	mat.m[0][0] = n.x;
	mat.m[0][1] = n.y;
	mat.m[0][2] = 0.0f;
	mat.m[1][0] = -n.y;
	mat.m[1][1] = n.x;
	mat.m[1][2] = 0.0f;
	mat.m[2][0] = (1.0f - mat.m[0][0])*origin.x - mat.m[1][0] * origin.y;
	mat.m[2][1] = (1.0f - mat.m[1][1])*origin.y - mat.m[0][1] * origin.x;
	mat.m[2][2] = 1.0f;
	return mat;
}

matrix<float32, 3, 3> translating_matrix(float32 x, float32 y)
{
	matrix<float32, 3, 3> mat;
	mat.m[0][0] = 1.0f;
	mat.m[0][1] = 0.0f;
	mat.m[0][2] = 0.0f;
	mat.m[1][0] = 0.0f;
	mat.m[1][1] = 1.0f;
	mat.m[1][2] = 0.0f;
	mat.m[2][0] = x;
	mat.m[2][1] = y;
	mat.m[2][2] = 1.0f;
	return mat;
}

geometry_path::geometry_path()
{
	orientation = face_orientation::counterclockwise;
}

void geometry_path::move(vector<float32, 2> point)
{
	geometry_path_data unit;
	unit.type = geometry_path_unit::move;
	unit.p1 = point;
	data.push(unit);
}

void geometry_path::push_line(vector<float32, 2> point)
{
	geometry_path_data unit;
	unit.type = geometry_path_unit::line;
	unit.p1 = point;
	data.push(unit);
}

void geometry_path::push_quadratic_arc(vector<float32, 2> point1, vector<float32, 2> point2)
{
	geometry_path_data unit;
	unit.type = geometry_path_unit::quadratic_arc;
	unit.p1 = point1;
	unit.p2 = point2;
	data.push(unit);
}

void geometry_path::push_elliptic_arc(vector<float32, 2> point, float32 radius_ratio, float32 begin_angle, float32 end_angle, float32 rotation)
{
	if(data.size == 0) return;
	vector<float32, 2> n1 = vector<float32, 2>(cos(begin_angle), sin(begin_angle)),
		n2 = vector<float32, 2>(cos(end_angle), sin(end_angle)),
		nr = vector<float32, 2>(cos(rotation), sin(rotation)),
		p1;
	if(data[data.size - 1].type == geometry_path_unit::quadratic_arc)
		p1 = data[data.size - 1].p2;
	else p1 = data[data.size - 1].p1;
	geometry_path_data unit;
	unit.type = geometry_path_unit::elliptic_arc;
	float32 d1 = (nr.x + nr.y * nr.y / nr.x) * (n1.x - n2.x),
		d2 = nr.x * (n2.y - n1.y) - nr.y * nr.y / nr.x * (n1.y - n2.y);
	unit.rx = (p1.y * nr.y / nr.x - point.y * nr.y / nr.x - point.x + p1.x) / d1;
	unit.ry = (point.y - p1.y - point.x * nr.y / nr.x + p1.x * nr.y / nr.x) / d2;
	if(abs(d1) < 0.0001f)
		unit.rx = unit.ry / radius_ratio;
	else if(abs(d2) < 0.0001f)
		unit.ry = unit.rx * radius_ratio;
	unit.p2.x = p1.x - unit.rx * n1.x * nr.x + unit.ry * n1.y * nr.y;
	unit.p2.y = p1.y - unit.rx * n1.x * nr.y - unit.ry * n1.y * nr.x;
	unit.begin_angle = begin_angle;
	unit.end_angle = end_angle;
	unit.rotation = rotation;
	matrix<float32, 3, 3> transform = rotating_matrix(rotation, unit.p2);
	unit.p1 = elliptic_arc_point(unit.p2, unit.rx, unit.ry, unit.end_angle);
	vector<float32, 3> p = vector<float32, 3>(unit.p1.x, unit.p1.y, 1.0f) * transform;
	unit.p1 = vector<float32, 2>(p.x, p.y);
	data.push(unit);
}

void geometry_path::push_rectangle(rectangle<float32> rect)
{
	move(vector<float32, 2>(rect.position.x, rect.position.y));
	push_line(vector<float32, 2>(rect.position.x + rect.extent.x, rect.position.y));
	push_line(vector<float32, 2>(rect.position.x + rect.extent.x, rect.position.y + rect.extent.y));
	push_line(vector<float32, 2>(rect.position.x, rect.position.y + rect.extent.y));
	push_line(vector<float32, 2>(rect.position.x, rect.position.y));
}

void geometry_path::push_rounded_rectangle(rounded_rectangle<float32> rrect)
{
	move(vector<float32, 2>(rrect.rect.position.x + rrect.rx, rrect.rect.position.y));
	push_line(vector<float32, 2>(rrect.rect.position.x + rrect.rect.extent.x - rrect.rx, rrect.rect.position.y));
	push_elliptic_arc(vector<float32, 2>(rrect.rect.position.x + rrect.rect.extent.x, rrect.rect.position.y + rrect.ry),
		rrect.ry / rrect.rx, 0.75f, 0.0f, 0.0f);
	push_line(vector<float32, 2>(rrect.rect.position.x + rrect.rect.extent.x, rrect.rect.position.y + rrect.rect.extent.y - rrect.ry));
	push_elliptic_arc(vector<float32, 2>(rrect.rect.position.x + rrect.rect.extent.x - rrect.rx, rrect.rect.position.y + rrect.rect.extent.y),
		rrect.ry / rrect.rx, 0.0f, 0.25f, 0.0f);
	push_line(vector<float32, 2>(rrect.rect.position.x + rrect.rx, rrect.rect.position.y + rrect.rect.extent.y));
	push_elliptic_arc(vector<float32, 2>(rrect.rect.position.x, rrect.rect.position.y + rrect.rect.extent.y - rrect.ry),
		rrect.ry / rrect.rx, 0.25f, 0.5f, 0.0f);
	push_line(vector<float32, 2>(rrect.rect.position.x, rrect.rect.position.y + rrect.ry));
	push_elliptic_arc(vector<float32, 2>(rrect.rect.position.x + rrect.rx, rrect.rect.position.y),
		rrect.ry / rrect.rx, 0.5f, 0.75f, 0.0f);
}
