#include "WormSwarmSystem.h"
#include "Mesh.h"
#include "glm/gtc/matrix_transform.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cfloat>

namespace {
    constexpr float MIN_VECTOR_LENGTH = 1e-5f;
    constexpr float MIN_DIST_SQ = 1e-6f;
    constexpr float MAX_DT = 0.05f;

    // ---- Силы Boids: ускорение = RATE * maxSpeed * вес из WormTuning ----
    // Привязка к maxSpeed нужна, чтобы соотношение сил не зависело от слайдера Speed
    // (раньше при Speed=3 Separation давал ~125 ед/с² и уносил червей от цели).
    constexpr float SEPARATION_RATE = 4.0f;       // при полном перекрытии: 4 макс.скорости в секунду
    constexpr float COHESION_RATE = 1.0f;
    constexpr float WANDER_RATE = 1.0f;
    constexpr float WANDER_TURN_RATE = 6.0f;      // рад/с, скорость «дрожания» курса
    constexpr float SEEK_RATE = 4.0f;             // 1/с, скорость выхода на курс к цели
    constexpr float ARRIVE_RADIUS = 3.0f;         // внутри — замедляемся (иначе все слипаются в точку)
    constexpr float MIN_ARRIVE_FACTOR = 0.1f;
    constexpr float MIN_TARGET_DIST_SQ = 0.01f;
    constexpr float MIN_NEIGHBOR_RADIUS = 0.1f;
    constexpr float COHESION_RANGE_MUL_SQ = 4.0f; // (2R)^2: соседи, видимые для cohesion

    // ---- Ползание по поверхностям ----
    constexpr float SKIN_WIDTH = 0.02f;                   // зазор между брюхом и гранью
    constexpr float CONTACT_DIST = SKIN_WIDTH * 3.0f;     // «касаемся» поверхности
    constexpr float SNAP_RANGE = 1.5f;                    // дальше — поверхность не ищем
    constexpr float ELLIPSOID_FIT_MIN = 0.5f;             // не сжимаем эллипсоид сильнее этого
    constexpr float MAX_SURFACE_OFFSET = 0.15f;
    constexpr float WALL_NORMAL_Y = 0.7f;                 // |n.y| < порога → стена
    constexpr float CLIMB_GAIN = 1.0f;                    // во что превращаем упор в стену
    constexpr float CLIMB_HEIGHT_MARGIN = 0.5f;           // лезем вверх, только если цель выше червя на столько
    constexpr float WALL_DESCENT_RATE = 3.0f;             // сползание по стене (× maxSpeed в с⁻¹), если лезть не нужно
    constexpr float NORMAL_TURN_RATE = 12.0f;             // 1/с, плавность поворота корпуса
    constexpr int   MAX_CONTACTS = 4;

    // ---- Анимация ----
    constexpr float WIGGLE_IDLE = 0.25f;     // доля волны у стоящего червя
    constexpr float WAVE_IDLE_SPEED = 2.0f;  // рад/с
    constexpr float WAVE_SPEED_GAIN = 3.0f;  // рад на единицу скорости
    constexpr float PHASE_WRAP = 628.31853f; // 100 * 2π — сброс без видимого скачка

    constexpr float SPAWN_HALF_SIZE = 10.0f;
    constexpr float SPAWN_SPEED = 1.0f;

    constexpr uint32_t HASH_X = 73856093u;
    constexpr uint32_t HASH_Y = 19349663u;
    constexpr uint32_t HASH_Z = 83492791u;

    inline uint32_t hashCell(int x, int y, int z) {
        uint32_t h = static_cast<uint32_t>(x) * HASH_X
            ^ static_cast<uint32_t>(y) * HASH_Y
            ^ static_cast<uint32_t>(z) * HASH_Z;
        return h & 4095u;  // TABLE_SIZE - 1
    }

    glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback) {
        float len = glm::length(v);
        if (len > MIN_VECTOR_LENGTH) {
            return v / len;
        }
        return fallback;
    }

    bool isFinite(const glm::vec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    // Ближайшая точка поверхности: point — на грани, normal — внешняя нормаль,
    // dist — знаковое расстояние (отрицательное = червь внутри объекта).
    struct SurfaceHit {
        glm::vec3 point;
        glm::vec3 normal;
        float dist;
        float inset;   // добавка к зазору: surfaceOffset для объектов, 0 для пола
    };

    void queryObb(const glm::vec3& p, const ObbCollider& c, SurfaceHit& out) {
        glm::vec3 d = p - c.center;
        glm::vec3 local(glm::dot(d, c.axisX),
            glm::dot(d, c.axisY),
            glm::dot(d, c.axisZ));

        glm::vec3 clamped(
            glm::clamp(local.x, -c.halfExtents.x, c.halfExtents.x),
            glm::clamp(local.y, -c.halfExtents.y, c.halfExtents.y),
            glm::clamp(local.z, -c.halfExtents.z, c.halfExtents.z));

        glm::vec3 delta = local - clamped;
        float distSq = glm::dot(delta, delta);

        // Снаружи: ближайшая точка — clamped, нормаль — вдоль delta.
        if (distSq > MIN_DIST_SQ) {
            float dist = std::sqrt(distSq);
            out.point = c.center + c.axisX * clamped.x + c.axisY * clamped.y + c.axisZ * clamped.z;
            out.normal = (c.axisX * delta.x + c.axisY * delta.y + c.axisZ * delta.z) / dist;
            out.dist = dist;
            return;
        }

        // Внутри: выталкиваем на ближайшую грань (иначе червь застревал внутри колонны).
        glm::vec3 e = c.halfExtents - glm::abs(local);
        glm::vec3 faceLocal = local;
        float depth;
        if (e.x <= e.y && e.x <= e.z) {
            float s = (local.x >= 0.0f) ? 1.0f : -1.0f;
            out.normal = c.axisX * s;
            faceLocal.x = s * c.halfExtents.x;
            depth = e.x;
        }
        else if (e.y <= e.z) {
            float s = (local.y >= 0.0f) ? 1.0f : -1.0f;
            out.normal = c.axisY * s;
            faceLocal.y = s * c.halfExtents.y;
            depth = e.y;
        }
        else {
            float s = (local.z >= 0.0f) ? 1.0f : -1.0f;
            out.normal = c.axisZ * s;
            faceLocal.z = s * c.halfExtents.z;
            depth = e.z;
        }
        out.point = c.center + c.axisX * faceLocal.x + c.axisY * faceLocal.y + c.axisZ * faceLocal.z;
        out.dist = -depth;
    }

    // Эллипсоид с полуосями halfExtents: нормаль = градиент неявной поверхности,
    // расстояние — первое приближение (по лучу из центра, спроецированное на нормаль).
    // Для червей этого достаточно, а sqrt/деления — единицы на запрос.
    void queryEllipsoid(const glm::vec3& p, const ObbCollider& c, SurfaceHit& out) {
        glm::vec3 d = p - c.center;
        glm::vec3 local(glm::dot(d, c.axisX),
            glm::dot(d, c.axisY),
            glm::dot(d, c.axisZ));

        glm::vec3 q = local / c.halfExtents;
        float qLenSq = glm::dot(q, q);
        if (qLenSq < MIN_DIST_SQ) {
            // Ровно в центре: выталкиваем вверх.
            out.normal = c.axisY;
            out.point = c.center + c.axisY * c.halfExtents.y;
            out.dist = -c.halfExtents.y;
            return;
        }

        glm::vec3 gradLocal = local / (c.halfExtents * c.halfExtents);
        gradLocal /= glm::length(gradLocal);
        out.normal = c.axisX * gradLocal.x + c.axisY * gradLocal.y + c.axisZ * gradLocal.z;

        float qLen = std::sqrt(qLenSq);
        float localLen = glm::length(local);
        float radialGap = localLen * (1.0f - 1.0f / qLen);               // < 0 внутри
        float cosToNormal = glm::dot(local / localLen, gradLocal);       // > 0 для эллипсоида
        out.dist = radialGap * cosToNormal;
        out.point = p - out.normal * out.dist;
    }

    // Во сколько раз сжать эллипсоид, вписанный в AABB, чтобы он шёл по «типичной»
    // поверхности меша (медиана радиусов вершин), а не по углам габаритной коробки.
    float ellipsoidFitScale(const Mesh* mesh, const glm::vec3& center, const glm::vec3& half) {
        if (!mesh) return 1.0f;
        const auto& verts = mesh->getVertices();
        if (verts.empty()) return 1.0f;

        std::vector<float> radii;
        radii.reserve(verts.size());
        for (const auto& v : verts) {
            radii.push_back(glm::length((v.pos - center) / half));
        }
        auto mid = radii.begin() + radii.size() / 2;
        std::nth_element(radii.begin(), mid, radii.end());
        return glm::clamp(*mid, ELLIPSOID_FIT_MIN, 1.0f);
    }

    // Локальный AABB меша (по всем вершинам, без учёта индексов).
    void aabbFromMesh(const Mesh* mesh, glm::vec3& outCenter, glm::vec3& outHalfExtents) {
        outCenter = glm::vec3(0.0f);
        outHalfExtents = glm::vec3(0.5f);
        if (!mesh) return;

        const auto& verts = mesh->getVertices();
        if (verts.empty()) return;

        glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);
        for (const auto& v : verts) {
            mn = glm::min(mn, v.pos);
            mx = glm::max(mx, v.pos);
        }
        outCenter = (mn + mx) * 0.5f;
        outHalfExtents = (mx - mn) * 0.5f;

        if (outHalfExtents.x < MIN_VECTOR_LENGTH) outHalfExtents.x = 0.5f;
        if (outHalfExtents.y < MIN_VECTOR_LENGTH) outHalfExtents.y = 0.5f;
        if (outHalfExtents.z < MIN_VECTOR_LENGTH) outHalfExtents.z = 0.5f;
    }
}

