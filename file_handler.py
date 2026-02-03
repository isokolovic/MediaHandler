import os
import stat
import shutil
import platform
from logger import log_message, LogLevel, log_file_processing
from datetime import datetime

EMPTY_FILENAME = "Unnamed"

image_extensions = [".jpg", ".jpeg", ".heic", ".png", ".bmp"]
video_extensions = [".mp4", ".avi", ".mov"] #.3gp not supported currently (encoder compatibility)
other_extensions = [".gif", ".mp3", ".3gp"]
# Think twice before modifying (. = file extension, " " used in folder naming, etc.) 
special_chars = " %:/,\\{}~[]<>*?čćžđšČĆŽŠĐ"
folder_special_chars = "%:/,.\\{}~[]<>*?čćžđšČĆŽŠĐ"

def is_directory(path):
    """
    Checks if the provided path is a directory
    Parameters:
        path (str): Path provided by the user.
    Returns:
        bool: True if the path is a directory
    """
    return os.path.isdir(path)

def is_image(filename):
    """
    Checks if the file type is an image.
    Parameters:
        filename (str): Path to the file.
    Returns:
        bool: True if the file is an image;
    """
    ext = os.path.splitext(filename)[1].lower()
    return ext in image_extensions

def is_video(filename):
    """
    Checks if the file type is a video.
    Parameters:
        filename (str): Path to the file.
    Returns:
        bool: True if the file is a video;
    """
    ext = os.path.splitext(filename)[1].lower()
    return ext in video_extensions

def is_gif_misc(filename):
    """
    Checks if the file type is a gif or other supported.
    Parameters:
        filename (str): Path to the file.
    Returns:
        bool: True if the file is gif or supported;
    """
    ext = os.path.splitext(filename)[1].lower()
    return ext in other_extensions

def copy_file(source, destination):
    """
    Copies a file from the source to the destination.
    Parameters:
        source (str): Path to the source file.
        destination (str): Path to the destination file.
    Returns:
        bool: True if copying is successful.
    """
    try:
        shutil.copy2(source, destination)
        if platform.system() != "Windows":
            os.chmod(destination, 0o644)
        return True
    except Exception as e:
        log_message(LogLevel.LOG_ERROR, f"Failed to copy from {source} to {destination}: {e}")
        return False

def create_directory(path):
    """
   Creates a directory at the specified path.
    Parameters:
        path (str): Directory path to be created.
    Returns:
        bool: True if the directory was created successfully
    """
    try:
        os.makedirs(path, exist_ok=True)
        return True
    except Exception as e:
        log_message(LogLevel.LOG_ERROR, f"Failed to create directory {path}: {e}")
        return False

def file_exists(path):
    """
    Checks if a file exists at the specified path.
    Parameters:
        path (str): Path to the file.
    Returns:
        bool: True if the file exists
    """   
    return os.path.exists(path)

def clean_name(element, is_directory = False):
    """
    Cleans a file or folder name by removing special characters.
    Parameters:
        element (str): Element to be cleaned.
    Returns:
        str: Cleaned name, or None if an error occurs, or EMTPY_FILENAME if the result is empty.
    """
    if not element:
        log_message(LogLevel.LOG_ERROR, "NULL element in clean_name")
        return None

    special = folder_special_chars if is_directory else special_chars
    cleaned_name = ''.join([ch for ch in element if ch not in special])

    if not cleaned_name:
        return "" if is_directory else EMPTY_FILENAME
    return cleaned_name

def extract_relative_dir(source_path, file_path):
    """
    Extracts the relative subfolder path from a full file path.
    Parameters:
        source_path (str): Root source directory.
        file_path (str): Full path to the file.
    Returns:
        str: Relative subfolder path.
    """
    if not source_path or not file_path:
        log_message(LogLevel.LOG_ERROR, "Null parameter in extract_relative_dir")
        return None

    # Normalize paths
    source_path = os.path.normpath(source_path)
    file_path = os.path.normpath(file_path)

    if not file_path.startswith(source_path):
        log_message(LogLevel.LOG_WARNING, f"File {file_path} is not under source {source_path}")
        return ""

    relative_path = os.path.relpath(file_path, source_path)
    relative_dir = os.path.dirname(relative_path)
    if relative_dir == '.' or not relative_dir:
        return ""

    # Clean the relative path
    cleaned_dir = clean_name(relative_dir, True)
    if cleaned_dir is None:
        log_message(LogLevel.LOG_ERROR, f"Failed to clean relative directory {relative_path}")
        return ""
    return cleaned_dir

def get_file_size(path):
    """
    Gets the size of the file at the specified path.
    Parameters:
        path (str): Path to the file.
    Returns:
        int: File size in bytes.
    """
    try:
        return os.path.getsize(path)
    except:
        log_message(LogLevel.LOG_ERROR, f"File {path} doesn't exist")
        return -1

