import os
from logger import init_logger, close_logger, log_message, LOG_FILE, LogLevel

def test_logger_initialization(target_dir):
    """Verify custom logger creates the log file and writes a test message."""
    log_path = os.path.join(target_dir, LOG_FILE)

    # Clean up any existing log file
    if os.path.exists(log_path):
        os.remove(log_path)

    init_logger(log_path)
    log_message(LogLevel.LOG_INFO, "Logger test message")
    close_logger()

    assert os.path.isfile(log_path), "Log file was not created"
    assert os.path.getsize(log_path) > 0, "Log file is empty"

    with open(log_path, "r") as f:
        content = f.read()
        assert "Logger test message" in content, "Log message not found in log file"
