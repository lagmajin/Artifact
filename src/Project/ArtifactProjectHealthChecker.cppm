module;
#include <utility>

#include <QString>
#include <QVector>
#include <QSet>
#include <QStack>
#include <QMap>
#include <QFileInfo>
#include <QStringList>
#include <QRegularExpression>

module Artifact.Project.Health;

import Artifact.Project;
import Artifact.Project.Items;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Composition; // CompositionLayerのインポート
import Utils.Id;
import Frame.Range;
import Frame.Position;

namespace Artifact {

ProjectHealthReport ArtifactProjectHealthChecker::check(ArtifactProject* project) {
    ProjectHealthReport report;
    if (!project) {
        report.isHealthy = false;
        report.issues.push_back({HealthIssueSeverity::Error, "Project pointer is null", "Project", "System"});
        return report;
    }

    checkCircularReferences(project, report);
    checkDuplicateIDs(project, report);
    checkFrameRanges(project, report);
    checkMissingAssets(project, report);
    checkBrokenReferences(project, report);
    checkNamingIssues(project, report);
    checkSpellingIssues(project, report);

    for (const auto& issue : report.issues) {
        if (issue.severity == HealthIssueSeverity::Error) {
            report.isHealthy = false;
            break;
        }
    }

    return report;
}

AutoRepairResult ArtifactProjectHealthChecker::checkAndRepair(ArtifactProject* project, const AutoRepairOptions& options) {
    AutoRepairResult result;
    if (!project) {
        result.skippedCount++;
        result.appliedFixes.push_back({
            HealthIssueSeverity::Error,
            "Auto-repair skipped: project pointer is null",
            "Project",
            "System"
        });
        return result;
    }

    if (options.repairFrameRanges) {
        repairFrameRanges(project, result, options);
    }
    if (options.removeMissingAssets) {
        repairMissingAssets(project, result, options);
    }
    if (options.removeBrokenReferences) {
        repairBrokenReferences(project, result, options);
    }
    return result;
}

void ArtifactProjectHealthChecker::checkCircularReferences(ArtifactProject* project, ProjectHealthReport& report) {
    // 全コンポジションを収集
    auto items = project->projectItems();
    QMap<QString, ArtifactAbstractComposition*> compMap;
    QMap<QString, QString> compNames; // エラー報告用

    std::function<void(ProjectItem*)> gatherComps = [&](ProjectItem* item) {
        if (!item) return;
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto res = project->findComposition(compItem->compositionId);
            if (res.success) {
                if (auto comp = res.ptr.lock()) {
                    QString idStr = comp->id().toString();
                    compMap.insert(idStr, comp.get());
                    compNames.insert(idStr, compItem->name.toQString());
                }
            }
        }
        for (auto child : item->children) gatherComps(child);
    };

    for (auto root : items) gatherComps(root);

    // DFS (深さ優先探索) を用いた有向グラフの閉路検出
    QSet<QString> visited;
    QSet<QString> recStack;

    std::function<bool(const QString&, QStringList&)> dfs = [&](const QString& compId, QStringList& path) -> bool {
        visited.insert(compId);
        recStack.insert(compId);
        path.push_back(compNames.value(compId, compId)); // 名前をパスに記録

        if (compMap.contains(compId)) {
            auto comp = compMap[compId];
            for (auto layer : comp->allLayer()) {
                if (!layer) continue;
                // コンポジションレイヤーかどうかを判定
                if (auto compLayer = dynamic_cast<ArtifactCompositionLayer*>(layer.get())) {
                    QString targetId = compLayer->sourceCompositionId().toString();
                    if (!visited.contains(targetId)) {
                        if (dfs(targetId, path)) return true;
                    } else if (recStack.contains(targetId)) {
                        // 閉路を検出 (自身が再帰スタックに存在するノードに到達した)
                        path.push_back(compNames.value(targetId, targetId));
                        return true; 
                    }
                }
            }
        }

        recStack.remove(compId);
        path.pop_back();
        return false;
    };

