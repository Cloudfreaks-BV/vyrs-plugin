/*
    VYRS plug-in. Copyright (C) 2026 CloudFreaks B.V.

    This program is free software: you can redistribute it and/or modify it under the terms of the
    GNU Affero General Public License as published by the Free Software Foundation, version 3.
    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
    the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file.
*/

#include "PluginEditor.h"

using namespace juce;

// ---------------------------------------------------------------- look

VyrsLook::VyrsLook()
{
    setColour (ResizableWindow::backgroundColourId, vyrs::ink);
    setColour (TextButton::buttonColourId, vyrs::coal);
    setColour (TextButton::textColourOffId, vyrs::paper);
    setColour (TextButton::textColourOnId, vyrs::ink);
    setColour (TextButton::buttonOnColourId, vyrs::flare);
    setColour (ComboBox::backgroundColourId, vyrs::coal);
    setColour (ComboBox::outlineColourId, vyrs::line);
    setColour (ComboBox::textColourId, vyrs::paper);
    setColour (ComboBox::arrowColourId, vyrs::dust);
    setColour (PopupMenu::backgroundColourId, vyrs::coal);
    setColour (PopupMenu::textColourId, vyrs::paper);
    setColour (PopupMenu::highlightedBackgroundColourId, vyrs::ash);
    setColour (PopupMenu::highlightedTextColourId, vyrs::paper);
    setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    setColour (Label::textColourId, vyrs::paper);
    setColour (Label::backgroundWhenEditingColourId, vyrs::coal);
    setColour (Label::outlineWhenEditingColourId, vyrs::flare);
    setColour (TextEditor::backgroundColourId, vyrs::coal);
    setColour (TextEditor::textColourId, vyrs::paper);
    setColour (TextEditor::highlightColourId, vyrs::flare.withAlpha (0.4f));
    setColour (ScrollBar::thumbColourId, vyrs::line);
}

void VyrsLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto primary = b.getToggleState() || b.getProperties()["primary"];
    auto fill = primary ? (over ? vyrs::ember : vyrs::flare) : (down ? vyrs::ash : vyrs::coal);
    if (! b.isEnabled())
        fill = fill.withAlpha (0.45f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 8.0f);
    if (! primary)
    {
        g.setColour (over ? Colour (0xff5a4c40) : vyrs::line);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
    }
}

Font VyrsLook::getTextButtonFont (TextButton&, int h)
{
    return FontOptions (jmin (14.0f, (float) h * 0.5f), Font::bold);
}

static void primary (TextButton& b)
{
    b.getProperties().set ("primary", true);
    b.setColour (TextButton::textColourOffId, vyrs::ink);
}

// ---------------------------------------------------------------- file row

FileRow::FileRow (var s, var t, var f, VyrsProcessor& p, std::function<void()> c)
    : song (s), take (t), file (f), proc (p), changed (std::move (c))
{
    auto target = VyrsApi::targetFor (song, take, file, proc.wav);
    // only treat it as ready if it was downloaded before under exactly this name
    if (target.existsAsFile())
        local = target;
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void FileRow::get()
{
    if (loading || isReady())
        return;
    loading = true;
    error = {};
    repaint();
    SafePointer<FileRow> self (this);
    proc.api.prepare (song, take, file, proc.wav, [self] (File f, String e)
    {
        if (self == nullptr) return;
        self->loading = false;
        self->local = f;
        self->error = e;
        self->setMouseCursor (f.existsAsFile() ? MouseCursor::DraggingHandCursor : MouseCursor::PointingHandCursor);
        self->repaint();
        if (self->changed) self->changed();
    });
}

void FileRow::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    auto ready = isReady();
    g.setColour (Colour (0xff1c1713));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (ready ? (isMouseOver() ? vyrs::flare : Colour (0xff4a3b2e)) : vyrs::line);
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    auto dot = Rectangle<float> (14.0f, r.getCentreY() - 5.0f, 10.0f, 10.0f);
    g.setColour (ready ? vyrs::flare : loading ? Colour (0xffffd23f) : Colour (0xff3a322a));
    g.fillEllipse (dot);

    auto text = getLocalBounds().withTrimmedLeft (34).reduced (6, 8);
    g.setColour (vyrs::paper);
    g.setFont (FontOptions (14.0f, Font::bold));
    g.drawText (file["name"].toString(), text.removeFromTop (18), Justification::centredLeft, true);
    g.setFont (FontOptions (12.0f));
    g.setColour (error.isNotEmpty() ? vyrs::ember : vyrs::dust);
    g.drawText (error.isNotEmpty() ? error : ready ? "Drag into your DAW" : loading ? "Getting it…" : String ("Click to get ") + (proc.wav ? "WAV" : "MP3"),
                text, Justification::centredLeft, true);
}

void FileRow::mouseUp (const MouseEvent& e)
{
    dragging = false;
    if (! e.mouseWasDraggedSinceMouseDown() && ! isReady())
        get();
}

void FileRow::mouseDrag (const MouseEvent& e)
{
    if (dragging || ! isReady() || e.getDistanceFromDragStart() < 4)
        return;
    dragging = true;
    DragAndDropContainer::performExternalDragDropOfFiles ({ local.getFullPathName() }, false, this);
}

// ---------------------------------------------------------------- score panel

void ScorePanel::setScore (const var& s, bool isStale)
{
    score = s;
    stale = isStale;
    repaint();
}

