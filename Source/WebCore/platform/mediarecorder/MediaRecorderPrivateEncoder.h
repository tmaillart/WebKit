/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_RECORDER)

#include "CAAudioStreamDescription.h"
#include "MediaRecorderPrivateWriter.h"
#include "SharedBuffer.h"
#include "VideoEncoder.h"
#include <atomic>
#include <span>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/MediaTime.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NativePromise.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WorkQueue.h>

typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;
typedef struct opaqueCMBufferQueueTriggerToken *CMBufferQueueTriggerToken;

namespace WebCore {

class AudioSampleBufferCompressor;
class InProcessCARingBuffer;
class FragmentedSharedBuffer;
struct MediaRecorderPrivateOptions;
class MediaSample;
class PlatformAudioData;
class VideoFrame;
struct VideoInfo;

class MediaRecorderPrivateEncoder final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<MediaRecorderPrivateEncoder, WTF::DestructionThread::Main> {
public:
    static RefPtr<MediaRecorderPrivateEncoder> create(bool hasAudio, bool hasVideo, const MediaRecorderPrivateOptions&);
    ~MediaRecorderPrivateEncoder();

    void appendVideoFrame(VideoFrame&);
    void appendAudioSampleBuffer(const PlatformAudioData&, const AudioStreamDescription&, const WTF::MediaTime&, size_t);
    void fetchData(CompletionHandler<void(RefPtr<FragmentedSharedBuffer>&&, double)>&&);

    void pause();
    void resume();
    void stopRecording();
    void close();

    String mimeType() const;
    unsigned audioBitRate() const;
    unsigned videoBitRate() const;

    bool hasAudio() const { return m_hasAudio; }
    bool hasVideo() const { return m_hasVideo; }

private:
    MediaRecorderPrivateEncoder(bool hasAudio, bool hasVideo);
    bool initialize(const MediaRecorderPrivateOptions&, UniqueRef<MediaRecorderPrivateWriter>&&);

    static WorkQueue& queueSingleton();

    Ref<MediaRecorderPrivateWriterListener> listener();

    class Listener;
    friend class Listener;
    void appendData(std::span<const uint8_t>);

    MediaTime currentTime() const;
    MediaTime currentEndTime() const;

    void flushDataBuffer();

    static void compressedAudioOutputBufferCallback(void*, CMBufferQueueTriggerToken);

    RefPtr<FragmentedSharedBuffer> takeData();

    MediaTime nextVideoFrameTime(const MediaTime&);
    MediaTime lastMuxedSampleTime() const;

    void generateMIMEType();

    void audioSamplesAvailableStarted(const AudioStreamBasicDescription&);
    void audioSamplesAvailable(size_t, size_t);
    RefPtr<AudioSampleBufferCompressor> audioCompressor() const;
    void enqueueCompressedAudioSampleBuffers();

    void appendVideoFrame(const MediaTime&, Ref<VideoFrame>&&);
    Ref<GenericPromise> encodePendingVideoFrames();
    void processVideoEncoderActiveConfiguration(const VideoEncoder::Config&, const VideoEncoderActiveConfiguration&);
    void enqueueCompressedVideoFrame(VideoEncoder::EncodedFrame&&);

    Ref<GenericPromise> flushPendingData(const MediaTime&);
    void partiallyFlushEncodedQueues();
    Ref<GenericPromise> waitForMatchingAudio(const MediaTime&);
    void flushToEndSegment(const MediaTime&);
    void flushAllEncodedQueues();
    using Result = MediaRecorderPrivateWriter::Result;
    Result muxNextFrame();

    void maybeStartWriter();
    bool hasMuxedDataSinceEndSegment() const;