// ============================================================
//  Жизненный цикл / счёт
// ============================================================

// xorshift32: дёшево, без rand() и без аллокаций.
float WormSwarmSystem::random01() {
    m_rng ^= m_rng << 13;
    m_rng ^= m_rng >> 17;
    m_rng ^= m_rng << 5;
    return static_cast<float>(m_rng >> 8) * (1.0f / 16777216.0f);
}

void WormSwarmSystem::resetWorm(size_t index) {
    WormInstanceData& w = m_instances[index];
    w.position = glm::vec3((random01() - 0.5f) * 2.0f * SPAWN_HALF_SIZE,
        SKIN_WIDTH,
        (random01() - 0.5f) * 2.0f * SPAWN_HALF_SIZE);
    w.velocity = glm::vec3((random01() - 0.5f) * SPAWN_SPEED, 0.0f,
        (random01() - 0.5f) * SPAWN_SPEED);
    w.surfaceNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    w.phase = random01() * 6.2832f;
    w.scale = 0.5f + random01() * 0.5f;
    w.wiggle = 0.0f;

    float a = random01() * 6.2832f;
    m_wanderDir[index] = glm::vec2(std::cos(a), std::sin(a));
}

void WormSwarmSystem::setCount(uint32_t count) {
    size_t oldSize = m_instances.size();
    m_instances.resize(count);
    m_wanderDir.resize(count);

    m_cellHead.assign(TABLE_SIZE, INDEX_NONE);
    m_nextWorm.assign(count, INDEX_NONE);

    for (size_t i = oldSize; i < count; ++i) {
        resetWorm(i);
    }
}

void WormSwarmSystem::init(uint32_t count, const std::vector<SceneObject>& objects,
    Mesh* cubeMesh, Mesh* idolMesh) {
    setCount(count);
    rebuildSurfaceGrid(objects, cubeMesh, idolMesh);

    for (size_t i = 0; i < m_instances.size(); ++i) {
        resetWorm(i);
    }
}

