#include <pch.h>

#include <core/math/mathlib.h>

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/mathlib/mathlib_base.cpp#L3466
float AngleDiff(float destAngle, float srcAngle)
{
	float delta = fmodf(destAngle - srcAngle, 360.0f);
	if (destAngle > srcAngle)
	{
		if (delta >= 180)
			delta -= 360;
	}
	else
	{
		if (delta <= -180)
			delta += 360;
	}
	return delta;
}


// quaternion funcs
void QuaternionSlerp(const Quaternion& p, const Quaternion& q, float t, Quaternion& qt)
{
	Quaternion q2;
	// 0.0 returns p, 1.0 return q.

	// decide if one of the quaternions is backwards
	QuaternionAlign(p, q, q2);

	QuaternionSlerpNoAlign(p, q2, t, qt);
}

void QuaternionSlerpNoAlign(const Quaternion& p, const Quaternion& q, float t, Quaternion& qt)
{
	float omega, cosom, sinom, sclp, sclq;
	int i;

	// 0.0 returns p, 1.0 return q.

	cosom = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];

	if ((1.0f + cosom) > 0.000001f) {
		if ((1.0f - cosom) > 0.000001f) {
			omega = acos(cosom);
			sinom = sin(omega);
			sclp = sin((1.0f - t) * omega) / sinom;
			sclq = sin(t * omega) / sinom;
		}
		else {
			// TODO: add short circuit for cosom == 1.0f?
			sclp = 1.0f - t;
			sclq = t;
		}
		for (i = 0; i < 4; i++) {
			qt[i] = sclp * p[i] + sclq * q[i];
		}
	}
	else {
		assertm(&qt != &q, "invalid quaternion");

		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];
		sclp = sin((1.0f - t) * (0.5f * M_PI));
		sclq = sin(t * (0.5f * M_PI));
		for (i = 0; i < 3; i++) {
			qt[i] = sclp * p[i] + sclq * qt[i];
		}
	}

	assertm(qt.IsValid(), "invalid quaternion");
}

#if MATH_SIMD
__forceinline __m128 QuaternionAlignSIMD(const __m128& p, const __m128& q)
{
	// decide if one of the quaternions is backwards
	__m128 a = _mm_sub_ps(p, q);
	__m128 b = _mm_add_ps(p, q);
	a = Dot4SIMD(a, a);
	b = Dot4SIMD(b, b);
	__m128 cmp = _mm_cmpgt_ps(a, b);
	__m128 result = MaskedAssign(cmp, NegateSIMD(q), q);
	return result;
}

FORCEINLINE __m128 QuaternionNormalizeSIMD(const __m128& q)
{
	__m128 radius, result, mask;
	radius = Dot4SIMD(q, q);
	mask = _mm_cmpeq_ps(radius, simd_Four_Zeros); // all ones iff radius = 0
	result = _mm_rsqrt_ps(radius);
	result = _mm_mul_ps(result, q);
	return MaskedAssign(mask, q, result);	// if radius was 0, just return q
}

__forceinline __m128 QuaternionBlendNoAlignSIMD(const __m128& p, const __m128& q, float t)
{
	__m128 sclp, sclq, result;
	sclq = ReplicateX4(t);
	sclp = _mm_sub_ps(simd_Four_Ones, sclq); // splat is such a silly name for this
	result = _mm_mul_ps(sclp, p);
	result = MaddSIMD(sclq, q, result);
	return QuaternionNormalizeSIMD(result);
}

__forceinline __m128 QuaternionBlendSIMD(const __m128& p, const __m128& q, float t)
{
	__m128 q2, result;
	q2 = QuaternionAlignSIMD(p, q);
	result = QuaternionBlendNoAlignSIMD(p, q2, t);
	return result;
}
#endif // USE_SIMD