    // 全てのコンポジションノードを起点としてチェック
    for (auto it = compMap.begin(); it != compMap.end(); ++it) {
        if (!visited.contains(it.key())) {
            QStringList path;
            if (dfs(it.key(), path)) {
                report.issues.push_back({
                    HealthIssueSeverity::Error,
                    QString("Circular reference (Infinite Loop) detected in composition nesting: %1").arg(path.join(" -> ")),
                    compNames.value(it.key(), "Composition"),
                    "CircularReference"
                });
            }
        }
    }
}

void ArtifactProjectHealthChecker::checkDuplicateIDs(ArtifactProject* project, ProjectHealthReport& report) {
    auto items = project->projectItems();
    
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto res = project->findComposition(compItem->compositionId);
            if (res.success) {
                auto comp = res.ptr.lock();
                if (comp) {
                    QSet<QString> layerIds;
                    auto layers = comp->allLayer();
                    for (auto layer : layers) {
                        if (!layer) continue;
                        QString idStr = layer->id().toString();
                        if (layerIds.contains(idStr)) {
                            report.issues.push_back({
                                HealthIssueSeverity::Error,
                                QString("Duplicate Layer ID detected: %1").arg(idStr),
                                compItem->name.toQString(),
                                "DuplicateID"
                            });
                        }
                        layerIds.insert(idStr);
                    }
                }
            }
        }
        
        for (auto child : item->children) traverse(child);
    };

    for (auto root : items) traverse(root);
}

void ArtifactProjectHealthChecker::checkFrameRanges(ArtifactProject* project, ProjectHealthReport& report) {
    auto items = project->projectItems();
    
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto res = project->findComposition(compItem->compositionId);
            if (res.success) {
                auto comp = res.ptr.lock();
                if (comp) {
                    auto range = comp->frameRange();
                    if (range.duration() <= 0) {
                        report.issues.push_back({
                            HealthIssueSeverity::Warning,
                            "Composition duration is zero or negative",
                            compItem->name.toQString(),
                            "FrameRange"
                        });
                    }
                }
            }
        }
        
        for (auto child : item->children) traverse(child);
    };

    for (auto root : items) traverse(root);
}

void ArtifactProjectHealthChecker::checkBrokenReferences(ArtifactProject* project, ProjectHealthReport& report) {
    auto items = project->projectItems();
    
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto res = project->findComposition(compItem->compositionId);
            if (res.success) {
                auto comp = res.ptr.lock();
                if (comp) {
                    for (const auto& layer : comp->allLayer()) {
                        if (!layer) continue;
                        
                        if (auto compLayer = dynamic_cast<ArtifactCompositionLayer*>(layer.get())) {
                            auto sourceRes = project->findComposition(compLayer->sourceCompositionId());
                            if (!sourceRes.success || sourceRes.ptr.expired()) {
                                report.issues.push_back({
                                    HealthIssueSeverity::Error,
                                    QString("Composition layer references missing composition: %1").arg(compLayer->sourceCompositionId().toString()),
                                    compItem->name.toQString() + " / " + layer->layerName(),
                                    "BrokenReference"
                                });
                            }
                        }
                        
                        // We can add other reference checks here (e.g. Missing media in VideoLayer)
                    }
                }
            }
        }
        
        for (auto child : item->children) traverse(child);
    };

    for (auto root : items) traverse(root);
}

