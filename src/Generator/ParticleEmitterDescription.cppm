module;
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QColor>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Generator.Particle;




import Core.AI.Describable;

namespace Artifact {

/**
 * @brief ParticleEmitter description provider
 * 
 * Example implementation of IDescribable for ParticleEmitter class.
 * This provides multilingual descriptions for AI assistants.
 */
class ParticleEmitterDescription : public ArtifactCore::IDescribable {
public:
    QString className() const override {
        return "ParticleEmitter";
    }
    
    ArtifactCore::LocalizedText briefDescription() const override {
        return loc(
            "Emits particles with configurable properties and behaviors.",
            "設定可能なプロパティと動作を持つパーティクルを放出するエミッターです。",
            "发射具有可配置属性和行为的粒子。"
        );
    }
    
    ArtifactCore::LocalizedText detailedDescription() const override {
        return loc(
            "ParticleEmitter is the core component of the particle system. It creates and manages "
            "particles with various emission shapes (Point, Sphere, Box, Circle, Line), controls "
            "emission rates, and defines initial particle properties such as velocity, color, scale, "
            "and lifetime. Effectors can be added to modify particle behavior during simulation.",
            
            "ParticleEmitterはパーティクルシステムの中核コンポーネントです。様々な放出シェイプ"
            "（Point、Sphere、Box、Circle、Line）でパーティクルを作成・管理し、放出レートを制御し、"
            "速度、色、スケール、寿命などの初期プロパティを定義します。エフェクターを追加して"
            "シミュレーション中のパーティクル挙動を変更できます。",
            
            "ParticleEmitter是粒子系统的核心组件。它以各种发射形状（Point、Sphere、Box、Circle、Line）"
            "创建和管理粒子，控制发射速率，并定义初始粒子属性如速度、颜色、缩放和生命周期。"
            "可以添加效果器来修改模拟过程中的粒子行为。"
        );
    }
    
    QList<ArtifactCore::PropertyDescription> propertyDescriptions() const override {
        return {
            // Shape
            {
                "shape",
                loc("Emitter shape type", "エミッターのシェイプタイプ", "发射器形状类型"),
                "EmitterShape",
                "Point",
                {},
                {},
                {"Point", "Sphere", "Box", "Circle", "Rectangle", "Line"},
                loc("Determines the emission area geometry", "放出エリアの形状を決定", "决定发射区域的几何形状")
            },
            
            // Emission
            {
                "rate",
                loc("Particles emitted per second", "1秒あたりの放出パーティクル数", "每秒发射的粒子数"),
                "float",
                "10.0",
                "0",
                "1000",
                {},
                loc("Used in Continuous emission mode", "連続放出モードで使用", "用于连续发射模式")
            },
            {
                "burstCount",
                loc("Particles emitted in burst mode", "バーストモードでの放出数", "爆发模式下的发射数量"),
                "int",
                "100",
                "1",
                "10000",
                {},
                loc("Used in Burst emission mode", "バースト放出モードで使用", "用于爆发发射模式")
            },
            
            // Lifetime
            {
                "lifeMin",
                loc("Minimum particle lifetime (seconds)", "パーティクル最小寿命（秒）", "粒子最小生命周期（秒）"),
                "float",
                "1.0",
                "0.01",
                "60.0",
                {},
                {}
            },
            {
                "lifeMax",
                loc("Maximum particle lifetime (seconds)", "パーティクル最大寿命（秒）", "粒子最大生命周期（秒）"),
                "float",
                "2.0",
                "0.01",
                "60.0",
                {},
                {}
            },
            
            // Velocity
            {
                "speedMin",
                loc("Minimum initial speed", "初期速度の最小値", "初始速度最小值"),
                "float",
                "50.0",
                "0",
                "10000",
                {},
                loc("Pixels per second", "ピクセル/秒", "像素/秒")
            },
            {
                "speedMax",
                loc("Maximum initial speed", "初期速度の最大値", "初始速度最大值"),
                "float",
                "100.0",
                "0",
                "10000",
                {},
                loc("Pixels per second", "ピクセル/秒", "像素/秒")
            },
            {
                "direction",
                loc("Base emission direction", "基本放出方向", "基本发射方向"),
                "QVector3D",
                "(0, -1, 0)",
                {},
                {},
                {},
                loc("Normalized direction vector", "正規化された方向ベクトル", "归一化方向向量")
            },
            {
                "directionSpread",
                loc("Direction randomization angle (degrees)", "方向ランダム化角度（度）", "方向随机化角度（度）"),
                "float",
                "360.0",
                "0",
                "360",
                {},
                loc("360 = full sphere, 0 = single direction", "360=全方向、0=単一方向", "360=全方向，0=单一方向")
            },
            
            // Scale
            {
                "scaleMin",
                loc("Minimum initial scale", "初期スケールの最小値", "初始缩放最小值"),
                "float",
                "1.0",
                "0.01",
                "100.0",
                {},
                {}
            },
            {
                "scaleMax",
                loc("Maximum initial scale", "初期スケールの最大値", "初始缩放最大值"),
                "float",
                "1.0",
                "0.01",
                "100.0",
                {},
                {}
            },
            {
                "scaleEndMin",
                loc("Minimum scale at death", "消滅時スケールの最小値", "死亡时缩放最小值"),
                "float",
                "0.0",
                "0",
                "100.0",
                {},
                loc("Particles interpolate from scaleMin/Max to scaleEndMin/Max", 
                    "scaleMin/MaxからscaleEndMin/Maxへ補間", 
                    "粒子从scaleMin/Max插值到scaleEndMin/Max")
            },
            
            // Color
            {
                "colorStart",
                loc("Color at birth", "誕生時の色", "出生时的颜色"),
                "QColor",
                "white",
                {},
                {},
                {},
                loc("Use alpha for initial opacity", "アルファで初期不透明度を設定", "使用alpha设置初始不透明度")
            },
            {
                "colorEnd",
                loc("Color at death", "消滅時の色", "死亡时的颜色"),
                "QColor",
                "transparent",
                {},
                {},
                {},
                loc("Interpolates from colorStart", "colorStartから補間", "从colorStart插值")
            },
            
            // Physics
            {
                "drag",
                loc("Air resistance factor", "空気抵抗係数", "空气阻力系数"),
                "float",
                "0.0",
                "0",
                "10.0",
                {},
                loc("Higher values slow particles faster", "高い値で早く減速", "值越高减速越快")
            },
            {
                "maxParticles",
                loc("Maximum simultaneous particles", "同時最大パーティクル数", "同时最大粒子数"),
                "int",
                "1000",
                "1",
                "100000",
                {},
                loc("Oldest particles are removed when limit reached", 
                    "制限に達すると最も古いパーティクルが削除", 
                    "达到限制时删除最旧的粒子")
            }
        };
    }
    