def get_valid_directory(prompt):
    """
    Checks if the source or destination folder exists based on user input.
    Parameters:
        prompt (str): Prompt message for user input.
    Returns:
        str: Validated folder path.
    """
    print(prompt, end='')
    folder = input().strip()
    if not is_directory(folder):
        log_message(LogLevel.LOG_ERROR, f"Folder {folder} does not exist")
        return None
    return folder

def is_file_type_valid(filename):
    """
    Checks if the file needs to be processed based on its type.
    Parameters:
        filename (str): Path to the file.
    Returns:
        bool: True if the file should be processed
    """
    return is_image(filename) or is_video(filename) or is_gif_misc(filename)

def get_failed_files_from_log(log_path):
    """
    Parses a log file to extract file paths marked as failed.
    Parameters:
        log_path (str): Path to the log file.
    Returns:
        tuple:
            - failed_files (list of str): List of failed file paths.
            - num_failed (int): Total number of failed files.
        Returns None if the log file cannot be opened or read.
    """
    marker = "File Processing: "
    status_marker = " - FAILED"

    try:
        with open(log_path, "r") as log:
            failed_files = []
            for line in log:
                if marker not in line or status_marker not in line:
                    continue

                start = line.find(marker) + len(marker)
                end = line.find(status_marker, start)

                if end <= start or (end - start) >= 1024: #Sanity check
                    continue

                file_path = line[start:end].strip()
                failed_files.append(file_path)

            return failed_files, len(failed_files)

    except FileNotFoundError:
        log_message(LogLevel.LOG_ERROR, f"Log file not found: {log_path}")
        return None, 0
    except Exception as e:
        log_message(LogLevel.LOG_ERROR, f"Error reading log file {log_path}: {e}")
        return None, 0

def retry_failed_files(root_source, destination_folder, failed_files, num_failed):
    """
    Retries processing for a list of failed files.
    Parameters:
        root_source (str): Source directory.
        destination_folder (str): Destination directory.
        failed_files (list of str): List of failed file paths.
        num_failed (int): Number of failed files to retry.
    Returns:
        tuple:
            - total_processed (int): Number of files retried.
            - total_failed (int): Number of files that failed again.
    """
    processed = 0
    failed = 0    

    for i in range(num_failed):
        file_path = failed_files[i]
        log_message(LogLevel.LOG_INFO, f"Retrying file: {file_path}")
        processed += 1

        if not create_folder_process_file(root_source, destination_folder, file_path):
            failed += 1

    return processed, failed

def compress_file(file, output_file):
    """
    Compresses a file based on its type (image, video, or other).
    Parameters:
        file (str): Path to the input file.
        output_file (str): Path to the output compressed file.
    Returns:
        bool: True if compression or copy is successful
    """
    from compressor import compress_image, compress_video

    if not file or not output_file:
        log_message(LogLevel.LOG_ERROR, "Null input or output path in compress_file")
        return False

    ext = os.path.splitext(file)[1]
    if not ext:
        log_message(LogLevel.LOG_ERROR, f"No extension found for {file}")
        return False

    if is_image(file):
        status = compress_image(file, output_file, ext)
    elif is_video(file):
        status = compress_video(file, output_file)
    else:
        status = True  #Only copy other file types

    log_file_processing(file, status)
    return status

def get_file_creation_year(file_path):
    """
    Returns the creation year of a file
    (or last modified if creation > modified)
    Parameters:
        file_path (str): Path to the file.
    Returns:
        int: Year of file creation, or 0 on error.
    """
    try:
        st = os.stat(file_path)
        # Try to get creation time if available
        created = getattr(st, "st_birthtime", None) or getattr(st, "st_ctime", None)
        modified = st.st_mtime
        timestamp = min(created, modified) #Take the smaller one
        return datetime.fromtimestamp(timestamp).year
    except Exception as e:
        log_message(LogLevel.LOG_ERROR, f"Failed to extract timestamp from {file_path}: {e}")
        return 0

def move_file_to_year_folder(destination_folder, file_path, year):
    """
    Moves a file into a subfolder named after its creation year.
    Parameters:
        destination_folder (str): Base destination directory.
        file_path (str): Full path to the file to move.
        year (int): File creation year.
    Returns:
        int: 1 on success, 0 on failure.
    """
    if year <= 0:
        log_message(LogLevel.LOG_ERROR, f"Invalid year for {file_path}")
        return 0

    dest_dir = os.path.join(destination_folder, str(year))
    if not create_directory(dest_dir):
        log_message(LogLevel.LOG_ERROR, f"Failed to create year folder {dest_dir}")
        return 0

    filename = os.path.basename(file_path)
    dest_path = os.path.join(dest_dir, filename)

    try:
        os.rename(file_path, dest_path)
        log_message(LogLevel.LOG_INFO, f"Organized {file_path} to {dest_dir}")
        return 1
    except Exception as e:
        log_message(LogLevel.LOG_ERROR, f"Failed to move {file_path} to {dest_path}: {e}")
        return 0

