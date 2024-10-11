#pragma once
#include "global_operators.h"
#include "array.h"
#include "vector.h"
#include "matrix.h"

template<uint32 extent>
float32 vector_dot(vector<float32, extent> a, vector<float32, extent> b)
{
	return accumulate<extent>(product<float32, float32>, add<float32>, float32(0), a.coord, b.coord);
}

template<uint32 extent>
float32 vector_length(vector<float32, extent> v)
{
	return sqrt(vector_dot(v, v));
}

template<uint32 extent>
vector<float32, 2> vector_normal(vector<float32, extent> v)
{
	return v /= vector_length(v);
}

template<uint32 extent>
vector<float32, 2> vector_cross(vector<float32, extent> a, vector<float32, extent> b)
{
	if constexpr(extent == 2)
		return vector<float32, 2>(
			a.x * b.y - a.y * b.x,
			a.x * b.y - a.y * b.x);
	else if constexpr(extent == 3)
		return vector<float32, 3>(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x);
	else return vector<float32, extent>();
}

template<typename value_type, uint32 rows, uint32 columns>
void set_identity_matrix(matrix<value_type, rows, columns> *mat)
{
	*mat = static_cast<value_type>(0);
	for(uint32 i = 0; i < min(rows, columns); i++)
		mat->m[i][i] = static_cast<value_type>(1);
}

template<typename value_type, uint32 n>
value_type determinant(matrix<value_type, n, n> &mat)
{
	if constexpr(n == 1)
		return mat.m[0][0];
	else if constexpr(n == 2)
		return mat.m[0][0] * mat.m[1][1] - mat.m[0][1] * mat.m[1][0];
	else if constexpr(n == 3)
		return mat.m[0][0] * mat.m[1][1] * mat.m[2][2] + mat.m[0][1] * mat.m[1][2] * mat.m[2][0]
			+ mat.m[0][2] * mat.m[1][0] * mat.m[2][1] - mat.m[0][2] * mat.m[1][1] * mat.m[2][0]
			- mat.m[0][1] * mat.m[1][0] * mat.m[2][2] - mat.m[0][0] * mat.m[1][2] * mat.m[2][1];
	else if constexpr(n == 4)
		return mat.m[0][3] * mat.m[1][2] * mat.m[2][1] * mat.m[3][0] - mat.m[0][2] * mat.m[1][3] * mat.m[2][1] * mat.m[3][0]
			- mat.m[0][3] * mat.m[1][1] * mat.m[2][2] * mat.m[3][0] + mat.m[0][1] * mat.m[1][3] * mat.m[2][2] * mat.m[3][0]
			+ mat.m[0][2] * mat.m[1][1] * mat.m[2][3] * mat.m[3][0] - mat.m[0][1] * mat.m[1][2] * mat.m[2][3] * mat.m[3][0]
			- mat.m[0][3] * mat.m[1][2] * mat.m[2][0] * mat.m[3][1] + mat.m[0][2] * mat.m[1][3] * mat.m[2][0] * mat.m[3][1]
			+ mat.m[0][3] * mat.m[1][0] * mat.m[2][2] * mat.m[3][1] - mat.m[0][0] * mat.m[1][3] * mat.m[2][2] * mat.m[3][1]
			- mat.m[0][2] * mat.m[1][0] * mat.m[2][3] * mat.m[3][1] + mat.m[0][0] * mat.m[1][2] * mat.m[2][3] * mat.m[3][1]
			+ mat.m[0][3] * mat.m[1][1] * mat.m[2][0] * mat.m[3][2] - mat.m[0][1] * mat.m[1][3] * mat.m[2][0] * mat.m[3][2]
			- mat.m[0][3] * mat.m[1][0] * mat.m[2][1] * mat.m[3][2] + mat.m[0][0] * mat.m[1][3] * mat.m[2][1] * mat.m[3][2]
			+ mat.m[0][1] * mat.m[1][0] * mat.m[2][3] * mat.m[3][2] - mat.m[0][0] * mat.m[1][1] * mat.m[2][3] * mat.m[3][2]
			- mat.m[0][2] * mat.m[1][1] * mat.m[2][0] * mat.m[3][3] + mat.m[0][1] * mat.m[1][2] * mat.m[2][0] * mat.m[3][3]
			+ mat.m[0][2] * mat.m[1][0] * mat.m[2][1] * mat.m[3][3] - mat.m[0][0] * mat.m[1][2] * mat.m[2][1] * mat.m[3][3]
			- mat.m[0][1] * mat.m[1][0] * mat.m[2][2] * mat.m[3][3] + mat.m[0][0] * mat.m[1][1] * mat.m[2][2] * mat.m[3][3];
	else
	{
		value_type result = static_cast<value_type>(0);
		matrix<value_type, n - 1, n - 1> subm;
		for(uint32 i = 0; i < n; i++)
		{
			for(uint32 j = 1; j < n; j++)
			{
				for(uint32 k = 0; k < n; k++)
					if(k < i) subm.m[j - 1][k] = mat.m[j][k];
					else if(k > i) subm.m[j - 1][k - 1] = mat.m[j][k];
			}
			if(i & 1) result -= mat.m[0][i] * determinant(subm);
			else result += mat.m[0][i] * determinant(subm);
		}
		return result;
	}
}

