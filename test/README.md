# Tests

Offline-first suite

```sh
xmake test
```

## Groups

| Group | Binary | Timeout | Needs |
|-------|--------|---------|--------|
| `unit` | `test_unit_core`, `test_unit_runtime` | 10s | gtest; core also needs FFmpeg |
| `integration` | `test_integration` | 30s | FFmpeg + `fixtures/av_1s.mkv` |
| `network` | `test_network` | 90s | public HTTPS (opt-in) |

```sh
xmake test -g unit
xmake test -g integration
```

## What each layer checks

### `unit/core` — value types

- Chrono ↔ FFmpeg timestamp conversion at 1/1000
- `PixelFormat` string names match FFmpeg
- `Value` list/map storage

### `unit/runtime` — graph runtime (no media)

Uses `ProbeElement` only:

| Suite | Asserts |
|-------|---------|
| `ElementStateTest` | Lifecycle order; forward failure rolls back to call-time origin |
| `PadTest` | Push / event / query delivery; error propagation; unlink |
| `BinStateTest` | Children change state sink→source up, source→sink down; sibling rollback on failure |
| `PipelineStateTest` | Context + clock shared while `Running` |

### `integration` — real elements, still offline

| Test | Graph |
|------|--------|
| `QueueIntegrationTest` | Source → `Queue` → Sink; PTS order 1/2/3 ms |
| `LocalMediaIntegrationTest` | `UrlSource(file)` → SW `Decoder` ×2 → sinks; EOS + frames |

`LocalMedia` links pads **after** `Paused` so dynamic stream pads exist.

### `network` — opt-in smoke

`UrlSource` against GStreamer public sintel trailer; first video pad → ProbeSink; EOS within 60s. **Not** part of default CI.

```sh
xmake f --network_tests=y
xmake test -g network
```

Disable again:

```sh
xmake f --network_tests=n
```

## Key fixture: `ProbeElement`

Defined in `support/probe_element.hpp`. Configurable as Source / Transform / Sink:

- Records samples (as `SampleObservation`), events, queries
- Optional shared `StateTrace` for multi-element order
- One-shot failure injection on state changes or pad ops
- `waitForSamples` / `waitForEndOfStream` (null sample = EOS)

## Run notes

- `set_rundir` is the **project root**, so fixture paths are `test/fixtures/...`.
