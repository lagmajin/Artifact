module;

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

export module Artifact.Audio.MidiProposal;

export namespace Artifact {

struct MidiNote {
    int pitch = 60;
    double startBeat = 0.0;
    double durationBeats = 1.0;
    int velocity = 100;
};

struct MidiTrack {
    QString name;
    int channel = 0;
    QVector<MidiNote> notes;
};

struct MidiProposal {
    int schemaVersion = 1;
    double tempo = 120.0;
    int timeSignatureNumerator = 4;
    int timeSignatureDenominator = 4;
    QString key;
    double lengthBeats = 16.0;
    QVector<MidiTrack> tracks;
};

struct MidiProposalParseResult {
    MidiProposal proposal;
    QStringList errors;

    bool ok() const { return errors.isEmpty(); }
};

MidiProposalParseResult parseMidiProposalJson(const QByteArray& json);
QStringList validateMidiProposal(const MidiProposal& proposal);
QByteArray serializeMidiProposalJson(const MidiProposal& proposal);

} // namespace Artifact
