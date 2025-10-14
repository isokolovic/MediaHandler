import os
import sys
import stat
from file_handler import *
from logger import *

def run_media_migration(root_source, source_folder, destination_folder, processed, failed):
    """
    Recursively processes media files from source_folder and writes compressed versions to destination_folder.
    Parameters:
        root_source (str): Top-level source directory used to compute relative paths.
        source_folder (str): Current directory being processed.
        destination_folder (str): Target directory for compressed media output.
        processed (int): Processed files
        failed (int): Failed files
    Returns:
        tuple: (processed, failed) — processed and failed files
    """
    log_message(LogLevel.LOG_INFO, f"Starting media migration with source: {source_folder} and destination {destination_folder}")

    try:
        entries = os.listdir(source_folder)
    except Exception as e:
        log_message(LogLevel.LOG_ERROR, f"Failed to open a directory {source_folder}: {str(e)}")
        return processed, failed

    for entry in entries:
        full_path = os.path.join(source_folder, entry)

        try:
            st = os.stat(full_path)
        except:
            log_message(LogLevel.LOG_ERROR, f"Failed to stat {full_path}")
            continue

        if stat.S_ISDIR(st.st_mode):
            processed, failed = run_media_migration(root_source, full_path, destination_folder)
        elif is_file_type_valid(entry):
            log_message(LogLevel.LOG_INFO, f"Processing file: {entry}")
            processed += 1
            if not create_folder_process_file(root_source, destination_folder, full_path):
                failed += 1

    return processed, failed

def main():
    # Retry and organize modes
    valid_modes = {"-r", "-o"}
    args = set(sys.argv[1:])

    retry_mode = "-r" in args
    organize_mode = "-o" in args

    log_mode = "a" if retry_mode else "w"
    init_logger(LOG_FILE, log_mode)
    
    if args and not args.issubset(valid_modes):
        log_message(LogLevel.LOG_ERROR, f"Unknown argument(s): {args - valid_modes}")
        close_logger()
        sys.exit(1)

    if not retry_mode:
        source_folder = get_valid_directory("Enter the path to the source folder: ")
        if source_folder is None:
            close_logger()
            sys.exit(1)

        target_folder = get_valid_directory("Enter the path to the destination folder: ")
        if target_folder is None:
            close_logger()
            sys.exit(1)
                
        log_message(LogLevel.LOG_INFO, f"SOURCE_FOLDER: {source_folder}")
        log_message(LogLevel.LOG_INFO, f"TARGET_FOLDER: {target_folder}")

    # Processed files counters: 
    processed = 0
    failed = 0
            
    if retry_mode:
        failed_files, num_failed = get_failed_files_from_log(LOG_FILE)
        source_folder, target_folder = get_source_target_from_log(LOG_FILE)
        if num_failed > 0 and failed_files:
            retry_failed_files(source_folder, target_folder, failed_files, num_failed)            
        else:
            log_message(LogLevel.LOG_WARNING, f"No failed files found in {LOG_FILE}",)
        close_logger()
        sys.exit(0)
    elif organize_mode:
        organize_files(source_folder, source_folder, target_folder)
        close_logger()
        sys.exit(0)
    else:
        processed, failed = run_media_migration(source_folder, source_folder, target_folder, processed, failed) 

    close_logger()
    log_summary(processed, failed)

    sys.exit(0)

if __name__ == "__main__":
    main()