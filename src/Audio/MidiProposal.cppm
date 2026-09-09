module;

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <cmath>

module Artifact.Audio.MidiProposal;

import std;

namespace Artifact {
namespace {

constexpr int kMaxTracks = 64;
constexpr int kMaxNotesPerTrack = 10000;
constexpr double kMaxLengthBeats = 4096.0;

bool finite(double value) {
    return std::isfinite(value);
}

bool hasNumber(const QJsonObject& object, const QString& key) {
    return object.contains(key) && object.value(key).isDouble();
}

} // namespace

QStringList validateMidiProposal(const MidiProposal& proposal) {
    QStringList errors;
    if (proposal.schemaVersion != 1) {
        errors << QStringLiteral("Unsupported schemaVersion");
    }
    if (!finite(proposal.tempo) || proposal.tempo < 20.0 || proposal.tempo > 300.0) {
        errors << QStringLiteral("tempo must be between 20 and 300");
    }
    if (proposal.timeSignatureNumerator < 1 || proposal.timeSignatureNumerator > 32) {
        errors << QStringLiteral("timeSignature numerator is out of range");
    }
    if (proposal.timeSignatureDenominator < 1 || proposal.timeSignatureDenominator > 32) {
        errors << QStringLiteral("timeSignature denominator is out of range");
    }
    if (!finite(proposal.lengthBeats) || proposal.lengthBeats <= 0.0 ||
        proposal.lengthBeats > kMaxLengthBeats) {
        errors << QStringLiteral("lengthBeats is out of range");
    }
    if (proposal.tracks.isEmpty()) {
        errors << QStringLiteral("At least one track is required");
    }
    if (proposal.tracks.size() > kMaxTracks) {
        errors << QStringLiteral("Too many tracks");
    }

    for (int trackIndex = 0; trackIndex < proposal.tracks.size(); ++trackIndex) {
        const MidiTrack& track = proposal.tracks.at(trackIndex);
        if (track.channel < 0 || track.channel > 15) {
            errors << QStringLiteral("Track %1 has an invalid MIDI channel").arg(trackIndex);
        }
        if (track.notes.size() > kMaxNotesPerTrack) {
            errors << QStringLiteral("Track %1 has too many notes").arg(trackIndex);
            continue;
        }
        for (int noteIndex = 0; noteIndex < track.notes.size(); ++noteIndex) {
            const MidiNote& note = track.notes.at(noteIndex);
            if (note.pitch < 0 || note.pitch > 127) {
                errors << QStringLiteral("Track %1 note %2 has an invalid pitch")
                              .arg(trackIndex).arg(noteIndex);
            }
            if (!finite(note.startBeat) || note.startBeat < 0.0) {
                errors << QStringLiteral("Track %1 note %2 has an invalid startBeat")
                              .arg(trackIndex).arg(noteIndex);
            }
            if (!finite(note.durationBeats) || note.durationBeats <= 0.0) {
                errors << QStringLiteral("Track %1 note %2 has an invalid durationBeats")
                              .arg(trackIndex).arg(noteIndex);
            }
            if (note.startBeat + note.durationBeats > proposal.lengthBeats + 1.0e-9) {
                errors << QStringLiteral("Track %1 note %2 exceeds proposal length")
                              .arg(trackIndex).arg(noteIndex);
            }
            if (note.velocity < 1 || note.velocity > 127) {
                errors << QStringLiteral("Track %1 note %2 has an invalid velocity")
                              .arg(trackIndex).arg(noteIndex);
            }
        }
    }
    return errors;
}

MidiProposalParseResult parseMidiProposalJson(const QByteArray& json) {
    MidiProposalParseResult result;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.errors << QStringLiteral("MIDI proposal must be a JSON object: %1")
                             .arg(parseError.errorString());
        return result;
    }

    const QJsonObject root = document.object();
    if (!hasNumber(root, QStringLiteral("schemaVersion")) ||
        !hasNumber(root, QStringLiteral("tempo")) ||
        !hasNumber(root, QStringLiteral("lengthBeats")) ||
        !root.value(QStringLiteral("tracks")).isArray()) {
        result.errors << QStringLiteral("MIDI proposal is missing required fields");
        return result;
    }

    result.proposal.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt();
    result.proposal.tempo = root.value(QStringLiteral("tempo")).toDouble();
    result.proposal.lengthBeats = root.value(QStringLiteral("lengthBeats")).toDouble();
    result.proposal.key = root.value(QStringLiteral("key")).toString().trimmed();