/** Draws the panel when g is given; always returns the height it takes at this width. */
int ScorePanel::render (Graphics* g, int width)
{
    const float pad = 14.0f, w = (float) width - 2 * pad;
    float y = pad;
    auto block = [&] (const String& text, const Font& f, Colour c, float gapAfter, float indent = 0.0f)
    {
        AttributedString a;
        a.setWordWrap (AttributedString::byWord);
        a.append (text, f, c);
        TextLayout l;
        l.createLayout (a, jmax (40.0f, w - indent));
        if (g != nullptr)
            l.draw (*g, { pad + indent, y, w - indent, l.getHeight() });
        y += l.getHeight() + gapAfter;
    };

    if (g != nullptr)
    {
        g->setColour (vyrs::coal);
        g->fillRoundedRectangle (getLocalBounds().toFloat(), 12.0f);
        g->setColour (vyrs::line);
        g->drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 12.0f, 1.0f);
    }

    if (! score.isObject())
    {
        block ("Hit score", FontOptions (14.0f, Font::bold), vyrs::paper, 6);
        block (busy ? "Listening like an A&R…"
                    : "Score your lyrics to see how they stack up against what makes hits in this genre work: hook, replay value, virality and how human it sounds.",
               FontOptions (12.0f), vyrs::dust, 0);
        return (int) (y + pad);
    }

    auto alpha = busy ? 0.4f : 1.0f;
    if (g != nullptr)
    {
        g->setColour (vyrs::flare.withAlpha (alpha));
        g->setFont (FontOptions (54.0f, Font::bold));
        auto num = score["hitPotential"].toString();
        g->drawText (num, (int) pad, (int) y, 110, 58, Justification::centredLeft);
        auto x = (int) pad + GlyphArrangement::getStringWidthInt (g->getCurrentFont(), num) + 12;
        g->setColour (vyrs::paper.withAlpha (alpha));
        g->setFont (FontOptions (15.0f, Font::bold));
        g->drawText (score["band"]["label"].toString(), x, (int) y + 12, (int) w - x, 20, Justification::centredLeft, true);
        g->setColour (vyrs::dust);
        g->setFont (FontOptions (12.0f));
        g->drawText ("Hit potential", x, (int) y + 32, (int) w - x, 18, Justification::centredLeft, true);
    }
    y += 64;
    if (stale)
        block (busy ? "Scoring the new lyrics…" : "The lyrics changed since this score.", FontOptions (12.0f), vyrs::ember, 6);
    if (score["hookLine"].toString().isNotEmpty())
        block (String (CharPointer_UTF8 ("\xe2\x80\x9c")) + score["hookLine"].toString() + String (CharPointer_UTF8 ("\xe2\x80\x9d")),
               FontOptions (14.0f, Font::italic), vyrs::paper.withAlpha (alpha), 12);

    for (auto [label, key] : std::initializer_list<std::pair<const char*, const char*>> {
             { "Hook strength", "hookStrength" }, { "Replay value", "replayValue" }, { "Virality", "virality" }, { "Human feel", "humanFeel" }, { "Originality", "originality" } })
    {
        auto v = jlimit (0, 100, (int) score[key]);
        if (g != nullptr)
        {
            g->setFont (FontOptions (12.0f));
            g->setColour (vyrs::dust);
            g->drawText (label, (int) pad, (int) y, (int) w, 16, Justification::centredLeft);
            g->setColour (vyrs::paper);
            g->drawText (String (v), (int) pad, (int) y, (int) w, 16, Justification::centredRight);
            g->setColour (vyrs::ash);
            g->fillRoundedRectangle (pad, y + 19, w, 4, 2);
            g->setColour (vyrs::flare.withAlpha (alpha));
            g->fillRoundedRectangle (pad, y + 19, w * (float) v / 100.0f, 4, 2);
        }
        y += 31;
    }
    y += 6;

    if (auto* st = score["strengths"].getArray(); st != nullptr && ! st->isEmpty())
    {
        block ("Working", FontOptions (12.0f, Font::bold), vyrs::dust, 6);
        for (auto& x : *st)
        {
            if (g != nullptr)
            {
                g->setColour (vyrs::flare);
                g->fillEllipse (pad, y + 5, 5, 5);
            }
            block (x.toString(), FontOptions (12.5f), vyrs::paper, 6, 14);
        }
        y += 8;
    }

    if (auto* tips = score["tips"].getArray(); tips != nullptr && ! tips->isEmpty())
    {
        block ("Make it stronger", FontOptions (12.0f, Font::bold), vyrs::dust, 8);
        for (auto& t : *tips)
        {
            auto top = y;
            y += 10;
            auto sev = t["severity"].toString();
            auto label = t["label"].toString().isNotEmpty() ? t["label"].toString() : t["message"].toString();
            auto text = t["instruction"].toString() + (t["section"].toString().isNotEmpty() ? " (" + t["section"].toString() + ")" : String());
            block (label, FontOptions (13.0f, Font::bold), vyrs::paper, 3, 22);
            block (text, FontOptions (12.0f), vyrs::dust, 10, 22);
            if (g != nullptr)
            {
                g->setColour (sev == "HIGH" ? vyrs::ember : sev == "MEDIUM" ? Colour (0xffffd23f) : vyrs::dust);
                g->fillEllipse (pad + 9, top + 15, 6, 6);
                g->setColour (vyrs::line);
                g->drawRoundedRectangle (pad, top, w, y - top, 10.0f, 1.0f);
            }
            y += 8;
        }
    }
    return (int) (y + pad);
}

// ---------------------------------------------------------------- editor