// ============================================================
//  rebuildSurfaceGrid: только OBB-примитивы (без треугольников)
// ============================================================
void WormSwarmSystem::rebuildSurfaceGrid(const std::vector<SceneObject>& objects,
    Mesh* cubeMesh, Mesh* idolMesh) {
    // Локальные габариты мешей считаем только при смене меша (обход всех вершин дорог).
    if (cubeMesh != m_cubeMesh || idolMesh != m_idolMesh) {
        m_cubeMesh = cubeMesh;
        m_idolMesh = idolMesh;
        aabbFromMesh(cubeMesh, m_cubeBounds.center, m_cubeBounds.half);
        aabbFromMesh(idolMesh, m_idolBounds.center, m_idolBounds.half);
        m_idolBounds.ellipsoidScale = ellipsoidFitScale(idolMesh, m_idolBounds.center, m_idolBounds.half);
    }

    m_colliders.clear();
    m_modelCache.clear();

    for (const auto& obj : objects) {
        glm::mat4 model = obj.getModelMatrix();
        m_modelCache.push_back(model);  // для всех объектов — индексы должны совпадать с objects

        if (obj.isWaterfall || obj.isLightMarker) continue;

        Mesh* mesh = obj.useIdolMesh ? idolMesh : cubeMesh;
        if (!mesh) continue;
        const LocalBounds& bounds = obj.useIdolMesh ? m_idolBounds : m_cubeBounds;

        glm::vec3 colX(model[0]);
        glm::vec3 colY(model[1]);
        glm::vec3 colZ(model[2]);

        float sx = glm::length(colX);
        float sy = glm::length(colY);
        float sz = glm::length(colZ);
        if (sx < MIN_VECTOR_LENGTH) sx = 1.0f;
        if (sy < MIN_VECTOR_LENGTH) sy = 1.0f;
        if (sz < MIN_VECTOR_LENGTH) sz = 1.0f;

        ObbCollider c;
        c.axisX = colX / sx;
        c.axisY = colY / sy;
        c.axisZ = colZ / sz;
        c.halfExtents = bounds.half * glm::vec3(sx, sy, sz);
        if (obj.useIdolMesh) {
            c.halfExtents *= bounds.ellipsoidScale;
        }
        c.center = glm::vec3(model * glm::vec4(bounds.center, 1.0f));
        c.ellipsoid = obj.useIdolMesh;  // органические формы огибаем плавно

        m_colliders.push_back(c);
    }
}

// Объект сдвинули/повернули/добавили — коллайдеры устарели.
bool WormSwarmSystem::objectsChanged(const std::vector<SceneObject>& objects) const {
    if (objects.size() != m_modelCache.size()) {
        return true;
    }
    for (size_t i = 0; i < objects.size(); ++i) {
        if (objects[i].getModelMatrix() != m_modelCache[i]) {
            return true;
        }
    }
    return false;
}

// ============================================================
//  Zero-Allocation Spatial Hash Grid
//  Размер ячейки = радиус Separation (меняется из UI каждый кадр — таблица
//  перестраивается за один проход O(N), так что это бесплатно).
// ============================================================
void WormSwarmSystem::rebuildWormGrid() {
    std::fill(m_cellHead.begin(), m_cellHead.end(), INDEX_NONE);

    const float invR = 1.0f / m_neighborRadius;
    const size_t n = m_instances.size();

    for (size_t i = 0; i < n; ++i) {
        const glm::vec3& p = m_instances[i].position;
        int cx = static_cast<int>(std::floor(p.x * invR));
        int cy = static_cast<int>(std::floor(p.y * invR));
        int cz = static_cast<int>(std::floor(p.z * invR));

        uint32_t h = hashCell(cx, cy, cz);
        m_nextWorm[i] = m_cellHead[h];
        m_cellHead[h] = static_cast<int32_t>(i);
    }
}