template<typename value_type, uint32 rows, uint32 columns>
matrix<value_type, columns, rows> transpose_matrix(matrix<value_type, rows, columns> &mat)
{
	matrix<value_type, columns, rows> result;
	for(uint32 i = 0; i < rows; i++)
	{
		for(uint32 j = 0; j < columns; j++)
			result.m[j][i] = mat.m[i][j];
	}
	return result;
}

template<typename value_type, uint32 n>
void invert_matrix(matrix<value_type, n, n> *mat)
{
	value_type d = determinant(*mat);
	if(d != static_cast<value_type>(0))
	{
		matrix<value_type, n, n> cfm;
		matrix<value_type, n - 1, n - 1> subm;
		for(uint32 i = 0; i < n; i++)
		{
			for(uint32 j = 0; j < n; j++)
			{
				for(uint32 k = 0; k < n; k++)
				{
					for(uint32 p = 0; p < n; p++)
						if(k < i && p < j) subm.m[k][p] = mat->m[k][p];
						else if(k < i && p > j) subm.m[k][p - 1] = mat->m[k][p];
						else if(k > i && p < j) subm.m[k - 1][p] = mat->m[k][p];
						else if(k > i && p > j) subm.m[k - 1][p - 1] = mat->m[k][p];
				}
				if((i + j) & 1) cfm.m[i][j] = -determinant(subm);
				else cfm.m[i][j] = determinant(subm);
			}
		}
		*mat = transpose_matrix(cfm);
		*mat *= (static_cast<value_type>(1) / d);
	}
}
vector<float32, 2> quadratic_bezier_point(
	vector<float32, 2> p1,
	vector<float32, 2> p2,
	vector<float32, 2> p3,
	float32 t);

vector<float32, 2> elliptic_arc_point(
	vector<float32, 2> center,
	float32 rx,
	float32 ry,
	float32 angle);

vector<float32, 2> closest_line_point(
	vector<float32, 2> point,
	vector<float32, 2> line_point1,
	vector<float32, 2> line_point2);

vector<float32, 2> closest_infinite_line_point(
	vector<float32, 2> point,
	vector<float32, 2> linePoint1,
	vector<float32, 2> linePoint2);

bool intersect_lines(
	vector<float32, 2> line1_point1,
	vector<float32, 2> line1_point2,
	vector<float32, 2> line2_point1,
	vector<float32, 2> line2_point2,
	vector<float32, 2> *intersect_point);

bool ccw_test(vector<float32, 2> a, vector<float32, 2> b, vector<float32, 2> c);

matrix<float32, 3, 3> scaling_matrix(float32 x, float32 y, vector<float32, 2> origin);

matrix<float32, 3, 3> rotating_matrix(float32 angle, vector<float32, 2> origin);

matrix<float32, 3, 3> translating_matrix(float32 x, float32 y);

template<typename value_type> struct rectangle
{
	vector<value_type, 2> position;
	vector<value_type, 2> extent;

	rectangle() {}

	rectangle(vector<value_type, 2> position, vector<value_type, 2> extent) 
		: position(position), extent(extent) {}

	template<typename right_type> rectangle(rectangle<right_type> &rect)
	{
		position.x = value_type(rect.position.x);
		position.y = value_type(rect.position.y);
		extent.x = value_type(rect.extent.x);
		extent.y = value_type(rect.extent.y);
	}

	bool hit_test(vector<value_type, 2> point)
	{
		return point.x >= position.x
			&& point.x < position.x + extent.x
			&& point.y >= position.y
			&& point.y < position.y + extent.y;
	}
};

template<typename value_type> struct rounded_rectangle
{
	rectangle<value_type> rect;
	value_type rx;
	value_type ry;

	rounded_rectangle() {}

	rounded_rectangle(rectangle<value_type> rect, value_type rx, value_type ry)
		: rect(rect), rx(rx), ry(ry) {}
};

enum struct geometry_path_unit
{
	move,
	line,
	quadratic_arc,
	elliptic_arc
};

struct geometry_path_data
{
	geometry_path_unit type;
	vector<float32, 2> p1;
	vector<float32, 2> p2;
	float32 rx;
	float32 ry;
	float32 begin_angle;
	float32 end_angle;
	float32 rotation;
};

enum struct face_orientation
{
	counterclockwise,
	clockwise
};

struct geometry_path
{
	array<geometry_path_data> data;
	face_orientation orientation;

	geometry_path();
	void move(vector<float32, 2> point);
	void push_line(vector<float32, 2> point);
	void push_quadratic_arc(vector<float32, 2> point1, vector<float32, 2> point2);
	void push_elliptic_arc(vector<float32, 2> point, float32 radius_ratio, float32 begin_angle, float32 end_angle, float32 rotation);
	void push_rectangle(rectangle<float32> rect);
	void push_rounded_rectangle(rounded_rectangle<float32> rrect);
};
