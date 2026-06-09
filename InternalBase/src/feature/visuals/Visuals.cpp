#include "Visuals.h"
#include "esp/Esp.h"
#include "enemycounter/EnemyCounter.h"
#include "chams/Chams.h"     // ← добавлено (пока не используется напрямую)

void Visuals::Render()
{
    ESP::Render();
    EnemyCounter::Render();
}