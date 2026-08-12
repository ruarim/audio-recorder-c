#include "audio_recorder.h"
#include <string.h>

struct audio_recorder
{
    AudioQueueRef queue;
    ExtAudioFileRef file;
    AudioStreamBasicDescription format;
    recorder_config cfg;
    char *path_copy;
};

static void input_cb(
    void *context,
    AudioQueueRef inAQ,
    AudioQueueBufferRef inBuffer,
    const AudioTimeStamp *inStartTime,
    UInt32 inNumberPacketDescriptions,
    const AudioStreamPacketDescription *inPacketDesc)
{
    audio_recorder *_context = (audio_recorder *)context;
    OSStatus status = {0};

    AudioStreamBasicDescription format = _context->format;
    UInt32 size = sizeof(format);

    AudioBufferList bufferList;
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0].mNumberChannels = format.mChannelsPerFrame;
    bufferList.mBuffers[0].mDataByteSize = inBuffer->mAudioDataByteSize;
    bufferList.mBuffers[0].mData = inBuffer->mAudioData;
    UInt32 numFrames = inBuffer->mAudioDataByteSize / format.mBytesPerFrame;

    status = ExtAudioFileWriteAsync(_context->file, numFrames, &bufferList);
    if (status != noErr)
    {
        fprintf(stderr, "Error saving to file: %d\n", (int)status);
    }

    status = AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    if (status != noErr)
    {
        fprintf(stderr, "Error re-enqueuing buffer: %d\n", (int)status);
    }
}

static bool config_ok(const recorder_config *cfg)
{
    return cfg != NULL && cfg->num_buffers > 0 && cfg->buffer_secs > 0.0f;
}

OSStatus recorder_create(const recorder_config *cfg, audio_recorder **out)
{
    if (!config_ok(cfg) || out == NULL)
        return kAudio_ParamError;

    *out = NULL;
    audio_recorder *r = calloc(1, sizeof(*r));
    if (r == NULL)
        return -1;
    r->cfg = *cfg;

    OSStatus status = noErr;

    AudioStreamBasicDescription asbd = {0};
    asbd.mSampleRate = cfg->sample_rate > 0 ? cfg->sample_rate : 48000.0;
    asbd.mFormatID = kAudioFormatLinearPCM;
    asbd.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    asbd.mChannelsPerFrame = cfg->channels > 0 ? cfg->channels : 2;
    asbd.mBitsPerChannel = 16;
    asbd.mBytesPerFrame = (asbd.mBitsPerChannel / 8) * asbd.mChannelsPerFrame;
    asbd.mBytesPerPacket = asbd.mBytesPerFrame;
    asbd.mFramesPerPacket = 1;

    status = AudioQueueNewInput(&asbd, input_cb, r, NULL, NULL, 0, &r->queue);

    if (status != noErr)
        goto fail;

    UInt32 fmt_size = sizeof(r->format);
    status = AudioQueueGetProperty(
        r->queue, kAudioQueueProperty_StreamDescription,
        &r->format, &fmt_size);

    if (status != noErr)
        goto fail;

    // file setup
    if (cfg->out_path != NULL)
    {
        r->path_copy = strdup(cfg->out_path);
        if (r->path_copy == NULL)
        {
            status = -1;
            goto fail;
        }

        CFURLRef url = CFURLCreateFromFileSystemRepresentation(
            kCFAllocatorDefault, (const UInt8 *)r->path_copy,
            strlen(r->path_copy), false);

        status = ExtAudioFileCreateWithURL(url, kAudioFileWAVEType, &r->format,
                                           NULL, kAudioFileFlags_EraseFile, &r->file);
        CFRelease(url);
        if (status != noErr)
            goto fail;
    }

    UInt32 buffer_size = r->format.mSampleRate * r->format.mBytesPerFrame * cfg->buffer_secs;
    for (UInt32 i = 0; i < cfg->num_buffers; i++)
    {
        AudioQueueBufferRef buffer = NULL;
        status = AudioQueueAllocateBuffer(r->queue, buffer_size, &buffer);
        if (status != noErr)
            goto fail;
        status = AudioQueueEnqueueBuffer(r->queue, buffer, 0, NULL);
        if (status != noErr)
            goto fail;
    }

    *out = r;
    return noErr;

fail:
    recorder_destroy(r);
    return status;
}

OSStatus recorder_start(audio_recorder *r)
{
    OSStatus status = AudioQueueStart(r->queue, NULL);
    if (status != noErr)
    {
        fprintf(stderr, "Error starting audio queue: %d", (int)status);
    }
    return status;
}

OSStatus recorder_stop(audio_recorder *r)
{
    OSStatus status = AudioQueueStop(r->queue, false);
    if (status != noErr)
    {
        fprintf(stderr, "Error stopping audio queue: %d", (int)status);
    }
    return status;
}

OSStatus recorder_destroy(audio_recorder *r)
{
    if (r == NULL)
        return noErr;
    if (r->queue)
    {
        AudioQueueStop(r->queue, true);
        AudioQueueDispose(r->queue, true);
    }
    if (r->file)
        ExtAudioFileDispose(r->file);
    free(r->path_copy);
    free(r);
    return noErr;
}
