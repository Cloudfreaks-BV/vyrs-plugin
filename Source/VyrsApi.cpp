/*
    VYRS plug-in. Copyright (C) 2026 CloudFreaks B.V.

    This program is free software: you can redistribute it and/or modify it under the terms of the
    GNU Affero General Public License as published by the Free Software Foundation, version 3.
    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
    the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file.
*/

#include "VyrsApi.h"
#include <juce_cryptography/juce_cryptography.h>

#if JUCE_MAC || JUCE_LINUX
 #include <unistd.h>
 #include <sys/stat.h>
#endif

using namespace juce;

static const char* defaultServer = "https://vyrs.cloudfreaks.nl";

File VyrsApi::appDir()
{
    auto dir = File::getSpecialLocation (File::userApplicationDataDirectory)
                  #if JUCE_MAC
                   .getChildFile ("Application Support")
                  #endif
                   .getChildFile ("VYRS");
    dir.createDirectory();
    return dir;
}

File VyrsApi::library()
{
    return File::getSpecialLocation (File::userMusicDirectory).getChildFile ("VYRS");
}

/** Same as the desktop app: sha256 of the lower-case host name without ".local", so both count as one computer. */
String VyrsApi::machineId()
{
    String host;
   #if JUCE_MAC || JUCE_LINUX
    char buf[256] = {};
    if (gethostname (buf, sizeof (buf) - 1) == 0)
        host = String (buf);
   #else
    host = SystemStats::getComputerName();
   #endif
    host = host.toLowerCase();
    if (host.endsWith (".local"))
        host = host.dropLastCharacters (6);
    return SHA256 (host.toUTF8()).toHexString().substring (0, 32);
}

VyrsApi::VyrsApi()
{
    // a file can point the plug-in at another server (development)
    auto custom = appDir().getChildFile ("server.txt");
    server = custom.existsAsFile() ? custom.loadFileAsString().trim() : String (defaultServer);
    while (server.endsWith ("/"))
        server = server.dropLastCharacters (1);

    auto tokenFile = appDir().getChildFile ("plugin.token");
    if (tokenFile.existsAsFile())
        token = tokenFile.loadFileAsString().trim();
}

VyrsApi::~VyrsApi()
{
    *alive = false;
    polling = false;
    pool.removeAllJobs (true, 20000);
}

String VyrsApi::getToken() const
{
    const ScopedLock sl (lock);
    return token;
}

void VyrsApi::setToken (const String& t)
{
    {
        const ScopedLock sl (lock);
        token = t;
    }
    auto f = appDir().getChildFile ("plugin.token");
    if (t.isEmpty())
    {
        f.deleteFile();
        return;
    }
    f.replaceWithText (t);
   #if JUCE_MAC || JUCE_LINUX
    f.setExecutePermission (false);
    ::chmod (f.getFullPathName().toRawUTF8(), 0600); // only this user can read the token
   #endif
}

bool VyrsApi::isSignedIn() const { return getToken().isNotEmpty(); }

void VyrsApi::signOut()
{
    polling = false;
    setToken ({});
}

void VyrsApi::run (std::function<void()> job)
{
    pool.addJob (std::move (job));
}

VyrsApi::Result VyrsApi::request (const String& path, const String& method, const String& jsonBody, int timeoutMs)
{
    Result r;
    URL url (server + path);
    String headers;
    if (auto t = getToken(); t.isNotEmpty())
        headers << "Authorization: Bearer " << t << "\r\n";
    if (jsonBody.isNotEmpty())
    {
        headers << "Content-Type: application/json\r\n";
        url = url.withPOSTData (jsonBody);
    }

    auto opts = URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                    .withExtraHeaders (headers)
                    .withConnectionTimeoutMs (timeoutMs)
                    .withStatusCode (&r.status)
                    .withHttpRequestCmd (method);

    if (auto in = url.createInputStream (opts))
    {
        auto text = in->readEntireStreamAsString();
        r.json = JSON::parse (text);
        if (r.status == 401 && isSignedIn())
            setToken ({});
        if (r.status >= 400)
            r.error = r.json.getProperty ("error", "VYRS answered " + String (r.status)).toString();
    }
    else
    {
        r.error = "Can't reach VYRS. Check your internet connection.";
    }
    return r;
}