VyrsEditor::VyrsEditor (VyrsProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    setResizable (true, true);
    setResizeLimits (720, 560, 1600, 1200);
    setSize (920, 620);

    primary (connectBtn);
    primary (armBtn);
    connectBtn.onClick = [this] { connect (false); };
    createBtn.onClick = [this] { connect (true); };
    cancelBtn.onClick = [this] { proc.api.cancelConnect(); showSignedOut(); };

    songList.setRowHeight (52);
    songList.setColour (ListBox::outlineColourId, Colours::transparentBlack);
    takeBox.onChange = [this] { selectTake (takeBox.getSelectedItemIndex()); };

    auto setFormat = [this] (bool w)
    {
        proc.wav = w;
        wavBtn.setToggleState (w, dontSendNotification);
        mp3Btn.setToggleState (! w, dontSendNotification);
        rebuildFiles();
    };
    wavBtn.onClick = [setFormat] { setFormat (true); };
    mp3Btn.onClick = [setFormat] { setFormat (false); };
    wavBtn.setToggleState (proc.wav, dontSendNotification);
    mp3Btn.setToggleState (! proc.wav, dontSendNotification);

    studioBtn.onClick = [this] { if (auto s = currentSong(); s.isObject()) proc.api.openInBrowser ("/song/" + s["id"].toString()); };
    refreshBtn.onClick = [this] { loadSongs(); };
    folderBtn.onClick = [] { auto d = VyrsApi::library(); d.createDirectory(); d.startAsProcess(); };
    signOutBtn.onClick = [this] { proc.disarm(); proc.api.signOut(); showSignedOut(); };
    getAllBtn.onClick = [this] { for (auto* r : rows) if (r->kind() != "mix") r->get(); };

    // a new version of the song, made in VYRS; its full mix is fetched so it's ready to drag
    primary (newTakeBtn);
    notesEdit.setTextToShowWhenEmpty ("Production notes, optional", vyrs::dust);
    notesEdit.setColour (TextEditor::outlineColourId, vyrs::line);
    notesEdit.setColour (TextEditor::focusedOutlineColourId, vyrs::flare);
    notesEdit.setFont (FontOptions (13.0f));
    // the same knobs as the studio console
    for (auto [i, e] : std::initializer_list<std::pair<int, const char*>> { { 1, "Laid-back" }, { 2, "Relaxed" }, { 3, "Steady" }, { 4, "Driving" }, { 5, "Explosive" } })
        energyBox.addItem (String ("Energy: ") + e, i);
    energyBox.setSelectedId (3, dontSendNotification);
    tempoBox.addItem ("Tempo: auto", 1);
    tempoBox.addItem ("Tempo: match my DAW", 2);
    tempoBox.setSelectedId (1, dontSendNotification);
    voiceBox.addItem ("Voice: as it suits", 1);
    int vi = 2;
    for (auto* v : { "Warm", "Raspy", "Breathy", "Powerful", "Soulful", "Bright", "Deep", "Young" })
        voiceBox.addItem (String ("Voice: ") + v, vi++);
    voiceBox.setSelectedId (1, dontSendNotification);

    newTakeBtn.onClick = [this]
    {
        auto s = currentSong();
        if (! s.isObject() || makingTake) return;
        if (s["access"].toString() == "view") return (void) (info = "This song is shared with you to listen only.");
        makingTake = true;
        info = "Recording a new take in VYRS… about a minute.";
        newTakeBtn.setEnabled (false);
        repaint();
        SafePointer<VyrsEditor> self (this);
        auto songId = s["id"].toString();
        StringArray parts;
        if (notesEdit.getText().trim().isNotEmpty()) parts.add (notesEdit.getText().trim());
        parts.add (energyBox.getText().fromFirstOccurrenceOf (": ", false, false).toLowerCase() + " energy");
        auto bpm = proc.host().bpm;
        if (tempoBox.getSelectedId() == 2 && bpm >= 60 && bpm <= 180) parts.add ("exactly " + String (roundToInt (bpm)) + " BPM");
        StringArray voice;
        if (voiceBox.getSelectedId() > 1) voice.add (voiceBox.getText().fromFirstOccurrenceOf (": ", false, false));
        proc.api.newTake (songId, parts.joinIntoString (", "), voice, [self, songId] (VyrsApi::Result r)
        {
            if (self == nullptr) return;
            self->makingTake = false;
            self->newTakeBtn.setEnabled (true);
            if (! r.ok())
            {
                self->info = r.error;
                self->repaint();
                return;
            }
            self->notesEdit.clear();
            self->info = "New take ready: drag it onto a track.";
            self->proc.trackId = r.json["id"].toString();
            self->proc.songId = songId;
            self->loadSongs();
            Timer::callAfterDelay (1500, [self] { if (self != nullptr && ! self->rows.isEmpty()) self->rows[0]->get(); });
        });
    };

    dragAll.setText ("Drag all channels", dontSendNotification);
    dragAll.setJustificationType (Justification::centred);
    dragAll.setColour (Label::backgroundColourId, vyrs::flare);
    dragAll.setColour (Label::textColourId, vyrs::ink);
    dragAll.setFont (FontOptions (14.0f, Font::bold));
    dragAll.setMouseCursor (MouseCursor::DraggingHandCursor);
    dragAll.addMouseListener (this, false);
    dragAll.setTooltip ("Drag every channel at once: each lands on its own track");

    filesView.setViewedComponent (&filesHolder, false);
    filesView.setScrollBarsShown (true, false);

    takeStartEdit.setEditable (true);
    takeStartEdit.setText (String (proc.takeStart, 2), dontSendNotification);
    takeStartEdit.setColour (Label::backgroundColourId, vyrs::coal);
    takeStartEdit.setColour (Label::outlineColourId, vyrs::line);
    takeStartEdit.onTextChange = [this]
    {
        proc.takeStart = jmax (0.0, takeStartEdit.getText().getDoubleValue());
        takeStartEdit.setText (String (proc.takeStart, 2), dontSendNotification);
    };
    takeStartLabel.setColour (Label::textColourId, vyrs::dust);
    takeStartLabel.setFont (FontOptions (12.0f));
    armBtn.onClick = [this]
    {
        auto st = proc.recState();
        if (st == VyrsProcessor::Rec::armed) return proc.disarm();
        auto s = currentSong();
        auto t = currentTake();
        if (! s.isObject() || ! t.isObject()) return;
        if (s["access"].toString() == "view")
            return (void) (info = "This song is shared with you to listen only, so you can't add a vocal.");
        proc.arm (s["id"].toString(), t["id"].toString(), (int) t["number"], proc.takeStart);
    };

    // lyrics: write and score the song without leaving the DAW
    takesTab.onClick = [this] { showPane (Pane::takes); };
    lyricsTab.onClick = [this] { showPane (Pane::lyrics); };
    takesTab.setToggleState (true, dontSendNotification);
    primary (scoreBtn);
    saveBtn.onClick = [this] { saveLyrics (false); };
    scoreBtn.onClick = [this] { runScore(); };
    rewriteBtn.onClick = [this] { runRewrite(); };
    lyricsEdit.onSave = [this] { saveLyrics (false); };
    lyricsEdit.setMultiLine (true, true);
    lyricsEdit.setReturnKeyStartsNewLine (true);
    lyricsEdit.setTabKeyUsedAsCharacter (false);
    lyricsEdit.setScrollbarsShown (true);
    lyricsEdit.setFont (FontOptions (15.0f));
    lyricsEdit.setIndents (14, 12);
    lyricsEdit.setColour (TextEditor::outlineColourId, vyrs::line);
    lyricsEdit.setColour (TextEditor::focusedOutlineColourId, vyrs::flare);
    lyricsEdit.setTextToShowWhenEmpty ("[Verse 1]  then your lines underneath; a new [Chorus] starts the next section", vyrs::dust);
    lyricsEdit.onTextChange = [this] { updateLyricsButtons(); };
    scoreView.setViewedComponent (&scorePanel, false);
    scoreView.setScrollBarsShown (true, false);
    updateLyricsButtons();

    for (auto* c : std::initializer_list<Component*> { &connectBtn, &createBtn, &cancelBtn, &songList, &takeBox, &wavBtn, &mp3Btn, &studioBtn,
                                                       &refreshBtn, &signOutBtn, &folderBtn, &getAllBtn, &dragAll, &filesView, &armBtn,
                                                       &takeStartLabel, &takeStartEdit, &newTakeBtn, &notesEdit, &energyBox, &tempoBox, &voiceBox,
                                                       &takesTab, &lyricsTab, &saveBtn, &scoreBtn, &rewriteBtn, &lyricsEdit, &scoreView })
        addChildComponent (c);

    sourceLink.setFont (FontOptions (11.0f), false, Justification::centredLeft);
    sourceLink.setColour (HyperlinkButton::textColourId, vyrs::dust);
    addAndMakeVisible (sourceLink);

    startTimerHz (10);
    start();
}

VyrsEditor::~VyrsEditor()
{
    flushLyrics(); // closing the window must not lose what was typed
    dragAll.removeMouseListener (this);
    setLookAndFeel (nullptr);
}

void VyrsEditor::start()
{
    if (! proc.api.isSignedIn())
        return showSignedOut();
    view = View::loading;
    resized();
    SafePointer<VyrsEditor> self (this);
    proc.api.me ([self] (VyrsApi::Result r)
    {
        if (self == nullptr) return;
        if (! r.ok())
        {
            if (! self->proc.api.isSignedIn()) return self->showSignedOut ("You were disconnected. Connect again to continue.");
            self->view = View::error;
            self->info = r.error;
            self->resized();
            self->repaint();
            return;
        }
        self->meName = r.json["name"].toString();
        self->meLine = r.json["tracksLeft"].toString() + " of " + r.json["tracksPerMonth"].toString() + " tracks left";
        self->loadSongs();
    });
}

