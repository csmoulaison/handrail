#ifndef handrail_math_h_INCLUDED
#define handrail_math_h_INCLUDED

#define M_PI 3.14159265358979323846

typedef struct {
	union {
		struct { f32 x; f32 y; };
		f32 comps[2];
	};
} v2;

typedef struct {
	union {
		struct { f32 x; f32 y; f32 z; };
		struct { f32 r; f32 g; f32 b; };
		f32 comps[3];
	};
} v3;

typedef struct {
	union {
		struct { f32 x; f32 y; f32 z; f32 w; };
		struct { f32 r; f32 g; f32 b; f32 a; };
		f32 comps[4];
	};
} v4;

typedef struct {
	union {
		struct { i32 x; i32 y; };
		i32 comps[2];
	};
} iv2;

struct Rect {
	f32 x, y, w, h;
};

// Scalar
static inline bool f32_within_epsilon(f32 a, f32 b, f32 e);
static inline f32  f32_min(f32 a, f32 b);
static inline f32  f32_max(f32 a, f32 b);
static inline f32  f32_clamp(f32 v, f32 min, f32 max);
static inline f32  f32_move_to_zero(f32 value, f32 amount);
static inline f32  f32_lerp(f32 a, f32 b, f32 t);
static inline f32  f32_smoothstep(f32 n);
static inline f32  f32_smootherstep(f32 n);
static inline u32  u32_round_up_to_power_of_2(u32 value);
static inline i32  true_mod(i32 a, i32 b);
// IVector2
static inline iv2  iv2_new(i32 x, i32 y);
static inline bool iv2_eq(iv2 a, iv2 b);
static inline iv2  iv2_scale(iv2 v, i32 scale);
static inline iv2  iv2_add(iv2 a, iv2 b);
static inline iv2  iv2_sub(iv2 a, iv2 b);
static inline f32  iv2_distance(iv2 a, iv2 b);
static inline v2   v2_from_iv2(iv2 v);
// Vector2
static inline v2   v2_new(f32 x, f32 y);
static inline v2   v2_identity();
static inline v2   v2_zero();
static inline void v2_copy(f32* dst, v2 v);
static inline v2   v2_normalize(v2 v);
static inline v2   v2_scale(v2 v, f32 s);
static inline v2   v2_add(v2 a, v2 b);
static inline v2   v2_sub(v2 a, v2 b);
static inline v2   v2_mult(v2 a, v2 b);
static inline v2   v2_div(v2 a, v2 b);
static inline v2   v2_abs(v2 a);
static inline v2   v2_min(v2 a, v2 b);
static inline v2   v2_max(v2 a, v2 b);
static inline v2   v2_lerp(v2 a, v2 b, f32 t);
static inline f32  v2_distance_squared(v2 a, v2 b);
static inline f32  v2_distance(v2 a, v2 b);
static inline f32  v2_dot(v2 a, v2 b);
static inline f32  v2_cross(v2 a, v2 b);
// Vector3
static inline v3 v3_new(f32 x, f32 y, f32 z);
static inline v3 v3_identity();
static inline v3 v3_zero();
static inline void v3_copy(f32* dst, v3 v);
static inline v3 v3_add(v3 a, v3 b);
static inline v3 v3_sub(v3 a, v3 b);
static inline f32 v3_magnitude(v3 v);
static inline v3 v3_scale(v3 v, f32 s);
static inline v3 v3_normalize(v3 v);
static inline v3 v3_cross(v3 a, v3 b);
static inline v3 v3_lerp(v3 a, v3 b, f32 t);
static inline f32 v3_distance_squared(v3 a, v3 b);
static inline f32 v3_distance(v3 a, v3 b);
static inline f32 v3_dot(v3 a, v3 b);
// Vector4
static inline v4 v4_new(f32 x, f32 y, f32 z, f32 w);
static inline v4 v4_zero();
static inline v4 v4_identity();
static inline v4 v4_lerp(v4 a, v4 b, f32 t);
static inline void v4_copy(f32* dst, v4 v);
// Radians
static inline f32 radians(f32 degrees);
// 4x4 matrices
// These are all column major.
// 
// NOTO: Make res (dst) the first parameter rather than last.
void m4_identity(f32* res);
void m4_perspective(f32 fovy, f32 aspect, f32 zfar, f32 znear, f32* res);
void m4_lookat(v3 origin, v3 target, v3 up, f32* res);
void m4_mul(f32* a, f32* b, f32* res);
static inline void m4_translation(v3 v, f32* res);
void m4_scale(v3 v, f32* res);
void m4_rotation(f32 x, f32 y, f32 z, f32* res);
void m4_from_quat(f32* q, f32* res);
// Quaternions
void quat_mult(f32* r, f32* s, f32* res);
void quat_inverse(f32* q, f32* res);

