#include <stdio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioToolbox/ExtendedAudioFile.h>

volatile bool g_keep_recording = true;

void handle_user_interupt(int signal)
{
  if (signal == SIGINT)
  {
    printf("\nFinalising Audio...\n");
    g_keep_recording = false;
  }
}

typedef struct
{
  ExtAudioFileRef file;
  AudioStreamBasicDescription format;
} AudioCallbackContext;

void AudioCallback(
    void *context,
    AudioQueueRef inAQ,
    AudioQueueBufferRef inBuffer,
    const AudioTimeStamp *inStartTime,
    UInt32 inNumberPacketDescriptions,
    const AudioStreamPacketDescription *inPacketDesc)
{
  AudioCallbackContext *_context = (AudioCallbackContext *)context;
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

int main(int argc, char *argv[])
{
  if (argc < 3)
  {
    fprintf(
        stderr,
        "Missing num buffers or len of buffers (secs) in arguments 1 and 2 respectively.\n");
    return 1;
  }
  // cleanly exit on ctrl+c
  signal(SIGINT, handle_user_interupt);

  OSStatus status = {0};

  AudioStreamBasicDescription format = {0};
  format.mSampleRate = 48000.0;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
  format.mFramesPerPacket = 1;
  format.mChannelsPerFrame = 1;
  format.mBitsPerChannel = 16;
  format.mBytesPerPacket = 2;
  format.mBytesPerFrame = 2;

  AudioCallbackContext callback_data = {0};
  callback_data.format = format;

  // SETUP FILE WRITER
  ExtAudioFileRef fileRef;
  CFURLRef fileUrl = CFURLCreateWithFileSystemPath(
      kCFAllocatorDefault,
      CFSTR("output.wav"),
      kCFURLPOSIXPathStyle,
      false);
  status = ExtAudioFileCreateWithURL(
      fileUrl,
      kAudioFileWAVEType,
      &format,
      NULL,
      kAudioFileFlags_EraseFile,
      &fileRef);

  if (status != noErr)
  {
    fprintf(stderr, "Error creating audio file handler: %d", (int)status);
  }

  // create callback data
  // should also pass format?
  callback_data.file = fileRef;

  // REGISTER QUEUE
  AudioQueueRef queue;
  status = AudioQueueNewInput(
      &format,
      AudioCallback,
      &callback_data,
      NULL,
      NULL,
      0,
      &queue);

  if (status != noErr)
  {
    fprintf(stderr, "Error creating queue: %d", (int)status);
  }

  const int kNumBuffers = atoi(argv[1]);
  const float bufferLenSecs = atof(argv[2]);
  UInt32 bufferSizeBytes = format.mSampleRate * format.mBytesPerFrame * bufferLenSecs;

  fprintf(
      stdout,
      "Creating buffers with, num buffers (%d), lens secs (%f)\n",
      kNumBuffers,
      bufferLenSecs);

  // ALLOCATE CALLBACK BUFFERS
  AudioQueueBufferRef buffers[kNumBuffers];
  for (int i = 0; i < kNumBuffers; i++)
  {
    status = AudioQueueAllocateBuffer(queue, bufferSizeBytes, &buffers[i]);
    if (status != noErr)
    {
      fprintf(stderr, "error allocating buffer %d: %d", i, (int)status);
      return 1;
    }

    status = AudioQueueEnqueueBuffer(queue, buffers[i], 0, NULL);
    if (status != noErr)
    {
      fprintf(stderr, "Error enqueuing buffer %d: %d", i, (int)status);
      return 1;
    }
  }

  // START
  status = AudioQueueStart(queue, NULL);
  if (status != noErr)
  {
    fprintf(stderr, "Error starting audio queue: %d", (int)status);
  }

  // LOOP MAIN THREAD
  printf("Recording...\nPress Ctrl+C to stop and save.\n");

  while (g_keep_recording)
  {
    usleep(100000);
  }

  // CLEANUP
  AudioQueueStop(queue, false);
  AudioQueueDispose(queue, false);
  ExtAudioFileDispose(fileRef);

  printf("Done. File saved successfully.\n");

  return 0;
}
