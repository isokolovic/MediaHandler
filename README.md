# Media Files Handler
**For those low on space and done with paying for cloud storage.**

---

Simple tool to compress and organize multimedia files. Also available in [Python](../../tree/py) and [C](../../tree/c).

**Modes:**
- **Normal**: compresses files from source, recreates folder structure at destination
- **Retry** `-r`: re-attempts files that failed in the previous run
- **Organize** `--organize`: moves files into `output/YYYY/` folders by creation year — **existing folder structure will change**

---

## Setup

### Dependencies

- **Windows**: install [vcpkg](https://github.com/microsoft/vcpkg) and add it to `PATH` — all libraries are pulled automatically at configure time
- **Linux**:
  ```bash
  sudo apt install libjpeg-dev libheif-dev libexif-dev libavcodec-dev libavformat-dev \
    libavutil-dev libswscale-dev libpng-dev libspdlog-dev nlohmann-json3-dev
  ```

### Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Run

```bash
./media_handler --input /source --output /dest [-r | --organize]
```

---

## Configuration

CLI flags override `config.json` values. All fields are optional.

```json
{
  "input_dir": "/source",
  "output_dir": "/dest",
  "threads": 8,
  "crf": 23,
  "video_preset": "medium",
  "log_level": "info",
  "json_log": false
}
```

| Flag | Description |
|---|---|
| `-i, --input` | Source directory |
| `-o, --output` | Destination directory |
| `-t, --threads` | Worker thread count |
| `--crf` | Video quality (0–51, lower = better) |
| `--preset` | Encoding preset (`ultrafast` → `veryslow`) |
| `-l, --log-level` | `trace` `debug` `info` `warn` `error` |
| `-j, --json` | Structured JSON log output |
| `-r, --retry` | Retry failed files from last run |
| `--organize` | Move files into `output/YYYY/` folders |