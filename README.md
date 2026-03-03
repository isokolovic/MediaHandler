## Media Files Handler
**For those low on space and done with paying for cloud storage.**

---

Batch compress and organize multimedia files while preserving folder structure. Also available in [Python](../../tree/py) and [C](../../tree/c).

Multithreaded processing via a shared work queue. Compression via FFmpeg (video) and libjpeg/libheif (images). Run state is persisted after every file — interrupted jobs resume where they left off.

**Example Usage:**
- Specify the source and destination directories via CLI or `config.json`
- The tool compresses multimedia files from the source folder and recreates the folder structure at the destination

**Modes:**
- **Retry Mode:** run with additional `-r` argument to retry compression for failed files. Progress is tracked in `.mediahandler_state` in the output directory — do not delete this file between runs.
- **Organize Mode:** run with additional `--organize` argument to sort files into folders by creation year. **Beware**, files will be moved from the existing folder structure to a new one — source files are not retained.

---

**Setup Instructions**

1. **Install Build Tools**:
   - **Windows**: Install [CMake](https://cmake.org/download/). Add to `PATH`.
   - **Linux**: Run `sudo apt update && sudo apt install cmake`.

2. **Install Dependencies**:
   - **Windows**: Install [vcpkg](https://github.com/microsoft/vcpkg) and add it to `PATH`:
     ```cmd
     cd C:\
     git clone https://github.com/microsoft/vcpkg.git
     cd vcpkg
     .\bootstrap-vcpkg.bat
     ```
     All libraries are resolved automatically at configure time via the `vcpkg.json`. 
   - **Linux**:
     ```bash
     git clone https://github.com/microsoft/vcpkg.git
     cd vcpkg
     ./bootstrap-vcpkg.sh
     export VCPKG_ROOT=$(pwd)
     export PATH=$VCPKG_ROOT:$PATH
     ```

3. **Build and Run**:
   - In a terminal, navigate to the project folder:
     ```bash
     cd path/to/project/folder
     ```
   - Build the project:
     - **Windows**:
       ```cmd
       mkdir build
       cd build
       cmake ..
       cmake --build .
       ```
     - **Linux**:
       ```bash
       mkdir build && cd build && cmake .. && cmake --build .
       ```
   - Run the executable:
     - **Windows**:
       ```cmd
       .\media_handler.exe --input C:\source --output C:\dest [-r | --organize]
       ```
     - **Linux**:
       ```bash
       ./media_handler --input /source --output /dest [-r | --organize]
       ```

---

**Configuration**

All options can be set in `config.json` at the project root. CLI flags override config values.

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

flag | default | description
--- | --- | ---
`-i, --input` | config.json | source directory
`-o, --output` | config.json | destination directory
`-t, --threads` | cpu count | number of parallel worker threads
`--crf` | 23 | video quality 0–51, lower = better quality, larger file, practical range 18–28
`--preset` | medium | ffmpeg encoding preset, trades speed for compression efficiency (`ultrafast` → `veryslow`)
`-r, --retry` | | reprocess only files that failed in the last run
`-j, --json` | | emit logs as json, one object per line, useful for log aggregation
`-l, --log-level` | info | verbosity: `trace` `debug` `info` `warn` `error` `critical`
`--organize` | | move files into `output/YYYY/` by creation date. Files are not compressed, only moved. 

Logs are written to `media_handler.log` in the working directory alongside console output.