#ifdef CSM_IMPLEMENTATION

static inline bool f32_within_epsilon(f32 a, f32 b, f32 e) {
    return (fabs(a - b) <= e);
}

static inline f32 f32_min(f32 a, f32 b) {
	if(a > b) return b;
	return a;
}

static inline f32 f32_max(f32 a, f32 b) {
	if(a < b) return b;
	return a;
}

static inline f32 f32_clamp(f32 v, f32 min, f32 max) {
	if(v < min) return min;
	if(v > max) return max;
	return v;
}

static inline f32 f32_move_to_zero(f32 value, f32 amount) {
	if(value > 0.0f) {
		value -= amount;
		if(value < 0.0f) value = 0.0f;
	}
	if(value < 0.0f) {
		value += amount;
		if(value > 0.0f) value = 0.0f;
	}
	return value;
}

f32 f32_lerp(f32 a, f32 b, f32 t) {
	return (1.0f - t) * a + t * b;
}

static inline f32 f32_smoothstep(f32 n) {
    return n * n * (3.0 - 2.0 * n);
}

static inline f32 f32_smootherstep(f32 n) {
    return n * n / (2.0 * n * n - 2.0 * n + 1.0);
}

static inline u32 u32_round_up_to_power_of_2(u32 value) {
		unsigned long index;
		_BitScanReverse(&index, value - 1);
		assert(index < 31);
		return 1U << (index + 1);
}

static inline i32 true_mod(i32 a, i32 b) {
    return ((a % b) + b) % b;
}

static inline iv2 iv2_new(i32 x, i32 y) {
	return (iv2){{{ x, y }}};
}

static inline bool iv2_eq(iv2 a, iv2 b) {
	return a.x == b.x && a.y == b.y;
}

static inline iv2 iv2_scale(iv2 v, i32 scale) {
	return iv2_new(v.x * scale, v.y * scale);
}

static inline iv2 iv2_add(iv2 a, iv2 b) {
	return iv2_new(a.x + b.x, a.y + b.y);
}

static inline iv2 iv2_sub(iv2 a, iv2 b) {
	return iv2_new(a.x - b.x, a.y - b.y);
}

static inline f32 iv2_distance(iv2 a, iv2 b) {
	return v2_distance(v2_from_iv2(a), v2_from_iv2(b));
}

static inline v2 v2_from_iv2(iv2 v) {
	return v2_new(v.x, v.y);
}

static inline v2 v2_new(f32 x, f32 y) {
	return (v2){{{ x, y }}};
}

static inline v2 v2_identity() {
	return v2_new(1.0f, 1.0f);
}

static inline v2 v2_zero() {
	return v2_new(0.0f, 0.0f);
}

static inline void v2_copy(f32* dst, v2 v) {
	dst[0] = v.x;
	dst[1] = v.y;
}

static inline f32 v2_magnitude(v2 v) {
	return sqrt(v.x * v.x + v.y * v.y);
}

static inline v2 v2_scale(v2 v, f32 s) {
	return v2_new(v.x * s, v.y * s);
}

static inline v2 v2_normalize(v2 v) {
	f32 mag = v2_magnitude(v);
	if(mag == 0.0f) {
		return v2_zero();
	}
	return v2_new(v.x / mag, v.y / mag);
}