// ============================================================
//  Separation + локальная Cohesion за один обход соседей.
//  Строго O(1) на червячка благодаря MAX_NEIGHBORS.
// ============================================================
void WormSwarmSystem::flock(uint32_t index, glm::vec3& velocity, float dt, float maxSpeed) {
    const glm::vec3 pos = m_instances[index].position;
    const float invR = 1.0f / m_neighborRadius;

    int cx = static_cast<int>(std::floor(pos.x * invR));
    int cy = static_cast<int>(std::floor(pos.y * invR));
    int cz = static_cast<int>(std::floor(pos.z * invR));

    const float rSq = m_neighborRadius * m_neighborRadius;
    const float cohesionRangeSq = rSq * COHESION_RANGE_MUL_SQ;

    glm::vec3 push(0.0f);
    glm::vec3 centerSum(0.0f);
    int pushCount = 0;
    int centerCount = 0;
    int examined = 0;
    bool stop = false;

    for (int dz = -1; dz <= 1 && !stop; ++dz) {
        for (int dy = -1; dy <= 1 && !stop; ++dy) {
            for (int dx = -1; dx <= 1 && !stop; ++dx) {
                uint32_t h = hashCell(cx + dx, cy + dy, cz + dz);

                for (int32_t j = m_cellHead[h]; j != INDEX_NONE; j = m_nextWorm[j]) {
                    if (j == static_cast<int32_t>(index)) continue;

                    if (examined >= MAX_NEIGHBORS) {
                        stop = true;
                        break;
                    }
                    ++examined;

                    const glm::vec3& other = m_instances[j].position;
                    glm::vec3 diff = pos - other;
                    float distSq = glm::dot(diff, diff);

                    // Коллизии хеша: далёкие червяки из чужих ячеек отбрасываем.
                    if (distSq > cohesionRangeSq) continue;

                    centerSum += other;
                    ++centerCount;

                    if (distSq < rSq && distSq > MIN_DIST_SQ) {
                        float dist = std::sqrt(distSq);
                        push += diff * ((m_neighborRadius - dist) / dist);
                        ++pushCount;
                    }
                }
            }
        }
    }

    // Separation: сила нормируется на радиус (≤ 1) ДО умножения на вес из UI.
    if (pushCount > 0) {
        push /= static_cast<float>(pushCount);
        float pushLenSq = glm::dot(push, push);
        if (pushLenSq > MIN_DIST_SQ) {
            float pushLen = std::sqrt(pushLenSq);
            float k = std::min(pushLen * invR, 1.0f) / pushLen;
            velocity += push * (k * dt * maxSpeed * SEPARATION_RATE * m_tuning.separation);
        }
    }

    // Cohesion: к центру ближайших соседей, а не всей стаи; затухает вблизи центра.
    if (centerCount > 0) {
        glm::vec3 toCenter = centerSum / static_cast<float>(centerCount) - pos;
        float lenSq = glm::dot(toCenter, toCenter);
        if (lenSq > MIN_DIST_SQ) {
            float len = std::sqrt(lenSq);
            float falloff = std::min(len * invR, 1.0f) / len;
            velocity += toCenter * (falloff * dt * maxSpeed * COHESION_RATE * m_tuning.cohesion);
        }
    }
}

// ============================================================
//  Wander: поворот единичного вектора малым углом без sin/cos.
// ============================================================
void WormSwarmSystem::wander(uint32_t index, glm::vec3& velocity, float turnStep, float accelDt) {
    glm::vec2& dir = m_wanderDir[index];

    float d = (random01() * 2.0f - 1.0f) * turnStep;
    float c = 1.0f - 0.5f * d * d;  // cos(d) ≈ 1 - d²/2, sin(d) ≈ d
    glm::vec2 r(dir.x * c - dir.y * d, dir.x * d + dir.y * c);
    dir = r / std::sqrt(glm::dot(r, r));  // |r| ≈ 1, обнуления не бывает

    velocity.x += dir.x * accelDt;
    velocity.z += dir.y * accelDt;
}

