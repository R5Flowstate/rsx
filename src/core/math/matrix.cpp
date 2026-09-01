#include <pch.h>

#include <core/math/matrix.h>


// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/mathlib/mathlib_base.cpp#L416
void MatrixGetColumn(const matrix3x4_t& in, int column, Vector& out)
{
	out.x = in[0][column];
	out.y = in[1][column];
	out.z = in[2][column];
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/mathlib/mathlib_base.cpp#L423
void MatrixSetColumn(const Vector& in, int column, matrix3x4_t& out)
{
	out[0][column] = in.x;
	out[1][column] = in.y;
	out[2][column] = in.z;
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/mathlib/mathlib_base.cpp#L144
void MatrixAngles(const matrix3x4_t& matrix, RadianEuler& angles, Vector& position)
{
	MatrixGetColumn(matrix, 3, position);
	MatrixAngles(matrix, angles);
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/mathlib/mathlib_base.cpp#L150
void MatrixAngles(const matrix3x4_t& matrix, Quaternion& q, Vector& pos)
{
	float trace;
	trace = matrix[0][0] + matrix[1][1] + matrix[2][2] + 1.0f;
	if (trace > 1.0f + FLT_EPSILON)
	{
		q.x = (matrix[2][1] - matrix[1][2]);
		q.y = (matrix[0][2] - matrix[2][0]);
		q.z = (matrix[1][0] - matrix[0][1]);
		q.w = trace;
	}
	else if (matrix[0][0] > matrix[1][1] && matrix[0][0] > matrix[2][2])
	{
		trace = 1.0f + matrix[0][0] - matrix[1][1] - matrix[2][2];
		q.x = trace;
		q.y = (matrix[1][0] + matrix[0][1]);
		q.z = (matrix[0][2] + matrix[2][0]);
		q.w = (matrix[2][1] - matrix[1][2]);
	}
	else if (matrix[1][1] > matrix[2][2])
	{
		trace = 1.0f + matrix[1][1] - matrix[0][0] - matrix[2][2];
		q.x = (matrix[0][1] + matrix[1][0]);
		q.y = trace;
		q.z = (matrix[2][1] + matrix[1][2]);
		q.w = (matrix[0][2] - matrix[2][0]);
	}
	else
	{
		trace = 1.0f + matrix[2][2] - matrix[0][0] - matrix[1][1];
		q.x = (matrix[0][2] + matrix[2][0]);
		q.y = (matrix[2][1] + matrix[1][2]);
		q.z = trace;
		q.w = (matrix[1][0] - matrix[0][1]);
	}

	QuaternionNormalize(q);

#if 0
	// check against the angle version
	RadianEuler ang;
	MatrixAngles(matrix, ang);
	Quaternion test;
	AngleQuaternion(ang, test);
	float d = QuaternionDotProduct(q, test);
	assert(fabs(d) > 0.99 && fabs(d) < 1.01);
#endif

	MatrixGetColumn(matrix, 3, pos);
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/mathlib/mathlib_base.cpp#L208
void MatrixAngles(const matrix3x4_t& matrix, float* angles)
{
	float forward[3];
	float left[3];
	float up[3];

	//
	// Extract the basis vectors from the matrix. Since we only need the Z
	// component of the up vector, we don't get X and Y.
	//
	forward[0] = matrix[0][0];
	forward[1] = matrix[1][0];
	forward[2] = matrix[2][0];
	left[0] = matrix[0][1];
	left[1] = matrix[1][1];
	left[2] = matrix[2][1];
	up[2] = matrix[2][2];

	float xyDist = sqrtf(forward[0] * forward[0] + forward[1] * forward[1]);

	// enough here to get angles?
	if (xyDist > 0.001f)
	{
		// (yaw)	y = ATAN( forward.y, forward.x );		-- in our space, forward is the X axis
		angles[1] = RAD2DEG(atan2f(forward[1], forward[0]));

		// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
		angles[0] = RAD2DEG(atan2f(-forward[2], xyDist));

		// (roll)	z = ATAN( left.z, up.z );
		angles[2] = RAD2DEG(atan2f(left[2], up[2]));
	}
	else	// forward is mostly Z, gimbal lock-
	{
		// (yaw)	y = ATAN( -left.x, left.y );			-- forward is mostly z, so use right for yaw
		angles[1] = RAD2DEG(atan2f(-left[0], left[1]));

		// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
		angles[0] = RAD2DEG(atan2f(-forward[2], xyDist));

		// Assume no roll in this case as one degree of freedom has been lost (i.e. yaw == roll)
		angles[2] = 0;
	}
}