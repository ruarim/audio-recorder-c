#pragma once
#include <AudioToolbox/AudioToolbox.h>

typedef struct audio_recorder audio_recorder;

typedef void (*recorder_data_cb)(void *user_data,
                                 const AudioBufferList *audio,
                                 UInt32 num_frames);

typedef struct {
    double sample_rate; 
    UInt32 channels;
    UInt32 num_buffers;
    float buffer_secs;
    const char *out_path;
    recorder_data_cb on_data;
    void *on_data_ctx;
} recorder_config;

OSStatus recorder_create(const recorder_config *cfg, audio_recorder **out);
OSStatus recorder_start(audio_recorder *r);
OSStatus recorder_stop(audio_recorder *r);
OSStatus recorder_destroy(audio_recorder *r);

static void input_cb(
    void *context,
    AudioQueueRef inAQ,
    AudioQueueBufferRef inBuffer,
    const AudioTimeStamp *inStartTime,
    UInt32 inNumberPacketDescriptions,
    const AudioStreamPacketDescription *inPacketDesc);