// ============================================================
//  Ползание: пол (y=0) + OBB объектов.
//  1) собираем контакты (≤ MAX_CONTACTS), 2) выбираем «основную» поверхность —
//     ту, В КОТОРУЮ червь упирается (мин. dot(normal, курс)), иначе ближайшую,
//  3) прижимаем позицию, 4) проецируем скорость на касательную плоскость,
//     упор в стену превращаем в подъём/спуск, 5) плавно доворачиваем корпус.
// ============================================================
void WormSwarmSystem::adhereToSurface(WormInstanceData& worm, float stickBlend, float normalBlend,
    const glm::vec3& toTarget, float dt, float maxSpeed) {
    SurfaceHit contacts[MAX_CONTACTS];
    int contactCount = 0;
    int blockerIdx = -1;
    float blockerScore = FLT_MAX;
    SurfaceHit nearest{ glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), FLT_MAX, 0.0f };
    const float inset = glm::clamp(m_tuning.surfaceOffset, -MAX_SURFACE_OFFSET, MAX_SURFACE_OFFSET);

    const glm::vec3 heading = safeNormalize(worm.velocity, glm::vec3(0.0f));

    auto consider = [&](const SurfaceHit& h) {
        if (h.dist >= SNAP_RANGE) return;
        if (h.dist < nearest.dist) nearest = h;
        if (h.dist > CONTACT_DIST || contactCount >= MAX_CONTACTS) return;

        float score = glm::dot(h.normal, heading);
        if (score < blockerScore) {
            blockerScore = score;
            blockerIdx = contactCount;
        }
        contacts[contactCount++] = h;
        };

    // Пол: бесконечная плоскость y = 0.
    consider(SurfaceHit{ glm::vec3(worm.position.x, 0.0f, worm.position.z),
        glm::vec3(0.0f, 1.0f, 0.0f), worm.position.y, 0.0f });

    // Ориентированные параллелепипеды объектов (Pillar, Idol, ...).
    for (const auto& c : m_colliders) {
        SurfaceHit hit;
        if (c.ellipsoid) {
            queryEllipsoid(worm.position, c, hit);
        }
        else {
            queryObb(worm.position, c, hit);
        }
        hit.inset = inset;
        consider(hit);
    }

    const bool inContact = blockerIdx >= 0;
    if (!inContact && nearest.dist == FLT_MAX) {
        return;  // всё дальше SNAP_RANGE — оставляем как есть
    }
    const SurfaceHit primary = inContact ? contacts[blockerIdx] : nearest;
    const glm::vec3& n = primary.normal;

    // --- Позиция: при проникновении снап жёсткий, иначе по surfaceStick ---
    const float primarySkin = SKIN_WIDTH + primary.inset;
    glm::vec3 target = primary.point + n * primarySkin;
    float blend = (primary.dist < primarySkin) ? 1.0f : stickBlend;
    worm.position = glm::mix(worm.position, target, blend);

    // Остальные контакты (угол пол/стена) не должны быть продавлены.
    for (int k = 0; k < contactCount; ++k) {
        if (k == blockerIdx) continue;
        float d = glm::dot(worm.position - contacts[k].point, contacts[k].normal);
        float skin = SKIN_WIDTH + contacts[k].inset;
        if (d < skin) {
            worm.position += contacts[k].normal * (skin - d);
        }
    }

    // --- Скорость: проекция на касательную плоскость основной поверхности ---
    float vn = glm::dot(n, worm.velocity);
    glm::vec3 v = worm.velocity - n * vn;

    // Стена: лезем вверх только если цель заметно выше. Иначе упор превращается
    // в скольжение вбок (в сторону цели — так огибаем препятствие), а сама стена
    // слегка «стягивает» червя вниз, чтобы он вернулся на пол, а не висел на ней.
    if (std::fabs(n.y) < WALL_NORMAL_Y) {
        float k2 = 1.0f - n.y * n.y;  // >= 0.51 для стен, деление безопасно
        float invK = 1.0f / std::sqrt(k2);
        glm::vec3 wallUp(-n.x * n.y * invK, k2 * invK, -n.z * n.y * invK);
        const bool wantsUp = toTarget.y > CLIMB_HEIGHT_MARGIN;

        if (wantsUp) {
            if (vn < 0.0f) {
                v += wallUp * (-vn * CLIMB_GAIN);
            }
        }
        else {
            if (vn < 0.0f) {
                glm::vec3 side(n.z, 0.0f, -n.x);  // горизонтальная касательная = cross(up, n)
                side *= invK;
                float sideSign = (glm::dot(side, toTarget) >= 0.0f) ? 1.0f : -1.0f;
                v += side * (-vn * sideSign);
            }
            v -= wallUp * (maxSpeed * WALL_DESCENT_RATE * dt);
        }
    }

    // Не пробиваем остальные контакты.
    for (int k = 0; k < contactCount; ++k) {
        if (k == blockerIdx) continue;
        float vk = glm::dot(contacts[k].normal, v);
        if (vk < 0.0f) {
            v -= contacts[k].normal * vk;
        }
    }
    worm.velocity = v;

    // --- Ориентация: корпус плавно доворачивается к нормали (в т.ч. на рёбрах) ---
    glm::vec3 mixed = glm::mix(worm.surfaceNormal, n, normalBlend);
    worm.surfaceNormal = safeNormalize(mixed, n);
}