void ArtifactProjectHealthChecker::checkNamingIssues(ArtifactProject* project, ProjectHealthReport& report) {
    auto items = project->projectItems();
    
    // Common typo patterns: empty names, placeholder names, suspicious characters
    static const QRegularExpression suspiciousChars(R"([<>:"|?*\\])");
    static const QRegularExpression placeholderPattern(R"(^(Composition|Layer|Folder|Untitled)\s*\d*$)", QRegularExpression::CaseInsensitiveOption);
    
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        
        const QString name = item->name.toQString();
        
        // Check for empty names
        if (name.trimmed().isEmpty()) {
            report.issues.push_back({
                HealthIssueSeverity::Warning,
                "Item has an empty name",
                item->type() == eProjectItemType::Composition ? "Composition" :
                item->type() == eProjectItemType::Folder ? "Folder" : "Item",
                "Naming"
            });
        } else {
            // Check for suspicious characters
            if (suspiciousChars.match(name).hasMatch()) {
                report.issues.push_back({
                    HealthIssueSeverity::Warning,
                    QString("Name contains suspicious characters that may cause issues: %1").arg(name),
                    name,
                    "Naming"
                });
            }
            
            // Check for placeholder names (only as Info, not Warning)
            if (placeholderPattern.match(name).hasMatch()) {
                report.issues.push_back({
                    HealthIssueSeverity::Info,
                    QString("Item uses a placeholder name: %1").arg(name),
                    name,
                    "Naming"
                });
            }
        }
        
        // Check composition layers for naming
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto res = project->findComposition(compItem->compositionId);
            if (res.success) {
                auto comp = res.ptr.lock();
                if (comp) {
                    for (const auto& layer : comp->allLayer()) {
                        if (!layer) continue;
                        const QString layerName = layer->layerName();
                        if (layerName.trimmed().isEmpty()) {
                            report.issues.push_back({
                                HealthIssueSeverity::Warning,
                                "Layer has an empty name",
                                compItem->name.toQString(),
                                "Naming"
                            });
                        }
                    }
                }
            }
        }
        
        for (auto child : item->children) traverse(child);
    };

    for (auto root : items) traverse(root);
}

namespace {
    // Common English words and technical terms that should be considered correct
    static const QSet<QString> kCommonWords = {
        // Technical terms
        "composition", "layer", "effect", "render", "export", "import", "project",
        "animation", "timeline", "keyframe", "mask", "blend", "filter", "shader",
        "texture", "canvas", "viewport", "preview", "sequence", "frame", "video",
        "audio", "image", "solid", "text", "shape", "camera", "light", "null",
        "adjustment", "precomp", "footage", "asset", "folder", "bin", "scene",
        "color", "alpha", "blur", "glow", "shadow", "reflection", "refraction",
        "transform", "position", "rotation", "scale", "opacity", "anchor",
        "background", "foreground", "overlay", "composite", "merge", "split",
        "clip", "trim", "cut", "copy", "paste", "undo", "redo", "save", "load",
        "open", "close", "new", "delete", "create", "duplicate", "rename",
        "select", "deselect", "move", "resize", "rotate", "flip", "mirror",
        "top", "bottom", "left", "right", "center", "middle", "edge", "corner",
        "width", "height", "depth", "size", "length", "duration", "speed",
        "start", "end", "begin", "finish", "first", "last", "next", "previous",
        "main", "sub", "extra", "temp", "test", "draft", "final", "version",
        "v1", "v2", "v3", "v4", "v5", "01", "02", "03", "04", "05",
        "intro", "outro", "title", "subtitle", "caption", "credit", "logo",
        "banner", "thumbnail", "icon", "button", "panel", "window", "dialog",
        "menu", "toolbar", "statusbar", "sidebar", "header", "footer",
        "upper", "lower", "inner", "outer", "front", "back", "side",
        "horizontal", "vertical", "diagonal", "radial", "linear", "circular",
        "gradient", "pattern", "noise", "grain", "dither", "posterize",
        "invert", "contrast", "brightness", "saturation", "hue", "vibrance",
        "exposure", "gamma", "levels", "curves", "threshold", "posterize",
        "desaturate", "grayscale", "monochrome", "sepia", "vintage", "retro",
        "modern", "classic", "minimal", "maximal", "simple", "complex",
        "basic", "advanced", "custom", "default", "preset", "template",
        // Common short words (3+ chars)
        "the", "and", "for", "are", "but", "not", "you", "all", "can", "had",
        "her", "was", "one", "our", "out", "has", "have", "been", "were",
        "they", "this", "that", "with", "from", "what", "when", "where",
        "who", "how", "why", "which", "will", "would", "could", "should",
        // Common creative terms
        "cinematic", "dramatic", "dynamic", "smooth", "fluid", "clean",
        "bold", "vivid", "dark", "light", "bright", "soft", "hard",
        "warm", "cool", "neon", "pastel", "metallic", "glossy", "matte",
        "abstract", "geometric", "organic", "natural", "digital", "analog",
        "retro", "futuristic", "minimalist", "maximalist", "elegant",
        "grunge", "vintage", "modern", "contemporary", "artistic",
        "creative", "professional", "amateur", "experimental", "stylized",
        "realistic", "stylised", "cartoon", "anime", "manga", "comic",
        "illustration", "painting", "drawing", "sketch", "watercolor",
        "oil", "acrylic", "pencil", "ink", "charcoal", "pastel",
        // Japanese romanized common words
        "sakura", "kaze", "hikari", "yami", "tsuki", "taiyou", "hoshi",
        "mizu", "hi", "yuki", "hana", "ao", "aka", "midori", "shiro", "kuro",
    };