    QList<ArtifactCore::MethodDescription> methodDescriptions() const override {
        return {
            {
                "update",
                loc("Update particle simulation by delta time", "デルタタイムでパーティクルシミュレーションを更新", "按时间增量更新粒子模拟"),
                "void",
                {"float"},
                {"deltaTime"},
                loc("Advances simulation and emits new particles", "シミュレーションを進め、新しいパーティクルを放出", "推进模拟并发射新粒子"),
                {"emitter->update(0.016f);  // 60fps"}
            },
            {
                "emitParticles",
                loc("Emit specified number of particles", "指定数のパーティクルを放出", "发射指定数量的粒子"),
                "void",
                {"int"},
                {"count"},
                loc("Manually emit particles", "手動でパーティクルを放出", "手动发射粒子"),
                {"emitter->emitParticles(50);"}
            },
            {
                "emitBurst",
                loc("Emit burst of particles based on burstCount", "burstCountに基づいてパーティクルを一斉放出", "根据burstCount发射一批粒子"),
                "void",
                {},
                {},
                loc("Triggers a single burst emission", "バースト放出をトリガー", "触发一次爆发发射"),
                {"emitter->emitBurst();"}
            },
            {
                "addEffector",
                loc("Add a particle effector to modify behavior", "パーティクル挙動を変更するエフェクターを追加", "添加粒子效果器以修改行为"),
                "void",
                {"std::unique_ptr<ParticleEffector>"},
                {"effector"},
                loc("Effectors modify particle properties during simulation", "エフェクターはシミュレーション中にパーティクルプロパティを変更", "效果器在模拟过程中修改粒子属性"),
                {
                    "auto force = std::make_unique<ForceEffector>();",
                    "force->force = QVector3D(0, -100, 0);  // Gravity",
                    "emitter->addEffector(std::move(force));"
                }
            },
            {
                "clear",
                loc("Remove all particles", "全パーティクルを削除", "移除所有粒子"),
                "void",
                {},
                {},
                loc("Resets simulation state", "シミュレーション状態をリセット", "重置模拟状态"),
                {"emitter->clear();"}
            },
            {
                "preWarm",
                loc("Pre-simulate for specified duration", "指定時間分事前シミュレーション", "预模拟指定时长"),
                "void",
                {"float"},
                {"duration"},
                loc("Useful for effects like smoke that need time to build up", 
                    "煙など、時間をかけて構築する必要があるエフェクトに便利", 
                    "适用于烟雾等需要时间积累的效果"),
                {"emitter->preWarm(2.0f);  // Pre-simulate 2 seconds"}
            }
        };
    }
    