    const QJsonArray timeSignature = root.value(QStringLiteral("timeSignature")).toArray();
    if (timeSignature.size() == 2 && timeSignature.at(0).isDouble() &&
        timeSignature.at(1).isDouble()) {
        result.proposal.timeSignatureNumerator = timeSignature.at(0).toInt();
        result.proposal.timeSignatureDenominator = timeSignature.at(1).toInt();
    }

    const QJsonArray tracks = root.value(QStringLiteral("tracks")).toArray();
    if (tracks.size() > kMaxTracks) {
        result.errors << QStringLiteral("Too many tracks");
    }
    const int trackCount = std::min(static_cast<int>(tracks.size()), kMaxTracks);
    for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex) {
        const QJsonValue& trackValue = tracks.at(trackIndex);
        if (!trackValue.isObject()) {
            result.errors << QStringLiteral("Each track must be an object");
            continue;
        }
        const QJsonObject trackObject = trackValue.toObject();
        if (!hasNumber(trackObject, QStringLiteral("channel")) ||
            !trackObject.value(QStringLiteral("notes")).isArray()) {
            result.errors << QStringLiteral("Track is missing channel or notes");
            continue;
        }
        MidiTrack track;
        track.name = trackObject.value(QStringLiteral("name")).toString().trimmed();
        track.channel = trackObject.value(QStringLiteral("channel")).toInt();
        const QJsonArray notes = trackObject.value(QStringLiteral("notes")).toArray();
        if (notes.size() > kMaxNotesPerTrack) {
            result.errors << QStringLiteral("Track has too many notes");
        }
        const int noteCount = std::min(static_cast<int>(notes.size()), kMaxNotesPerTrack);
        for (int noteIndex = 0; noteIndex < noteCount; ++noteIndex) {
            const QJsonValue& noteValue = notes.at(noteIndex);
            if (!noteValue.isObject()) {
                result.errors << QStringLiteral("Each note must be an object");
                continue;
            }
            const QJsonObject noteObject = noteValue.toObject();
            if (!hasNumber(noteObject, QStringLiteral("pitch")) ||
                !hasNumber(noteObject, QStringLiteral("startBeat")) ||
                !hasNumber(noteObject, QStringLiteral("durationBeats")) ||
                !hasNumber(noteObject, QStringLiteral("velocity"))) {
                result.errors << QStringLiteral("Note is missing required fields");
                continue;
            }
            MidiNote note;
            note.pitch = noteObject.value(QStringLiteral("pitch")).toInt();
            note.startBeat = noteObject.value(QStringLiteral("startBeat")).toDouble();
            note.durationBeats = noteObject.value(QStringLiteral("durationBeats")).toDouble();
            note.velocity = noteObject.value(QStringLiteral("velocity")).toInt();
            track.notes.append(note);
        }
        result.proposal.tracks.append(track);
    }

    if (result.errors.isEmpty()) {
        result.errors = validateMidiProposal(result.proposal);
    }
    return result;
}

QByteArray serializeMidiProposalJson(const MidiProposal& proposal) {
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), proposal.schemaVersion);
    root.insert(QStringLiteral("tempo"), proposal.tempo);
    root.insert(QStringLiteral("timeSignature"), QJsonArray{
        proposal.timeSignatureNumerator, proposal.timeSignatureDenominator});
    root.insert(QStringLiteral("key"), proposal.key);
    root.insert(QStringLiteral("lengthBeats"), proposal.lengthBeats);

    QJsonArray tracks;
    for (const MidiTrack& track : proposal.tracks) {
        QJsonObject trackObject;
        trackObject.insert(QStringLiteral("name"), track.name);
        trackObject.insert(QStringLiteral("channel"), track.channel);
        QJsonArray notes;
        for (const MidiNote& note : track.notes) {
            QJsonObject noteObject;
            noteObject.insert(QStringLiteral("pitch"), note.pitch);
            noteObject.insert(QStringLiteral("startBeat"), note.startBeat);
            noteObject.insert(QStringLiteral("durationBeats"), note.durationBeats);
            noteObject.insert(QStringLiteral("velocity"), note.velocity);
            notes.append(noteObject);
        }
        trackObject.insert(QStringLiteral("notes"), notes);
        tracks.append(trackObject);
    }
    root.insert(QStringLiteral("tracks"), tracks);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

} // namespace Artifact