// ============================================================
//  update(): единственный вход в горячий путь, ноль аллокаций,
//  внутри цикла нет sin/cos/atan2/pow — всё посчитано до цикла.
// ============================================================
void WormSwarmSystem::update(float deltaTime, const glm::vec3& targetPos,
    const std::vector<SceneObject>& objects,
    const EngineSettings& settings) {
    auto t0 = std::chrono::high_resolution_clock::now();

    // Коллайдеры должны следовать за объектами (вращающийся Idol, ручное перемещение).
    if (objectsChanged(objects)) {
        rebuildSurfaceGrid(objects, m_cubeMesh, m_idolMesh);
    }

    if (m_instances.empty()) {
        m_lastUpdateMs = 0.0f;
        return;
    }

    const float dt = std::min(deltaTime, MAX_DT);
    m_neighborRadius = std::max(settings.wormSeparationRadius, MIN_NEIGHBOR_RADIUS);
    rebuildWormGrid();

    // --- Всё, что не зависит от червя, считаем один раз за кадр ---
    const float maxSpeed = std::max(settings.wormSpeed, 0.0f);
    const float maxSpeedSq = maxSpeed * maxSpeed;
    const float invMaxSpeed = (maxSpeed > MIN_VECTOR_LENGTH) ? 1.0f / maxSpeed : 0.0f;
    const float seekLerp = std::min(dt * SEEK_RATE * m_tuning.seek, 1.0f);
    const float wanderTurn = WANDER_TURN_RATE * dt;
    const float wanderAccelDt = WANDER_RATE * maxSpeed * m_tuning.wander * dt;
    const float wiggleAmp = glm::clamp(settings.wormWiggleAmplitude, 0.0f, 1.0f);
    const float normalBlend = 1.0f - std::exp(-dt * NORMAL_TURN_RATE);

    const float stick = glm::clamp(m_tuning.surfaceStick, 0.0f, 1.0f);
    const float stickBlend = (stick >= 1.0f) ? 1.0f : 1.0f - std::pow(1.0f - stick, dt * 60.0f);

    for (size_t i = 0; i < m_instances.size(); ++i) {
        auto& worm = m_instances[i];
        const uint32_t idx = static_cast<uint32_t>(i);

        // --- Seek с «прибытием»: у цели замедляемся, а не слипаемся в точку ---
        glm::vec3 toTarget = targetPos - worm.position;
        float targetDistSq = glm::dot(toTarget, toTarget);
        if (targetDistSq > MIN_TARGET_DIST_SQ) {
            float targetDist = std::sqrt(targetDistSq);
            float arrive = glm::clamp(targetDist * (1.0f / ARRIVE_RADIUS), MIN_ARRIVE_FACTOR, 1.0f);
            glm::vec3 desired = toTarget * (maxSpeed * arrive / targetDist);
            worm.velocity = glm::mix(worm.velocity, desired, seekLerp);
        }

        // --- Separation + Cohesion + Wander ---
        flock(idx, worm.velocity, dt, maxSpeed);
        wander(idx, worm.velocity, wanderTurn, wanderAccelDt);

        if (!isFinite(worm.velocity)) {
            worm.velocity = glm::vec3(0.0f);
        }

        // --- Клампим скорость ---
        float speedSq = glm::dot(worm.velocity, worm.velocity);
        if (speedSq > maxSpeedSq && speedSq > MIN_DIST_SQ) {
            worm.velocity *= maxSpeed / std::sqrt(speedSq);
        }

        // --- Интеграция ---
        worm.position += worm.velocity * dt;
        if (!isFinite(worm.position)) {
            worm.position = glm::vec3(0.0f, SKIN_WIDTH, 0.0f);
            worm.velocity = glm::vec3(0.0f);
        }

        // --- Ползание: прижимаем к поверхности, скорость → в касательную плоскость ---
        adhereToSurface(worm, stickBlend, normalBlend, toTarget, dt, maxSpeed);

        // --- Анимация: фаза и амплитуда волны зависят от реальной скорости ---
        float speed = std::sqrt(glm::dot(worm.velocity, worm.velocity));
        float speed01 = std::min(speed * invMaxSpeed, 1.0f);
        worm.wiggle = wiggleAmp * (WIGGLE_IDLE + (1.0f - WIGGLE_IDLE) * speed01);

        worm.phase += dt * (WAVE_IDLE_SPEED + WAVE_SPEED_GAIN * speed);
        if (worm.phase > PHASE_WRAP) {
            worm.phase -= PHASE_WRAP;
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    m_lastUpdateMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
}