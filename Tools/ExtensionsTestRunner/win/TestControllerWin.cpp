/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Naver Corp. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestController.h"

#define __STDC__ 1

#include "PlatformWebView.h"
#include <WebCore/NotImplemented.h>
#include <WebKit/WKPageGroup.h>
#include <WebKit/WKString.h>
#include <wtf/text/WTFString.h>
#include <wtf/MainThread.h>

#include <fcntl.h>
#include <io.h>
#include <string>

namespace TestRunner {

using namespace std;

static HANDLE webProcessCrashingEvent;
static const char webProcessCrashingEventName[] = "WebKitTestRunner.WebProcessCrashing";
// This is the longest we'll wait (in seconds) for the web process to finish crashing and a crash
// log to be saved. This interval should be just a tiny bit longer than it will ever reasonably
// take to save a crash log.
static const double maximumWaitForWebProcessToCrash = 60;

const LPWSTR injectedBundleDLL = L"ExtensionsInjectedBundle.dll";

static LONG WINAPI exceptionFilter(EXCEPTION_POINTERS*)
{
    fputs("#CRASHED\n", stderr);
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

void TestController::notifyDone()
{
    WTF::initializeUIThread();

    ::SetErrorMode(SEM_NOGPFAULTERRORBOX);
    // Check environment variable "WTR_DEBUG" is defined to raise crash dialog for debug.
    //  - defines env : set WTR_DEBUG=1 
    //  - undefines env : set WTR_DEBUG=
    if (::GetEnvironmentVariableW(L"WTR_DEBUG", 0, 0)) {
        ::SetErrorMode(SetErrorMode(0)|SEM_NOGPFAULTERRORBOX);
        ::_set_abort_behavior(0,_WRITE_ABORT_MSG);
    }

    ::SetUnhandledExceptionFilter(exceptionFilter);

    _setmode(1, _O_BINARY);
    _setmode(2, _O_BINARY);

    webProcessCrashingEvent = ::CreateEventA(0, FALSE, FALSE, webProcessCrashingEventName);
}

void TestController::platformInitialize()
{
    notImplemented();
}

WKPreferencesRef TestController::platformPreferences()
{
    return WKPageGroupGetPreferences(m_pageGroup.get());
}

void TestController::platformDestroy()
{
    notImplemented();
}

WKContextRef TestController::platformAdjustContext(WKContextRef context, WKContextConfigurationRef)
{
    return context;
}

void TestController::platformInitializeContext()
{
    notImplemented();
}

void TestController::platformCreateWebView(WKPageConfigurationRef configuration)
{
    m_mainWebView = std::make_unique<PlatformWebView>(configuration);
}

enum RunLoopResult { TimedOut, ObjectSignaled, ConditionSatisfied };

static RunLoopResult runRunLoopUntil(bool& condition, HANDLE object, double timeout)
{
    DWORD end = (timeout >= 0.0) ? ::GetTickCount() + timeout * 1000 : 0x7FFFFFFF;
    while (!condition) {
        DWORD now = ::GetTickCount();
        if (now > end)
            return TimedOut;

        DWORD objectCount = object ? 1 : 0;
        const HANDLE* objects = object ? &object : 0;
        DWORD result = ::MsgWaitForMultipleObjectsEx(objectCount, objects, end - now, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (result == WAIT_TIMEOUT)
            return TimedOut;

        if (objectCount && result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + objectCount)
            return ObjectSignaled;

        ASSERT(result == WAIT_OBJECT_0 + objectCount);
        // There are messages in the queue. Process them.
        
        MSG msg;
        while (::PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
        
    }

    return ConditionSatisfied;
}

void TestController::platformRunUntil(bool& condition, double timeout)
{
    // FIXME: No timeout should occur if timeout is equal to m_noTimeout (necessary when running performance tests).
    RunLoopResult result = runRunLoopUntil(condition, webProcessCrashingEvent, timeout);
    if (result == TimedOut || result == ConditionSatisfied)
        return;
    ASSERT(result == ObjectSignaled);

    // The web process is crashing. A crash log might be being saved, which can take a long
    // time, and we don't want to time out while that happens.

    // First, let the test harness know this happened so it won't think we've hung. But
    // make sure we don't exit just yet!
    m_shouldExitWhenWebProcessCrashes = false;
    processDidCrash();
    m_shouldExitWhenWebProcessCrashes = true;

    // Then spin a run loop until it finishes crashing to give time for a crash log to be saved. If
    // it takes too long for a crash log to be saved, we'll just give up.
    bool neverSetCondition = false;
    result = runRunLoopUntil(neverSetCondition, 0, maximumWaitForWebProcessToCrash);
    ASSERT_UNUSED(result, result == TimedOut);
    exit(1);
}

static const String& exePath()
{
    static String path;
    static bool initialized;

    if (initialized)
        return path;
    initialized = true;

    TCHAR buffer[MAX_PATH];
    GetModuleFileName(GetModuleHandle(0), buffer, ARRAYSIZE(buffer));
    path = buffer;
    int lastSlash = path.reverseFind('\\');
    if (lastSlash != -1 && lastSlash + 1 < path.length())
        path = path.substring(0, lastSlash + 1);

    return path;
}

void TestController::initializeInjectedBundlePath()
{
    m_injectedBundlePath = adoptWK(WKStringCreateWithUTF8CString(String(exePath() + String(injectedBundleDLL)).utf8().data()));
}

} // namespace TestRunner
