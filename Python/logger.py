from datetime import datetime
from enum import Enum

class LogLevel(Enum):
    LOG_INFO = 1
    LOG_WARNING = 2
    LOG_ERROR = 3
    LOG_DEBUG = 4

log_file = None
LOG_FILE = "Log.log" #Log file name (and path!)

def log_summary(processed, failed):
    """Final log """
    log_message(LogLevel.LOG_INFO, f"Summary: Processed {processed}, Failed {failed}")

def close_logger():
    """Closes the log file"""
    global log_file
    if log_file:
        log_file.close()
        log_file = None

def log_file_processing(file_path, success):
    """
    Logs the file processing status — passed or failed.
    Parameters:
        file_path (str): Path to the file being processed.
        success (bool): True if compression succeeded
    """
    level = LogLevel.LOG_ERROR if not success else LogLevel.LOG_INFO
    status = "SUCCESS" if success else "FAILED"
    log_message(level, f"File Processing: {file_path} - {status}")

def init_logger(filename, mode = "w"):
    """
    Opens the log file and prepares the logger for use.
    Parameters:
        filename (str): Path to the log file.
    """
    global log_file
    try:
        log_file = open(filename, mode)
    except OSError as e:
        log_message(LogLevel.LOG_ERROR, f"Failed to open the log file {filename}: {e}")
        log_file = None

def log_message(level, format_str, *args):
    """
    Handles logging of formatted messages to both console and file.
    Parameters:
        level (LogLevel): Logging level (INFO, WARNING, ERROR, DEBUG).
        format_str (str): Format string for the log message.
        *args: Optional arguments to format into the message.
    """
    global log_file
    if not log_file:
        return
    
    level_map = {
    LogLevel.LOG_INFO: "INFO",
    LogLevel.LOG_WARNING: "WARNING",
    LogLevel.LOG_ERROR: "ERROR",
    LogLevel.LOG_DEBUG: "DEBUG" 
    }
    level_str = level_map.get(level, "UNKNOWN")

    time_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    message = format_str % args if args else format_str
    formatted = f"{time_str} - {level_str} - {message}"

    print(formatted)
    log_file.write(formatted + "\n")
    log_file.flush()