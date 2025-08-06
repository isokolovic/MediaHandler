# 📁 Media Files Handler

Simple Python script that modifies and organizes multimedia files.  
It compresses photos and videos and saves them to another location, preserving folder structure. 

**Upcoming update: organize files by creation date:**

**📸 Example Usage:**
- Place the script in a folder containing multimedia files.
- Modify the script to specify the output directory (`destination_folder`).
- The script will compress multimedia files and recreate the folder structure at `destination_folder`. 

**🛠️ Setup Instructions:**

1. **🐍 Install Python**:
   - Download and install Python 3.8+ from [python.org](https://www.python.org/downloads/).   

2. **📦 Install Dependencies**:
   - Open a terminal and run:
     ```bash
     pip install Pillow pillow-heif
     ```
   - Install `ffmpeg`:
     - **Windows** 💻: Download from [ffmpeg.org](https://ffmpeg.org/download.html), extract, and add to PATH (e.g., `C:\ffmpeg\bin`).
     - **Linux** 🐧: Run `sudo apt update && sudo apt install ffmpeg`.     

3. **🚀 Run the Script**:
   - Save the script as `media_handler.py`.
   - In the script, set `destination_folder` (e.g., `destination_folder = "D:\\output"` for Windows or `destination_folder = "/home/user/output"` for Linux).
   - In a terminal, navigate to the script’s folder:
     ```bash
     cd path/to/script/folder
     ```
   - Run the script with:
     ```bash
     python media_handler.py
     ```