void VyrsEditor::showSignedOut (const String& error)
{
    view = View::signedOut;
    info = error;
    code = {};
    resized();
    repaint();
}

void VyrsEditor::connect (bool newAccount)
{
    SafePointer<VyrsEditor> self (this);
    info = {};
    proc.api.connect (newAccount,
        [self, newAccount] (String c, String e)
        {
            if (self == nullptr) return;
            if (e.isNotEmpty()) return self->showSignedOut (e);
            self->code = c;
            self->info = newAccount ? "Your browser opened VYRS. Make your account and choose a plan; then check that the browser shows this code and click Connect the app."
                                    : "Your browser opened VYRS. Check that it shows this code, then click Connect the app.";
            self->view = View::connecting;
            self->resized();
            self->repaint();
        },
        [self] (String e)
        {
            if (self == nullptr) return;
            if (e.isNotEmpty()) return self->showSignedOut (e);
            self->start();
        });
}

void VyrsEditor::loadSongs()
{
    SafePointer<VyrsEditor> self (this);
    proc.api.songs ([self] (VyrsApi::Result r)
    {
        if (self == nullptr) return;
        if (! r.ok())
        {
            if (! self->proc.api.isSignedIn()) return self->showSignedOut ("You were disconnected. Connect again to continue.");
            self->view = View::error;
            self->info = r.error;
            self->resized();
            self->repaint();
            return;
        }
        self->songs = r.json;
        self->view = View::library;
        self->songList.updateContent();
        // reopen the song and take this project used last time
        auto want = self->proc.songId;
        int found = 0;
        for (int i = 0; i < self->getNumRows(); ++i)
            if (self->songs[i]["id"].toString() == want) found = i;
        self->songList.selectRow (found);
        self->selectSong (self->songs[found]["id"].toString());
        self->resized();
        self->repaint();
    });
}

int VyrsEditor::getNumRows() { return songs.isArray() ? songs.size() : 0; }

void VyrsEditor::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    auto s = songs[row];
    if (selected)
    {
        g.setColour (vyrs::ash);
        g.fillRoundedRectangle (Rectangle<float> (0, 2, (float) w, (float) h - 4).reduced (2, 0), 8.0f);
    }
    g.setColour (vyrs::paper);
    g.setFont (FontOptions (14.0f, Font::bold));
    g.drawText (s["title"].toString(), 12, 6, w - 20, 20, Justification::centredLeft, true);
    g.setColour (vyrs::dust);
    g.setFont (FontOptions (12.0f));
    auto n = s["takes"].size();
    g.drawText (s["genre"].toString() + " · " + String (n) + (n == 1 ? " take" : " takes") + (s["access"].toString() == "owner" ? "" : " · shared"),
                12, 26, w - 20, 18, Justification::centredLeft, true);
}

void VyrsEditor::selectedRowsChanged (int row)
{
    if (row >= 0 && row < getNumRows())
        selectSong (songs[row]["id"].toString());
}

var VyrsEditor::currentSong() const { return songIndex >= 0 ? songs[songIndex] : var(); }
var VyrsEditor::currentTake() const { auto s = currentSong(); return s.isObject() ? s["takes"][takeIndex] : var(); }

void VyrsEditor::selectSong (const String& id)
{
    if (lyricsSong.isNotEmpty() && id != lyricsSong)
    {
        // another song: save the edits of this one in the background and start clean
        flushLyrics();
        lyricsSong = {};
        lyricsSections = lyricsScore = var();
        savedText = scoredText = {};
        lyricsBase = 0;
        lyricsLoading = saving = scoring = rewriting = false;
        lyricsEdit.clear();
        scorePanel.setBusy (false);
        setLyricsInfo ({});
    }
    songIndex = -1;
    for (int i = 0; i < getNumRows(); ++i)
        if (songs[i]["id"].toString() == id) songIndex = i;
    proc.songId = id;
    takeBox.clear (dontSendNotification);
    auto s = currentSong();
    int want = 0;
    for (int i = 0; s.isObject() && i < s["takes"].size(); ++i)
    {
        auto t = s["takes"][i];
        String label = "Take " + t["number"].toString();
        if ((double) t["bpm"] > 0) label << "  ·  " << roundToInt ((double) t["bpm"]) << " BPM";
        if (t["key"].toString().isNotEmpty()) label << "  ·  " << t["key"].toString();
        takeBox.addItem (label, i + 1);
        if (t["id"].toString() == proc.trackId) want = i;
    }
    takeBox.setSelectedItemIndex (want, dontSendNotification);
    selectTake (want);
    if (pane == Pane::lyrics && s.isObject() && lyricsSong != id)
        loadLyrics();
    updateLyricsButtons();
}

void VyrsEditor::selectTake (int index)
{
    takeIndex = jmax (0, index);
    if (auto t = currentTake(); t.isObject())
        proc.trackId = t["id"].toString();
    rebuildFiles();
}

void VyrsEditor::rebuildFiles()
{
    rows.clear();
    auto s = currentSong();
    auto t = currentTake();
    if (t.isObject())
        for (auto& f : *t["files"].getArray())
        {
            auto* r = rows.add (new FileRow (s, t, f, proc, [this] { layoutFiles(); repaint(); }));
            filesHolder.addAndMakeVisible (r);
        }
    layoutFiles();
    repaint();
}

void VyrsEditor::layoutFiles()
{
    auto w = jmax (200, filesView.getWidth() - 10);
    auto cols = jmax (1, w / 220);
    auto cw = (w - (cols - 1) * 8) / cols;
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds ((i % cols) * (cw + 8), (i / cols) * 64, cw, 56);
    filesHolder.setSize (w, ((rows.size() + cols - 1) / cols) * 64);

    int stems = 0, ready = 0;
    for (auto* r : rows)
        if (r->kind() != "mix") { ++stems; if (r->isReady()) ++ready; }
    getAllBtn.setVisible (view == View::library && stems > 1 && ready < stems);
    dragAll.setVisible (view == View::library && stems > 1 && ready == stems);
}

void VyrsEditor::mouseDrag (const MouseEvent& e)
{
    if (e.eventComponent != &dragAll || e.getDistanceFromDragStart() < 4 || e.mods.isPopupMenu())
        return;
    StringArray files;
    for (auto* r : rows)
        if (r->kind() != "mix" && r->isReady()) files.add (r->localFile().getFullPathName());
    if (! files.isEmpty())
        DragAndDropContainer::performExternalDragDropOfFiles (files, false, &dragAll);
}

void VyrsEditor::timerCallback()
{
    using R = VyrsProcessor::Rec;
    auto st = proc.recState();
    armBtn.setButtonText (st == R::armed ? "Armed: press play in your DAW (click to cancel)"
                        : st == R::recording ? "Recording… stop your DAW to finish"
                        : st == R::uploading ? "Sending to VYRS…"
                        : "Record my vocal onto this take");
    armBtn.setEnabled (st != R::recording && st != R::uploading);
    repaint (hostArea);
    repaint (recArea);
    repaint (headerArea);
}

