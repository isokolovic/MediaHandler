import os
import pytest
from file_handler import organize_files, get_file_creation_year, move_file_to_year_folder, retry_failed_files, image_extensions, video_extensions, other_extensions

supported_retry_failed = image_extensions + video_extensions
supported_organize = image_extensions + video_extensions + other_extensions

@pytest.mark.parametrize("ext", supported_retry_failed)
def test_zz_retry_single_extension(source_dir, target_dir, ext):
    """Retry compression for one file of the given extension."""
    failed_file = next(
        (os.path.join(source_dir, f) for f in os.listdir(source_dir) if f.lower().endswith(ext)),
        None
    )
    assert failed_file, f"No file with extension {ext} found in source_dir"

    failed = [failed_file]
    retry_failed_files(source_dir, target_dir, failed, 1)

    name = os.path.basename(failed_file)
    out_path = os.path.join(target_dir, name)
    assert os.path.exists(out_path), f"{name} was not created in target_dir"
    assert os.path.getsize(out_path) > 0, f"{name} exists but is empty"

@pytest.mark.parametrize("ext", supported_organize)
def test_zzzz_move_file_to_year_folder(source_dir, target_dir, ext):
    """Move one file of the given extension into its year folder and verify placement."""
    """Hack: test named zzzz to run last since it MOVES files from source_folder (needed for other tests)"""

    # Find one file with the given extension
    file_path = next(
        (os.path.join(source_dir, f) for f in os.listdir(source_dir) if f.lower().endswith(ext)),
        None
    )
    assert file_path, f"Test failed: no file with extension {ext} found in source_dir"

    # Extract year and move
    year = get_file_creation_year(file_path)
    assert year > 0, f"Test failed: invalid year extracted from {file_path}"

    result = move_file_to_year_folder(target_dir, file_path, year)
    assert result == 1, f"Test failed: move_file_to_year_folder returned {result} for {file_path}"

    # Verify final location
    expected_path = os.path.join(target_dir, str(year), os.path.basename(file_path))
    assert os.path.exists(expected_path), f"Test failed: {file_path} was not moved to {expected_path}"
    assert os.path.getsize(expected_path) > 0, f"Test failed: {expected_path} exists but is empty"