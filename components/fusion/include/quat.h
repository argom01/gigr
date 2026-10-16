#ifndef QUAT_H
#define QUAT_H

// Quaternions are stored as { w, x, y, z }.

void quat_identity(float q[4]);
void quat_normalize(float q[4]);
void quat_mul(const float a[4], const float b[4], float out[4]);
void quat_conj(const float q[4], float out[4]);

// out = q * v * q^-1
void quat_rotate(const float q[4], const float v[3], float out[3]);

// Shortest rotation taking unit vector `from` onto unit vector `to`
void quat_from_two_vectors(const float from[3], const float to[3], float q[4]);

// Z-Y-X (yaw, pitch, roll) angles in degrees
void quat_to_euler_deg(const float q[4], float *roll, float *pitch, float *yaw);

// World "up" (0,0,1) expressed in the body frame, for q = body -> world
void quat_up_in_body(const float q[4], float up[3]);

#endif