    // Common typo patterns (character substitutions that indicate typos)
    static const QMap<QString, QString> kCommonTypos = {
        {"recieve", "receive"}, {"occured", "occurred"}, {"seperate", "separate"},
        {"definately", "definitely"}, {"occassion", "occasion"}, {"accomodate", "accommodate"},
        {"acheive", "achieve"}, {"calender", "calendar"}, {"collegue", "colleague"},
        {"comming", "coming"}, {"committ", "commit"}, {"completly", "completely"},
        {"concious", "conscious"}, {"curiousity", "curiosity"}, {"embarass", "embarrass"},
        {"existance", "existence"}, {"goverment", "government"}, {"grammer", "grammar"},
        {"happend", "happened"}, {"harrass", "harass"}, {"humourous", "humorous"},
        {"immediatly", "immediately"}, {"independant", "independent"}, {"knowlege", "knowledge"},
        {"liason", "liaison"}, {"libary", "library"}, {"maintainance", "maintenance"},
        {"millenium", "millennium"}, {"mispell", "misspell"}, {"naturaly", "naturally"},
        {"neccessary", "necessary"}, {"noticable", "noticeable"}, {"occurance", "occurrence"},
        {"persistant", "persistent"}, {"posession", "possession"}, {"potatos", "potatoes"},
        {"preceed", "precede"}, {"privelege", "privilege"}, {"professer", "professor"},
        {"promiss", "promise"}, {"publically", "publicly"},         {"realy", "really"},
        {"reccomend", "recommend"}, {"refered", "referred"}, {"religous", "religious"},
        {"rythm", "rhythm"}, {"sentance", "sentence"}, {"sieze", "seize"},
        {"succesful", "successful"}, {"suprise", "surprise"}, {"tommorow", "tomorrow"},
        {"truely", "truly"},         {"unfortunatly", "unfortunately"}, {"untill", "until"},
        {"wich", "which"}, {"writting", "writing"},
    };

    bool looksLikeTypo(const QString& word) {
        if (word.length() < 4) return false;
        if (kCommonWords.contains(word.toLower())) return false;
        
        // Check for common typo patterns
        QString lower = word.toLower();
        if (kCommonTypos.contains(lower)) return true;
        
        // Check for repeated characters (e.g., "biiig", "sooo")
        for (int i = 0; i < lower.length() - 2; ++i) {
            if (lower[i] == lower[i+1] && lower[i] == lower[i+2]) return true;
        }
        
        // Check for alternating caps in weird patterns (e.g., "TeSt")
        int alternations = 0;
        for (int i = 1; i < lower.length() - 1; ++i) {
            bool currUpper = word[i].isUpper();
            bool prevUpper = word[i-1].isUpper();
            bool nextUpper = word[i+1].isUpper();
            if (currUpper != prevUpper && currUpper != nextUpper) alternations++;
        }
        if (alternations > 2 && word.length() > 4) return true;
        
        // Check for missing vowels or too many consonants in a row
        int maxConsonants = 0;
        int currentConsonants = 0;
        for (QChar c : lower) {
            if (c.isLetter() && !QString("aeiou").contains(c)) {
                currentConsonants++;
                maxConsonants = std::max(maxConsonants, currentConsonants);
            } else {
                currentConsonants = 0;
            }
        }
        if (maxConsonants >= 5) return true;
        
        return false;
    }

