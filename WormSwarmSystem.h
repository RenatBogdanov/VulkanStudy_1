#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

#include "RenderTypes.h"

class Mesh;

// 64 байта, std430-совместимо с WormData в shaders/worm.vert.
struct WormInstanceData {
    glm::vec3 position;
    float     wiggle;        // амплитуда волны изгиба [0..1] (уже учитывает скорость)
    glm::vec3 velocity;
    float     padding2;
    glm::vec3 surfaceNormal; // единичная нормаль поверхности (ось «вверх» корпуса)
    float     phase;         // фаза бегущей волны (растёт со скоростью)
    float     scale;
    float     padding3[3];
};
static_assert(sizeof(WormInstanceData) == 64, "WormInstanceData должен совпадать с std430-структурой в worm.vert");

// Ориентированный ограничивающий параллелепипед (OBB) в мировых координатах.
// Заменяет перебор треугольников в adhereToSurface (PS2-подход: грубо, но O(1)).
struct ObbCollider {
    glm::vec3 center;
    glm::vec3 axisX;        // unit
    glm::vec3 axisY;        // unit
    glm::vec3 axisZ;        // unit
    glm::vec3 halfExtents;  // вдоль осей axisX/Y/Z
    bool      ellipsoid = false; // true: эллипсоид с полуосями halfExtents (плавная нормаль, без прямых углов)
};

// Коэффициенты Boids, крутятся из вкладки Worms в реальном времени.
struct WormTuning {
    float seek = 1.0f;          // тяга к цели (источнику света)
    float separation = 1.0f;    // отталкивание от соседей
    float cohesion = 0.2f;      // притяжение к локальному центру соседей
    float wander = 0.7f;        // случайное блуждание
    float surfaceStick = 1.0f;  // 0 — не притягивать, 1 — жёстко прижимать к поверхности
    float surfaceOffset = 0.0f; // сдвиг от граней объектов вдоль нормали (< 0 — «утопить»); на пол не действует
};

class WormSwarmSystem {
public:
    WormSwarmSystem() = default;
    ~WormSwarmSystem() = default;

    void init(uint32_t count, const std::vector<SceneObject>& objects, Mesh* cubeMesh, Mesh* idolMesh);
    void update(float deltaTime, const glm::vec3& targetPos,
        const std::vector<SceneObject>& objects, const EngineSettings& settings);
    void rebuildSurfaceGrid(const std::vector<SceneObject>& objects, Mesh* cubeMesh, Mesh* idolMesh);

    const std::vector<WormInstanceData>& getInstances() const { return m_instances; }
    uint32_t getCount() const { return static_cast<uint32_t>(m_instances.size()); }
    void setCount(uint32_t count);

    WormTuning& getTuning() { return m_tuning; }
    const WormTuning& getTuning() const { return m_tuning; }

    // Время выполнения последнего update() в миллисекундах (для отладки в EditorUI).
    float getLastUpdateMs() const { return m_lastUpdateMs; }

private:
    // ---- Данные стаи ---------------------------------------------------
    std::vector<WormInstanceData> m_instances;
    std::vector<glm::vec2> m_wanderDir;   // единичное направление блуждания (XZ), не уходит на GPU

    // ---- Аналитические коллайдеры (пол + OBB объектов) ----------------
    std::vector<ObbCollider> m_colliders;

    // Кэш для автоперестройки коллайдеров при движении объектов.
    struct LocalBounds {
        glm::vec3 center{ 0.0f };
        glm::vec3 half{ 0.5f };
        float ellipsoidScale = 1.0f;  // подгонка эллипсоида под реальные вершины меша
    };
    Mesh* m_cubeMesh = nullptr;
    Mesh* m_idolMesh = nullptr;
    LocalBounds m_cubeBounds;
    LocalBounds m_idolBounds;
    std::vector<glm::mat4> m_modelCache;   // матрицы объектов на момент последней сборки

    // ---- Zero-Allocation Spatial Hash (связные списки по бакетам) ------
    static constexpr uint32_t TABLE_SIZE = 4096;
    static constexpr uint32_t TABLE_MASK = TABLE_SIZE - 1;
    static constexpr int32_t  INDEX_NONE = -1;
    static constexpr int      MAX_NEIGHBORS = 12;

    std::vector<int32_t> m_cellHead;   // размер TABLE_SIZE
    std::vector<int32_t> m_nextWorm;   // размер N

    WormTuning m_tuning;
    uint32_t m_rng = 0x9E3779B9u;
    float m_neighborRadius = 1.0f;
    float m_lastUpdateMs = 0.0f;

    // ---- Внутренние шаги ----------------------------------------------
    bool objectsChanged(const std::vector<SceneObject>& objects) const;
    float random01();
    void resetWorm(size_t index);
    void rebuildWormGrid();
    void flock(uint32_t index, glm::vec3& velocity, float dt, float maxSpeed);
    void wander(uint32_t index, glm::vec3& velocity, float turnStep, float accelDt);
    void adhereToSurface(WormInstanceData& worm, float stickBlend, float normalBlend,
        const glm::vec3& toTarget, float dt, float maxSpeed);
};