static inline v2 v2_add(v2 a, v2 b) {
	return v2_new(a.x + b.x, a.y + b.y);
}

static inline v2 v2_sub(v2 a, v2 b) {
	return v2_new(a.x - b.x, a.y - b.y);
}

static inline v2 v2_mult(v2 a, v2 b) {
	return v2_new(a.x * b.x, a.y * b.y);
}

static inline v2 v2_div(v2 a, v2 b) {
	return v2_new(a.x / b.x, a.y / b.y);
}

static inline v2 v2_abs(v2 a) {
	return v2_new(fabs(a.x), fabs(a.y));
}

static inline v2 v2_min(v2 a, v2 b) {
	return v2_new(min(a.x, b.x), min(a.y, b.y));
}

static inline v2 v2_max(v2 a, v2 b) {
	return v2_new(max(a.x, b.x), max(a.y, b.y));
}

static inline v2 v2_lerp(v2 a, v2 b, f32 t) {
	return v2_new(lerp(a.x, b.x, t), lerp(a.y, b.y, t));
}

static inline f32 v2_distance_squared(v2 a, v2 b) {
	f32 dx = a.x - b.x;
	f32 dy = a.y - b.y;
	return dx * dx + dy * dy;
}

static inline f32 v2_distance(v2 a, v2 b) {
	return sqrt(v2_distance_squared(a, b));
}

static inline f32 v2_dot(v2 a, v2 b) {
	return a.x * b.x + a.y * b.y;
}

static inline f32 v2_cross(v2 a, v2 b) {
	return a.x * b.y - a.y * b.x;
}

static inline v3 v3_new(f32 x, f32 y, f32 z) {
	return (v3){{{ x, y, z }}};
}

static inline v3 v3_identity() {
	return v3_new(1.0f, 1.0f, 1.0f);
}

static inline v3 v3_zero() {
	return v3_new(0.0f, 0.0f, 0.0f);
}

static inline void v3_copy(f32* dst, v3 v) {
	dst[0] = v.x;
	dst[1] = v.y;
	dst[2] = v.z;
}

