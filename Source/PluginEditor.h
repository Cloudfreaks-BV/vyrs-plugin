/*
    VYRS plug-in. Copyright (C) 2026 CloudFreaks B.V.

    This program is free software: you can redistribute it and/or modify it under the terms of the
    GNU Affero General Public License as published by the Free Software Foundation, version 3.
    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
    the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file.
*/

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace vyrs
{
    const juce::Colour ink { 0xff0b0908 }, coal { 0xff16120f }, ash { 0xff241e19 }, line { 0xff2e2721 },
                       dust { 0xff9a9188 }, paper { 0xfff2ede6 }, flare { 0xffff5f1f }, ember { 0xffff7a3d }, rec { 0xffff3b30 };
}

class VyrsLook : public juce::LookAndFeel_V4
{
public:
    VyrsLook();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int height) override;
};

/** One file of a take: click to get it, then drag it into the DAW. */
class FileRow : public juce::Component
{
public:
    FileRow (juce::var song, juce::var take, juce::var file, VyrsProcessor& p, std::function<void()> changed);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void get();
    bool isReady() const { return local.existsAsFile(); }
    bool isLoading() const { return loading; }
    juce::File localFile() const { return local; }
    juce::String kind() const { return file["kind"].toString(); }

private:
    juce::var song, take, file;
    VyrsProcessor& proc;
    std::function<void()> changed;
    juce::File local;
    bool loading = false, dragging = false;
    juce::String error;
};

/** The Hit score of a song and its feedback, as in the studio's Score panel. Sits in a Viewport. */
class ScorePanel : public juce::Component
{
public:
    void setScore (const juce::var& s, bool stale);
    void setBusy (bool b) { busy = b; repaint(); }
    /** Lays the panel out for this width and returns the height it needs. */
    int heightFor (int width) { return render (nullptr, width); }
    void paint (juce::Graphics& g) override { render (&g, getWidth()); }

private:
    int render (juce::Graphics*, int width);
    juce::var score;
    bool stale = false, busy = false;
};

/** The lyrics text: Cmd+S (Ctrl+S) saves instead of typing an "s". */
class LyricsEditor : public juce::TextEditor
{
public:
    std::function<void()> onSave;
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress ('s', juce::ModifierKeys::commandModifier, 0))
        {
            if (onSave) onSave();
            return true;
        }
        return juce::TextEditor::keyPressed (k);
    }
};

class VyrsEditor : public juce::AudioProcessorEditor, private juce::Timer, private juce::ListBoxModel
{
public:
    explicit VyrsEditor (VyrsProcessor&);
    ~VyrsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDrag (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged (int row) override;

    void start();
    void showSignedOut (const juce::String& error = {});
    void connect (bool newAccount);
    void loadSongs();
    void selectSong (const juce::String& id);
    void selectTake (int index);
    void rebuildFiles();
    void layoutFiles();
    juce::var currentSong() const;
    juce::var currentTake() const;

    // lyrics
    enum class Pane { takes, lyrics };
    void showPane (Pane);
    void loadLyrics();
    void applyLyrics (const juce::var& json);
    void saveLyrics (bool overwrite, std::function<void()> then = {});
    void flushLyrics();
    void runScore();
    void runRewrite();
    void layoutScore();
    bool lyricsDirty() const;
    bool lyricsReadOnly() const { return lyricsAccess == "view"; }
    void updateLyricsButtons();
    void setLyricsInfo (const juce::String& text, bool bad = false);
    static juce::String sectionsToText (const juce::var& sections);
    /** Parses "[Verse 1]" headings and lines back into sections, keeping the ids of the sections they came from. */
    static juce::var textToSections (const juce::String& text, const juce::var& prev, juce::String& error);

    VyrsProcessor& proc;
    VyrsLook look;
    enum class View { loading, signedOut, connecting, library, error } view = View::loading;

    juce::String meName, meLine, code, info;
    juce::var songs;
    int songIndex = -1, takeIndex = 0;

    // signed out / connecting
    juce::TextButton connectBtn { "Connect to VYRS" }, createBtn { "New to VYRS? Create an account" }, cancelBtn { "Cancel" };

    // library
    juce::ListBox songList { "Songs", this };
    juce::ComboBox takeBox;
    juce::TextButton wavBtn { "WAV" }, mp3Btn { "MP3" }, studioBtn { "Open in studio" }, refreshBtn { "Refresh" }, signOutBtn { "Disconnect" },
                     folderBtn { "Open VYRS folder" }, getAllBtn { "Get all channels" };
    juce::Label dragAll;
    juce::TextButton newTakeBtn { "REC new take" };
    /** AGPLv3: the source of the plug-in must be one click away */
    juce::HyperlinkButton sourceLink { "Free software (AGPLv3) · source code", juce::URL ("https://github.com/Cloudfreaks-BV/vyrs-plugin") };
    juce::TextEditor notesEdit;
    juce::ComboBox energyBox, tempoBox, voiceBox;
    bool makingTake = false;
    juce::Viewport filesView;
    juce::Component filesHolder;
    juce::OwnedArray<FileRow> rows;

    // recording
    juce::TextButton armBtn { "Record my vocal onto this take" };
    juce::Label takeStartLabel { {}, "Take starts in my DAW at (seconds)" }, takeStartEdit;

    // lyrics
    Pane pane = Pane::takes;
    juce::TextButton takesTab { "Takes" }, lyricsTab { "Lyrics" }, saveBtn { "Save" }, scoreBtn { "Score" }, rewriteBtn { "Rewrite section" };
    LyricsEditor lyricsEdit;
    ScorePanel scorePanel;
    juce::Viewport scoreView;
    juce::String lyricsSong, lyricsAccess, savedText, scoredText, lyricsInfo;
    juce::var lyricsSections, lyricsScore;
    juce::int64 lyricsBase = 0;
    bool lyricsLoading = false, saving = false, scoring = false, rewriting = false, lyricsInfoBad = false;

    juce::Rectangle<int> headerArea, hostArea, recArea, dragAllArea, titleArea, hintArea, statusArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VyrsEditor)
};
