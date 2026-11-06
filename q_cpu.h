/*
 * q_cpu.h -- which 68k a build is for (2026-11-06).
 *
 * CPU-specific code tests only these names, never the compiler's own symbols
 * (Dennis Boon's request): GCC sets __mc68060__ / __mc68040__ from -m68060 /
 * -m68040; a vbcc or SAS/C build passes -DQ_CPU_060, -DQ_CPU_040 or
 * -DQ_CPU_020 itself.
 *
 *   Q_CPU_060, Q_CPU_040, Q_CPU_020   exactly one is defined
 *   Q_SOFT_TRIG   sin/cos without the 68881's transcendental instructions:
 *                 the 68040 and 68060 have none in hardware, the 68040.library
 *                 or 68060.library emulates each one (an F-line exception and
 *                 a software routine)
 */
#ifndef Q_CPU_H
#define Q_CPU_H

#if !defined(Q_CPU_060) && !defined(Q_CPU_040) && !defined(Q_CPU_020)
#if defined(__mc68060__)
#define Q_CPU_060 1
#elif defined(__mc68040__)
#define Q_CPU_040 1
#else
#define Q_CPU_020 1
#endif
#endif

#if defined(Q_CPU_060) || defined(Q_CPU_040)
#define Q_SOFT_TRIG 1
#endif

#endif