void VyrsApi::connect (bool newAccount, std::function<void (String, String)> started, std::function<void (String)> finished)
{
    polling = true;
    auto flag = alive;
    run ([this, flag, newAccount, started, finished]
    {
        DynamicObject::Ptr body = new DynamicObject();
        body->setProperty ("name", "VYRS plug-in on " + SystemStats::getComputerName().upToFirstOccurrenceOf (".local", false, true));
        body->setProperty ("machine", machineId());
        body->setProperty ("kind", "plugin"); // the plug-in needs the Ultimate plan
        auto r = request ("/api/desktop/link", "POST", JSON::toString (var (body.get()), true));
        if (! r.ok())
        {
            MessageManager::callAsync ([flag, started, e = r.error] { if (*flag) started ({}, e); });
            return;
        }
        auto code = r.json["code"].toString();
        auto poll = r.json["poll"].toString();
        auto connectPath = r.json["path"].toString();
        auto expires = Time::getMillisecondCounterHiRes() + 1000.0 * (double) (int) r.json["expiresIn"];

        auto target = newAccount ? "/register?next=" + URL::addEscapeChars (connectPath, true) : connectPath;
        MessageManager::callAsync ([this, flag, started, code, target]
        {
            if (! *flag) return;
            started (code, {});
            URL (server + target).launchInDefaultBrowser();
        });

        DynamicObject::Ptr pb = new DynamicObject();
        pb->setProperty ("poll", poll);
        auto pollBody = JSON::toString (var (pb.get()), true);
        while (polling && *flag && Time::getMillisecondCounterHiRes() < expires)
        {
            Thread::sleep (2500);
            if (! polling || ! *flag) return;
            auto p = request ("/api/desktop/poll", "POST", pollBody);
            if (p.status == 200 && p.json["token"].toString().isNotEmpty())
            {
                setToken (p.json["token"].toString());
                polling = false;
                MessageManager::callAsync ([flag, finished] { if (*flag) finished ({}); });
                return;
            }
            if (p.status != 202)
            {
                polling = false;
                MessageManager::callAsync ([flag, finished, e = p.error] { if (*flag) finished (e.isNotEmpty() ? e : "The code expired. Try again."); });
                return;
            }
        }
        if (polling && *flag)
        {
            polling = false;
            MessageManager::callAsync ([flag, finished] { if (*flag) finished ("The code expired. Try again."); });
        }
    });
}

void VyrsApi::cancelConnect() { polling = false; }

void VyrsApi::me (std::function<void (Result)> done)
{
    auto flag = alive;
    run ([this, flag, done] { auto r = request ("/api/desktop/me", "GET"); MessageManager::callAsync ([flag, done, r] { if (*flag) done (r); }); });
}

void VyrsApi::songs (std::function<void (Result)> done)
{
    auto flag = alive;
    run ([this, flag, done] { auto r = request ("/api/desktop/songs", "GET"); MessageManager::callAsync ([flag, done, r] { if (*flag) done (r); }); });
}

static String cleanName (const String& s)
{
    auto out = s.removeCharacters ("\\/:*?\"<>|").replaceCharacters ("\r\n\t", "   ").trim();
    while (out.contains ("  "))
        out = out.replace ("  ", " ");
    return out.isEmpty() ? String ("Untitled") : out.substring (0, 80);
}

String VyrsApi::fileNameFor (const var& song, const var& take, const var& file, bool wav)
{
    StringArray parts { cleanName (song["title"].toString()), "Take " + take["number"].toString(), cleanName (file["name"].toString()) };
    if ((double) take["bpm"] > 0)
        parts.add (String (roundToInt ((double) take["bpm"])) + " BPM");
    if (take["key"].toString().isNotEmpty())
        parts.add (cleanName (take["key"].toString()));
    return parts.joinIntoString (" - ") + (wav ? ".wav" : ".mp3");
}