    QStringList tokenizeText(const QString& text) {
        QStringList tokens;
        QString current;
        for (QChar c : text) {
            if (c.isLetterOrNumber() || c == '\'') {
                current += c;
            } else {
                if (!current.isEmpty()) {
                    tokens.append(current);
                    current.clear();
                }
            }
        }
        if (!current.isEmpty()) tokens.append(current);
        return tokens;
    }
}

void ArtifactProjectHealthChecker::checkSpellingIssues(ArtifactProject* project, ProjectHealthReport& report) {
    if (!project) return;
    
    QSet<QString> checkedWords;
    auto checkWord = [&](const QString& word, const QString& context, const QString& targetName) {
        if (word.length() < 4) return;
        QString lower = word.toLower();
        if (checkedWords.contains(lower)) return;
        checkedWords.insert(lower);
        
        if (looksLikeTypo(lower)) {
            QString suggestion;
            if (kCommonTypos.contains(lower)) {
                suggestion = kCommonTypos.value(lower);
            }
            report.issues.push_back({
                HealthIssueSeverity::Info,
                suggestion.isEmpty()
                    ? QString("Possible typo detected: '%1' in %2").arg(word, context)
                    : QString("Possible typo: '%1' (did you mean '%2'?) in %3").arg(word, suggestion, context),
                targetName,
                "Spelling"
            });
        }
    };
    
    // Check project name
    QString projectName = project->settings().projectName();
    for (const QString& token : tokenizeText(projectName)) {
        checkWord(token, "project name", projectName);
    }
    
    // Check project AI metadata
    QString aiDesc = project->aiDescription();
    for (const QString& token : tokenizeText(aiDesc)) {
        checkWord(token, "AI description", projectName);
    }
    
    QStringList aiTags = project->aiTags();
    for (const QString& tag : aiTags) {
        for (const QString& token : tokenizeText(tag)) {
            checkWord(token, "AI tag", tag);
        }
    }
    
    QString aiNotes = project->aiNotes();
    for (const QString& token : tokenizeText(aiNotes)) {
        checkWord(token, "AI notes", projectName);
    }
    
    // Check composition and layer names
    auto items = project->projectItems();
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            const QString compName = compItem->name.toQString();
            
            for (const QString& token : tokenizeText(compName)) {
                checkWord(token, "composition name", compName);
            }
            
            auto res = project->findComposition(compItem->compositionId);
            if (res.success) {
                auto comp = res.ptr.lock();
                if (comp) {
                    for (const auto& layer : comp->allLayer()) {
                        if (!layer) continue;
                        const QString layerName = layer->layerName();
                        
                        for (const QString& token : tokenizeText(layerName)) {
                            checkWord(token, "layer name", compName + " / " + layerName);
                        }
                    }
                }
            }
        }
        
        for (auto child : item->children) traverse(child);
    };

    for (auto root : items) traverse(root);
}

