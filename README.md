# Media Files Handler

**For those low on space and done with paying for cloud storage.** 

---

Simple tool to compress and organize multimedia files (photos and videos). Available in two versions:  
- **Python**: Easy to use and modify.  
- **C**: Optimized for speed and large-scale processing.

**Example Usage (both versions):** 

- Specify the source and destination directories as command-line arguments.  
- The tool compresses multimedia files (from source folder) and recreates the folder structure at the destination.

**Modes:** 

- **Retry Mode:** run via cmd with additional `-r` argument to retry compression for failed files (e.g. .\media_migration.exe -r).
- **Organize Mode:** run via cmd with additional `-o` argument to sort files into folders by creation year.

---

### Setup Instructions

## Python Version

1. **Install Python**:  
   - Download and install Python 3.8+ from [python.org](https://www.python.org/downloads/).  
   - Ensure `python --version` works in your terminal.

2. **Install Dependencies**:  
   - Open a terminal and run:  
     ```bash
     pip install Pillow pillow-heif
     ```  
   - Install `ffmpeg`:  
     - **Windows**: Download from [ffmpeg.org](https://ffmpeg.org/download.html), extract, and add to `PATH` (e.g., `C:\ffmpeg\bin`).  
     - **Linux**: Run `sudo apt update && sudo apt install ffmpeg`.

3. **Run the Script**:  
   - Save the script
   - In a terminal, navigate to the script’s folder:  
     ```bash
     cd path/to/script/folder
     ```  
   - Run the script with source and destination folders:  
     ```bash
     python media_handler.py C:\source C:\dest  # Windows
     python media_handler.py /source /dest      # Linux
     ```  
   - The script compresses files and moves them to `destination_folder`, preserving the folder structure.

---

## C Version

### Setup Instructions

1. **Install Build Tools**:  
   - **Windows** : Install [CMake](https://cmake.org/download/) and [Ninja](https://ninja-build.org/). Add to PATH.  
   - **Linux** : Run `sudo apt update && sudo apt install cmake ninja-build git`.

2. **Install Dependencies**:  
   - **Windows** : Use `vcpkg` to install libraries, as `libexif` and `ffmpeg` lack full native support on Windows, requiring a dependency manager for prebuilt libraries:  
     - Install `vcpkg`:  
       ```cmd
       cd C:\
       git clone https://github.com/microsoft/vcpkg.git
       cd vcpkg
       .\bootstrap-vcpkg.bat
       ```
       - Add vcpkg to PATH
       ```cmd
       Edit environment variables -> Add vcpkg root folder to PATH (e.g. C:\vcpkg)
       ```

     - Install dependencies:  
       ```cmd
       vcpkg install libjpeg-turbo:x64-windows-static libheif:x64-windows-static libexif:x64-windows-static ffmpeg:x64-windows-static libpng:x64-windows
       vcpkg integrate install
       ```  
     - For more on `vcpkg`, see [Microsoft’s documentation](https://learn.microsoft.com/en-us/vcpkg/) or the [GitHub repository](https://github.com/microsoft/vcpkg).  
   - **Linux**: Run:  
     ```bash
     sudo apt install libjpeg-dev libheif-dev libexif-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libpng-dev
     ```

3. **Build and Run**:  
   - Save the source files
   - In a terminal, navigate to the project folder:  
     ```bash
     cd path/to/project/folder
     ```  
   - Build the project:  
     - **Windows** :  
       ```cmd
       mkdir build
       cd build
       cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ..
       cmake --build .
       ```  
     - **Linux** :  
       ```bash
       mkdir build
       cd build
       cmake -G "Ninja" ..
       cmake --build .
       ```  
   - Run the executable:  
     - **Windows** : `.\media_migration.exe C:\source C:\dest` (add -r or -o for modes)  
     - **Linux** : `./media_migration /source /dest` (add -r or -o for modes)