File VyrsApi::targetFor (const var& song, const var& take, const var& file, bool wav)
{
    return library().getChildFile (cleanName (song["title"].toString())).getChildFile (fileNameFor (song, take, file, wav));
}

void VyrsApi::prepare (const var& song, const var& take, const var& file, bool wav, std::function<void (File, String)> done)
{
    auto target = targetFor (song, take, file, wav);
    if (target.existsAsFile())
        return done (target, {});

    auto flag = alive;
    auto songId = song["id"].toString();
    auto name = file["file"].toString();
    run ([this, flag, done, target, songId, name, wav]
    {
        target.getParentDirectory().createDirectory();
        auto part = target.withFileExtension (target.getFileExtension().substring (1) + ".part");
        int status = 0;
        URL url (server + "/api/desktop/file");
        url = url.withParameter ("song", songId).withParameter ("file", name).withParameter ("format", wav ? "wav" : "mp3");
        auto opts = URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                        .withExtraHeaders ("Authorization: Bearer " + getToken() + "\r\n")
                        .withConnectionTimeoutMs (60000)
                        .withStatusCode (&status);
        String error;
        if (auto in = url.createInputStream (opts))
        {
            if (status == 200)
            {
                part.deleteFile();
                {
                    FileOutputStream out (part);
                    if (out.openedOk())
                        out.writeFromInputStream (*in, -1);
                    else
                        error = "Couldn't save the file in Music/VYRS.";
                }
                if (error.isEmpty() && ! part.moveFileTo (target))
                    error = "Couldn't save the file in Music/VYRS.";
            }
            else
            {
                error = JSON::parse (in->readEntireStreamAsString()).getProperty ("error", "Download failed.").toString();
            }
        }
        else
        {
            error = "Can't reach VYRS.";
        }
        MessageManager::callAsync ([flag, done, target, error] { if (*flag) done (error.isEmpty() ? target : File(), error); });
    });
}

void VyrsApi::uploadVocal (const String& songId, const String& trackId, const File& wav, int offsetMs, std::function<void (Result)> done)
{
    auto flag = alive;
    run ([this, flag, done, songId, trackId, wav, offsetMs]
    {
        Result r;
        URL url = URL (server + "/api/desktop/vocal")
                      .withParameter ("song", songId)
                      .withParameter ("track", trackId)
                      .withParameter ("rights", "yes")
                      .withParameter ("offsetMs", String (offsetMs))
                      .withFileToUpload ("file", wav, "audio/wav");
        auto opts = URL::InputStreamOptions (URL::ParameterHandling::inPostData)
                        .withExtraHeaders ("Authorization: Bearer " + getToken() + "\r\n")
                        .withConnectionTimeoutMs (120000)
                        .withStatusCode (&r.status);
        if (auto in = url.createInputStream (opts))
        {
            r.json = JSON::parse (in->readEntireStreamAsString());
            if (r.status >= 400)
                r.error = r.json.getProperty ("error", "Upload failed.").toString();
        }
        else
        {
            r.error = "Can't reach VYRS.";
        }
        MessageManager::callAsync ([flag, done, r] { if (*flag) done (r); });
    });
}

void VyrsApi::newTake (const String& songId, const String& style, const StringArray& voice, std::function<void (Result)> done)
{
    auto flag = alive;
    run ([this, flag, songId, style, voice, done]
    {
        DynamicObject::Ptr b = new DynamicObject();
        b->setProperty ("songId", songId);
        b->setProperty ("style", style.substring (0, 400));
        Array<var> v;
        for (auto& x : voice) v.add (x);
        b->setProperty ("voiceStyle", v);
        // making a take takes a minute or two
        auto r = request ("/api/audio", "POST", JSON::toString (var (b.get()), true), 300000);
        MessageManager::callAsync ([flag, done, r] { if (*flag) done (r); });
    });
}

void VyrsApi::openInBrowser (const String& path) const
{
    URL (server + path).launchInDefaultBrowser();
}