static inline v3 v3_add(v3 a, v3 b) {
	return v3_new(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline v3 v3_sub(v3 a, v3 b) {
	return v3_new(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline f32 v3_magnitude(v3 v) {
	return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline v3 v3_scale(v3 v, f32 s) {
	return v3_new(v.x * s, v.y * s, v.z * s);
}

static inline v3 v3_normalize(v3 v) {
	f32 mag = v3_magnitude(v);
	if(mag == 0.0f) {
		return v3_zero();
	}
	return v3_new(v.x / mag, v.y / mag, v.z / mag);
}

static inline v3 v3_cross(v3 a, v3 b) {
	return v3_new(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	);
}

static inline v3 v3_lerp(v3 a, v3 b, f32 t) {
	return v3_new(lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t));
}

static inline f32 v3_dot(v3 a, v3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline f32 v3_distance_squared(v3 a, v3 b) {
	f32 dx = a.x - b.x;
	f32 dy = a.y - b.y;
	f32 dz = a.z - b.z;
	return dx * dx + dy * dy + dz * dz;
}

static inline f32 v3_distance(v3 a, v3 b) {
	return sqrt(v3_distance_squared(a, b));
}

static inline v4 v4_new(f32 x, f32 y, f32 z, f32 w) {
	return (v4){{{ x, y, z, w }}};
}

static inline v4 v4_zero() {
	return v4_new(0.0f, 0.0f, 0.0f, 0.0f);
}

static inline v4 v4_identity() {
	return v4_new(1.0f, 1.0f, 1.0f, 1.0f);
}

static inline v4 v4_lerp(v4 a, v4 b, f32 t) {
	return v4_new(lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t), lerp(a.w, b.w, t));
}

static inline void v4_copy(f32* dst, v4 v) {
	dst[0] = v.x;
	dst[1] = v.y;
	dst[2] = v.z;
	dst[3] = v.w;
}

static inline f32 radians(f32 degrees) {
	return degrees * 0.0174533;
}

void m4_identity(f32* res) {
	for(i8 i = 0; i < 16; i++) {
		res[i] = 0.0f;
	}
	res[0] = 1.0f;
	res[5] = 1.0f;
	res[10] = 1.0f;
	res[15] = 1.0f;
}

void m4_perspective(f32 fovy, f32 aspect, f32 zfar, f32 znear, f32* res) {
	assert(zfar != znear);

	f32 rad = fovy;
	f32 tan_half_fovy = tan(rad / 2.0f);

	for(i8 i = 0; i < 16; i++) {
		res[i] = 0.0f;
	}
	res[0] = 1.0f / (aspect * tan_half_fovy);
	res[5] = 1.0f / (tan_half_fovy);
	res[10] = - (zfar + znear) / (zfar - znear);
	res[11] = - 1.0f;
	res[14] = - (2.0f * zfar * znear) / (zfar - znear);
}

void m4_lookat(v3 origin, v3 target, v3 up, f32* res) {
	v3 dir = v3_sub(target, origin);
	v3 f = v3_normalize(dir);
	v3 u = v3_normalize(up); 
	v3 cross_fu = v3_cross(f, u);
	v3 s = v3_normalize(cross_fu);
	u = v3_cross(s, f);

	res[0] = s.x;
	res[4] = s.y;
	res[8] = s.z;
	res[1] = u.x;
	res[5] = u.y;
	res[9] = u.z;
	res[2] = -f.x;
	res[6] = -f.y;
	res[10] = -f.z;
	res[12] = -v3_dot(s, origin);
	res[13] = -v3_dot(u, origin);
	res[14] = v3_dot(f, origin);
}

void m4_mul(f32* a, f32* b, f32* res) {
	// NOTO: just do the algorithm
	f32 a00 = a[0];
	f32 a01 = a[1];
	f32 a02 = a[2]; 
	f32 a03 = a[3];
	f32 a10 = a[4];
	f32 a11 = a[5];
	f32 a12 = a[6];
	f32 a13 = a[7];
	f32 a20 = a[8];
	f32 a21 = a[9];
	f32 a22 = a[10];
	f32 a23 = a[11];
	f32 a30 = a[12];
	f32 a31 = a[13];
	f32 a32 = a[14];
	f32 a33 = a[15];

	f32 b00 = b[0];
	f32 b01 = b[1];
	f32 b02 = b[2]; 
	f32 b03 = b[3];
	f32 b10 = b[4];
	f32 b11 = b[5];
	f32 b12 = b[6];
	f32 b13 = b[7];
	f32 b20 = b[8];
	f32 b21 = b[9];
	f32 b22 = b[10];
	f32 b23 = b[11];
	f32 b30 = b[12];
	f32 b31 = b[13];
	f32 b32 = b[14];
	f32 b33 = b[15];

	res[0] = a00 * b00 + a10 * b01 + a20 * b02 + a30 * b03;
	res[1] = a01 * b00 + a11 * b01 + a21 * b02 + a31 * b03;
	res[2] = a02 * b00 + a12 * b01 + a22 * b02 + a32 * b03;
	res[3] = a03 * b00 + a13 * b01 + a23 * b02 + a33 * b03;
	res[4] = a00 * b10 + a10 * b11 + a20 * b12 + a30 * b13;
	res[5] = a01 * b10 + a11 * b11 + a21 * b12 + a31 * b13;
	res[6] = a02 * b10 + a12 * b11 + a22 * b12 + a32 * b13;
	res[7] = a03 * b10 + a13 * b11 + a23 * b12 + a33 * b13;
	res[8] = a00 * b20 + a10 * b21 + a20 * b22 + a30 * b23;
	res[9] = a01 * b20 + a11 * b21 + a21 * b22 + a31 * b23;
	res[10] = a02 * b20 + a12 * b21 + a22 * b22 + a32 * b23;
	res[11] = a03 * b20 + a13 * b21 + a23 * b22 + a33 * b23;
	res[12] = a00 * b30 + a10 * b31 + a20 * b32 + a30 * b33;
	res[13] = a01 * b30 + a11 * b31 + a21 * b32 + a31 * b33;
	res[14] = a02 * b30 + a12 * b31 + a22 * b32 + a32 * b33;
	res[15] = a03 * b30 + a13 * b31 + a23 * b32 + a33 * b33;
}

static inline void m4_translation(v3 v, f32* res) {
	res[0] = 1.0f;
	res[1] = 0.0f;
	res[2] = 0.0f;
	res[3] = 0.0f;
	res[4] = 0.0f;
	res[5] = 1.0f;
	res[6] = 0.0f;
	res[7] = 0.0f;
	res[8] = 0.0f;
	res[9] = 0.0f;
	res[10] = 1.0f;
	res[11] = 0.0f;
	res[12] = v.x;
	res[13] = v.y;
	res[14] = v.z;
	res[15] = 1.0f;
}

void m4_scale(v3 v, f32* res) {
	for(i8 i = 0; i < 16; i++) {
		res[i] = 0.0f;
	}
	res[0] = v.x;
	res[5] = v.y;
	res[10] = v.z;
	res[15] = 1.0f;
}

void m4_rotation(f32 x, f32 y, f32 z, f32* res) {
	f32 cosx = cos(x);
	f32 cosy = cos(y);
	f32 cosz = cos(z);
	f32 sinx = sin(x);
	f32 siny = sin(y);
	f32 sinz = sin(z);

    res[0] = cosy * cosx;
    res[1] = cosz * sinx + sinz * siny * cosx;
    res[2] = sinz * sinx - cosz * siny * cosx;
    res[3] = 0;
    res[4] = -cosy * sinx;
    res[5] = cosz * cosx - sinz* siny * sinx;
    res[6] = sinz * cosx + cosz* siny * sinx;
    res[7] = 0;
    res[8] = siny;
    res[9] = -sinz * cosy;
    res[10] = cosz * cosy;
    res[11] = 0;
    res[12] = 0;
    res[13] = 0;
    res[14] = 0;
    res[15] = 1;
}

void m4_from_quat(f32* q, f32* res) {
	res[0] = q[0] * q[0] + q[1] * q[1] - q[2] * q[2] - q[3] * q[3];
	res[1] = 2.0f * q[1] * q[2] + 2.0f * q[0] * q[3];
	res[2] = 2.0f * q[1] * q[3] - 2.0f * q[0] * q[2];
	res[3] = 0.0f;

	res[4] = 2.0f * q[1] * q[2] - 2.0f * q[0] * q[3];
	res[5] = 1.0f - 2.0f * q[1] * q[1] - 2 * q[3] * q[3];
	res[6] = 2.0f * q[2] * q[3] + 2.0f * q[0] * q[1];
	res[7] = 0.0f;

	res[8] = 2.0f * q[1] * q[3] + 2.0f * q[0] * q[2];
	res[9] = 2.0f * q[2] * q[3] - 2.0f * q[0] * q[1];
	res[10] = 1.0f - 2.0f * q[1] * q[1] - 2.0f * q[2] * q[2];
	res[11] = 0.0f;

	res[12] = 0.0f;
	res[13] = 0.0f;
	res[14] = 0.0f;
	res[15] = 1.0f;
}

void quat_mult(f32* r, f32* s, f32* res) {
	// w,x,y,z
	res[0] = r[0] * s[0] - r[1] * s[1] - r[2] * s[2] - r[3] * s[3];
	res[1] = r[0] * s[1] + r[1] * s[0] - r[2] * s[3] + r[3] * s[2];
	res[2] = r[0] * s[2] + r[1] * s[3] + r[2] * s[0] - r[3] - s[1];
	res[3] = r[0] * s[3] - r[1] * s[2] + r[2] * s[1] + r[3] * s[0];
}

void quat_inverse(f32* q, f32* res) {
	res[0] = q[0];
	res[1] = -q[1];
	res[2] = -q[2];
	res[3] = -q[3];
}

#endif
#endif
