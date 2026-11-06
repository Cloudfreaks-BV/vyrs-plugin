/*
    VYRS plug-in. Copyright (C) 2026 CloudFreaks B.V.

    This program is free software: you can redistribute it and/or modify it under the terms of the
    GNU Affero General Public License as published by the Free Software Foundation, version 3.
    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
    the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file.
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

VyrsProcessor::VyrsProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true))
{
    writerThread.startThread();
    startTimerHz (10);
}

VyrsProcessor::~VyrsProcessor()
{
    stopTimer();
    {
        const SpinLock::ScopedLockType sl (writerLock);
        activeWriter = nullptr;
    }
    writer.reset();
    writerThread.stopThread (2000);
}

bool VyrsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VyrsProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
}

void VyrsProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    bool playing = false;
    double seconds = -1;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying() || pos->getIsRecording();
            seconds = pos->getTimeInSeconds().orFallback (-1.0);
            hostBpm = pos->getBpm().orFallback (0.0);
        }
    hostPlaying = playing;
    hostSeconds = seconds;

    // armed → recording on the first block the DAW plays; recording → stop when it stops
    auto state = rec.load();
    if (state == Rec::armed && playing)
    {
        recStartSeconds = jmax (0.0, seconds);
        recSamples = 0;
        rec = state = Rec::recording;
    }
    if (state == Rec::recording)
    {
        if (! playing)
        {
            rec = Rec::done; // the message thread closes the file and uploads it
            return;
        }
        const SpinLock::ScopedTryLockType sl (writerLock);
        if (sl.isLocked() && activeWriter != nullptr)
            activeWriter->write (buffer.getArrayOfReadPointers(), buffer.getNumSamples());
        recSamples += buffer.getNumSamples();
    }
}

double VyrsProcessor::recordedSeconds() const
{
    return sampleRate > 0 ? (double) recSamples.load() / sampleRate : 0.0;
}

void VyrsProcessor::arm (const String& song, const String& track, int takeNumber, double takeStartSeconds)
{
    if (rec == Rec::recording || rec == Rec::uploading)
        return;
    disarm();
    recFile = File::getSpecialLocation (File::tempDirectory).getChildFile ("VYRS vocal " + Time::getCurrentTime().formatted ("%Y-%m-%d %H.%M.%S") + ".wav");
    recFile.deleteFile();
    auto channels = jmax (1, getTotalNumInputChannels());
    if (auto stream = recFile.createOutputStream())
    {
        WavAudioFormat wavFormat;
        std::unique_ptr<OutputStream> os (stream.release());
        if (auto w = wavFormat.createWriterFor (os, AudioFormatWriterOptions{}.withSampleRate (sampleRate).withNumChannels (channels).withBitsPerSample (24)))
        {
            writer = std::make_unique<AudioFormatWriter::ThreadedWriter> (w.release(), writerThread, 32768);
            const SpinLock::ScopedLockType sl (writerLock);
            activeWriter = writer.get();
        }
    }
    if (writer == nullptr)
    {
        rec = Rec::failed;
        setMessage ("Couldn't start a recording file.");
        return;
    }
    recSong = song;
    recTrack = track;
    recTake = takeNumber;
    recTakeStart = takeStartSeconds;
    rec = Rec::armed;
    setMessage ("Armed. Press play in your DAW and sing; stop the DAW to finish.");
}

void VyrsProcessor::disarm()
{
    if (rec == Rec::recording || rec == Rec::uploading)
        return;
    {
        const SpinLock::ScopedLockType sl (writerLock);
        activeWriter = nullptr;
    }
    writer.reset();
    if (rec == Rec::armed)
        recFile.deleteFile();
    rec = Rec::idle;
    setMessage ({});
}

void VyrsProcessor::timerCallback()
{
    if (rec == Rec::done)
        finishRecording();
}

void VyrsProcessor::finishRecording()
{
    {
        const SpinLock::ScopedLockType sl (writerLock);
        activeWriter = nullptr;
    }
    writer.reset(); // flushes and closes the WAV file

    auto seconds = recordedSeconds();
    if (seconds < 1.0)
    {
        recFile.deleteFile();
        rec = Rec::idle;
        setMessage ("That was shorter than a second, so nothing was sent.");
        return;
    }

    // where the vocal starts, relative to where the take starts in the DAW
    auto offsetMs = roundToInt ((recStartSeconds.load() - recTakeStart) * 1000.0);
    rec = Rec::uploading;
    setMessage ("Sending " + String (seconds, 1) + " s of vocal to take " + String (recTake) + "…");
    auto file = recFile;
    auto take = recTake;
    api.uploadVocal (recSong, recTrack, file, offsetMs, [this, file, take, offsetMs] (VyrsApi::Result r)
    {
        file.deleteFile();
        if (r.ok())
        {
            rec = Rec::idle;
            auto at = offsetMs / 1000.0;
            setMessage ("Your vocal is on take " + String (take) + " in VYRS, starting at " + String (at, 2) + " s. Open the studio to mix it.");
        }
        else
        {
            rec = Rec::failed;
            setMessage (r.error);
        }
    });
}

void VyrsProcessor::getStateInformation (MemoryBlock& dest)
{
    ValueTree s ("VYRS");
    s.setProperty ("song", songId, nullptr);
    s.setProperty ("track", trackId, nullptr);
    s.setProperty ("takeStart", takeStart, nullptr);
    s.setProperty ("wav", wav, nullptr);
    MemoryOutputStream out (dest, false);
    s.writeToStream (out);
}

void VyrsProcessor::setStateInformation (const void* data, int size)
{
    auto s = ValueTree::readFromData (data, (size_t) size);
    if (! s.hasType ("VYRS"))
        return;
    songId = s["song"].toString();
    trackId = s["track"].toString();
    takeStart = (double) s.getProperty ("takeStart", 0.0);
    wav = (bool) s.getProperty ("wav", true);
}

AudioProcessorEditor* VyrsProcessor::createEditor() { return new VyrsEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VyrsProcessor(); }
