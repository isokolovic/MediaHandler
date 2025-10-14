import os
import pytest
from compressor import compress_image, compress_video
from file_handler import image_extensions, video_extensions, other_extensions, copy_file

def get_test_file(source_dir, ext):
    """Return the first file in source_dir matching the given extension."""
    for f in os.listdir(source_dir):
        if f.lower().endswith(ext):
            return os.path.join(source_dir, f)
    return None

@pytest.mark.parametrize("ext", image_extensions)
def test_image_compression(source_dir, target_dir, ext):
    """Compress one file for each supported image extension and verify output."""
    src = get_test_file(source_dir, ext)
    assert src, f"Test failed: no image file found for {ext}"

    dst = os.path.join(target_dir, f"compressed{ext}")
    result = compress_image(src, dst, ext)
    assert result, f"Test failed: compress_image returned {result} for {src}"
    assert os.path.exists(dst), f"Test failed: compressed file not created at {dst}"
    assert os.path.getsize(dst) > 0, f"Test failed: compressed file {dst} is empty"
    assert os.path.getsize(dst) < os.path.getsize(src), f"Test failed: compressed file {dst} is not smaller than source {src}"

@pytest.mark.parametrize("ext", video_extensions)
def test_video_compression(source_dir, target_dir, ext):
    """Compress one file for each supported video extension and verify output."""
    src = get_test_file(source_dir, ext)
    assert src, f"Test failed: no video file found for {ext}"

    dst = os.path.join(target_dir, f"compressed{ext}")
    result = compress_video(src, dst)
    assert result, f"Test failed: compress_video returned {result} for {src}"
    assert os.path.exists(dst), f"Test failed: compressed file not created at {dst}"
    assert os.path.getsize(dst) > 0, f"Test failed: compressed file {dst} is empty"
    assert os.path.getsize(dst) < os.path.getsize(src), f"Test failed: compressed file {dst} is not smaller than source {src}"

@pytest.mark.parametrize("ext", other_extensions)
def test_other_file_passthrough(source_dir, target_dir, ext):
    """Copy one file for each supported passthrough extension and verify output."""
    src = get_test_file(source_dir, ext)
    assert src, f"Test failed: no passthrough file found for {ext}"

    dst = os.path.join(target_dir, f"copied{ext}")
    result = copy_file(src, dst)
    assert result, f"Test failed: copy_file returned {result} for {src}"
    assert os.path.exists(dst), f"Test failed: copied file not created at {dst}"
    assert os.path.getsize(dst) > 0, f"Test failed: copied file {dst} is empty"
    assert os.path.getsize(dst) == os.path.getsize(src), f"Test failed: copied file {dst} size mismatch with source {src}"
