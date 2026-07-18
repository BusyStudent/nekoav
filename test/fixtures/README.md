# Test media fixtures

`av_1s.mkv` is a one-second, 64x64 Matroska fixture with MPEG-4 video and
mono PCM audio. It is intentionally small and uses codecs built into FFmpeg so
the default integration suite is deterministic and does not need the network,
an audio device, or a window system.

It was generated with FFmpeg 7.1:

```sh
ffmpeg -f lavfi -i testsrc2=size=64x64:rate=10 \
  -f lavfi -i sine=frequency=440:sample_rate=8000 \
  -t 1 -c:v mpeg4 -q:v 8 -pix_fmt yuv420p -c:a pcm_s16le \
  -y test/fixtures/av_1s.mkv
```

After regenerating the file, update its SHA-256 below:

```text
SHA256: 30CFC76DE879DEBB9DB09A5FD6AE54D2A001606E2E6FAFBE04C9BF8A306DD788
```