void VyrsEditor::paint (Graphics& g)
{
    g.fillAll (vyrs::ink);

    // header
    g.setColour (vyrs::line);
    g.drawHorizontalLine (headerArea.getBottom(), 0.0f, (float) getWidth());
    auto logo = headerArea.reduced (20, 14).removeFromLeft (24).withSizeKeepingCentre (24, 24).toFloat();
    g.setColour (vyrs::flare);
    g.fillRoundedRectangle (logo, 6.0f);
    g.setColour (vyrs::ink);
    const float bars[] { 0.35f, 0.6f, 0.85f, 0.55f, 0.4f };
    for (int i = 0; i < 5; ++i)
    {
        auto bh = logo.getHeight() * bars[i] * 0.7f;
        g.fillRoundedRectangle (logo.getX() + 4.5f + (float) i * 3.2f, logo.getCentreY() - bh / 2, 2.0f, bh, 1.0f);
    }
    g.setColour (vyrs::paper);
    g.setFont (FontOptions (17.0f, Font::bold));
    g.drawText ("VYRS", (int) logo.getRight() + 10, headerArea.getY(), 60, headerArea.getHeight(), Justification::centredLeft);

    // what the DAW is doing
    auto h = proc.host();
    String hostText = h.bpm > 0 ? "DAW " + String (h.bpm, h.bpm == std::floor (h.bpm) ? 0 : 1) + " BPM" : String ("DAW tempo unknown");
    hostText << (h.playing ? "  ·  playing" : "  ·  stopped");
    if (h.seconds >= 0) hostText << "  ·  " << String (h.seconds, 1) << " s";
    g.setColour (vyrs::dust);
    g.setFont (FontOptions (13.0f));
    g.drawText (hostText, hostArea, Justification::centredLeft);
    if (meName.isNotEmpty() && view == View::library)
        g.drawText (meName + "  ·  " + meLine, headerArea.withTrimmedRight (400).withTrimmedLeft (380), Justification::centredRight, true);

    auto body = getLocalBounds().withTrimmedTop (headerArea.getBottom()).reduced (24);
    if (view == View::loading)
    {
        g.setColour (vyrs::dust);
        g.drawText ("Loading…", body, Justification::centred);
    }
    else if (view == View::signedOut || view == View::connecting || view == View::error)
    {
        auto c = body.withSizeKeepingCentre (jmin (520, body.getWidth()), 360);
        g.setColour (vyrs::paper);
        g.setFont (FontOptions (30.0f, Font::bold));
        g.drawFittedText (view == View::connecting ? "Approve in your browser" : view == View::error ? "Can't reach VYRS" : "Your VYRS songs, inside your DAW",
                          c.removeFromTop (76), Justification::topLeft, 2);
        if (view == View::connecting)
        {
            auto box = c.removeFromTop (64).withWidth (260);
            g.setColour (vyrs::coal);
            g.fillRoundedRectangle (box.toFloat(), 12.0f);
            g.setColour (vyrs::paper);
            g.setFont (FontOptions (Font::getDefaultMonospacedFontName(), 28.0f, Font::bold));
            g.drawText (code, box, Justification::centred);
            c.removeFromTop (12);
        }
        g.setColour (vyrs::dust);
        g.setFont (FontOptions (14.0f));
        auto text = view == View::signedOut ? "Connect the plug-in to your VYRS account. Your browser opens; you approve there. The plug-in never asks for your password." : info;
        if (view == View::signedOut && info.isNotEmpty()) text = info;
        g.setColour (view == View::signedOut && info.isNotEmpty() ? vyrs::ember : vyrs::dust);
        g.drawFittedText (text, c.removeFromTop (60), Justification::topLeft, 4);
    }
    else if (view == View::library)
    {
        // song title and tempo hint
        auto s = currentSong();
        auto t = currentTake();
        g.setColour (vyrs::paper);
        g.setFont (FontOptions (24.0f, Font::bold));
        g.drawText (s["title"].toString(), titleArea, Justification::centredLeft, true);
        if (pane == Pane::lyrics)
        {
            auto bad = lyricsInfoBad;
            auto text = lyricsInfo;
            if (! bad && lyricsDirty() && ! saving && ! scoring && ! rewriting)
               #if JUCE_MAC
                text = String (CharPointer_UTF8 ("Unsaved changes  \xc2\xb7  \xe2\x8c\x98S saves"));
               #else
                text = String (CharPointer_UTF8 ("Unsaved changes  \xc2\xb7  Ctrl+S saves"));
               #endif
            g.setColour (bad ? vyrs::ember : lyricsDirty() ? vyrs::ember : vyrs::dust);
            g.setFont (FontOptions (12.5f));
            g.drawFittedText (text, statusArea, Justification::centredRight, 2);
            return;
        }
        if (t.isObject())
        {
            String hint;
            auto tb = (double) t["bpm"];
            if (tb > 0 && h.bpm > 0 && std::abs (tb - h.bpm) > 0.5)
                hint = "This take is " + String (roundToInt (tb)) + " BPM and your DAW is at " + String (roundToInt (h.bpm)) + ". Set the DAW to " + String (roundToInt (tb)) + " so it lines up.";
            else if (tb > 0)
                hint = "Tempo matches your DAW.";
            else
                hint = "Click a file to get it, then drag it onto a track. Files are saved in Music/VYRS.";
            g.setColour (hint.startsWith ("This take") ? vyrs::ember : vyrs::dust);
            g.setFont (FontOptions (13.0f));
            g.drawText (hint, hintArea, Justification::centredLeft, true);
        }

        // recording panel
        g.setColour (vyrs::coal);
        g.fillRoundedRectangle (recArea.toFloat(), 12.0f);
        g.setColour (vyrs::line);
        g.drawRoundedRectangle (recArea.toFloat().reduced (0.5f), 12.0f, 1.0f);
        auto ra = recArea.reduced (14, 12);
        g.setColour (vyrs::paper);
        g.setFont (FontOptions (14.0f, Font::bold));
        g.drawText ("Record your vocal", ra.removeFromTop (20), Justification::centredLeft);
        auto st = proc.recState();
        if (st == VyrsProcessor::Rec::recording)
        {
            g.setColour (vyrs::rec);
            g.fillEllipse ((float) recArea.getRight() - 30, (float) recArea.getY() + 15, 12, 12);
            g.setColour (vyrs::paper);
            g.drawText (String (proc.recordedSeconds(), 1) + " s", recArea.getRight() - 110, recArea.getY() + 10, 70, 22, Justification::centredRight);
        }
        g.setColour (vyrs::dust);
        g.setFont (FontOptions (12.0f));
        auto msg = proc.recMessage();
        if (msg.isEmpty())
            msg = "Put VYRS on your vocal track with input monitoring on. Arm, press play in your DAW and sing; when you stop the DAW, the vocal goes to this take in time. Uploading costs no tracks.";
        if (info.isNotEmpty()) msg = info;
        g.drawFittedText (msg, recArea.reduced (14, 12).withTrimmedTop (24).withTrimmedBottom (40), Justification::topLeft, 3);
    }
}

