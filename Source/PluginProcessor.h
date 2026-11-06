/*
    VYRS plug-in. Copyright (C) 2026 CloudFreaks B.V.

    This program is free software: you can redistribute it and/or modify it under the terms of the
    GNU Affero General Public License as published by the Free Software Foundation, version 3.
    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
    the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file.
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "VyrsApi.h"

/**
    The VYRS plug-in. Audio passes through untouched; the plug-in reads the DAW's tempo and position,
    and can record what comes in (your vocal) while the DAW plays, to put it on a VYRS take in time.
*/
class VyrsProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    VyrsProcessor();
    ~VyrsProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VYRS"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- what the DAW is doing (read on the message thread) ----
    struct HostInfo { double bpm = 0; bool playing = false; double seconds = -1; };
    HostInfo host() const { return { hostBpm.load(), hostPlaying.load(), hostSeconds.load() }; }

    // ---- recording a vocal onto a take ----
    enum class Rec { idle, armed, recording, uploading, done, failed };
    Rec recState() const { return rec.load(); }
    juce::String recMessage() const { const juce::ScopedLock sl (msgLock); return message; }
    double recordedSeconds() const;
    /** Arms recording for this take: it starts when the DAW starts playing and ends when it stops. */
    void arm (const juce::String& songId, const juce::String& trackId, int takeNumber, double takeStartSeconds);
    void disarm();

    VyrsApi api;

    /** Remembered with the DAW project: which song and take were open, and where the take starts in the DAW. */
    juce::String songId, trackId;
    double takeStart = 0.0;
    bool wav = true;

private:
    void timerCallback() override;
    void setMessage (const juce::String& m) { const juce::ScopedLock sl (msgLock); message = m; }
    void finishRecording();

    std::atomic<double> hostBpm { 0 }, hostSeconds { -1 };
    std::atomic<bool> hostPlaying { false };
    std::atomic<Rec> rec { Rec::idle };
    std::atomic<double> recStartSeconds { 0 };
    std::atomic<juce::int64> recSamples { 0 };
    double sampleRate = 48000.0;

    juce::TimeSliceThread writerThread { "VYRS recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    juce::SpinLock writerLock;
    juce::AudioFormatWriter::ThreadedWriter* activeWriter = nullptr;
    juce::File recFile;
    juce::String recSong, recTrack;
    int recTake = 0;
    double recTakeStart = 0;

    juce::CriticalSection msgLock;
    juce::String message;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VyrsProcessor)
};