def organize_files(root_source, source_folder, destination_folder):
    """
    Recursively organizes valid files from a source directory into year-based subfolders under the destination.
    Parameters:
        root_source (str): The top-level source directory. Used for logging or relative path extraction.
        source_folder (str): The current directory being processed.
        destination_folder (str): The base destination directory where files will be moved.
    Returns:
        tuple:
            - processed_count (int): Total number of valid files processed.
            - failed_count (int): Total number of files that failed to be moved or stat'ed.
    """
    processed = 0
    failed = 0

    try:
        entries = os.listdir(source_folder)
    except Exception as e:
        log_message(LogLevel.LOG_ERROR, f"Failed to open directory {source_folder}: {e}")
        return processed, failed

    for entry in entries:
        if entry in {".", ".."}:
            continue

        full_path = os.path.join(source_folder, entry)

        try:
            st = os.stat(full_path)
        except Exception:
            log_message(LogLevel.LOG_ERROR, f"Failed to stat {full_path}")
            failed += 1
            continue

        if stat.S_ISDIR(st.st_mode):
            sub_processed, sub_failed = organize_files(root_source, full_path, destination_folder)
            processed += sub_processed
            failed += sub_failed
        elif is_file_type_valid(entry):
            processed += 1
            year = get_file_creation_year(full_path)
            if not move_file_to_year_folder(destination_folder, full_path, year):
                failed += 1

    return processed, failed

def create_folder_process_file(source_folder, destination_folder, filename):
    """
    Creates the necessary subdirectory and processes a file by copying and compressing it.
    Parameters:
        source_folder (str): Root source directory used to compute the relative subfolder.
        destination_folder (str): Base destination directory where the file should be placed.
        filename (str): Full path to the file to be processed.
    Returns:
        bool: True if the file was successfully copied and compressed, False otherwise.
    """
    if not source_folder or not destination_folder or not filename:
        log_message(LogLevel.LOG_ERROR, "Null parameter in create_folder_process_file")
        return False

    base_filename = os.path.basename(filename)
    clean_filename = clean_name(base_filename, is_directory=False)
    if clean_filename is None:
        log_message(LogLevel.LOG_ERROR, f"Failed to clean filename {base_filename}")
        return False

    subdirectory = extract_relative_dir(source_folder, filename)
    if subdirectory is None:
        log_message(LogLevel.LOG_ERROR, f"Failed to extract subdirectory for {filename}")
        return False

    destination_dir = os.path.join(destination_folder, subdirectory) if subdirectory else destination_folder
    if not create_directory(destination_dir):
        log_message(LogLevel.LOG_ERROR, f"Failed to create destination directory {destination_dir}")
        return False

    destination_path = os.path.join(destination_dir, clean_filename)

    ext = os.path.splitext(base_filename)[1]
    base_name = os.path.splitext(base_filename)[0]
    clean_basename = clean_name(base_name, is_directory=False)
    if clean_basename is None:
        log_message(LogLevel.LOG_ERROR, "Failed to clean basename")
        return False

    temp_file = os.path.join(destination_dir, clean_basename + ext)

    if file_exists(destination_path):
        original_size = get_file_size(filename)
        existing_size = get_file_size(destination_path)
        if original_size > existing_size:
            log_message(LogLevel.LOG_WARNING, f"Compressed copy already exists at {destination_path}")
            return True

    if not copy_file(filename, temp_file):
        log_message(LogLevel.LOG_ERROR, f"Failed to create temporary file copy for {temp_file}")
        return False

    return compress_file(filename, temp_file)

def get_source_target_from_log(log_file):
    """
    Get source_folder and target_folder from .log file (needed for -r mode where prompt is not shown)   
    Returns:
        tuple: (source, destination) — the paths extracted from SOURCE_FOLDER and TARGET_FOLDER log entries.    
    """
    source = None
    target = None
    try:
        with open(log_file, "r") as f:
            for line in f:
                if "SOURCE_FOLDER:" in line:
                    source = line.strip().split("SOURCE_FOLDER:")[1].strip()
                elif "TARGET_FOLDER:" in line:
                    target = line.strip().split("TARGET_FOLDER:")[1].strip()
    except Exception as e:
        log_message(LogLevel.LOG_ERROR, f"Failed to extract session paths from log: {e}")
    return source, target