void ArtifactProjectHealthChecker::repairBrokenReferences(ArtifactProject* project, AutoRepairResult& result, const AutoRepairOptions& options) {
    if (!options.removeBrokenReferences) return;

    auto items = project->projectItems();
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto res = project->findComposition(compItem->compositionId);
            if (res.success) {
                auto comp = res.ptr.lock();
                if (comp) {
                    QVector<LayerID> toRemove;
                    for (const auto& layer : comp->allLayer()) {
                        if (!layer) continue;
                        
                        if (auto compLayer = dynamic_cast<ArtifactCompositionLayer*>(layer.get())) {
                            auto sourceRes = project->findComposition(compLayer->sourceCompositionId());
                            if (!sourceRes.success || sourceRes.ptr.expired()) {
                                toRemove.push_back(layer->id());
                            }
                        }
                    }

                    for (const auto& id : toRemove) {
                        comp->removeLayer(id);
                        result.fixedCount++;
                        result.appliedFixes.push_back({
                            HealthIssueSeverity::Warning,
                            QString("Removed layer with broken reference: %1").arg(id.toString()),
                            compItem->name.toQString(),
                            "BrokenReference"
                        });
                    }
                }
            }
        }
        
        for (auto child : item->children) traverse(child);
    };

    for (auto root : items) traverse(root);
}
void ArtifactProjectHealthChecker::checkMissingAssets(ArtifactProject* project, ProjectHealthReport& report) {
    auto items = project->projectItems();

    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;

        if (item->type() == eProjectItemType::Footage) {
            auto* footage = static_cast<FootageItem*>(item);
            QFileInfo fi(footage->filePath);
            if (!fi.exists() || !fi.isFile()) {
                report.issues.push_back({
                    HealthIssueSeverity::Error,
                    QString("Missing asset file: %1").arg(footage->filePath),
                    footage->name.toQString(),
                    "MissingAsset"
                });
            }
        }

        for (auto* child : item->children) {
            traverse(child);
        }
    };

    for (auto* root : items) {
        traverse(root);
    }
}

void ArtifactProjectHealthChecker::repairFrameRanges(ArtifactProject* project, AutoRepairResult& result, const AutoRepairOptions& options) {
    auto items = project->projectItems();
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto compRes = project->findComposition(compItem->compositionId);
            if (compRes.success) {
                if (auto comp = compRes.ptr.lock()) {
                    if (options.normalizeCompositionRanges) {
                        auto range = comp->frameRange();
                        if (range.duration() <= 0) {
                            const auto start = range.start();
                            comp->setFrameRange(ArtifactCore::FrameRange(start, start + 1));
                            result.fixedCount++;
                            result.appliedFixes.push_back({
                                HealthIssueSeverity::Warning,
                                QString("Fixed composition frame range to [%1, %2]").arg(start).arg(start + 1),
                                compItem->name.toQString(),
                                "FrameRange"
                            });
                        }
                    }

                    for (const auto& layer : comp->allLayer()) {
                        if (!layer) continue;
                        const int64_t inFrame = layer->inPoint().framePosition();
                        const int64_t outFrame = layer->outPoint().framePosition();
                        if (outFrame <= inFrame) {
                            layer->setOutPoint(ArtifactCore::FramePosition(inFrame + 1));
                            result.fixedCount++;
                            result.appliedFixes.push_back({
                                HealthIssueSeverity::Warning,
                                QString("Adjusted layer out-point from %1 to %2").arg(outFrame).arg(inFrame + 1),
                                layer->layerName(),
                                "FrameRange"
                            });
                        }
                    }
                }
            }
        }
        for (auto* child : item->children) traverse(child);
    };

    for (auto* root : items) {
        traverse(root);
    }
}

void ArtifactProjectHealthChecker::repairMissingAssets(ArtifactProject* project, AutoRepairResult& result, const AutoRepairOptions& options) {
    if (!options.removeMissingAssets) return;

    QVector<ProjectItem*> toRemove;
    auto items = project->projectItems();
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        if (item->type() == eProjectItemType::Footage) {
            auto* footage = static_cast<FootageItem*>(item);
            QFileInfo fi(footage->filePath);
            if (!fi.exists() || !fi.isFile()) {
                toRemove.push_back(item);
            }
        }
        for (auto* child : item->children) traverse(child);
    };
    for (auto* root : items) {
        traverse(root);
    }

    for (auto* item : toRemove) {
        auto* footage = static_cast<FootageItem*>(item);
        const QString filePath = footage->filePath;
        const QString targetName = footage->name.toQString();
        if (project->removeItem(item)) {
            result.fixedCount++;
            result.appliedFixes.push_back({
                HealthIssueSeverity::Warning,
                QString("Removed missing asset entry: %1").arg(filePath),
                targetName,
                "MissingAsset"
            });
        } else {
            result.skippedCount++;
        }
    }
}

} // namespace Artifact