    ArtifactCore::LocalizedText usageExamples() const override {
        return loc(
            "// Create fire effect\n"
            "auto emitter = new ParticleEmitter();\n"
            "auto& params = emitter->params();\n"
            "params.shape = EmitterShape::Point;\n"
            "params.rate = 50.0f;\n"
            "params.lifeMin = 0.5f;\n"
            "params.lifeMax = 1.5f;\n"
            "params.speedMin = 50.0f;\n"
            "params.speedMax = 150.0f;\n"
            "params.direction = QVector3D(0, -1, 0);  // Upward\n"
            "params.colorStart = QColor(255, 200, 50);\n"
            "params.colorEnd = QColor(255, 50, 0, 0);\n"
            "\n"
            "// Add gravity\n"
            "auto gravity = std::make_unique<ForceEffector>();\n"
            "gravity->force = QVector3D(0, 50, 0);  // Slight upward for fire\n"
            "emitter->addEffector(std::move(gravity));\n"
            "\n"
            "// Update in game loop\n"
            "emitter->update(deltaTime);",
            
            "// 炎のエフェクトを作成\n"
            "auto emitter = new ParticleEmitter();\n"
            "auto& params = emitter->params();\n"
            "params.shape = EmitterShape::Point;\n"
            "params.rate = 50.0f;\n"
            "params.lifeMin = 0.5f;\n"
            "params.lifeMax = 1.5f;\n"
            "params.speedMin = 50.0f;\n"
            "params.speedMax = 150.0f;\n"
            "params.direction = QVector3D(0, -1, 0);  // 上向き\n"
            "params.colorStart = QColor(255, 200, 50);\n"
            "params.colorEnd = QColor(255, 50, 0, 0);\n"
            "\n"
            "// 重力を追加\n"
            "auto gravity = std::make_unique<ForceEffector>();\n"
            "gravity->force = QVector3D(0, 50, 0);  // 炎用に少し上向き\n"
            "emitter->addEffector(std::move(gravity));\n"
            "\n"
            "// ゲームループで更新\n"
            "emitter->update(deltaTime);",
            
            "// 创建火焰效果\n"
            "auto emitter = new ParticleEmitter();\n"
            "auto& params = emitter->params();\n"
            "params.shape = EmitterShape::Point;\n"
            "params.rate = 50.0f;\n"
            "params.lifeMin = 0.5f;\n"
            "params.lifeMax = 1.5f;\n"
            "params.speedMin = 50.0f;\n"
            "params.speedMax = 150.0f;\n"
            "params.direction = QVector3D(0, -1, 0);  // 向上\n"
            "params.colorStart = QColor(255, 200, 50);\n"
            "params.colorEnd = QColor(255, 50, 0, 0);\n"
            "\n"
            "// 添加重力\n"
            "auto gravity = std::make_unique<ForceEffector>();\n"
            "gravity->force = QVector3D(0, 50, 0);  // 火焰稍微向上\n"
            "emitter->addEffector(std::move(gravity));\n"
            "\n"
            "// 在游戏循环中更新\n"
            "emitter->update(deltaTime);"
        );
    }
    
    QList<ArtifactCore::LocalizedText> useCases() const override {
        return {
            loc("Fire and flame effects with upward motion and color gradients",
                "上向きの動きと色グラデーションを持つ炎・火炎エフェクト",
                "具有向上运动和颜色渐变的火焰效果"),
            loc("Smoke and steam with large expanding particles",
                "大きく広がるパーティクルの煙・蒸気",
                "大颗粒扩散的烟雾和蒸汽效果"),
            loc("Rain, snow, and weather effects",
                "雨、雪、天候エフェクト",
                "雨、雪和天气效果"),
            loc("Explosions and debris with burst emission",
                "バースト放出の爆発・破片",
                "爆炸和碎片爆发发射"),
            loc("Magic and sparkle effects with additive blending",
                "加算ブレンドの魔法・スパークルエフェクト",
                "使用加法混合的魔法和闪光效果"),
            loc("Confetti and celebration effects",
                "紙吹雪・お祝いエフェクト",
                "彩纸和庆祝效果")
        };
    }
    
    QStringList relatedClasses() const override {
        return {
            "ParticleSystem",
            "ParticleEmitter", 
            "ParticleEffector",
            "ForceEffector",
            "VortexEffector",
            "TurbulenceEffector",
            "AttractorEffector",
            "ParticlePresets",
            "ArtifactParticleLayer"
        };
    }
};

// Register this description
static ArtifactCore::AutoRegisterDescribable<ParticleEmitterDescription> _reg_ParticleEmitter("ParticleEmitter");

} // namespace Artifact