void VyrsEditor::resized()
{
    sourceLink.setBounds (getLocalBounds().removeFromBottom (18).reduced (24, 0).withWidth (300));
    headerArea = getLocalBounds().removeFromTop (54);
    auto typing = lyricsEdit.hasKeyboardFocus (true);
    hostArea = headerArea.reduced (20, 0).withTrimmedLeft (110).withWidth (280);
    for (auto* c : getChildren())
        c->setVisible (c == &sourceLink);

    auto body = getLocalBounds().withTrimmedTop (headerArea.getBottom()).withTrimmedBottom (14).reduced (24);

    if (view == View::signedOut || view == View::connecting || view == View::error)
    {
        auto c = body.withSizeKeepingCentre (jmin (520, body.getWidth()), 360);
        auto buttons = c.withTrimmedTop (view == View::connecting ? 230 : 160).removeFromTop (40);
        if (view == View::signedOut)
        {
            connectBtn.setBounds (buttons.removeFromLeft (180));
            buttons.removeFromLeft (10);
            createBtn.setBounds (buttons.removeFromLeft (270));
            connectBtn.setVisible (true);
            createBtn.setVisible (true);
        }
        else if (view == View::connecting)
        {
            cancelBtn.setBounds (buttons.removeFromLeft (110));
            cancelBtn.setVisible (true);
        }
        else
        {
            refreshBtn.setBounds (buttons.removeFromLeft (120));
            refreshBtn.onClick = [this] { refreshBtn.onClick = [this] { loadSongs(); }; start(); };
            refreshBtn.setVisible (true);
        }
        return;
    }

    if (view != View::library)
        return;

    auto top = headerArea.reduced (16, 11);
    signOutBtn.setBounds (top.removeFromRight (110));
    top.removeFromRight (8);
    refreshBtn.setBounds (top.removeFromRight (90));
    top.removeFromRight (8);
    folderBtn.setBounds (top.removeFromRight (150));
    for (auto* c : { &signOutBtn, &refreshBtn, &folderBtn })
        c->setVisible (true);

    songList.setBounds (body.removeFromLeft (252));
    songList.setVisible (true);
    body.removeFromLeft (24);

    auto tabRow = body.removeFromTop (30);
    takesTab.setBounds (tabRow.removeFromLeft (84));
    tabRow.removeFromLeft (6);
    lyricsTab.setBounds (tabRow.removeFromLeft (84));
    studioBtn.setBounds (tabRow.removeFromRight (130));
    for (auto* c : { &takesTab, &lyricsTab, &studioBtn })
        c->setVisible (true);
    body.removeFromTop (10);

    auto titleRow = body.removeFromTop (34);
    if (pane == Pane::lyrics)
    {
        statusArea = titleRow.removeFromRight (jmin (300, titleRow.getWidth() / 2));
        titleArea = titleRow.withTrimmedRight (8);
        body.removeFromTop (8);
        auto tools = body.removeFromTop (32);
        saveBtn.setBounds (tools.removeFromLeft (90));
        tools.removeFromLeft (8);
        scoreBtn.setBounds (tools.removeFromLeft (110));
        tools.removeFromLeft (8);
        rewriteBtn.setBounds (tools.removeFromLeft (150));
        body.removeFromTop (12);
        if (body.getWidth() >= 640)
        {
            scoreView.setBounds (body.removeFromRight (290));
            body.removeFromRight (14);
        }
        else
        {
            scoreView.setBounds (body.removeFromBottom (jmin (240, body.getHeight() / 2)));
            body.removeFromBottom (12);
        }
        lyricsEdit.setBounds (body);
        for (auto* c : std::initializer_list<Component*> { &saveBtn, &scoreBtn, &rewriteBtn, &lyricsEdit, &scoreView })
            c->setVisible (true);
        layoutScore();
        if (typing)
            lyricsEdit.grabKeyboardFocus();
        return;
    }
    mp3Btn.setBounds (titleRow.removeFromRight (56));
    wavBtn.setBounds (titleRow.removeFromRight (56));
    titleArea = titleRow.withTrimmedRight (8);
    body.removeFromTop (10);
    auto takeRow = body.removeFromTop (32);
    takeBox.setBounds (takeRow.removeFromLeft (jmin (320, takeRow.getWidth() / 2)));
    takeRow.removeFromLeft (10);
    getAllBtn.setBounds (takeRow.removeFromLeft (160));
    dragAll.setBounds (getAllBtn.getBounds());
    body.removeFromTop (8);
    auto knobRow = body.removeFromTop (28);
    auto third = (knobRow.getWidth() - 16) / 3;
    energyBox.setBounds (knobRow.removeFromLeft (third));
    knobRow.removeFromLeft (8);
    tempoBox.setBounds (knobRow.removeFromLeft (third));
    knobRow.removeFromLeft (8);
    voiceBox.setBounds (knobRow);
    body.removeFromTop (6);
    auto newRow = body.removeFromTop (32);
    newTakeBtn.setBounds (newRow.removeFromRight (140));
    newRow.removeFromRight (8);
    notesEdit.setBounds (newRow);
    hintArea = body.removeFromTop (24);

    recArea = body.removeFromBottom (150);
    body.removeFromBottom (12);
    filesView.setBounds (body);

    auto ra = recArea.reduced (14, 12);
    auto controls = ra.removeFromBottom (34);
    armBtn.setBounds (controls.removeFromLeft (jmin (380, controls.getWidth() - 300)));
    controls.removeFromLeft (12);
    takeStartLabel.setBounds (controls.removeFromLeft (220));
    takeStartEdit.setBounds (controls.removeFromLeft (70));

    for (auto* c : std::initializer_list<Component*> { &studioBtn, &mp3Btn, &wavBtn, &takeBox, &filesView, &armBtn, &takeStartLabel, &takeStartEdit, &newTakeBtn, &notesEdit, &energyBox, &tempoBox, &voiceBox })
        c->setVisible (true);
    layoutFiles();
}

// ---------------------------------------------------------------- lyrics

bool VyrsEditor::keyPressed (const KeyPress& key)
{
    // when the lyrics text has focus it handles this itself (LyricsEditor)
    if (key == KeyPress ('s', ModifierKeys::commandModifier, 0) && view == View::library && pane == Pane::lyrics)
    {
        saveLyrics (false);
        return true;
    }
    return false;
}

void VyrsEditor::showPane (Pane p)
{
    pane = p;
    takesTab.setToggleState (p == Pane::takes, dontSendNotification);
    lyricsTab.setToggleState (p == Pane::lyrics, dontSendNotification);
    if (auto s = currentSong(); p == Pane::lyrics && s.isObject() && lyricsSong != s["id"].toString())
        loadLyrics();
    resized();
    repaint();
    if (p == Pane::lyrics && lyricsEdit.isShowing() && ! lyricsReadOnly())
        lyricsEdit.grabKeyboardFocus();
}

