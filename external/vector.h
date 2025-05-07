#pragma once
#ifndef _VECTOR_ESP_
#define _VECTOR_ESP_
#include <numbers>
#include <cmath>

struct view_matrix_t 
{
	float* operator[](int index) {
		return matrix[index];
	}

	float matrix[4][4];
};

struct Vector2
{
	// constructor
	constexpr Vector2(
		const float x = 0.f,
		const float y = 0.f) noexcept :
		x(x), y(y) { }

	// operator overloads
	constexpr const Vector2& operator-(const Vector2& other) const noexcept
	{
		return Vector2{ x - other.x, y - other.y };
	}

	constexpr const Vector2& operator+(const Vector2& other) const noexcept
	{
		return Vector2{ x + other.x, y + other.y};
	}

	constexpr const Vector2& operator/(const float factor) const noexcept
	{
		return Vector2{ x / factor, y / factor};
	}

	constexpr const Vector2& operator*(const float factor) const noexcept
	{
		return Vector2{ x * factor, y * factor};
	}
	constexpr const float& operator*(const Vector2 v) const noexcept
	{
		return x * v.x + y * v.y;
	}

	constexpr const bool operator>(const Vector2& other) const noexcept {
		return x > other.x && y > other.y;
	}

	constexpr const bool operator>=(const Vector2& other) const noexcept {
		return x >= other.x && y >= other.y;
	}

	constexpr const bool operator<(const Vector2& other) const noexcept {
		return x < other.x&& y < other.y;
	}

	constexpr const bool operator<=(const Vector2& other) const noexcept {
		return x <= other.x && y <= other.y;
	}

	// struct data
	float x, y;
};

struct Vector3
{
	// constructor
	constexpr Vector3(
		const float x = 0.f,
		const float y = 0.f,
		const float z = 0.f) noexcept :
		x(x), y(y), z(z) { }

	// operator overloads
	constexpr const Vector3& operator-(const Vector3& other) const noexcept
	{
		return Vector3{ x - other.x, y - other.y, z - other.z };
	}

	constexpr const Vector3& operator+(const Vector3& other) const noexcept
	{
		return Vector3{ x + other.x, y + other.y, z + other.z };
	}

	constexpr const Vector3& operator/(const float factor) const noexcept
	{
		return Vector3{ x / factor, y / factor, z / factor };
	}

	constexpr const Vector3& operator*(const float factor) const noexcept
	{
		return Vector3{ x * factor, y * factor, z * factor };
	}
	constexpr const float& operator*(const Vector3 v) const noexcept
	{
		return x * v.x + y * v.y + z * v.z;
	}

	constexpr const bool operator>(const Vector3& other) const noexcept {
		return x > other.x && y > other.y && z > other.z;
	}

	constexpr const bool operator>=(const Vector3& other) const noexcept {
		return x >= other.x && y >= other.y && z >= other.z;
	}

	constexpr const bool operator<(const Vector3& other) const noexcept {
		return x < other.x&& y < other.y&& z < other.z;
	}

	constexpr const bool operator<=(const Vector3& other) const noexcept {
		return x <= other.x && y <= other.y && z <= other.z;
	}

	// utils
	constexpr const Vector3& ToAngle() const noexcept
	{
		return Vector3{
			std::atan2(-z, std::hypot(x, y)) * (180.0f / std::numbers::pi_v<float>),
			std::atan2(y, x) * (180.0f / std::numbers::pi_v<float>),
			0.0f };
	}

	float length() const {
		return std::sqrt(x * x + y * y + z * z);
	}

	float length2d() const {
		return std::sqrt(x * x + y * y);
	}

	constexpr const bool IsZero() const noexcept
	{
		return x == 0.f && y == 0.f && z == 0.f;
	}

	float calculate_distance(const Vector3& point) const {
		float dx = point.x - x;
		float dy = point.y - y;
		float dz = point.z - z;

		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	Vector3 normalize() const {
		float l = std::sqrt(x * x + y * y + z * z);
		return Vector3{
			x / l,
			y / l,
			z / l
		};
	}

	Vector3 WTS(view_matrix_t matrix, float screenWidth, float screenHeight) const {
		float _x = matrix[0][0] * x + matrix[0][1] * y + matrix[0][2] * z + matrix[0][3];
		float _y = matrix[1][0] * x + matrix[1][1] * y + matrix[1][2] * z + matrix[1][3];

		float  w = matrix[3][0] * x + matrix[3][1] * y + matrix[3][2] * z + matrix[3][3];

		float inv_w = 1.f / w;
		_x += inv_w;
		_y += inv_w;

		float SightX = screenWidth / 2;
		float SightY = screenHeight / 2;
		float x, y;
		if (w > 0) {
			x = SightX + _x / w * SightX;
			y = SightY - _y / w * SightY;
		}
		else {
			x = SightX + _x / w * SightX;
			y = SightY - _y / w * SightY;
		}
		
		return Vector3{x,y,w};
	}

	Vector3 AngleToPos() {
		float _x = cos(y * std::numbers::pi / 180) * cos(x * std::numbers::pi / 180);
		float _y = sin(y * std::numbers::pi / 180) * cos(x * std::numbers::pi / 180);
		float _z = -1 * sin(x * std::numbers::pi / 180);
		return Vector3{ _x,_y,_z };
	}
	// struct data
	float x, y, z;
};
#endif