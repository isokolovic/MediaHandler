# Media Files Handler
**For those low on space and done with paying for cloud storage.**

---

Batch compress and organize multimedia files while preserving folder structure. Also available in [Python](../../tree/py) and [C](../../tree/c).

---

## Modes

| Mode | Flag | Description |
|---|---|---|
| **Compress** | *(default)* | Compress all media in source, recreate folder structure at destination. Skips already-compressed files on re-run. |
| **Retry** | `-r` | Re-attempt only the files that failed in the previous run. |
| **Organize** | `--organize` | Move files into `destination/YYYY/` subfolders based on creation date. No compression. **Source files are moved — original folder structure is removed.** |

---

## Setup

### Windows

Install [vcpkg](https://github.com/microsoft/vcpkg) and add it to `PATH`. All libraries are resolved automatically at configure time via `vcpkg.json`.

### Linux

```bash
sudo apt install libjpeg-dev libheif-dev libexif-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
  libpng-dev libspdlog-dev nlohmann-json3-dev
```

### Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

---

## Usage

```bash
./media_handler --input /source --output /dest [options]
```

| Flag | Default | Description |
|---|---|---|
| `-i, --input` | `config.json` value | Source directory. |
| `-o, --output` | `config.json` value | Destination directory. |
| `-t, --threads` | CPU count | Number of parallel worker threads. |
| `--crf` | `23` | Video quality (0–51). Lower = better quality, larger file. 18–28 is the practical range. |
| `--preset` | `medium` | FFmpeg encoding preset. Trades encoding speed for compression efficiency (`ultrafast` → `veryslow`). |
| `-r, --retry` | — | Reprocess only files that failed in the last run. Progress is tracked in `.mediahandler_state` in the output directory — do not delete this file between runs. |
| `-j, --json` | — | Emit logs as JSON (one object per line) instead of plain text. |
| `--organize` | — | Move files into `output/YYYY/` by creation date. **Source files are moved, not copied.** |

Logs are written to `media_handler.log` in the working directory, alongside plain-text console output.

---

## Configuration

All options can be set in `config.json` at the project root. CLI flags are overwritten by config file values.

```json
{
  "input_dir": "/path/to/source",
  "output_dir": "/path/to/destination",
  "threads": 8,
  "crf": 23,
  "video_preset": "medium",
  "log_level": "info",
  "json_log": false
}
```

All fields are optional — omitted values fall back to defaults.