    std::atomic<bool> m_isStopped { false };
    bool m_writerIsStarted WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    bool m_writerIsClosed WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    MediaTime m_lastMuxedSampleStartTime WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    MediaTime m_lastMuxedAudioSampleEndTime WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    bool m_hasMuxedAudioFrameSinceTakeData WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    bool m_hasMuxedVideoFrameSinceTakeData WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    bool m_nextVideoFrameMuxedShouldBeKeyframe WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { true };
    size_t m_pendingFlush WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { 0 };
    Ref<GenericPromise> m_currentFlushOperations WTF_GUARDED_BY_CAPABILITY(mainThread) { GenericPromise::createAndResolve() };

    String m_mimeType WTF_GUARDED_BY_CAPABILITY(mainThread);

    FourCharCode m_audioCodec { 0 }; // set on creation. const after
    String m_audioCodecMimeType WTF_GUARDED_BY_CAPABILITY(mainThread);
    std::optional<uint8_t> m_audioTrackIndex WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    RetainPtr<CMFormatDescriptionRef> m_audioFormatDescription WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    RetainPtr<CMFormatDescriptionRef> m_audioCompressedFormatDescription WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Lock m_ringBufferLock;
    std::unique_ptr<InProcessCARingBuffer> m_ringBuffer; // lock must be held when creating or deleting.
    RefPtr<AudioSampleBufferCompressor> m_audioCompressor; // set on creation. const after
    Deque<Ref<MediaSample>> m_encodedAudioFrames WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    std::atomic<size_t> m_lastEnqueuedAudioSampleCount { 0 };
    std::atomic<size_t> m_currentAudioSampleCount { 0 };
    std::atomic<uint32_t> m_currentAudioSamplingRate { 0 };
    std::optional<CAAudioStreamDescription> m_originalDescription;
    std::optional<std::pair<const MediaTime, GenericPromise::Producer>> m_pendingAudioFramePromise WTF_GUARDED_BY_CAPABILITY(queueSingleton());

    FourCharCode m_videoCodec { 0 }; // set on creation. const after
    String m_videoCodecMimeType WTF_GUARDED_BY_CAPABILITY(mainThread);
    std::optional<uint8_t> m_videoTrackIndex WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    RefPtr<VideoInfo> m_videoTrackInfo WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    RetainPtr<CMFormatDescriptionRef> m_videoFormatDescription WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    std::unique_ptr<VideoEncoder> m_videoEncoder WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Deque<std::pair<Ref<VideoFrame>, MediaTime>> m_pendingVideoFrames WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Deque<Ref<MediaSample>> m_encodedVideoFrames WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    bool m_firstVideoFrameProcessed WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    MediaTime m_lastEnqueuedRawVideoFrame WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { MediaTime::negativeInfiniteTime() };
    MediaTime m_lastVideoKeyframeTime WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    MediaTime m_lastReceivedCompressedVideoFrame WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { MediaTime::negativeInfiniteTime() };
    MediaTime m_currentVideoDuration WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    std::optional<MediaTime> m_firstVideoFrameTime WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    std::optional<MonotonicTime> m_resumeWallTime WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    std::optional<CGAffineTransform> m_videoTransform;
    RefPtr<GenericNonExclusivePromise> m_videoEncoderCreationPromise;

    Lock m_lock;
    SharedBufferBuilder m_data WTF_GUARDED_BY_LOCK(m_lock);
    static constexpr size_t s_dataBufferSize { 1024 };
    Vector<uint8_t> m_dataBuffer WTF_GUARDED_BY_LOCK(m_lock);
    double m_timeCode WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { 0 };

    uint64_t m_audioBitsPerSecond { 0 }; // set on creation. const after
    uint64_t m_videoBitsPerSecond { 0 }; // set on creation. const after

    bool m_isPaused WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    bool m_needKeyFrame WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { true };
    std::optional<MediaTime> m_endMuxedTime WTF_GUARDED_BY_CAPABILITY(queueSingleton());

    const MediaTime m_maxGOPDuration { MediaTime::createWithDouble(2) };
    const bool m_hasAudio { false };
    const bool m_hasVideo { false };
    const Ref<Listener> m_listener;
    std::unique_ptr<MediaRecorderPrivateWriter> m_writer; // Always set and immutable once initialize() has been called
};

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