void QuaternionBlend(const Quaternion& p, const Quaternion& q, float t, Quaternion& qt)
{
#if MATH_SIMD
	// Quaternion/RadianEuler/QAngle are only 4-byte aligned; callers pass stack locals that can
	// land non-16-aligned, so aligned SIMD mem ops (_mm_load_ps/_mm_store_ps) #GP-fault here
	// (the angel_city model-extract crash path, CalcBoneQuaternion). Unaligned variants are
	// identical in result and survive any caller alignment.
	__m128 psimd = _mm_loadu_ps(p.Base());
	__m128 qsimd = _mm_loadu_ps(q.Base());
	__m128 qtsimd = QuaternionBlendSIMD(psimd, qsimd, t);
	_mm_storeu_ps(qt.Base(), qtsimd);
#else
	// decide if one of the quaternions is backwards
	Quaternion q2;
	QuaternionAlign(p, q, q2);
	QuaternionBlendNoAlign(p, q2, t, qt);
#endif // USE_SIMD
}

void QuaternionBlendNoAlign(const Quaternion& p, const Quaternion& q, float t, Quaternion& qt)
{
	float sclp, sclq;
	int i;

	// 0.0 returns p, 1.0 return q.
	sclp = 1.0f - t;
	sclq = t;
	for (i = 0; i < 4; i++) {
		qt[i] = sclp * p[i] + sclq * q[i];
	}
	QuaternionNormalize(qt);
}

void QuaternionAlign(const Quaternion& p, const Quaternion& q, Quaternion& qt)
{
	int i;
	// decide if one of the quaternions is backwards
	float a = 0;
	float b = 0;
	for (i = 0; i < 4; i++)
	{
		a += (p[i] - q[i]) * (p[i] - q[i]);
		b += (p[i] + q[i]) * (p[i] + q[i]);
	}
	if (a > b)
	{
		for (i = 0; i < 4; i++)
		{
			qt[i] = -q[i];
		}
	}
	else if (&qt != &q)
	{
		for (i = 0; i < 4; i++)
		{
			qt[i] = q[i];
		}
	}

}

void QuaternionConjugate(const Quaternion& p, Quaternion& q)
{
	assertm(q.IsValid(), "invalid quaternion");

	q.x = -p.x;
	q.y = -p.y;
	q.z = -p.z;
	q.w = p.w;
}

// do this better!
float QuaternionNormalize(Quaternion& q)
{
	float radius, iradius;

	assert(q.IsValid());

	radius = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];

	if (radius) // > FLT_EPSILON && ((radius < 1.0f - 4*FLT_EPSILON) || (radius > 1.0f + 4*FLT_EPSILON))
	{
		__m128 _rad = _mm_load_ss(&radius);

		iradius = FastRSqrtFast(_rad);
		radius = FastSqrtFast(_rad);
		//iradius = 1.0f / radius;
		q[3] *= iradius;
		q[2] *= iradius;
		q[1] *= iradius;
		q[0] *= iradius;
	}
	return radius;
}