String VyrsEditor::sectionsToText (const var& sections)
{
    StringArray parts;
    if (auto* a = sections.getArray())
        for (auto& s : *a)
            parts.add ("[" + s["name"].toString() + "]\n" + s["text"].toString());
    return parts.joinIntoString ("\n\n");
}

var VyrsEditor::textToSections (const String& text, const var& prev, String& error)
{
    struct Sec { String name, text; };
    std::vector<Sec> out;
    for (auto& raw : StringArray::fromLines (text))
    {
        auto t = raw.trim();
        if (t.length() > 2 && t.startsWithChar ('[') && t.endsWithChar (']'))
        {
            auto name = t.substring (1, t.length() - 1).trim();
            out.push_back ({ name.isEmpty() ? String ("Section") : name, {} });
            continue;
        }
        if (out.empty())
        {
            if (t.isEmpty()) continue;
            out.push_back ({ "Verse", {} }); // lines before the first heading
        }
        auto& s = out.back();
        s.text << (s.text.isEmpty() ? "" : "\n") << raw.trimEnd();
    }

    // keep each section's id: same name first (it may have moved), else same place
    const int n = prev.isArray() ? prev.size() : 0;
    std::vector<bool> used ((size_t) n, false);
    Array<var> result;
    for (size_t i = 0; i < out.size(); ++i)
    {
        auto& s = out[i];
        while (s.text.endsWithChar ('\n')) s.text = s.text.dropLastCharacters (1);
        String id;
        for (int j = 0; j < n && id.isEmpty(); ++j)
            if (! used[(size_t) j] && prev[j]["name"].toString() == s.name) { id = prev[j]["id"].toString(); used[(size_t) j] = true; }
        if (id.isEmpty() && (int) i < n && ! used[i]) { id = prev[(int) i]["id"].toString(); used[i] = true; }
        if (id.isEmpty()) id = Uuid().toDashedString();
        DynamicObject::Ptr o = new DynamicObject();
        o->setProperty ("id", id);
        o->setProperty ("name", s.name);
        o->setProperty ("text", s.text);
        result.add (var (o.get()));

        // the same limits as the web studio
        if (error.isEmpty() && s.name.length() > 60) error = "Section names can be 60 characters at most.";
        if (error.isEmpty() && s.text.length() > 4000) error = "[" + s.name + "] is too long: 4000 characters at most.";
    }
    if (out.size() > 30) error = "A song can have 30 sections at most.";
    return result;
}

bool VyrsEditor::lyricsDirty() const
{
    return lyricsSong.isNotEmpty() && ! lyricsLoading && ! lyricsReadOnly() && lyricsEdit.getText() != savedText;
}

void VyrsEditor::setLyricsInfo (const String& text, bool bad)
{
    lyricsInfo = text;
    lyricsInfoBad = bad;
    repaint (statusArea);
}

void VyrsEditor::updateLyricsButtons()
{
    auto ready = lyricsSong.isNotEmpty() && ! lyricsLoading && ! lyricsReadOnly();
    auto dirty = lyricsDirty();
    saveBtn.setEnabled (ready && ! saving && dirty);
    saveBtn.setButtonText (saving ? "Saving…" : "Save");
    scoreBtn.setEnabled (ready && ! scoring);
    scoreBtn.setButtonText (scoring ? "Scoring…" : lyricsScore.isObject() ? "Score again" : "Score");
    rewriteBtn.setEnabled (ready && ! rewriting);
    rewriteBtn.setButtonText (rewriting ? "Rewriting…" : "Rewrite section");
    lyricsTab.setButtonText (dirty ? String (CharPointer_UTF8 ("Lyrics \xe2\x80\xa2")) : String ("Lyrics"));
    auto stale = lyricsScore.isObject() && scoredText.isNotEmpty() && lyricsEdit.getText() != scoredText;
    scorePanel.setScore (lyricsScore, stale);
    layoutScore();
    repaint (statusArea);
}

void VyrsEditor::layoutScore()
{
    auto w = jmax (160, scoreView.getWidth() - scoreView.getScrollBarThickness() - 4);
    scorePanel.setSize (w, jmax (scoreView.getHeight(), scorePanel.heightFor (w)));
}

void VyrsEditor::applyLyrics (const var& json)
{
    lyricsSections = json["sections"];
    lyricsBase = (int64) json["updatedAt"];
    lyricsAccess = json["access"].toString();
    savedText = sectionsToText (lyricsSections);
    lyricsEdit.setReadOnly (lyricsReadOnly());
    lyricsEdit.setText (savedText, false);
    lyricsScore = json["score"].isObject() ? json["score"] : var();
    scoredText = lyricsScore.isObject() ? savedText : String();
    setLyricsInfo (lyricsReadOnly() ? "Shared with you to read only." : String());
    updateLyricsButtons();
}

void VyrsEditor::loadLyrics()
{
    auto s = currentSong();
    if (! s.isObject())
        return;
    auto id = s["id"].toString();
    lyricsSong = id;
    lyricsLoading = true;
    lyricsEdit.clear();
    lyricsEdit.setReadOnly (true);
    setLyricsInfo ("Loading lyrics…");
    updateLyricsButtons();
    SafePointer<VyrsEditor> self (this);
    proc.api.lyrics (id, [self, id] (VyrsApi::Result r)
    {
        if (self == nullptr || self->lyricsSong != id) return;
        self->lyricsLoading = false;
        if (! r.ok())
        {
            if (! self->proc.api.isSignedIn()) return self->showSignedOut ("You were disconnected. Connect again to continue.");
            self->lyricsSong = {}; // try again next time
            self->setLyricsInfo (r.error, true);
            self->updateLyricsButtons();
            return;
        }
        self->applyLyrics (r.json);
        // edits that couldn't be saved before (window closed, offline) come back
        auto draft = self->proc.lyricsDrafts.getValue (id, {});
        if (draft.isNotEmpty() && draft != self->savedText && ! self->lyricsReadOnly())
        {
            self->lyricsEdit.setText (draft, false);
            self->setLyricsInfo ("Your unsaved edits are back. Save to keep them.");
            self->updateLyricsButtons();
        }
    });
}

