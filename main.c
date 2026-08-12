#include <stdio.h>
#include "audio_recorder.h"

static volatile bool g_running = true;
static void on_sigint(int sig)
{
  (void)sig;
  g_running = false;
}

int main(int argc, char **argv)
{
  recorder_config cfg = {0};
  cfg.sample_rate = 48000.0;
  cfg.channels = 2;
  cfg.num_buffers = atoi(argv[1]);
  cfg.buffer_secs = atof(argv[2]);
  cfg.out_path = argv[3];

  audio_recorder *rec = NULL;

  OSStatus err = recorder_create(&cfg, &rec);
  if (err)
  {
    fprintf(stderr, "create failed %d", (int)err);
    return 1;
  }

  signal(SIGINT, on_sigint);
  recorder_start(rec);

  printf("Recording... Ctrl+C to stop.\n");
  while (g_running)
    usleep(10000);

  recorder_stop(rec);

  return 0;
}