/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/ProcessQualified.h>
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
struct ScrollingNodeIDType;
using ScrollingNodeID = ProcessQualified<ObjectIdentifier<ScrollingNodeIDType>>;
}

namespace WebKit {

enum class RemoteScrollingUIStateChanges : uint8_t {
    ScrollSnapNodes     = 1 << 0,
    UserScrollNodes     = 1 << 1,
    RubberbandingNodes  = 1 << 2,
};

class RemoteScrollingUIState {
    WTF_MAKE_TZONE_ALLOCATED(RemoteScrollingUIState);
public:
    using Changes = RemoteScrollingUIStateChanges;

    RemoteScrollingUIState() = default;
    RemoteScrollingUIState(OptionSet<RemoteScrollingUIStateChanges>, HashSet<WebCore::ScrollingNodeID>&& nodesWithActiveScrollSnap, HashSet<WebCore::ScrollingNodeID>&& nodesWithActiveUserScrolls, HashSet<WebCore::ScrollingNodeID>&& nodesWithActiveRubberband);

    OptionSet<RemoteScrollingUIStateChanges> changes() const { return m_changes; }
    void clearChanges() { m_changes = { }; }
    
    void reset();

    const HashSet<WebCore::ScrollingNodeID>& nodesWithActiveScrollSnap() const { return m_nodesWithActiveScrollSnap; }
    void addNodeWithActiveScrollSnap(WebCore::ScrollingNodeID);
    void removeNodeWithActiveScrollSnap(WebCore::ScrollingNodeID);
    
    const HashSet<WebCore::ScrollingNodeID>& nodesWithActiveUserScrolls() const { return m_nodesWithActiveUserScrolls; }
    void addNodeWithActiveUserScroll(WebCore::ScrollingNodeID);
    void removeNodeWithActiveUserScroll(WebCore::ScrollingNodeID);
    void clearNodesWithActiveUserScroll();

    const HashSet<WebCore::ScrollingNodeID>& nodesWithActiveRubberband() const { return m_nodesWithActiveRubberband; }
    void addNodeWithActiveRubberband(WebCore::ScrollingNodeID);
    void removeNodeWithActiveRubberband(WebCore::ScrollingNodeID);
    void clearNodesWithActiveRubberband();

private:
    OptionSet<RemoteScrollingUIStateChanges> m_changes;
    HashSet<WebCore::ScrollingNodeID> m_nodesWithActiveScrollSnap;
    HashSet<WebCore::ScrollingNodeID> m_nodesWithActiveUserScrolls;
    HashSet<WebCore::ScrollingNodeID> m_nodesWithActiveRubberband;
};

} // namespace WebKit
