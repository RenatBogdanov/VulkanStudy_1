#version 450

layout(location = 0) out vec2 fragTexCoord;

void main() {
    // Генерируем полноэкранный треугольник без вершинного буфера.
    // ВАЖНО: НЕ инвертируем Y здесь.
    // Инверсия под Vulkan Y-down NDC уже выполнена в проекционной матрице
    // (proj[1][1] *= -1 в main.cpp) при рендере в offscreen-текстуру.
    // Двойная инверсия приводила к перевёрнутой сцене и «инвертированной»
    // оси Y у камеры.
    fragTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragTexCoord * 2.0f - 1.0f, 0.0f, 1.0f);
}