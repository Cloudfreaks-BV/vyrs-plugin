/*
    VYRS plug-in. Copyright (C) 2026 CloudFreaks B.V.

    This program is free software: you can redistribute it and/or modify it under the terms of the
    GNU Affero General Public License as published by the Free Software Foundation, version 3.
    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
    the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file.
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

/**
    Talks to the VYRS server for the plug-in: sign-in with a code approved in the browser
    (the plug-in never sees a password), the song list, file downloads and vocal uploads.
    All network calls run on a background thread; callbacks arrive on the message thread.
*/
class VyrsApi
{
public:
    VyrsApi();
    ~VyrsApi();

    struct Result
    {
        int status = 0;
        juce::var json;
        juce::String error;
        bool ok() const { return status >= 200 && status < 300 && error.isEmpty(); }
    };

    juce::String getServer() const { return server; }
    bool isSignedIn() const;
    void signOut();

    /** Starts sign-in: returns the code to show; opens the browser; keeps polling until approved. */
    void connect (bool newAccount, std::function<void (juce::String code, juce::String error)> started,
                  std::function<void (juce::String error)> finished);
    void cancelConnect();

    void me (std::function<void (Result)> done);
    void songs (std::function<void (Result)> done);

    /** Downloads a file of a take into Music/VYRS/<song>/ (once) and returns it. */
    void prepare (const juce::var& song, const juce::var& take, const juce::var& file, bool wav,
                  std::function<void (juce::File, juce::String error)> done);

    /** Sends a vocal recorded in the DAW to a take; offsetMs = where it starts relative to the take. */
    void uploadVocal (const juce::String& songId, const juce::String& trackId, const juce::File& wav, int offsetMs,
                      std::function<void (Result)> done);

    /** Makes a new take of the song in VYRS from its current lyrics (counts as one track). */
    void newTake (const juce::String& songId, const juce::String& style, const juce::StringArray& voice, std::function<void (Result)> done);

    void openInBrowser (const juce::String& path) const;

    static juce::File library();
    static juce::String fileNameFor (const juce::var& song, const juce::var& take, const juce::var& file, bool wav);
    /** Where a file of a take is (or will be) saved: Music/VYRS/<song>/<name>. */
    static juce::File targetFor (const juce::var& song, const juce::var& take, const juce::var& file, bool wav);

private:
    Result request (const juce::String& path, const juce::String& method, const juce::String& jsonBody = {}, int timeoutMs = 20000);
    void run (std::function<void()> job);
    void setToken (const juce::String& t);
    juce::String getToken() const;
    static juce::File appDir();
    static juce::String machineId();

    juce::String server;
    mutable juce::CriticalSection lock;
    juce::String token;
    juce::ThreadPool pool { 2 };
    std::atomic<bool> polling { false };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VyrsApi)
};
