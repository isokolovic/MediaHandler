## Media Files Handler

**For those low on space and done with paying for cloud storage.** 

---

Simple tool to compress and organize multimedia files.

**Example Usage:** 

- Specify the source and destination directories as command-line arguments.  
- The tool compresses multimedia files (from source folder) and recreates the folder structure at the destination.

**Modes:** 

- **Retry Mode:** run via cmd with additional `-r` argument to retry compression for failed files  
- **Organize Mode:** run via cmd with additional `-o` argument to sort files into folders by creation year. **Beware**, files will be moved from existing folder structure to a new one. 

---

## Setup Instructions

1. **Install Python**:  
   - Download and install Python from [python.org](https://www.python.org/downloads/).  

2. **Install Dependencies**:  
   - Open a terminal and run:  
     ```bash
     pip install Pillow pillow-heif av 
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