# =============================================================================
# NKMath/bancs/reference_libm.py
# Les résultats CORRECTS des fonctions mathématiques, calculés indépendamment de
# toute libm : arithmétique décimale de Python (module decimal), 50 chiffres
# significatifs (davantage pour réduire les très grands angles), pi par la
# formule de Machin, sin et cos par leur série de Taylor, exp, ln et sqrt par
# le module decimal. Chaque résultat est ARRONDI CORRECTEMENT au double (ou au
# float) le plus proche.
#
# Rodolf (22/10/2026) : « il faudra éprouver ces fonctions pour vérifier qu'elles
# sont exactes en résultat et ne s'écartent pas des résultats corrects ».
#
#   py -3 -I reference_libm.py <sortie.txt>
# Une ligne par point : « <fonction> <x en hexadécimal> <résultat correct en
# hexadécimal> » (16 chiffres pour un double, 8 pour un float).
# banc_libm.exe --reference <sortie.txt> compare NkLibm ET la libm à ces valeurs.
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# DATE : 2026-10-22
# =============================================================================
import random
import struct
import sys
from decimal import Decimal, localcontext

PREC = 50
PI_CHIFFRES = 420
random.seed(20261022)


def pi_machin(chiffres):
    with localcontext() as ctx:
        ctx.prec = chiffres + 10
        eps = Decimal(10) ** (-(chiffres + 8))

        def atan_inv(n):
            x = Decimal(1) / n
            x2 = x * x
            s = t = x
            k = 1
            while True:
                t = -t * x2
                k += 2
                terme = t / k
                if abs(terme) < eps:
                    return s
                s += terme

        return 16 * atan_inv(5) - 4 * atan_inv(239)


PI = pi_machin(PI_CHIFFRES)


def bits64(x):
    return struct.unpack("<Q", struct.pack("<d", x))[0]


def bits32(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def de_bits32(u):
    return struct.unpack("<f", struct.pack("<I", u & 0xFFFFFFFF))[0]


def vers_double(d):
    return float(d)  # str(Decimal) puis float() : arrondi correct


def vers_float(d):
    # Le float le plus proche de d, directement (pas d'arrondi double puis float).
    f = struct.unpack("<f", struct.pack("<f", float(d)))[0]
    if f != f or f in (float("inf"), float("-inf")):
        return f
    b = bits32(f)
    candidats = [f]
    for v in (b - 1, b + 1):
        if 0 <= v <= 0xFFFFFFFF:
            g = de_bits32(v)
            if g == g and abs(g) != float("inf") and (g > 0) == (f > 0):
                candidats.append(g)
    meilleur = None
    for c in candidats:
        e = abs(Decimal(c) - d)
        if meilleur is None or e < meilleur[0] or (e == meilleur[0] and bits32(c) % 2 == 0):
            meilleur = (e, c)
    return meilleur[1]


def sin_cos(x):
    """sin(x), cos(x) exacts à ~PREC chiffres, pour un double x (réduction exacte modulo pi/2)."""
    d = Decimal(x)
    with localcontext() as ctx:
        chiffres_entiers = max(0, d.adjusted() + 1)
        ctx.prec = PREC + chiffres_entiers + 10
        pio2 = PI / 2
        n = int((d / pio2).to_integral_value())
        r = d - n * pio2
        ctx.prec = PREC + 10
        r = +r
        r2 = r * r
        eps = Decimal(10) ** (-(PREC + 8))
        # sin
        s = t = r
        k = 1
        while abs(t) > eps:
            t = -t * r2 / ((k + 1) * (k + 2))
            k += 2
            s += t
        # cos
        c = t = Decimal(1)
        k = 0
        while abs(t) > eps:
            t = -t * r2 / ((k + 1) * (k + 2))
            k += 2
            c += t
        q = n % 4
        if q == 0:
            return s, c
        if q == 1:
            return c, -s
        if q == 2:
            return -s, -c
        return -c, s


def exp_d(x):
    with localcontext() as ctx:
        ctx.prec = PREC + 10
        return Decimal(x).exp()


def ln_d(x):
    with localcontext() as ctx:
        ctx.prec = PREC + 10
        return Decimal(x).ln()


def sqrt_d(x):
    with localcontext() as ctx:
        ctx.prec = PREC + 10
        return Decimal(x).sqrt()


def unif(a, b):
    return random.uniform(a, b)


def log_unif(a, b, signe):
    import math
    v = math.exp(random.uniform(math.log(a), math.log(b)))
    return -v if (signe and random.random() < 0.5) else v


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def main():
    out = open(sys.argv[1], "w")

    def ecrire64(nom, x, d):
        out.write("%s %016X %016X\n" % (nom, bits64(x), bits64(vers_double(d))))

    def ecrire32(nom, x, d):
        out.write("%s %08X %08X\n" % (nom, bits32(x), bits32(vers_float(d))))

    # ── sin, cos, tan ─────────────────────────────────────────────────────
    points = []
    points += [unif(-0.7853981633974483, 0.7853981633974483) for _ in range(20000)]
    points += [unif(-100.0, 100.0) for _ in range(20000)]
    points += [unif(-1e6, 1e6) for _ in range(8000)]
    points += [log_unif(2e6, 1e22, 1) for _ in range(4000)]
    points += [log_unif(1e22, 1e308, 1) for _ in range(1500)]
    # Les cas difficiles : les doubles les plus proches des multiples de pi/2, et le
    # double connu pour être le plus proche d'un multiple de pi/2 (Muller).
    for k in list(range(1, 3001)) + [random.randint(1, 10 ** 9) for _ in range(1000)]:
        points.append(float(k * PI / 2))
    points.append(6381956970095103.0 * 2.0 ** 797)
    for x in points:
        s, c = sin_cos(x)
        ecrire64("sin", x, s)
        ecrire64("cos", x, c)
        with localcontext() as ctx:
            ctx.prec = PREC + 10
            ecrire64("tan", x, s / c)

    # ── exp, log, sqrt ────────────────────────────────────────────────────
    for x in [unif(-745.0, 709.0) for _ in range(20000)] + [unif(-1.0, 1.0) for _ in range(20000)]:
        ecrire64("exp", x, exp_d(x))
    for x in ([log_unif(1e-307, 1e308, 0) for _ in range(20000)] + [unif(0.5, 2.0) for _ in range(20000)] +
              [1.0 + unif(-1e-3, 1e-3) for _ in range(5000)]):
        ecrire64("log", x, ln_d(x))
    for x in [log_unif(1e-300, 1e300, 0) for _ in range(20000)]:
        ecrire64("sqrt", x, sqrt_d(x))

    # ── versions float ────────────────────────────────────────────────────
    for x in [f32(unif(-100.0, 100.0)) for _ in range(20000)]:
        s, c = sin_cos(x)
        ecrire32("sinf", x, s)
        ecrire32("cosf", x, c)
    for x in [f32(unif(-87.0, 88.0)) for _ in range(20000)]:
        ecrire32("expf", x, exp_d(x))
    for x in ([f32(log_unif(1e-37, 1e38, 0)) for _ in range(20000)] + [f32(unif(0.9, 1.1)) for _ in range(20000)]):
        ecrire32("logf", x, ln_d(x))
        ecrire32("sqrtf", x, sqrt_d(x))
    out.close()


main()
