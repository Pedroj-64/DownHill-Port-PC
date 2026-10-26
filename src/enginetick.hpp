// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// T7: un tick del cuerpo de conducción con el bucle de colisión del motor (FUN_00131C90 + FUN_00132C10/FUN_00130F00, barrido elipsoidal FUN_00219970/FUN_002193C0/FUN_00218FB8,
// despenetración FUN_00131490, suspensión FUN_00133810, rozamiento de rueda FUN_00132FD8). Transcripción en float64 de tools/engine_tick_ref4.py + tools/sweep_ref.py
// (HIPÓTESIS hasta Σj ≥ 90 %: hoy 49 % con impacto; ver docs/formats/physics-module.md T7). Espacio del NGP (Z arriba), SIN el giro a Y arriba de Ground::load.
// Se valida bit a bit contra la referencia Python con tests/enginetick_check.cpp (vectores de ~/dh-states/h3/etick_*.bin, tools/enginetick_check.py).
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace etick {

struct D3 { double x = 0, y = 0, z = 0; };
inline D3 operator+(D3 a, D3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline D3 operator-(D3 a, D3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline D3 operator*(D3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline D3 operator*(double s, D3 a) { return a * s; }
inline D3 operator/(D3 a, double s) { return {a.x / s, a.y / s, a.z / s}; }
inline D3 operator-(D3 a) { return {-a.x, -a.y, -a.z}; }
inline double dot(D3 a, D3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline D3 cross(D3 a, D3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline double norm(D3 a) { return std::sqrt(dot(a, a)); }

struct M3 { D3 r[3]; };                                   // filas
inline D3 mulRow(D3 v, const M3& m) { return m.r[0] * v.x + m.r[1] * v.y + m.r[2] * v.z; }   // v @ M
inline D3 mulCol(const M3& m, D3 v) { return {dot(m.r[0], v), dot(m.r[1], v), dot(m.r[2], v)}; }   // M @ v
inline M3 mul(const M3& a, const M3& b) { M3 o; for (int i = 0; i < 3; i++) o.r[i] = mulRow(a.r[i], b); return o; }
inline M3 transpose(const M3& m) { M3 o; o.r[0] = {m.r[0].x, m.r[1].x, m.r[2].x}; o.r[1] = {m.r[0].y, m.r[1].y, m.r[2].y}; o.r[2] = {m.r[0].z, m.r[1].z, m.r[2].z}; return o; }

constexpr double kBig = -3.4028235e38;     // t de solape estático (-FLT_MAX)
constexpr double kSkin = 0.01;             // SK
constexpr double kDt = 0.02, kIinv = 0.001;

// ---------- malla de colisión (espacio NGP) ----------
struct Col {
    std::vector<std::array<D3, 3>> T; std::vector<D3> lo, hi; std::vector<int> surf;
    bool load(const std::vector<uint8_t>& raw) {
        if (raw.size() < 4) return false; uint32_t n; std::memcpy(&n, raw.data(), 4); if (raw.size() != 4 + (size_t)n * 40) return false;
        T.resize(n); lo.resize(n); hi.resize(n); surf.resize(n);
        for (uint32_t i = 0; i < n; i++) {
            const uint8_t* p = raw.data() + 4 + (size_t)i * 40; float f[9]; uint16_t s; std::memcpy(f, p, 36); std::memcpy(&s, p + 36, 2); surf[i] = s;
            for (int k = 0; k < 3; k++) T[i][k] = {f[3 * k], f[3 * k + 1], f[3 * k + 2]};
            lo[i] = {std::min({T[i][0].x, T[i][1].x, T[i][2].x}), std::min({T[i][0].y, T[i][1].y, T[i][2].y}), std::min({T[i][0].z, T[i][1].z, T[i][2].z})};
            hi[i] = {std::max({T[i][0].x, T[i][1].x, T[i][2].x}), std::max({T[i][0].y, T[i][1].y, T[i][2].y}), std::max({T[i][0].z, T[i][1].z, T[i][2].z})};
        }
        return true;
    }
};

struct Hit { double t = 0; D3 n, pt; double depth = 0; int surf = 0, tri = 0; };

// ---------- barrido ----------
namespace detail {
inline bool inside(D3 P, D3 A, D3 B, D3 C) {
    D3 v1 = cross(P - A, P - B), v2 = cross(P - B, P - C), v3 = cross(P - C, P - A);
    return dot(v1, v2) > 0 && dot(v1, v3) > 0;
}
struct EdgeHit { double t; D3 q; bool hasN; D3 n; double depth; };

// FUN_00218FB8. Devuelve false si no hay impacto.
inline bool edge(double R, double cur, D3 p0, D3 d, D3 bmin, D3 bmax, D3 Va, D3 Vb, EdgeHit& out) {
    const double va[3] = {Va.x, Va.y, Va.z}, vb[3] = {Vb.x, Vb.y, Vb.z}, bn[3] = {bmin.x, bmin.y, bmin.z}, bx[3] = {bmax.x, bmax.y, bmax.z};
    for (int i = 0; i < 3; i++) { double lo = va[i] < vb[i] ? va[i] : vb[i], hi = va[i] < vb[i] ? vb[i] : va[i]; if (bx[i] < lo || hi < bn[i]) return false; }
    D3 s = p0 - Va, e = Vb - Va; double se = dot(s, e), ee = dot(e, e);
    if (se < ee) {
        if (se < 0) {
            if (R * R < dot(s, s)) {} else { double L = std::sqrt(dot(s, s)); D3 n = s / L; out = {kBig, Va + n * kSkin, true, n, (R - L) + kSkin}; return true; }
        } else {
            D3 q = Va + e * (se / ee), w = p0 - q;
            if (R * R >= dot(w, w)) { double L = std::sqrt(dot(w, w)); D3 n = w / L; out = {kBig, q + n * kSkin, true, n, (R - L) + kSkin}; return true; }
        }
    }
    double ed = dot(e, d), sd = dot(s, d), ss = dot(s, s), dd = dot(d, d);
    double b = 2 * (se * ed - sd * ee), a = ed * ed - ee * dd;
    if (std::fabs(a) > 1e-4) {
        double disc = b * b - a * ((se * se + R * R * ee) - ss * ee) * 4.0;
        if (disc <= 0) return false;
        double sq = std::sqrt(disc), t1 = (-b - sq) / (2 * a), t2 = (-b + sq) / (2 * a), t = std::min(t1, t2);
        if (t < 0 || cur < t) return false;
        double u = dot((p0 + d * t) - Va, e) / ee;
        if (0 <= u && u <= 1) { out = {t, Va + e * u, false, {}, 0.0}; return true; }
    }
    double b2 = 2 * sd;
    if (std::fabs(dd) < 1e-4) return false;
    double disc = b2 * b2 - dd * (ss - R * R) * 4.0;
    if (disc <= 0) return false;
    double sq = std::sqrt(disc), t = std::min((-b2 - sq) / (2 * dd), (-b2 + sq) / (2 * dd));
    if (t < 0) return false;
    if (t <= cur) { out = {t, Va, false, {}, 0.0}; return true; }
    return false;
}

// FUN_002193C0. T en espacio ya deformado.
inline bool triHit(double R, double cur, D3 p0, D3 p1, D3 d, D3 bmin, D3 bmax, const std::array<D3, 3>& T, Hit& out) {
    D3 A = T[0], B = T[1], C = T[2];
    D3 n = cross(C - B, A - B); double ln = norm(n);
    if (ln < 1e-30) return false;
    n = n / ln; double dpl = dot(n, B), d0 = dot(n, p0) - dpl;
    if (d0 < 0) return false;
    double d0r = d0 - R;
    if (d0r <= 0) {
        D3 P = p0 + n * (kSkin - d0);
        if (inside(P, A, B, C)) { out = {kBig, n, P, kSkin - d0r}; return true; }
    } else {
        double d1 = dot(n, p1) - dpl - R;
        if (d1 > 0) return false;
        double t = d0r / (d0r - d1);
        if (t < 0 || t > 1) return false;
        double t2 = t - kSkin / (d0r - d1);
        if (cur < t2) return false;
        D3 P = p0 - n * R + d * t;
        if (inside(P, A, B, C)) { double tc = std::max(t2, 0.0); out = {tc, n, p0 - n * R + d * tc, kSkin - d1}; return true; }
    }
    if (R == 0) return false;
    bool have = false; EdgeHit best{}; double ce = cur;
    for (int i = 0; i < 3; i++) {
        EdgeHit h;
        if (edge(R, ce, p0, d, bmin, bmax, T[i], T[(i + 1) % 3], h) && (!have || h.t < best.t)) { best = h; have = true; ce = std::max(h.t, 0.0); }
    }
    if (!have) return false;
    if (best.t < 0) { out = {best.t, best.n, best.q, best.depth}; return true; }
    D3 nrm = ((p0 + d * best.t) - best.q) / R;
    out = {best.t, nrm, best.q, 0.0}; return true;
}
}   // namespace detail

// FUN_00219970 (fuerza bruta en la caja) con deformación elipsoidal M^T·diag(sc)·M. -> impactos en el orden del motor.
inline std::vector<Hit> sweep(const Col& col, D3 p0, D3 p1, double R, int cap, const M3& M, D3 sc) {
    D3 lo{std::min(p0.x, p1.x) - R, std::min(p0.y, p1.y) - R, std::min(p0.z, p1.z) - R}, hi{std::max(p0.x, p1.x) + R, std::max(p0.y, p1.y) + R, std::max(p0.z, p1.z) + R};
    D3 d = p1 - p0; double cur = 1.0; std::vector<Hit> hits;
    // A = M^T diag(sc) M, Ai = M^T diag(1/sc) M
    M3 A, Ai, Mt = transpose(M);
    { M3 S1, S2; const double s[3] = {sc.x, sc.y, sc.z};
      for (int i = 0; i < 3; i++) { S1.r[i] = M.r[i] * s[i]; S2.r[i] = M.r[i] * (1.0 / s[i]); }
      A = mul(Mt, S1); Ai = mul(Mt, S2); }
    for (size_t i = 0; i < col.T.size(); i++) {
        const D3 &l = col.lo[i], &h = col.hi[i];
        if (!(h.x >= lo.x && h.y >= lo.y && h.z >= lo.z && l.x <= hi.x && l.y <= hi.y && l.z <= hi.z)) continue;
        std::array<D3, 3> Tt = col.T[i];
        for (auto& v : Tt) v = p0 + mulRow(v - p0, transpose(A));   // (v - p0) @ A.T
        Hit hh;
        if (!detail::triHit(R, cur, p0, p1, d, lo, hi, Tt, hh)) continue;
        D3 ns = hh.n, Mn = mulCol(M, ns), nn = mulCol(Mt, D3{sc.x * Mn.x, sc.y * Mn.y, sc.z * Mn.z});   // M.T @ (sc * (M @ ns))
        hh.n = nn / norm(nn);
        hh.pt = p0 + mulCol(Ai, hh.pt - p0);
        if (hh.depth != 0.0) hh.depth = norm(mulCol(Ai, ns * hh.depth));
        hh.surf = col.surf[i]; hh.tri = (int)i;
        double t = hh.t;
        if (t > 0 || cur > 0) { if (cur <= t) continue; hits.assign(1, hh); }
        else { hits.push_back(hh); if ((int)hits.size() > cap) { hits.resize(cap); return hits; } }
        cur = t >= 0 ? t : 0.0;
    }
    return hits;
}

// ---------- estado del cuerpo ----------
struct Susp { double e = 0, off = 0, s = 0; };
struct State {
    double im = 0; D3 cml, cm, P, L, w; M3 M; D3 loc[3]; Susp su[2];
};
inline M3 rot(const M3& M, D3 w, double h) {
    double a = norm(w) * h; if (a < 1e-12) return M;
    D3 k = w / norm(w); M3 K; K.r[0] = {0, -k.z, k.y}; K.r[1] = {k.z, 0, -k.x}; K.r[2] = {-k.y, k.x, 0};
    M3 KK = mul(K, K), R; double s = std::sin(a), c = 1 - std::cos(a);
    for (int i = 0; i < 3; i++) { R.r[i] = K.r[i] * s + KK.r[i] * c; (&R.r[i].x)[i] += 1.0; }
    return mul(M, transpose(R));
}
inline Susp suspStep(Susp w, double h) {      // suspstep2: u ∈ [0, 1] acotado (FUN_00133810)
    double u = 0.5 + w.e + w.off, sv = w.s;
    if (u <= 0) { u = 0.0; sv = sv >= 0 ? sv : 0.0; } else if (u >= 1) { u = 1.0; sv = sv <= 0 ? sv : 0.0; }
    double c = sv < 0 ? 10 : 7, nu = std::min(std::max(u + sv * h, 0.0), 1.0);
    return {nu - 0.5 - w.off, w.off, sv + (80 * (1 - u) - c * sv) * h};
}
inline D3 centre(const State& S, int k, D3 cm, const M3& M, double e) {
    D3 loc = S.loc[k]; if (k < 2) loc.z = -1.8 - e;
    return cm + mulRow(loc - S.cml, M);
}
// FUN_00132FD8 (rozamiento lateral de rueda, hipótesis): recorta la velocidad lateral con tope 0.0225·coef.
inline D3 fric(D3 v, D3 n, D3 lat, double ramp, double coef0, int calls) {
    lat = lat - n * dot(lat, n); lat = lat / norm(lat); double cap = 0.0225 * coef0;
    for (int i = 0; i < calls; i++) {
        D3 vt = v - n * dot(v, n); double sv = dot(lat, vt), sg = sv > 0 ? 1.0 : (sv < 0 ? -1.0 : 0.0), dd = sg * std::min(std::fabs(sv) * ramp, cap); v = v - lat * dd;
    }
    return v;
}

// colgeo.probe: esfera de radio R en c -> (g, normal, superficie).
inline bool probe(const Col& col, D3 c, double R, D3& nrm, int& surf) {
    const double rad = R + 4; double bd = 1e300; int bi = -1; D3 bestP{};
    for (size_t i = 0; i < col.T.size(); i++) {
        const D3 &l = col.lo[i], &h = col.hi[i];
        if (!(h.x >= c.x - rad && h.y >= c.y - rad && h.z >= c.z - rad && l.x <= c.x + rad && l.y <= c.y + rad && l.z <= c.z + rad)) continue;
        const auto& T = col.T[i]; D3 A = T[0], B = T[1], C = T[2];
        D3 n = cross(B - A, C - A); double nl = std::max(norm(n), 1e-30); n = n / nl;
        D3 q = c - n * dot(c - A, n);
        auto side = [&](D3 P, D3 Q) { return dot(cross(Q - P, q - P), n) >= 0; };
        bool ins = side(A, B) && side(B, C) && side(C, A);
        D3 best = q; double bdd = 1e300;
        const D3 pr[3][2] = {{A, B}, {B, C}, {C, A}};
        for (auto& e : pr) {
            D3 ab = e[1] - e[0]; double tt = std::min(std::max(dot(c - e[0], ab) / std::max(dot(ab, ab), 1e-30), 0.0), 1.0); D3 s = e[0] + ab * tt;
            double d = norm(c - s); if (d < bdd) { best = s; bdd = d; }
        }
        if (ins) best = q;
        double d = norm(c - best);
        if (d < bd) { bd = d; bi = (int)i; bestP = best; }
    }
    if (bi < 0) return false;
    D3 dv = c - bestP; double nl = norm(dv);
    if (nl < 1e-9) { const auto& T = col.T[bi]; D3 nn = cross(T[1] - T[0], T[2] - T[0]); double ln = norm(nn); nrm = ln > 1e-12 ? nn / ln : D3{0, 0, 1}; }
    else nrm = dv / nl;
    surf = col.surf[bi]; return true;
}

struct In { State S; D3 E, Fq; double ramp = 1; double coefs[32]; int state[2] = {0, 0}; double rad[3]; D3 scl[3]; };
struct Out { D3 cm, P, L, w; M3 M; Susp su[2]; double j = 0; int nimp = 0, npen = 0; bool err = false; };

// Un tick de 1/50 s. `inp` se toma por valor (E y Fq se borran al primer impacto).
inline Out tick(In inp, const Col& col) {
    const State& S = inp.S; State st = S; D3 E = inp.E, Fq = inp.Fq; double rem = 1.0, jsum = 0; int nimp = 0, npen = 0, it = 1; bool hitw[2] = {false, false};
    const double im = S.im;
    auto adv = [&](State s, double h) {
        s.cm = s.cm + s.P * (h * im); s.M = rot(s.M, s.w, h); s.P = s.P + E * h; s.L = s.L + Fq * h; s.w = s.L * kIinv; s.su[0] = suspStep(s.su[0], h); s.su[1] = suspStep(s.su[1], h);
        return s;
    };
    auto cen = [&](const State& s, int k) { return centre(S, k, s.cm, s.M, k < 2 ? s.su[k].e : 0.0); };
    auto outOf = [&](bool err) { Out o; o.cm = st.cm; o.P = st.P; o.L = st.L; o.w = st.w; o.M = st.M; o.su[0] = st.su[0]; o.su[1] = st.su[1]; o.j = jsum; o.nimp = nimp; o.npen = npen; o.err = err; return o; };
    while (true) {
        double wn = norm(st.w), f = wn < 1e-9 ? rem : std::min(rem, (50 * 1.5707964) / wn);
        D3 c0[3], c1[3]; State st1 = adv(st, f * kDt);
        for (int k = 0; k < 3; k++) { c0[k] = cen(st, k); c1[k] = cen(st1, k); }
        struct H { double t; int k; int idx; Hit h; }; std::vector<H> hits;
        for (int k = 0; k < 3; k++) { auto hs = sweep(col, c0[k], c1[k], inp.rad[k], 10, st.M, inp.scl[k]); for (auto& h : hs) hits.push_back({h.t, k, (int)hits.size(), h}); }
        std::stable_sort(hits.begin(), hits.end(), [](const H& a, const H& b) { if (a.t != b.t) return a.t < b.t; if (a.k != b.k) return a.k < b.k; return a.idx < b.idx; });
        if (hits.empty()) { st = st1; rem -= f; }
        else if (hits[0].t < 0) {
            std::vector<const H*> pen, used; for (auto& x : hits) if (x.t < 0) pen.push_back(&x); used = pen; for (auto& x : hits) if (x.t == 0) used.push_back(&x);
            D3 sm{};
            for (size_t i = 0; i < pen.size(); i++) {
                D3 v = pen[i]->h.n * pen[i]->h.depth;
                for (size_t j = 0; j < used.size(); j++) if (j != i) { double dd = dot(v, used[j]->h.n); if (dd < 0) v = v + used[j]->h.n * (-dd); }
                sm = sm + v;
            }
            double ln = norm(sm);
            if (!(ln < 1e-12)) {
                D3 dr = sm / ln; double sc = 0.0;
                for (auto* x : pen) { double dn = dot(dr, x->h.n), q = dn != 0 ? x->h.depth / dn : 0.0; sc = std::max(sc, q); }
                D3 push = dr * sc;
                if (norm(push) > 1.0) return outOf(true);
                st.cm = st.cm + push;
            }
            npen++;
        } else {
            bool done = false; size_t i = 0;
            while (i < hits.size()) {
                const H& hh = hits[i]; i++; double t = hh.t; int k = hh.k;
                State stt = adv(st, f * t * kDt); D3 ck = cen(stt, k), r = ck - stt.cm, nrm = hh.h.n, d{}, vp;
                if (k < 2) { d = -stt.M.r[2]; vp = stt.P * im + cross(stt.w, r) + d * stt.su[k].s; } else vp = stt.P * im + cross(stt.w, r);
                double vn = dot(nrm, vp);
                if (vn < -1e-3) {
                    double K = im + dot(nrm, cross(cross(r, nrm) * kIinv, r)), j = -vn / K; D3 J = nrm * j, Jp = k < 2 ? J - d * (0.25 * dot(J, d)) : J;
                    stt.P = stt.P + Jp; stt.L = stt.L + cross(r, Jp); stt.w = stt.L * kIinv;
                    if (k < 2) { stt.su[k].s += 0.005 * dot(J, d); hitw[k] = true; }
                    jsum += j; nimp++; E = {}; Fq = {};
                    if (k < 2) stt.P = fric(stt.P * im, nrm, stt.M.r[0], inp.ramp, inp.coefs[hh.h.surf & 31], 1) / im;
                    st = stt; rem -= f * t; done = true;
                    while (i < hits.size() && hits[i].t == t) {
                        const H& h2 = hits[i]; i++; int k2 = h2.k; D3 c2 = cen(st, k2), r2 = c2 - st.cm, n2 = h2.h.n, d2{}, vp2;
                        if (k2 < 2) { d2 = -st.M.r[2]; vp2 = st.P * im + cross(st.w, r2) + d2 * st.su[k2].s; } else vp2 = st.P * im + cross(st.w, r2);
                        double vn2 = dot(n2, vp2);
                        if (vn2 < -1e-3) {
                            double K2 = im + dot(n2, cross(cross(r2, n2) * kIinv, r2)), j2 = -vn2 / K2; D3 J2 = n2 * j2, Jp2 = k2 < 2 ? J2 - d2 * (0.25 * dot(J2, d2)) : J2;
                            st.P = st.P + Jp2; st.L = st.L + cross(r2, Jp2); st.w = st.L * kIinv;
                            if (k2 < 2) { st.su[k2].s += 0.005 * dot(J2, d2); hitw[k2] = true; }
                            jsum += j2; nimp++;
                            if (k2 < 2) st.P = fric(st.P * im, n2, st.M.r[0], inp.ramp, inp.coefs[h2.h.surf & 31], 1) / im;
                        }
                    }
                    break;
                }
            }
            if (!done) { st = adv(st, f * kDt); rem -= f; }
        }
        if (rem < 0.01) rem = 0.0;
        it++;
        if (rem <= 0 || it > 9) break;
    }
    for (int k = 0; k < 2; k++) if (inp.state[k] == 1 && !hitw[k]) {
        D3 nrm; int sf; if (probe(col, cen(st, k), inp.rad[k], nrm, sf)) st.P = fric(st.P * im, nrm, st.M.r[0], inp.ramp, inp.coefs[sf & 31], 1) / im;
    }
    return outOf(false);
}

}   // namespace etick
