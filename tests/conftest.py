import os
import pytest
import shutil

SOURCE_DIR = os.path.abspath("")# Set this to the absolute path where your test media files are located
TARGET_DIR = os.path.join(SOURCE_DIR, "target")
os.makedirs(TARGET_DIR, exist_ok=True)

# Ensure target folder exists before tests run
if not os.path.isdir(TARGET_DIR):
    raise FileNotFoundError(f"TARGET_DIR not found: {TARGET_DIR}.")

@pytest.fixture(scope="session")
def source_dir():
    return SOURCE_DIR

@pytest.fixture(scope="session")
def target_dir():
    return TARGET_DIR

@pytest.fixture(scope="function", autouse=True)
def clear_target_dir_after_test(target_dir):
    """
    Clears all contents of target_dir after each test function.
    """
    yield  # Test runs here
    for f in os.listdir(target_dir):
        path = os.path.join(target_dir, f)
        if os.path.isfile(path):
            os.remove(path)
        elif os.path.isdir(path):
            shutil.rmtree(path)
