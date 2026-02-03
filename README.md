## Media Files Handler

**For those low on space and done with paying for cloud storage.** 

---

**Example Usage:** 

- Specify the source and destination directories as command-line arguments.  
- The tool compresses multimedia files (from source folder) and recreates the folder structure at the destination.

**Modes:** 

- **Retry Mode:** run via cmd with additional `-r` argument to retry compression for failed files  
- **Organize Mode:** run via cmd with additional `-o` argument to sort files into folders by creation year. **Beware**, files will be moved from existing folder structure to a new one. 

---

## Setup Instructions

1. **Install Build Tools**:  
   - **Windows** : Install [CMake](https://cmake.org/download/). Add to `PATH`.  
   - **Linux** : Run `sudo apt update && sudo apt install cmake`.

2. **Install Dependencies**:  
   - **Windows** : Use `vcpkg` to install libraries, as `libexif` and `ffmpeg` lack full native support on Windows, requiring a dependency manager for prebuilt libraries:  
     - Install `vcpkg`:  
       ```cmd
       cd C:\  
       git clone https://github.com/microsoft/vcpkg.git  
       cd vcpkg  
       .\bootstrap-vcpkg.bat  
       ```  
       - Add vcpkg to `PATH`  
       ```cmd
       Edit environment variables -> Add vcpkg root folder to `PATH` (e.g. C:\vcpkg)  
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
       cmake ..  
       cmake --build .  
       ```  
     - Hint:  
       ```cmd
       If using Visual Studio, make sure you configure Runtime Library to be Multi-threaded DLL (/MD), ensuring executable will rely on the system-installed DLLs.  
       (Properties / Configuration Properties / C/C++ / Code Generation / Runtime Library)  
       ```  
     - **Linux** :  
       ```bash
       mkdir build && cd build && cmake .. && cmake --build .  
       ```  
   - Run the executable:  
     - **Windows** :  
       ```cmd
       .\media_migration.exe C:\source C:\dest [-r | -o]  
       ```  
     - **Linux** :  
       ```bash
       ./media_migration /source /dest [-r | -o]  
       ```