void QuaternionMult(const Quaternion& p, const Quaternion& q, Quaternion& qt)
{
	assertm(q.IsValid(), "invalid quaternion");
	assertm(p.IsValid(), "invalid quaternion");

	if (&p == &qt)
	{
		Quaternion p2 = p;
		QuaternionMult(p2, q, qt);
		return;
	}

	// decide if one of the quaternions is backwards
	Quaternion q2;
	QuaternionAlign(p, q, q2);

	qt.x = p.x * q2.w + p.y * q2.z - p.z * q2.y + p.w * q2.x;
	qt.y = -p.x * q2.z + p.y * q2.w + p.z * q2.x + p.w * q2.y;
	qt.z = p.x * q2.y - p.y * q2.x + p.z * q2.w + p.w * q2.z;
	qt.w = -p.x * q2.x - p.y * q2.y - p.z * q2.z + p.w * q2.w;
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/mathlib/mathlib_base.cpp#L1868
void QuaternionMatrix(const Quaternion& q, matrix3x4_t& matrix)
{
#ifdef MATH_ASSERTS
	assert(q.IsValid());
#endif // MATH_ASSERTS

	matrix[0][0] = 1.0f - 2.0f * q.y * q.y - 2.0f * q.z * q.z;
	matrix[1][0] = 2.0f * q.x * q.y + 2.0f * q.w * q.z;
	matrix[2][0] = 2.0f * q.x * q.z - 2.0f * q.w * q.y;

	matrix[0][1] = 2.0f * q.x * q.y - 2.0f * q.w * q.z;
	matrix[1][1] = 1.0f - 2.0f * q.x * q.x - 2.0f * q.z * q.z;
	matrix[2][1] = 2.0f * q.y * q.z + 2.0f * q.w * q.x;

	matrix[0][2] = 2.0f * q.x * q.z + 2.0f * q.w * q.y;
	matrix[1][2] = 2.0f * q.y * q.z - 2.0f * q.w * q.x;
	matrix[2][2] = 1.0f - 2.0f * q.x * q.x - 2.0f * q.y * q.y;

	matrix[0][3] = 0.0f;
	matrix[1][3] = 0.0f;
	matrix[2][3] = 0.0f;
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/mathlib/mathlib_base.cpp#L1857
void QuaternionMatrix(const Quaternion& q, const Vector& pos, matrix3x4_t& matrix)
{
#ifdef MATH_ASSERTS
	assert(pos.IsValid());
#endif // MATH_ASSERTS

	QuaternionMatrix(q, matrix);

	matrix[0][3] = pos.x;
	matrix[1][3] = pos.y;
	matrix[2][3] = pos.z;
}

// quaternion to radian angle
void QuaternionAngles(const Quaternion& q, QAngle& angles)
{
#if MATH_ASSERTS
	assert(q.IsValid());
#endif // MATH_ASSERTS

	// LOOK HERE FOR QUATERNION ISSUES!!! ;3333
#if 1
	// FIXME: doing it this way calculates too much data, needs to do an optimized version...
	// [rika]: see above, kept running into issues with the other style though..
	matrix3x4_t matrix;
	QuaternionMatrix(q, matrix);
	MatrixAngles(matrix, angles);
#else
	float m11, m12, m13, m23, m33;

	m11 = (2.0f * q.w * q.w) + (2.0f * q.x * q.x) - 1.0f;
	m12 = (2.0f * q.x * q.y) + (2.0f * q.w * q.z);
	m13 = (2.0f * q.x * q.z) - (2.0f * q.w * q.y);
	m23 = (2.0f * q.y * q.z) + (2.0f * q.w * q.x);
	m33 = (2.0f * q.w * q.w) + (2.0f * q.z * q.z) - 1.0f;

	// FIXME: this code has a singularity near PITCH +-90
	angles[YAW] = RAD2DEG(atan2(m12, m11));
	angles[PITCH] = RAD2DEG(asin(-m13));
	angles[ROLL] = RAD2DEG(atan2(m23, m33));
#endif

#if MATH_ASSERTS
	assert(angles.IsValid());
#endif // MATH_ASSERTS
}

// degree angle to quaternion
static const __m128 aqScaleDegrees = _mm_set_ps1(DEG2RAD(0.5f));
void AngleQuaternion(const QAngle& angles, Quaternion& outQuat)
{
#if MATH_ASSERTS
	assert(angles.IsValid());
#endif // MATH_ASSERTS

	// pitch = x, yaw = y, roll = z
	float sr, sp, sy, cr, cp, cy;

#if MATH_SIMD
	__m128 degrees, /*scale,*/ sine, cosine;
	degrees = _mm_loadu_ps(angles.Base()); // QAngle is align-4; unaligned load (see RadianEuler overload)
	//scale = ReplicateX4(DEG2RAD(0.5f));
	degrees = _mm_mul_ps(degrees, aqScaleDegrees);

	sine = _mm_sincos_ps(&cosine, degrees);

	// NOTE: The ordering here is *different* from the AngleQuaternion above
	// because p, y, r are not in the same locations in QAngle + RadianEuler. Yay!
	sp = SubFloat(sine, 0);	sy = SubFloat(sine, 1);	sr = SubFloat(sine, 2);
	cp = SubFloat(cosine, 0);	cy = SubFloat(cosine, 1);	cr = SubFloat(cosine, 2);
#else
	SinCos(DEG2RAD(angles.y) * 0.5f, &sy, &cy);
	SinCos(DEG2RAD(angles.x) * 0.5f, &sp, &cp);
	SinCos(DEG2RAD(angles.z) * 0.5f, &sr, &cr);
#endif

	float srXcp = sr * cp, crXsp = cr * sp;
	outQuat.x = srXcp * cy - crXsp * sy; // X
	outQuat.y = crXsp * cy + srXcp * sy; // Y

	float crXcp = cr * cp, srXsp = sr * sp;
	outQuat.z = crXcp * sy - srXsp * cy; // Z
	outQuat.w = crXcp * cy + srXsp * sy; // W (real component)

#if MATH_ASSERTS
	assert(outQuat.IsValid());
#endif // MATH_ASSERTS
}

// quaternion to degree angle
void QuaternionAngles(const Quaternion& q, RadianEuler& angles)
{
#if MATH_ASSERTS
	assert(q.IsValid());
#endif // MATH_ASSERTS

	// FIXME: doing it this way calculates too much data, needs to do an optimized version...
	// [rika]: see above, kept running into issues with the other style though..
	matrix3x4_t matrix;
	QuaternionMatrix(q, matrix);
	MatrixAngles(matrix, angles);

#if MATH_ASSERTS
	assert(angles.IsValid());
#endif // MATH_ASSERTS
}

// radian angle to quaternion
static const __m128 aqScaleRadians = _mm_set_ps1(0.5f);
void AngleQuaternion(const RadianEuler& angles, Quaternion& outQuat)
{
#if MATH_ASSERTS
	assert(angles.IsValid());
#endif // MATH_ASSERTS

	// pitch = x, yaw = y, roll = z
	float sr, sp, sy, cr, cp, cy;

#if MATH_SIMD
	__m128 radians, /*scale,*/ sine, cosine;
	// RadianEuler is 3 floats (align 4); callers pass stack locals that are NOT guaranteed
	// 16-byte aligned (e.g. CalcBoneQuaternion's angle/angle1/angle2). An aligned _mm_load_ps
	// there faults (mulps [mem] #GP) -- this was the angel_city model-extract crash the anim
	// hang was masking. Use an unaligned load (the 4th lane is read-past-but-unused).
	radians = _mm_loadu_ps(angles.Base());
	//scale = ReplicateX4(0.5f);
	radians = _mm_mul_ps(radians, aqScaleRadians);

	sine = _mm_sincos_ps(&cosine, radians);

	// NOTE: The ordering here is *different* from the AngleQuaternion below
	// because p, y, r are not in the same locations in QAngle + RadianEuler. Yay!
	// [rika]: honestly this is so pain.. thinking about an enum for each
	sr = SubFloat(sine, 0);	sp = SubFloat(sine, 1);	sy = SubFloat(sine, 2);
	cr = SubFloat(cosine, 0);	cp = SubFloat(cosine, 1);	cy = SubFloat(cosine, 2);
#else
	SinCos(angles.z * 0.5f, &sy, &cy);
	SinCos(angles.y * 0.5f, &sp, &cp);
	SinCos(angles.x * 0.5f, &sr, &cr);
#endif

	float srXcp = sr * cp, crXsp = cr * sp;
	outQuat.x = srXcp * cy - crXsp * sy; // X
	outQuat.y = crXsp * cy + srXcp * sy; // Y

	float crXcp = cr * cp, srXsp = sr * sp;
	outQuat.z = crXcp * sy - srXsp * cy; // Z
	outQuat.w = crXcp * cy + srXsp * sy; // W (real component)

#if MATH_ASSERTS
	assert(outQuat.IsValid());
#endif // MATH_ASSERTS
}


// misc vector funcs
// Rotate a vector around the Z axis (YAW)
void VectorYawRotate(const Vector& in, float flYaw, Vector& out)
{
	if (&in == &out)
	{
		Vector tmp;
		tmp = in;
		VectorYawRotate(tmp, flYaw, out);
		return;
	}

	float sy, cy;

	SinCos(DEG2RAD(flYaw), &sy, &cy);

	out.x = in.x * cy - in.y * sy;
	out.y = in.x * sy + in.y * cy;
	out.z = in.z;
}

void NormalizeAngles(QAngle& angles)
{
	int i;

	// Normalize angles to -180 to 180 range
	for (i = 0; i < 3; i++)
	{
		if (angles[i] > 180.0)
		{
			angles[i] -= 360.0;
		}
		else if (angles[i] < -180.0)
		{
			angles[i] += 360.0;
		}
	}
}