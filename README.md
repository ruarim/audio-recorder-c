# audio-recorder-c

A small macOS command-line audio recorder written in C, using CoreAudio's AudioQueue to capture the microphone and write a WAV file.

## Build

```sh
./build.sh
```

## Run

```sh
./mic_recorder <num_buffers> <buffer_secs> <out_path>
```

Example (3 buffers of 0.5s each):

```sh
./run.sh
```

Press Ctrl+C to stop recording.