void VyrsEditor::saveLyrics (bool overwrite, std::function<void()> then)
{
    if (lyricsSong.isEmpty() || lyricsLoading || lyricsReadOnly() || saving)
        return;
    if (! lyricsDirty())
    {
        if (then) then();
        return;
    }
    String error;
    auto text = lyricsEdit.getText();
    auto sections = textToSections (text, lyricsSections, error);
    if (error.isNotEmpty())
        return setLyricsInfo (error, true);

    auto id = lyricsSong;
    proc.lyricsDrafts.set (id, text);
    saving = true;
    setLyricsInfo ("Saving…");
    updateLyricsButtons();
    SafePointer<VyrsEditor> self (this);
    auto* p = &proc;
    proc.api.saveLyrics (id, sections, overwrite ? 0 : lyricsBase, [self, p, id, text, then] (VyrsApi::Result r)
    {
        if (r.ok() && p->lyricsDrafts.getValue (id, {}) == text)
            p->lyricsDrafts.remove (StringRef (id));
        if (self == nullptr || self->lyricsSong != id) return;
        self->saving = false;
        if (r.status == 409)
        {
            // changed in the browser since it was opened here: the artist decides
            self->setLyricsInfo ("Changed in VYRS since you opened it.", true);
            self->updateLyricsButtons();
            auto server = r.json;
            AlertWindow::showAsync (MessageBoxOptions::makeOptionsOkCancel (MessageBoxIconType::WarningIcon, "These lyrics changed in VYRS",
                                        "They were edited somewhere else (maybe in your browser) after you opened them here. Keep your version here, or load the one in VYRS?",
                                        "Keep mine", "Load from VYRS", self.getComponent()),
                                    [self, id, server] (int keep)
                                    {
                                        if (self == nullptr || self->lyricsSong != id) return;
                                        if (keep == 1) return self->saveLyrics (true);
                                        self->proc.lyricsDrafts.remove (StringRef (id));
                                        self->applyLyrics (server);
                                    });
            return;
        }
        if (! r.ok())
        {
            if (! self->proc.api.isSignedIn()) return self->showSignedOut ("You were disconnected. Connect again to continue.");
            self->setLyricsInfo (r.error, true);
            self->updateLyricsButtons();
            return;
        }
        self->lyricsSections = r.json["sections"];
        self->lyricsBase = (int64) r.json["updatedAt"];
        self->savedText = text;
        self->setLyricsInfo ("Saved in VYRS.");
        self->updateLyricsButtons();
        if (then) then();
    });
}

void VyrsEditor::flushLyrics()
{
    if (! lyricsDirty())
        return;
    String error;
    auto text = lyricsEdit.getText();
    auto sections = textToSections (text, lyricsSections, error);
    auto id = lyricsSong;
    proc.lyricsDrafts.set (id, text); // kept until VYRS has it
    if (error.isNotEmpty() || saving)
        return;
    auto* p = &proc;
    proc.api.saveLyrics (id, sections, lyricsBase, [p, id, text] (VyrsApi::Result r)
    {
        if (r.ok() && p->lyricsDrafts.getValue (id, {}) == text)
            p->lyricsDrafts.remove (StringRef (id));
    });
}

void VyrsEditor::runScore()
{
    if (scoring || lyricsSong.isEmpty() || lyricsLoading || lyricsReadOnly())
        return;
    if (lyricsEdit.getText().trim().isEmpty())
        return setLyricsInfo ("Write some lyrics first.", true);
    if (lyricsDirty())
        return saveLyrics (false, [this] { runScore(); }); // the score is of what's saved in VYRS

    scoring = true;
    scorePanel.setBusy (true);
    setLyricsInfo ("Scoring… this takes about half a minute.");
    updateLyricsButtons();
    auto id = lyricsSong;
    auto text = lyricsEdit.getText();
    SafePointer<VyrsEditor> self (this);
    proc.api.score (id, [self, id, text] (VyrsApi::Result r)
    {
        if (self == nullptr || self->lyricsSong != id) return;
        self->scoring = false;
        self->scorePanel.setBusy (false);
        if (! r.ok())
        {
            if (! self->proc.api.isSignedIn()) return self->showSignedOut ("You were disconnected. Connect again to continue.");
            self->setLyricsInfo (r.error, true);
            self->updateLyricsButtons();
            return;
        }
        self->lyricsScore = r.json;
        self->scoredText = text;
        self->setLyricsInfo ("Hit score " + r.json["hitPotential"].toString() + ": " + r.json["band"]["label"].toString());
        self->updateLyricsButtons();
        self->scoreView.setViewPosition (0, 0);
    });
}

void VyrsEditor::runRewrite()
{
    if (rewriting || lyricsSong.isEmpty() || lyricsLoading || lyricsReadOnly())
        return;
    if (lyricsDirty())
        return saveLyrics (false, [this] { runRewrite(); }); // the server rewrites a saved section
    if (! lyricsSections.isArray() || lyricsSections.size() == 0)
        return setLyricsInfo ("Write a section first: a heading like [Verse 1] with lines under it.", true);

    // the section the cursor is in
    String ignore;
    auto before = textToSections (lyricsEdit.getText().substring (0, lyricsEdit.getCaretPosition()), {}, ignore);
    auto index = jlimit (0, lyricsSections.size() - 1, before.size() - 1);
    auto target = lyricsSections[index];
    auto id = lyricsSong;
    auto sectionId = target["id"].toString();
    auto name = target["name"].toString();

    rewriting = true;
    setLyricsInfo ("Rewriting [" + name + "]…");
    updateLyricsButtons();
    SafePointer<VyrsEditor> self (this);
    proc.api.rewrite (id, sectionId, {}, [self, id, sectionId, name] (VyrsApi::Result r)
    {
        if (self == nullptr || self->lyricsSong != id) return;
        self->rewriting = false;
        self->updateLyricsButtons();
        auto options = r.json["options"];
        if (! r.ok() || ! options.isArray() || options.size() == 0)
        {
            if (! self->proc.api.isSignedIn()) return self->showSignedOut ("You were disconnected. Connect again to continue.");
            return self->setLyricsInfo (r.ok() ? String ("No rewrite came back. Try again.") : r.error, true);
        }
        self->setLyricsInfo ("Pick one of the new takes on [" + name + "].");

        // every line of an option picks that option
        PopupMenu m;
        m.addSectionHeader ("New takes on [" + name + "]: click one to use it");
        for (int i = 0; i < options.size(); ++i)
        {
            m.addSeparator();
            m.addSectionHeader (options[i]["label"].toString());
            for (auto& line : StringArray::fromLines (options[i]["text"].toString()))
                if (line.trim().isNotEmpty())
                    m.addItem (i + 1, line.trim());
        }
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&self->rewriteBtn), [self, id, sectionId, options] (int pick)
        {
            if (self == nullptr || self->lyricsSong != id) return;
            if (pick <= 0) return self->setLyricsInfo ({});
            String error;
            auto sections = textToSections (self->lyricsEdit.getText(), self->lyricsSections, error);
            bool found = false;
            for (auto& s : *sections.getArray())
                if (s["id"].toString() == sectionId)
                {
                    s.getDynamicObject()->setProperty ("text", options[pick - 1]["text"].toString().trim());
                    found = true;
                }
            if (! found)
                return self->setLyricsInfo ("That section isn't in the lyrics any more.", true);
            self->lyricsEdit.setText (sectionsToText (sections), false);
            self->setLyricsInfo ("New take in place. Save to keep it.");
            self->updateLyricsButtons();
        });
    });
}
