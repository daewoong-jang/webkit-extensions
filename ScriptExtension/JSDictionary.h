/*
 * Copyright (C) 2016 Naver Corp. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JSDictionary_h
#define JSDictionary_h

#include "Dictionary.h"
#include "JSValue.h"

namespace Extensions {

class JSDictionary final : public JSValue {
public:
    static Ref<JSDictionary> create(JSContextRef, JSValueRef);
    ~JSDictionary() = default;

private:
    JSDictionary(JSContextRef, JSValueRef);
};

inline Ref<JSDictionary> JSDictionary::create(JSContextRef context, JSValueRef value)
{
    return adoptRef(*new JSDictionary(context, value));
}

inline JSDictionary::JSDictionary(JSContextRef context, JSValueRef value)
    : JSValue(context, value)
{
}

inline RefPtr<API::Dictionary> toAPI(RefPtr<JSDictionary> value)
{
    return value->toAPIDictionary();
}

}

#endif // JSDictionary_h
